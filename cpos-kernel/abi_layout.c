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
