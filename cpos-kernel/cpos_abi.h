/* SPDX-License-Identifier: MIT */
#ifndef CPOS_ABI_H
#define CPOS_ABI_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define CPOS_ABI_VERSION 1
#define CPOS_MAX_TOKENS 256

struct cpos_abi_info {
	__u32 size;
	__u32 version;
	__u32 max_tokens;
	__u32 reserved;
};

struct cpos_token_request {
	__u32 size;
	__u32 flags;
	__aligned_u64 token;
	__aligned_u64 reserved[2];
};

#define CPOS_IOC_MAGIC 0xc7
#define CPOS_IOC_QUERY _IOWR(CPOS_IOC_MAGIC, 0x00, struct cpos_abi_info)
#define CPOS_IOC_CREATE _IOWR(CPOS_IOC_MAGIC, 0x01, struct cpos_token_request)
#define CPOS_IOC_DROP _IOW(CPOS_IOC_MAGIC, 0x02, struct cpos_token_request)

#endif
