// SPDX-License-Identifier: GPL-2.0-only
/* EXPERIMENTAL: file-local token bookkeeping, not an NT object namespace. */
#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/jiffies.h>
#include <linux/magic.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/refcount.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include "cpos_abi.h"


struct cpos_file {
	struct mutex lock;
	spinlock_t state_lock;
	wait_queue_head_t waiters;
	u64 tokens[CPOS_MAX_TOKENS];
	u32 count;
	u64 next_token;
	struct cpos_object *objects[CPOS_MAX_OBJECTS];
	u32 object_count;
	u64 next_handle;
	kuid_t owner;
};

struct cpos_object {
	refcount_t refs;
	wait_queue_head_t waiters;
	u32 type;
	u32 count;
	u32 maximum;
	u64 handle;
	bool signaled;
	bool removed;
	struct task_struct *owner;
	struct file *file;
};

static bool cpos_reserved_zero(const u64 reserved[2]);
static void cpos_object_put(struct cpos_object *obj);

#define CPOS_MAX_WAIT_MS (24U * 60U * 60U * 1000U)

static bool cpos_wait_set_ready(struct cpos_file *ctx, struct cpos_object **objects,
				struct cpos_wait_set *wait, u32 *result,
				bool consume, bool *removed)
{
	unsigned long flags;
	u32 i, available_count = 0, selected = wait->count;
	bool ready;

	spin_lock_irqsave(&ctx->state_lock, flags);
	*removed = false;
	for (i = 0; i < wait->count; i++) {
		struct cpos_object *obj = objects[i];
		bool available;

		if (obj->removed) {
			*removed = true;
			break;
		}
		available = obj->type == CPOS_OBJECT_EVENT ? obj->signaled :
			obj->type == CPOS_OBJECT_MUTEX ? !obj->owner : obj->count != 0;
		if (available) {
			available_count++;
			if (selected == wait->count)
				selected = i;
		}
	}
	ready = !*removed && (wait->mode == CPOS_WAIT_ALL ? available_count == wait->count :
		available_count != 0);
	if (ready && consume) {
		*result = selected;
		for (i = 0; i < wait->count; i++) {
			struct cpos_object *obj;

			if (wait->mode == CPOS_WAIT_ANY && i != selected)
				continue;
			obj = objects[i];
			if (obj->type == CPOS_OBJECT_MUTEX) {
				obj->owner = current;
				get_task_struct(current);
			} else if (obj->type == CPOS_OBJECT_SEMAPHORE) {
				obj->count--;
			}
		}
	}
	spin_unlock_irqrestore(&ctx->state_lock, flags);
	return ready;
}

static long cpos_wait_set_ioctl(struct cpos_file *ctx, void __user *user)
{
	struct cpos_wait_set wait;
	struct cpos_object *objects[CPOS_MAX_WAIT_HANDLES];
	unsigned long deadline, remaining;
	long waited;
	u32 i, j, result = 0;
	int error = -ENOENT;
	bool ready;

	if (!uid_eq(current_fsuid(), ctx->owner))
		return -EACCES;
	if (copy_from_user(&wait, user, sizeof(wait)))
		return -EFAULT;
	if (wait.size != sizeof(wait) || wait.version != CPOS_ABI_VERSION_EXPERIMENTAL ||
	    (wait.mode != CPOS_WAIT_ANY && wait.mode != CPOS_WAIT_ALL) ||
	    !wait.count || wait.count > CPOS_MAX_WAIT_HANDLES ||
	    wait.timeout_ms > CPOS_MAX_WAIT_MS || wait.reserved0 ||
	    !cpos_reserved_zero(wait.reserved))
		return -EINVAL;
	for (i = wait.count; i < CPOS_MAX_WAIT_HANDLES; i++)
		if (wait.handles[i])
			return -EINVAL;
	mutex_lock(&ctx->lock);
	for (i = 0; i < wait.count; i++) {
		if (!wait.handles[i]) {
			error = -EINVAL;
			break;
		}
		for (j = 0; j < i; j++)
			if (wait.handles[i] == wait.handles[j])
				break;
		if (j != i) {
			error = -EINVAL;
			break;
		}
		for (j = 0; j < CPOS_MAX_OBJECTS; j++)
			if (ctx->objects[j] && ctx->objects[j]->handle == wait.handles[i])
				break;
		if (j == CPOS_MAX_OBJECTS)
			break;
		if (ctx->objects[j]->type != CPOS_OBJECT_EVENT &&
		    ctx->objects[j]->type != CPOS_OBJECT_MUTEX &&
		    ctx->objects[j]->type != CPOS_OBJECT_SEMAPHORE) {
			error = -EINVAL;
			break;
		}
		objects[i] = ctx->objects[j];
		refcount_inc(&objects[i]->refs);
	}
	mutex_unlock(&ctx->lock);
	if (i != wait.count) {
		while (i)
			cpos_object_put(objects[--i]);
		return error;
	}
	deadline = jiffies + msecs_to_jiffies(wait.timeout_ms);
	for (;;) {
		bool removed;

		ready = cpos_wait_set_ready(ctx, objects, &wait, &result, true, &removed);
		if (ready)
			break;
		if (removed) {
			for (i = 0; i < wait.count; i++)
				cpos_object_put(objects[i]);
			return -EIDRM;
		}
		remaining = time_before(jiffies, deadline) ? deadline - jiffies : 0;
		waited = wait_event_interruptible_timeout(ctx->waiters,
			cpos_wait_set_ready(ctx, objects, &wait, &result, false, &removed) || removed,
			remaining);
		if (waited < 0) {
			for (i = 0; i < wait.count; i++)
				cpos_object_put(objects[i]);
			return waited;
		}
		if (removed) {
			for (i = 0; i < wait.count; i++)
				cpos_object_put(objects[i]);
			return -EIDRM;
		}
		if (!waited)
			break;
	}
	if (!ready) {
		for (i = 0; i < wait.count; i++)
			cpos_object_put(objects[i]);
		return -ETIMEDOUT;
	}
	for (i = 0; i < wait.count; i++)
		cpos_object_put(objects[i]);
	return wait.mode == CPOS_WAIT_ANY ? result : 0;
}

static void cpos_object_put(struct cpos_object *obj)
{
	if (!refcount_dec_and_test(&obj->refs))
		return;
	if (obj->owner)
		put_task_struct(obj->owner);
	if (obj->file)
		fput(obj->file);
	kfree(obj);
}

static struct cpos_object *cpos_object_find_get(struct cpos_file *ctx, u64 handle)
{
	struct cpos_object *obj = NULL;
	u32 i;

	mutex_lock(&ctx->lock);
	for (i = 0; i < CPOS_MAX_OBJECTS; i++) {
		if (ctx->objects[i] && ctx->objects[i]->handle == handle) {
			obj = ctx->objects[i];
			refcount_inc(&obj->refs);
			break;
		}
	}
	mutex_unlock(&ctx->lock);
	return obj;
}

static int cpos_open(struct inode *inode, struct file *file)
{
	struct cpos_file *ctx;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	mutex_init(&ctx->lock);
	spin_lock_init(&ctx->state_lock);
	init_waitqueue_head(&ctx->waiters);
	ctx->next_token = 1;
	ctx->next_handle = 1;
	ctx->owner = current_fsuid();
	file->private_data = ctx;
	return 0;
}

static int cpos_release(struct inode *inode, struct file *file)
{
	struct cpos_file *ctx = file->private_data;
	u32 i;

	for (i = 0; i < CPOS_MAX_OBJECTS; i++)
		if (ctx->objects[i])
			cpos_object_put(ctx->objects[i]);
	kfree(ctx);
	return 0;
}

static long cpos_object_ioctl(struct cpos_file *ctx, unsigned int cmd,
			      void __user *user)
{
	struct cpos_object *obj;
	unsigned long flags;
	u32 i;

	if (!uid_eq(current_fsuid(), ctx->owner))
		return -EACCES;
	if (cmd == CPOS_IOC_OBJECT_CREATE) {
		struct cpos_object_request req;

		if (copy_from_user(&req, user, sizeof(req)))
			return -EFAULT;
		if (req.size != sizeof(req) || !cpos_reserved_zero(req.reserved) ||
		    req.handle || req.type < CPOS_OBJECT_EVENT || req.type > CPOS_OBJECT_FD_REF)
			return -EINVAL;
		if (req.type == CPOS_OBJECT_FD_REF)
			return -EOPNOTSUPP;
		if (req.type == CPOS_OBJECT_EVENT && (req.maximum_value || req.initial_value > 1))
			return -EINVAL;
		if (req.type == CPOS_OBJECT_MUTEX && (req.maximum_value || req.initial_value))
			return -EINVAL;
		if (req.type == CPOS_OBJECT_SEMAPHORE &&
		    (!req.maximum_value || req.initial_value > req.maximum_value))
			return -EINVAL;
		obj = kzalloc(sizeof(*obj), GFP_KERNEL);
		if (!obj)
			return -ENOMEM;
		refcount_set(&obj->refs, 1);
		init_waitqueue_head(&obj->waiters);
		obj->type = req.type;
		obj->count = req.initial_value;
		obj->maximum = req.maximum_value;
		obj->handle = req.handle;
		obj->signaled = req.initial_value != 0;
		mutex_lock(&ctx->lock);
		if (ctx->object_count == CPOS_MAX_OBJECTS || !ctx->next_handle) {
			mutex_unlock(&ctx->lock);
			cpos_object_put(obj);
			return -ENOSPC;
		}
		req.handle = ctx->next_handle++;
		obj->handle = req.handle;
		for (i = 0; i < CPOS_MAX_OBJECTS; i++)
			if (!ctx->objects[i])
				break;
		ctx->objects[i] = obj;
		ctx->object_count++;
		if (copy_to_user(user, &req, sizeof(req)))
			ctx->objects[i] = NULL;
		else {
			mutex_unlock(&ctx->lock);
			return 0;
		}
		ctx->object_count--;
		mutex_unlock(&ctx->lock);
		cpos_object_put(obj);
		return -EFAULT;
	}
	if (cmd == CPOS_IOC_OBJECT_DROP) {
		struct cpos_object_request req;

		if (copy_from_user(&req, user, sizeof(req)))
			return -EFAULT;
		if (req.size != sizeof(req) || !req.handle || req.type || req.initial_value ||
		    req.maximum_value || !cpos_reserved_zero(req.reserved))
			return -EINVAL;
		mutex_lock(&ctx->lock);
		for (i = 0; i < CPOS_MAX_OBJECTS; i++)
			if (ctx->objects[i] && ctx->objects[i]->handle == req.handle)
				break;
		if (i == CPOS_MAX_OBJECTS) {
			mutex_unlock(&ctx->lock);
			return -ENOENT;
		}
		obj = ctx->objects[i];
		ctx->objects[i] = NULL;
		ctx->object_count--;
		spin_lock_irqsave(&ctx->state_lock, flags);
		obj->removed = true;
		spin_unlock_irqrestore(&ctx->state_lock, flags);
		mutex_unlock(&ctx->lock);
		wake_up_all(&obj->waiters);
		wake_up_all(&ctx->waiters);
		cpos_object_put(obj);
		return 0;
	}
	if (cmd == CPOS_IOC_OBJECT_OP) {
		struct cpos_object_op op;
		long waited;
		unsigned long flags, deadline, remaining;
		bool ready;

		if (copy_from_user(&op, user, sizeof(op)))
			return -EFAULT;
		if (op.size != sizeof(op) || op.reserved0 || !cpos_reserved_zero(op.reserved) ||
		    !op.handle || op.timeout_ms > CPOS_MAX_WAIT_MS)
			return -EINVAL;
		obj = cpos_object_find_get(ctx, op.handle);
		if (!obj)
			return -ENOENT;
		if (op.operation == CPOS_OP_FD_GET) {
			int fd;

			if (obj->type != CPOS_OBJECT_FD_REF || !obj->file) {
				cpos_object_put(obj);
				return -EINVAL;
			}
			fd = receive_fd(obj->file, NULL, O_CLOEXEC);
			cpos_object_put(obj);
			return fd;
		}
		if (op.operation == CPOS_OP_SIGNAL && obj->type == CPOS_OBJECT_EVENT) {
			spin_lock_irqsave(&ctx->state_lock, flags);
			obj->signaled = true;
			spin_unlock_irqrestore(&ctx->state_lock, flags);
			wake_up_all(&obj->waiters);
			wake_up_all(&ctx->waiters);
			cpos_object_put(obj);
			return 0;
		}
		if (op.operation == CPOS_OP_RELEASE && obj->type == CPOS_OBJECT_EVENT) {
			spin_lock_irqsave(&ctx->state_lock, flags);
			obj->signaled = false;
			spin_unlock_irqrestore(&ctx->state_lock, flags);
			cpos_object_put(obj);
			return 0;
		}
		if (op.operation == CPOS_OP_SIGNAL && obj->type == CPOS_OBJECT_SEMAPHORE) {
			spin_lock_irqsave(&ctx->state_lock, flags);
			if (obj->count == obj->maximum) {
				spin_unlock_irqrestore(&ctx->state_lock, flags);
				cpos_object_put(obj);
				return -EOVERFLOW;
			}
			obj->count++;
			spin_unlock_irqrestore(&ctx->state_lock, flags);
			wake_up(&obj->waiters);
			wake_up_all(&ctx->waiters);
			cpos_object_put(obj);
			return 0;
		}
		if (op.operation == CPOS_OP_RELEASE && obj->type == CPOS_OBJECT_MUTEX) {
			struct task_struct *owner;

			spin_lock_irqsave(&ctx->state_lock, flags);
			if (obj->owner != current) {
				spin_unlock_irqrestore(&ctx->state_lock, flags);
				cpos_object_put(obj);
				return -EPERM;
			}
			owner = obj->owner;
			obj->owner = NULL;
			spin_unlock_irqrestore(&ctx->state_lock, flags);
			put_task_struct(owner);
			wake_up_all(&obj->waiters);
			wake_up_all(&ctx->waiters);
			cpos_object_put(obj);
			return 0;
		}
		if (op.operation != CPOS_OP_WAIT ||
		    (obj->type != CPOS_OBJECT_EVENT && obj->type != CPOS_OBJECT_MUTEX &&
		     obj->type != CPOS_OBJECT_SEMAPHORE)) {
			cpos_object_put(obj);
			return -EINVAL;
		}
		deadline = jiffies + msecs_to_jiffies(op.timeout_ms);
	wait_again:
		remaining = time_before(jiffies, deadline) ? deadline - jiffies : 0;
		waited = wait_event_interruptible_timeout(obj->waiters, ({
			spin_lock_irqsave(&ctx->state_lock, flags);
			ready = obj->removed ||
				(obj->type == CPOS_OBJECT_EVENT ? obj->signaled :
				 obj->type == CPOS_OBJECT_MUTEX ? !obj->owner : obj->count != 0);
			spin_unlock_irqrestore(&ctx->state_lock, flags);
			ready;
		}), remaining);
		if (waited < 0) {
			cpos_object_put(obj);
			return waited;
		}
		spin_lock_irqsave(&ctx->state_lock, flags);
		ready = !obj->removed && (obj->type == CPOS_OBJECT_EVENT ? obj->signaled :
			obj->type == CPOS_OBJECT_MUTEX ? !obj->owner : obj->count != 0);
		if (obj->removed) {
			spin_unlock_irqrestore(&ctx->state_lock, flags);
			cpos_object_put(obj);
			return -EIDRM;
		}
		if (ready) {
			if (obj->type == CPOS_OBJECT_MUTEX) {
				obj->owner = current;
				get_task_struct(current);
			} else if (obj->type == CPOS_OBJECT_SEMAPHORE) {
				obj->count--;
			}
		}
		spin_unlock_irqrestore(&ctx->state_lock, flags);
		if (!ready && waited > 0)
			goto wait_again;
		cpos_object_put(obj);
		return ready ? 0 : -ETIMEDOUT;
	}
	return -ENOTTY;
}

static bool cpos_reserved_zero(const u64 reserved[2])
{
	return reserved[0] == 0 && reserved[1] == 0;
}

static long cpos_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct cpos_file *ctx = file->private_data;
	void __user *user = (void __user *)arg;
	long ret;

	if (cmd == CPOS_IOC_OBJECT_CREATE || cmd == CPOS_IOC_OBJECT_DROP ||
	    cmd == CPOS_IOC_OBJECT_OP) {
		if (!uid_eq(current_fsuid(), ctx->owner))
			return -EACCES;
		ret = cpos_object_ioctl(ctx, cmd, user);
		return ret;
	}
	if (cmd == CPOS_IOC_WAIT_SET)
		return cpos_wait_set_ioctl(ctx, user);
	if (cmd == CPOS_IOC_FD_IMPORT) {
		struct cpos_fd_import req;
		struct cpos_object *obj;
		struct file *target;
		u32 i;

		if (!uid_eq(current_fsuid(), ctx->owner))
			return -EACCES;
		if (copy_from_user(&req, user, sizeof(req)))
			return -EFAULT;
		if (req.size != sizeof(req) || req.handle ||
		    !cpos_reserved_zero(req.reserved) || req.fd < 0)
			return -EINVAL;
		target = fget(req.fd);
		if (!target)
			return -EBADF;
		/* Admit only regular files from memory-backed filesystems. */
		if ((target->f_mode & FMODE_PATH) || !S_ISREG(file_inode(target)->i_mode) ||
		    (file_inode(target)->i_sb->s_magic != TMPFS_MAGIC &&
		     file_inode(target)->i_sb->s_magic != RAMFS_MAGIC)) {
			fput(target);
			return -EINVAL;
		}
		obj = kzalloc(sizeof(*obj), GFP_KERNEL);
		if (!obj) {
			fput(target);
			return -ENOMEM;
		}
		refcount_set(&obj->refs, 1);
		init_waitqueue_head(&obj->waiters);
		obj->type = CPOS_OBJECT_FD_REF;
		obj->file = target;
		mutex_lock(&ctx->lock);
		if (ctx->object_count == CPOS_MAX_OBJECTS || !ctx->next_handle) {
			mutex_unlock(&ctx->lock);
			cpos_object_put(obj);
			return -ENOSPC;
		}
		for (i = 0; i < CPOS_MAX_OBJECTS; i++)
			if (!ctx->objects[i])
				break;
		if (i == CPOS_MAX_OBJECTS) {
			mutex_unlock(&ctx->lock);
			cpos_object_put(obj);
			return -ENOSPC;
		}
		req.handle = ctx->next_handle++;
		obj->handle = req.handle;
		ctx->objects[i] = obj;
		ctx->object_count++;
		if (copy_to_user(user, &req, sizeof(req))) {
			ctx->objects[i] = NULL;
			ctx->object_count--;
			mutex_unlock(&ctx->lock);
			cpos_object_put(obj);
			return -EFAULT;
		}
		mutex_unlock(&ctx->lock);
		return 0;
	}

	if (cmd == CPOS_IOC_QUERY) {
		struct cpos_abi_info info;

		if (copy_from_user(&info, user, sizeof(info)))
			return -EFAULT;
		if (info.size != sizeof(info) || info.reserved || info.version || info.max_tokens)
			return -EINVAL;
		info.version = CPOS_ABI_VERSION;
		info.max_tokens = CPOS_MAX_TOKENS;
		if (copy_to_user(user, &info, sizeof(info)))
			return -EFAULT;
		return 0;
	}

	if (cmd == CPOS_IOC_CREATE || cmd == CPOS_IOC_DROP) {
		struct cpos_token_request req;
		u32 i;

		if (copy_from_user(&req, user, sizeof(req)))
			return -EFAULT;
		if (req.size != sizeof(req) || req.flags ||
		    !cpos_reserved_zero(req.reserved))
			return -EINVAL;
		if ((cmd == CPOS_IOC_CREATE && req.token) || (cmd == CPOS_IOC_DROP && !req.token))
			return -EINVAL;
		mutex_lock(&ctx->lock);
		if (cmd == CPOS_IOC_CREATE) {
			if (ctx->count == CPOS_MAX_TOKENS) {
				mutex_unlock(&ctx->lock);
				return -ENOSPC;
			}
			if (!ctx->next_token) {
				mutex_unlock(&ctx->lock);
				return -EOVERFLOW;
			}
			req.token = ctx->next_token++;
			if (copy_to_user(user, &req, sizeof(req))) {
				mutex_unlock(&ctx->lock);
				return -EFAULT;
			}
			ctx->tokens[ctx->count++] = req.token;
			mutex_unlock(&ctx->lock);
			return 0;
		}
		for (i = 0; i < ctx->count; i++)
			if (ctx->tokens[i] == req.token)
				break;
		if (i == ctx->count) {
			mutex_unlock(&ctx->lock);
			return -ENOENT;
		}
		ctx->tokens[i] = ctx->tokens[--ctx->count];
		mutex_unlock(&ctx->lock);
		return 0;
	}
	return -ENOTTY;
}

static const struct file_operations cpos_fops = {
	.owner = THIS_MODULE,
	.open = cpos_open,
	.release = cpos_release,
	.unlocked_ioctl = cpos_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = compat_ptr_ioctl,
#endif
};

static struct miscdevice cpos_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "compat",
	.fops = &cpos_fops,
	.mode = 0600,
};

static int __init cpos_init(void)
{
	return misc_register(&cpos_device);
}

static void __exit cpos_exit(void)
{
	misc_deregister(&cpos_device);
}

module_init(cpos_init);
module_exit(cpos_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Experimental Compatibility OS token bookkeeping device");
