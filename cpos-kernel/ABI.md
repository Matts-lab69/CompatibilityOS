<!-- SPDX-License-Identifier: MIT -->
# Experimental `/dev/compat` ABI (version 1)

This out-of-tree GPL-2.0-only module provides experimental file-local objects. Building it does
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

V1 tokens remain bookkeeping identifiers only; they do not implement
cross-open handles or access masks. The v2 ioctl commands below are implemented
as experimental per-open objects by the current kernel module. Unknown ioctl
commands still return `ENOTTY`.

## Experimental v2 objects

V1 commands and layouts above remain unchanged; `CPOS_IOC_QUERY` continues to
report version 1 for compatibility. Commands `0x10` to `0x12` implement the
experimental v2 payloads. `cpos_object_request` and `cpos_object_op` are each
40 bytes; canonical layouts and assertions are in `cpos_abi.h` and
`abi_layout.c`. All objects are capped at 256 per open; handles are opaque,
monotonically allocated 64-bit values and are not kernel addresses.

`CPOS_IOC_OBJECT_CREATE` takes `size=40`, `handle=0`, zero reserved words, a
type, and type-specific initial/max values. Event uses initial 0 or 1 and max
0; it is manual-reset/latched (`SIGNAL` sets, `RELEASE` resets). Mutex uses
initial 0 and max 0; successful wait acquires it and only the owning Linux task
may `RELEASE` it. Semaphore requires max > 0 and initial <= max; `SIGNAL`
increments and wait decrements. `FD_REF` and `FD_GET` are reserved and return
`EOPNOTSUPP`; descriptor transfer needs LSM authorization and a lifetime
model that prevents indirect reference cycles. `CPOS_IOC_OBJECT_OP` uses
`handle` and operation; `WAIT` accepts timeout milliseconds (zero polls, max
24 hours), interrupted waits return `EINTR`, timed-out waits `ETIMEDOUT`, and a
waiter whose object is dropped returns `EIDRM`. `SIGNAL` applies to event and
semaphore; `RELEASE` resets events or releases an owned mutex. Dropping or
closing the owning `/dev/compat` open releases its object reference.

Each open captures its filesystem UID; v2 ioctl calls from a different
filesystem UID fail `EACCES`. This blocks access after credential changes but
does not distinguish processes sharing the same UID and is not a named/global
namespace. Device mode `0600` remains an additional access restriction. Object
table operations use a per-open mutex; blocking waits release it before sleep,
use wait queues and a state spinlock, and hold an object reference across the
wait. Mutex ownership is task-based and is not robust: task exit while holding
the mutex does not auto-release it, and there is no deadlock detection or
priority inheritance. Objects cannot be shared across independent opens or
user namespaces; there is no global naming or cross-user sharing.

The present namespace is per-open-file, which is stricter than per-user
isolation and prevents sharing even between two opens by the same user. There
is no global named namespace, cross-process sharing, or user-namespace mapping;
adding one requires explicit ownership checks and lifetime rules. The
`/dev/compat` mode `0600` is an additional device-access restriction, not a
substitute for object authorization. Any enabled wait implementation must use
kernel wait queues with interruptible/time-bounded waits and must not sleep
while holding the per-file table mutex. Kernel mutex objects require defined
owner/deadlock semantics; mapping them to binary semaphores is not equivalent
to an ownership-tracking mutex.

This ABI is experimental and may change before any deployment.
An incompatible layout or command change requires a new ABI version and ioctl
number rather than silently reinterpreting a v1 request. CI compiles the
layout assertions as ordinary userspace C without opening the device.
