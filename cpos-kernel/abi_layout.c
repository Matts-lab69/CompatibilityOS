/* SPDX-License-Identifier: MIT */
#include <stddef.h>
#include "cpos_abi.h"

_Static_assert(CPOS_ABI_VERSION == 1, "ABI version");
_Static_assert(sizeof(struct cpos_abi_info) == 16, "query size");
_Static_assert(sizeof(struct cpos_token_request) == 32, "token size");
_Static_assert(offsetof(struct cpos_token_request, token) == 8, "token offset");
_Static_assert(offsetof(struct cpos_token_request, reserved) == 16, "reserved offset");
_Static_assert(_IOC_SIZE(CPOS_IOC_QUERY) == 16, "query ioctl payload size");
_Static_assert(_IOC_SIZE(CPOS_IOC_CREATE) == 32, "create ioctl payload size");
_Static_assert(_IOC_SIZE(CPOS_IOC_DROP) == 32, "drop ioctl payload size");
_Static_assert(sizeof(struct cpos_object_request) == 40, "object request size");
_Static_assert(offsetof(struct cpos_object_request, handle) == 8, "object handle offset");
_Static_assert(offsetof(struct cpos_object_request, reserved) == 24, "object reserved offset");
_Static_assert(sizeof(struct cpos_object_op) == 40, "object operation size");
_Static_assert(offsetof(struct cpos_object_op, handle) == 8, "operation handle offset");
_Static_assert(offsetof(struct cpos_object_op, reserved) == 24, "operation reserved offset");
_Static_assert(_IOC_SIZE(CPOS_IOC_OBJECT_CREATE) == 40, "object create ioctl payload size");
_Static_assert(_IOC_SIZE(CPOS_IOC_OBJECT_DROP) == 40, "object drop ioctl payload size");
_Static_assert(_IOC_SIZE(CPOS_IOC_OBJECT_OP) == 40, "object op ioctl payload size");
_Static_assert(sizeof(struct cpos_wait_set) == 552, "wait set size");
_Static_assert(offsetof(struct cpos_wait_set, handles) == 24, "wait set handle offset");
_Static_assert(offsetof(struct cpos_wait_set, reserved) == 536, "wait set reserved offset");
_Static_assert(_IOC_SIZE(CPOS_IOC_WAIT_SET) == 552, "wait set ioctl payload size");
_Static_assert(sizeof(struct cpos_fd_import) == 32, "fd import size");
_Static_assert(offsetof(struct cpos_fd_import, handle) == 8, "fd import handle offset");
_Static_assert(offsetof(struct cpos_fd_import, reserved) == 16, "fd import reserved offset");
_Static_assert(_IOC_SIZE(CPOS_IOC_FD_IMPORT) == 32, "fd import ioctl payload size");
_Static_assert(sizeof(struct cpos_named_object_request) == 96, "named object request size");
_Static_assert(offsetof(struct cpos_named_object_request, handle) == 24, "named handle offset");
_Static_assert(offsetof(struct cpos_named_object_request, name) == 32, "named name offset");
_Static_assert(_IOC_SIZE(CPOS_IOC_NAMED_OBJECT) == 96, "named ioctl payload size");
_Static_assert(sizeof(struct cpos_wait_set_v3) == 568, "v3 wait set size");
_Static_assert(offsetof(struct cpos_wait_set_v3, result_index) == 552, "v3 result offset");
_Static_assert(offsetof(struct cpos_wait_set_v3, abandoned_mask) == 560, "v3 abandoned mask offset");
_Static_assert(_IOC_SIZE(CPOS_IOC_WAIT_SET_V3) == 568, "v3 wait set ioctl size");
