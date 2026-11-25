// SPDX-License-Identifier: GPL-2.0-only
/* EXPERIMENTAL: per-open bindings over synchronization state. */
#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/jiffies.h>
#include <linux/list.h>
#include <linux/magic.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pagemap.h>
#include <linux/refcount.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/user_namespace.h>
#include <linux/wait.h>
#include "cpos_abi.h"


struct cpos_file {
	struct mutex lock;
	u64 tokens[CPOS_MAX_TOKENS];
	u32 count;
	u64 next_token;
	struct cpos_object *objects[CPOS_MAX_OBJECTS];
	u32 object_count;
	u64 next_handle;
	kuid_t owner;
	struct user_namespace *user_ns;
};

struct cpos_sync_state {
	refcount_t refs;
	struct list_head registry_node;
	struct user_namespace *user_ns;
	kuid_t named_uid;
	char name[CPOS_NAMED_NAME_MAX];
	u32 bindings;
	bool named;
	bool registered;
	u32 type;
	u32 count;
	u32 maximum;
	bool signaled;
	struct task_struct *owner;
	struct cpos_object *owner_binding;
	bool abandoned;
};

struct cpos_object {
	refcount_t refs;
	struct cpos_sync_state *sync;
	u64 handle;
	bool removed;
	struct file *file;
};

static DEFINE_SPINLOCK(cpos_state_lock);
static DECLARE_WAIT_QUEUE_HEAD(cpos_waiters);
static DEFINE_MUTEX(cpos_registry_lock);
static LIST_HEAD(cpos_named_states);
static u32 cpos_named_state_count;

static bool cpos_reserved_zero(const u64 reserved[2]);
static void cpos_object_put(struct cpos_object *obj);
static void cpos_sync_put(struct cpos_sync_state *sync);

#define CPOS_MAX_WAIT_MS (24U * 60U * 60U * 1000U)

static bool cpos_wait_set_ready(struct cpos_file *ctx, struct cpos_object **objects,
				struct cpos_wait_set *wait, u32 *result,
				bool consume, bool *removed, u64 *abandoned_mask,
				bool version3, struct cpos_wait_set_v3 *wait3,
				void __user *user, bool *copy_fault)
{
	unsigned long flags;
	u32 i, available_count = 0, selected = wait->count;
	bool ready;

	spin_lock_irqsave(&cpos_state_lock, flags);
	*removed = false;
	for (i = 0; i < wait->count; i++) {
		struct cpos_object *obj = objects[i];
		struct cpos_sync_state *sync = obj->sync;
		bool available;

		if (obj->removed) {
			*removed = true;
			break;
		}
		available = sync->type == CPOS_OBJECT_EVENT ? sync->signaled :
			sync->type == CPOS_OBJECT_MUTEX ? (!sync->owner && !sync->owner_binding) :
			sync->count != 0;
		if (available) {
			available_count++;
			if (selected == wait->count)
				selected = i;
		}
	}
	ready = !*removed && (wait->mode == CPOS_WAIT_ALL ? available_count == wait->count :
		available_count != 0);
	if (abandoned_mask)
		*abandoned_mask = 0;
	if (ready && consume && version3) {
		for (i = 0; i < wait->count; i++)
			if ((wait->mode == CPOS_WAIT_ALL || i == selected) &&
			    objects[i]->sync->type == CPOS_OBJECT_MUTEX &&
			    objects[i]->sync->named && objects[i]->sync->abandoned)
				*abandoned_mask |= 1ULL << i;
		wait3->result_index = wait->mode == CPOS_WAIT_ANY ? selected : 0;
		wait3->abandoned_mask = *abandoned_mask;
		pagefault_disable();
		if (__copy_to_user_inatomic(user, wait3, sizeof(*wait3))) {
			*copy_fault = true;
			pagefault_enable();
			spin_unlock_irqrestore(&cpos_state_lock, flags);
			return false;
		}
		pagefault_enable();
	}
	if (ready && consume) {
		*result = selected;
		for (i = 0; i < wait->count; i++) {
			struct cpos_object *obj;

			if (wait->mode == CPOS_WAIT_ANY && i != selected)
				continue;
			obj = objects[i];
			if (obj->sync->type == CPOS_OBJECT_MUTEX) {
				if (obj->sync->named) {
					obj->sync->owner_binding = obj;
					refcount_inc(&obj->refs);
					if (version3 && obj->sync->abandoned) {
						if (abandoned_mask)
							*abandoned_mask |= 1ULL << i;
						obj->sync->abandoned = false;
					}
				} else {
					obj->sync->owner = current;
					get_task_struct(current);
				}
			} else if (obj->sync->type == CPOS_OBJECT_SEMAPHORE) {
				obj->sync->count--;
			}
		}
	}
	spin_unlock_irqrestore(&cpos_state_lock, flags);
	return ready;
}

static long cpos_wait_set_ioctl(struct cpos_file *ctx, void __user *user, bool version3)
{
	struct cpos_wait_set wait;
	struct cpos_wait_set_v3 wait3;
	struct cpos_object *objects[CPOS_MAX_WAIT_HANDLES];
	unsigned long deadline, remaining;
	long waited;
	u32 i, j, k, result = 0;
	u64 abandoned_mask = 0;
	int error = -ENOENT;
	bool ready, copy_fault = false;

	if (!uid_eq(current_fsuid(), ctx->owner))
		return -EACCES;
	if (version3) {
		if (copy_from_user(&wait3, user, sizeof(wait3)))
			return -EFAULT;
		if (!access_ok(user, sizeof(wait3)) || fault_in_safe_writeable(user, sizeof(wait3)))
			return -EFAULT;
		memcpy(&wait, &wait3, sizeof(wait));
	} else if (copy_from_user(&wait, user, sizeof(wait))) {
		return -EFAULT;
	}
	if (wait.size != (version3 ? sizeof(wait3) : sizeof(wait)) ||
	    wait.version != (version3 ? CPOS_ABI_VERSION_WAIT_SET : CPOS_ABI_VERSION_EXPERIMENTAL) ||
	    (wait.mode != CPOS_WAIT_ANY && wait.mode != CPOS_WAIT_ALL) ||
	    !wait.count || wait.count > CPOS_MAX_WAIT_HANDLES ||
	    wait.timeout_ms > CPOS_MAX_WAIT_MS || wait.reserved0 ||
	    !cpos_reserved_zero(wait.reserved))
		return -EINVAL;
	if (version3 && (wait3.reserved1 || wait3.result_index || wait3.abandoned_mask))
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
		if (!ctx->objects[j]->sync) {
			error = -EINVAL;
			break;
		}
		if (ctx->objects[j]->sync->named &&
		    (ctx->objects[j]->sync->user_ns != current_user_ns() ||
		     !uid_eq(ctx->objects[j]->sync->named_uid, current_fsuid()))) {
			error = -EACCES;
			break;
		}
		if (!version3 && ctx->objects[j]->sync->type == CPOS_OBJECT_MUTEX &&
		    ctx->objects[j]->sync->named) {
			error = -EOPNOTSUPP;
			break;
		}
		objects[i] = ctx->objects[j];
		for (k = 0; k < i; k++)
			if (objects[k]->sync == objects[i]->sync && objects[i]->sync) {
				error = -EINVAL;
				break;
			}
		if (error == -EINVAL)
			break;
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

		ready = cpos_wait_set_ready(ctx, objects, &wait, &result, true, &removed,
					    version3 ? &abandoned_mask : NULL, version3,
					    version3 ? &wait3 : NULL, user, &copy_fault);
		if (copy_fault) {
			for (i = 0; i < wait.count; i++)
				cpos_object_put(objects[i]);
			return -EFAULT;
		}
		if (ready)
			break;
		if (removed) {
			for (i = 0; i < wait.count; i++)
				cpos_object_put(objects[i]);
			return -EIDRM;
		}
		remaining = time_before(jiffies, deadline) ? deadline - jiffies : 0;
		waited = wait_event_interruptible_timeout(cpos_waiters,
			cpos_wait_set_ready(ctx, objects, &wait, &result, false, &removed,
					    NULL, version3, NULL, NULL, NULL) || removed,
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
	if (obj->sync)
		cpos_sync_put(obj->sync);
	if (obj->file)
		fput(obj->file);
	kfree(obj);
}

static void cpos_sync_put(struct cpos_sync_state *sync)
{
	if (!refcount_dec_and_test(&sync->refs))
		return;
	if (sync->owner)
		put_task_struct(sync->owner);
	if (sync->user_ns)
		put_user_ns(sync->user_ns);
	kfree(sync);
}

static void cpos_named_binding_put(struct cpos_sync_state *sync)
{
	mutex_lock(&cpos_registry_lock);
	if (sync->registered && --sync->bindings == 0) {
		list_del_init(&sync->registry_node);
		cpos_named_state_count--;
		sync->registered = false;
	}
	mutex_unlock(&cpos_registry_lock);
}

static void cpos_binding_abandon(struct cpos_object *obj)
{
	unsigned long flags;
	bool owner_ref = false;

	spin_lock_irqsave(&cpos_state_lock, flags);
	if (obj->sync && obj->sync->named && obj->sync->owner_binding == obj) {
		obj->sync->owner_binding = NULL;
		obj->sync->abandoned = true;
		owner_ref = true;
	}
	spin_unlock_irqrestore(&cpos_state_lock, flags);
	if (owner_ref) {
		cpos_object_put(obj);
		wake_up_all(&cpos_waiters);
	}
}

static bool cpos_name_valid(const char name[CPOS_NAMED_NAME_MAX])
{
	u32 i;

	if (!name[0])
		return false;
	for (i = 0; i < CPOS_NAMED_NAME_MAX; i++) {
		unsigned char c = name[i];

		if (!c)
			return true;
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		      (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'))
			return false;
	}
	return false;
}

static struct cpos_sync_state *cpos_named_find(struct user_namespace *user_ns,
						       kuid_t uid, const char *name)
{
	struct cpos_sync_state *sync;

	list_for_each_entry(sync, &cpos_named_states, registry_node)
		if (sync->user_ns == user_ns && uid_eq(sync->named_uid, uid) &&
		    !strcmp(sync->name, name))
			return sync;
	return NULL;
}

static long cpos_named_object_ioctl(struct cpos_file *ctx, void __user *user)
{
	struct cpos_named_object_request req;
	struct cpos_sync_state *sync = NULL, *new_sync = NULL;
	struct cpos_object *obj;
	struct user_namespace *user_ns = current_user_ns();
	unsigned int i;
	int ret = 0;

	if (!uid_eq(current_fsuid(), ctx->owner))
		return -EACCES;
	if (ctx->user_ns != user_ns)
		return -EACCES;
	if (copy_from_user(&req, user, sizeof(req)))
		return -EFAULT;
	if (req.size != sizeof(req) || req.version != CPOS_ABI_VERSION_NAMED ||
	    req.handle || !cpos_name_valid(req.name))
		return -EINVAL;
	for (i = strnlen(req.name, CPOS_NAMED_NAME_MAX) + 1;
	     i < CPOS_NAMED_NAME_MAX; i++)
		if (req.name[i])
			return -EINVAL;
	if (req.action == CPOS_NAMED_CREATE_EXCLUSIVE) {
		if (req.type < CPOS_OBJECT_EVENT || req.type > CPOS_OBJECT_SEMAPHORE)
			return -EINVAL;
		if ((req.type == CPOS_OBJECT_EVENT &&
		     (req.maximum_value || req.initial_value > 1)) ||
		    (req.type == CPOS_OBJECT_MUTEX &&
		     (req.maximum_value || req.initial_value)) ||
		    (req.type == CPOS_OBJECT_SEMAPHORE &&
		     (!req.maximum_value || req.initial_value > req.maximum_value)))
			return -EINVAL;
	} else if (req.action == CPOS_NAMED_OPEN_EXISTING) {
		if (req.type || req.initial_value || req.maximum_value)
			return -EINVAL;
	} else {
		return -EINVAL;
	}

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj)
		return -ENOMEM;
	refcount_set(&obj->refs, 1);
	if (req.action == CPOS_NAMED_CREATE_EXCLUSIVE) {
		new_sync = kzalloc(sizeof(*new_sync), GFP_KERNEL);
		if (!new_sync) {
			kfree(obj);
			return -ENOMEM;
		}
		refcount_set(&new_sync->refs, 1);
		INIT_LIST_HEAD(&new_sync->registry_node);
		new_sync->type = req.type;
		new_sync->count = req.initial_value;
		new_sync->maximum = req.maximum_value;
		new_sync->signaled = req.initial_value != 0;
	}

	mutex_lock(&cpos_registry_lock);
	sync = cpos_named_find(user_ns, current_fsuid(), req.name);
	if (req.action == CPOS_NAMED_CREATE_EXCLUSIVE) {
		if (sync) {
			ret = -EEXIST;
			goto unlock_registry;
		}
		if (cpos_named_state_count == CPOS_MAX_OBJECTS) {
			ret = -ENOSPC;
			goto unlock_registry;
		}
		sync = new_sync;
	} else if (!sync) {
		ret = -ENOENT;
		goto unlock_registry;
	}
	mutex_lock(&ctx->lock);
	if (ctx->object_count == CPOS_MAX_OBJECTS || !ctx->next_handle) {
		ret = -ENOSPC;
		mutex_unlock(&ctx->lock);
		goto unlock_registry;
	}
	for (i = 0; i < CPOS_MAX_OBJECTS; i++)
		if (!ctx->objects[i])
			break;
	if (i == CPOS_MAX_OBJECTS) {
		ret = -ENOSPC;
		mutex_unlock(&ctx->lock);
		goto unlock_registry;
	}
	obj->sync = sync;
	obj->handle = ctx->next_handle;
	req.handle = obj->handle;
	if (copy_to_user(user, &req, sizeof(req))) {
		ret = -EFAULT;
		mutex_unlock(&ctx->lock);
		goto unlock_registry;
	}
	ctx->next_handle++;
	ctx->objects[i] = obj;
	ctx->object_count++;
	if (req.action == CPOS_NAMED_CREATE_EXCLUSIVE) {
		sync->named = true;
		sync->registered = true;
		sync->named_uid = current_fsuid();
		sync->user_ns = get_user_ns(user_ns);
		strscpy(sync->name, req.name, sizeof(sync->name));
		sync->bindings = 1;
		list_add(&sync->registry_node, &cpos_named_states);
		cpos_named_state_count++;
		new_sync = NULL;
	} else {
		refcount_inc(&sync->refs);
		sync->bindings++;
	}
	mutex_unlock(&ctx->lock);
	mutex_unlock(&cpos_registry_lock);
	return 0;

unlock_registry:
	mutex_unlock(&cpos_registry_lock);
	if (new_sync)
		cpos_sync_put(new_sync);
	kfree(obj);
	return ret;
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
	ctx->next_token = 1;
	ctx->next_handle = 1;
	ctx->owner = current_fsuid();
	ctx->user_ns = get_user_ns(current_user_ns());
	file->private_data = ctx;
	return 0;
}

static int cpos_release(struct inode *inode, struct file *file)
{
	struct cpos_file *ctx = file->private_data;
	u32 i;

	for (i = 0; i < CPOS_MAX_OBJECTS; i++)
		if (ctx->objects[i]) {
			cpos_binding_abandon(ctx->objects[i]);
			if (ctx->objects[i]->sync)
				cpos_named_binding_put(ctx->objects[i]->sync);
			cpos_object_put(ctx->objects[i]);
		}
	put_user_ns(ctx->user_ns);
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
		obj->sync = kzalloc(sizeof(*obj->sync), GFP_KERNEL);
		if (!obj->sync) {
			kfree(obj);
			return -ENOMEM;
		}
		refcount_set(&obj->refs, 1);
		refcount_set(&obj->sync->refs, 1);
		obj->sync->type = req.type;
		obj->sync->count = req.initial_value;
		obj->sync->maximum = req.maximum_value;
		obj->handle = req.handle;
		obj->sync->signaled = req.initial_value != 0;
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
		spin_lock_irqsave(&cpos_state_lock, flags);
		obj->removed = true;
		spin_unlock_irqrestore(&cpos_state_lock, flags);
		mutex_unlock(&ctx->lock);
		cpos_binding_abandon(obj);
		wake_up_all(&cpos_waiters);
		if (obj->sync)
			cpos_named_binding_put(obj->sync);
		cpos_object_put(obj);
		return 0;
	}
	if (cmd == CPOS_IOC_OBJECT_OP) {
		struct cpos_object_op op;
		long waited;
		unsigned long flags, deadline, remaining;
		bool ready, abandoned = false;

		if (copy_from_user(&op, user, sizeof(op)))
			return -EFAULT;
		if (op.size != sizeof(op) || op.reserved0 || !cpos_reserved_zero(op.reserved) ||
		    !op.handle || op.timeout_ms > CPOS_MAX_WAIT_MS)
			return -EINVAL;
		obj = cpos_object_find_get(ctx, op.handle);
		if (!obj)
			return -ENOENT;
		if (obj->sync && obj->sync->named &&
		    (obj->sync->user_ns != current_user_ns() ||
		     !uid_eq(obj->sync->named_uid, current_fsuid()))) {
			cpos_object_put(obj);
			return -EACCES;
		}
		if (op.operation == CPOS_OP_FD_GET) {
			int fd;

			if (obj->sync || !obj->file) {
				cpos_object_put(obj);
				return -EINVAL;
			}
			fd = receive_fd(obj->file, NULL, O_CLOEXEC);
			cpos_object_put(obj);
			return fd;
		}
		if (obj->sync && op.operation == CPOS_OP_SIGNAL && obj->sync->type == CPOS_OBJECT_EVENT) {
			spin_lock_irqsave(&cpos_state_lock, flags);
			obj->sync->signaled = true;
			spin_unlock_irqrestore(&cpos_state_lock, flags);
			wake_up_all(&cpos_waiters);
			cpos_object_put(obj);
			return 0;
		}
		if (obj->sync && op.operation == CPOS_OP_RELEASE && obj->sync->type == CPOS_OBJECT_EVENT) {
			spin_lock_irqsave(&cpos_state_lock, flags);
			obj->sync->signaled = false;
			spin_unlock_irqrestore(&cpos_state_lock, flags);
			cpos_object_put(obj);
			return 0;
		}
		if (obj->sync && op.operation == CPOS_OP_SIGNAL && obj->sync->type == CPOS_OBJECT_SEMAPHORE) {
			spin_lock_irqsave(&cpos_state_lock, flags);
			if (obj->sync->count == obj->sync->maximum) {
				spin_unlock_irqrestore(&cpos_state_lock, flags);
				cpos_object_put(obj);
				return -EOVERFLOW;
			}
			obj->sync->count++;
			spin_unlock_irqrestore(&cpos_state_lock, flags);
			wake_up_all(&cpos_waiters);
			cpos_object_put(obj);
			return 0;
		}
		if (obj->sync && op.operation == CPOS_OP_RELEASE && obj->sync->type == CPOS_OBJECT_MUTEX) {
			struct task_struct *owner;
			bool binding_owner;

			spin_lock_irqsave(&cpos_state_lock, flags);
			binding_owner = obj->sync->named;
			if ((binding_owner && obj->sync->owner_binding != obj) ||
			    (!binding_owner && obj->sync->owner != current)) {
				spin_unlock_irqrestore(&cpos_state_lock, flags);
				cpos_object_put(obj);
				return -EPERM;
			}
			owner = binding_owner ? NULL : obj->sync->owner;
			if (binding_owner)
				obj->sync->owner_binding = NULL;
			else
				obj->sync->owner = NULL;
			spin_unlock_irqrestore(&cpos_state_lock, flags);
			if (owner)
				put_task_struct(owner);
			wake_up_all(&cpos_waiters);
			if (binding_owner)
				cpos_object_put(obj);
			cpos_object_put(obj);
			return 0;
		}
		if (op.operation != CPOS_OP_WAIT || !obj->sync) {
			cpos_object_put(obj);
			return -EINVAL;
		}
		deadline = jiffies + msecs_to_jiffies(op.timeout_ms);
	wait_again:
		remaining = time_before(jiffies, deadline) ? deadline - jiffies : 0;
		waited = wait_event_interruptible_timeout(cpos_waiters, ({
			spin_lock_irqsave(&cpos_state_lock, flags);
			ready = obj->removed ||
				(obj->sync->type == CPOS_OBJECT_EVENT ? obj->sync->signaled :
				obj->sync->type == CPOS_OBJECT_MUTEX ?
				(!obj->sync->owner && !obj->sync->owner_binding) : obj->sync->count != 0);
			spin_unlock_irqrestore(&cpos_state_lock, flags);
			ready;
		}), remaining);
		if (waited < 0) {
			cpos_object_put(obj);
			return waited;
		}
		spin_lock_irqsave(&cpos_state_lock, flags);
		ready = !obj->removed && (obj->sync->type == CPOS_OBJECT_EVENT ? obj->sync->signaled :
			obj->sync->type == CPOS_OBJECT_MUTEX ?
			(!obj->sync->owner && !obj->sync->owner_binding) : obj->sync->count != 0);
		if (obj->removed) {
			spin_unlock_irqrestore(&cpos_state_lock, flags);
			cpos_object_put(obj);
			return -EIDRM;
		}
		if (ready) {
			if (obj->sync->type == CPOS_OBJECT_MUTEX) {
				if (obj->sync->named) {
					obj->sync->owner_binding = obj;
					refcount_inc(&obj->refs);
					abandoned = obj->sync->abandoned;
					obj->sync->abandoned = false;
				} else {
					obj->sync->owner = current;
					get_task_struct(current);
				}
			} else if (obj->sync->type == CPOS_OBJECT_SEMAPHORE) {
				obj->sync->count--;
			}
		}
		spin_unlock_irqrestore(&cpos_state_lock, flags);
		if (!ready && waited > 0)
			goto wait_again;
		cpos_object_put(obj);
		return ready ? (abandoned ? -EOWNERDEAD : 0) : -ETIMEDOUT;
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
		return cpos_wait_set_ioctl(ctx, user, false);
	if (cmd == CPOS_IOC_WAIT_SET_V3)
		return cpos_wait_set_ioctl(ctx, user, true);
	if (cmd == CPOS_IOC_NAMED_OBJECT)
		return cpos_named_object_ioctl(ctx, user);
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
