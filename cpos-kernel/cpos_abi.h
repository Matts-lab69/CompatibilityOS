/* SPDX-License-Identifier: MIT */
#ifndef CPOS_ABI_H
#define CPOS_ABI_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define CPOS_ABI_VERSION 1
#define CPOS_MAX_TOKENS 256
#define CPOS_ABI_VERSION_EXPERIMENTAL 2
#define CPOS_MAX_OBJECTS 256
#define CPOS_MAX_WAIT_HANDLES 64

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

/* Experimental v2 payloads; v1 structures and ioctl numbers remain unchanged. */
struct cpos_object_request {
	__u32 size;
	__u32 type;
	__aligned_u64 handle;
	__u32 initial_value;
	__u32 maximum_value;
	__aligned_u64 reserved[2];
};

struct cpos_object_op {
	__u32 size;
	__u32 operation;
	__aligned_u64 handle;
	__u32 timeout_ms;
	__u32 reserved0;
	__aligned_u64 reserved[2];
};

struct cpos_wait_set {
	__u32 size;
	__u32 version;
	__u32 mode;
	__u32 count;
	__u32 timeout_ms;
	__u32 reserved0;
	__aligned_u64 handles[CPOS_MAX_WAIT_HANDLES];
	__aligned_u64 reserved[2];
};

#define CPOS_WAIT_ANY 1
#define CPOS_WAIT_ALL 2

#define CPOS_OBJECT_EVENT 1
#define CPOS_OBJECT_MUTEX 2
#define CPOS_OBJECT_SEMAPHORE 3
#define CPOS_OBJECT_FD_REF 4

#define CPOS_OP_WAIT 1
#define CPOS_OP_SIGNAL 2
#define CPOS_OP_RELEASE 3
#define CPOS_OP_FD_GET 4

#define CPOS_IOC_OBJECT_CREATE _IOWR(CPOS_IOC_MAGIC, 0x10, struct cpos_object_request)
#define CPOS_IOC_OBJECT_DROP _IOW(CPOS_IOC_MAGIC, 0x11, struct cpos_object_request)
#define CPOS_IOC_OBJECT_OP _IOWR(CPOS_IOC_MAGIC, 0x12, struct cpos_object_op)
#define CPOS_IOC_WAIT_SET _IOW(CPOS_IOC_MAGIC, 0x13, struct cpos_wait_set)

#define CPOS_IOC_MAGIC 0xc7
#define CPOS_IOC_QUERY _IOWR(CPOS_IOC_MAGIC, 0x00, struct cpos_abi_info)
#define CPOS_IOC_CREATE _IOWR(CPOS_IOC_MAGIC, 0x01, struct cpos_token_request)
#define CPOS_IOC_DROP _IOW(CPOS_IOC_MAGIC, 0x02, struct cpos_token_request)

#endif
