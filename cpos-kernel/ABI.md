<!-- SPDX-License-Identifier: MIT -->
# Experimental `/dev/compat` ABI (version 1)

This out-of-tree GPL-2.0-only module is a bookkeeping skeleton. Building it does
not load or install it. No project component currently uses this interface.
Device access is mode `0600`; every open receives an independent token table,
freed on close. There is no global namespace, sharing, or cross-open access.

All requests use native fixed-width integer fields and must provide the exact
`size` shown below. Reserved fields and unsupported flags must be zero. Invalid
layouts/fields return `EINVAL`, inaccessible userspace memory returns `EFAULT`,
unknown commands return `ENOTTY`.

## Requests

`CPOS_IOC_QUERY` (`_IOWR(0xc7, 0x00, struct cpos_abi_info)`): input `size` is
16 and all other fields are zero. Output fields are `size=16`, `version=1`,
`max_tokens=256`, `reserved=0`.

`CPOS_IOC_CREATE` (`_IOWR(0xc7, 0x01, struct cpos_token_request)`): input has
`size=32`, `flags=0`, `token=0`, and both reserved words zero. Returns a
nonzero opaque 64-bit token in `token`. The token is only an identifier in this
open file's bounded table; it is not a kernel address, Linux fd, or resource
reference. Errors: `ENOSPC` at capacity; `EOVERFLOW` if identifiers exhaust.

`CPOS_IOC_DROP` (`_IOW(0xc7, 0x02, struct cpos_token_request)`): requires
`size=32`, `flags=0`, a nonzero token from this open file, and zero reserved
words. Removes that token; unknown tokens return `ENOENT`.

Layouts (canonical definitions in [`cpos_abi.h`](cpos_abi.h)):

```c
struct cpos_abi_info {
    __u32 size, version, max_tokens, reserved;
};
struct cpos_token_request {
    __u32 size, flags;
    __aligned_u64 token;
    __aligned_u64 reserved[2];
};
```

No cross-user objects, handle semantics, access masks, or signaling are
implemented. This ABI is experimental and may change before any deployment.
