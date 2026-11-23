// SPDX-License-Identifier: GPL-2.0-only
/* EXPERIMENTAL: file-local token bookkeeping, not an NT object namespace. */
#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "cpos_abi.h"

struct cpos_file {
	struct mutex lock;
	u64 tokens[CPOS_MAX_TOKENS];
	u32 count;
	u64 next_token;
};

static int cpos_open(struct inode *inode, struct file *file)
{
	struct cpos_file *ctx;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	mutex_init(&ctx->lock);
	ctx->next_token = 1;
	file->private_data = ctx;
	return 0;
}

static int cpos_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	return 0;
}

static bool cpos_reserved_zero(const u64 reserved[2])
{
	return reserved[0] == 0 && reserved[1] == 0;
}

static long cpos_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct cpos_file *ctx = file->private_data;
	void __user *user = (void __user *)arg;

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
