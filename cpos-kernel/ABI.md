<!-- SPDX-License-Identifier: MIT -->
# Experimental `/dev/compat` ABI (v1–v3)

This out-of-tree GPL-2.0-only module provides experimental synchronization objects. Building it does
not load or install it. The experimental userspace queue can optionally use a v2
event; Wine, the installer and `cpos-run` do not use this interface.
Device access is mode `0600`; every open receives an independent token table,
freed on close. Tokens and anonymous objects are file-local. V3 names allow
independent opens to share synchronization state without sharing handle values.

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
increments and wait decrements. Generic object creation still rejects
`FD_REF`; use the separate descriptor import command below. `CPOS_IOC_OBJECT_OP` uses
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
priority inheritance. These rules describe anonymous v2 objects; named v3
bindings follow the separate lifetime and authorization rules below.

`CPOS_IOC_WAIT_SET` (`_IOW(0xc7, 0x13, struct cpos_wait_set)`) takes a
552-byte input with `version=2`, `count=1..64`, distinct nonzero per-open
handles, the same timeout as `WAIT`, and zero unused handles and reserved
fields. `CPOS_WAIT_ANY` returns the input index of the first available object;
`CPOS_WAIT_ALL` returns zero and acquires all objects atomically, or none on
failure. Manual-reset events stay signaled. A dropped object wakes the waiter
and returns `EIDRM`. Recursive or abandoned mutexes and Windows APC semantics
are not implemented. `WAIT_SET` is the v2 request and retains its original
input-only layout and return convention; the separately numbered `WAIT_SET_V3`
is additive and does not change v1, v2, or the query-reported ABI version.
**HECHO VERIFICADO:** the [final guest job](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45427394757/job/138563022465) passed the wait-set smoke tests.

`CPOS_IOC_FD_IMPORT` (`_IOWR(0xc7, 0x14, struct cpos_fd_import)`) takes a
32-byte input with an open descriptor and zero handle/reserved fields. It
returns a handle in the same open table. Only regular tmpfs/ramfs files are
accepted; `O_PATH`, devices and other filesystem types are excluded. A
`CPOS_OP_FD_GET` operation on that handle returns a close-on-exec descriptor
through Linux `receive_fd`, which checks LSM policy. Dropping the handle
releases the retained file reference, not any descriptor already returned.
File references cannot participate in waits or be looked up by another open.
**HECHO VERIFICADO:** the same guest job passed the restricted file-reference smoke test on the CI kernel.

## Additive v3 named bindings

`CPOS_IOC_NAMED_OBJECT` (`_IOWR(0xc7, 0x15, struct cpos_named_object_request)`)
uses a fixed 96-byte request (`size=96`, `version=3`, zero input `handle`, and a
NUL-terminated ASCII name of 1–63 characters from `[A-Za-z0-9_.-]`; bytes after
the terminator must be zero). The key is the current user-namespace identity,
current filesystem UID, and exact case-sensitive name. V1 and v2 layouts and
commands are unchanged.

`action=CPOS_NAMED_CREATE_EXCLUSIVE` supplies an event, mutex, or semaphore type
and its normal v2 initial/maximum values. It returns a per-open handle and fails
with `EEXIST` if the key is present. `action=CPOS_NAMED_OPEN_EXISTING` requires
zero type/value inputs and returns a new per-open handle for an existing key;
missing names fail with `ENOENT`. The registry holds at most 256 live names.
Each successful binding is independent, and the registry entry is removed as
soon as the last binding is dropped or its device open is released. In-flight
operations retain state references so removal cannot free state prematurely.
There is no unlink-only operation. The namespace is not discoverable by name
outside these operations.

Two handles in one wait set that resolve to the same synchronization state are
rejected with `EINVAL`, because acquiring one state twice would make wait-all
ambiguous. Mutex owner-abandonment behavior remains as documented above and is
unchanged for anonymous objects.

For named mutexes, ownership belongs to the specific binding that acquired the
mutex. Dropping that binding or closing its `/dev/compat` open while it owns the
mutex abandons it, releases ownership, marks it abandoned, and wakes waiters.
The next successful acquisition consumes the abandoned indication and reports
it while still acquiring the mutex. Simple `CPOS_OP_WAIT` returns `EOWNERDEAD`
for that acquisition; the caller must treat the mutex as acquired and may use
`CPOS_OP_RELEASE` on the same binding. The implementation does not detect task
exit while the owning binding remains open.

Older `CPOS_IOC_WAIT_SET` (v2) requests that include a named mutex fail with
`EOPNOTSUPP` rather than consume an abandonment indication they cannot report.
Simple `CPOS_OP_WAIT` remains supported because its `EOWNERDEAD` result is
unambiguous. `CPOS_IOC_WAIT_SET_V3` uses a separate 568-byte layout and returns
the `WAIT_ANY` index in `result_index` (zero for `WAIT_ALL`); `abandoned_mask`
marks each acquired abandoned mutex by input index. Both output fields must be
zero on input. Output is published before object state is consumed; if output
memory is inaccessible, the call returns `EFAULT` without consuming objects or
clearing abandonment indications. Output bytes are unspecified on any error.
Anonymous mutex behavior and the v1/v2 layouts and return values are unchanged.

V3 sharing is limited to the documented user namespace and filesystem UID.
It does not transfer handles or file references across independent opens.
Device mode `0600` is an additional restriction, not a substitute for object
authorization. Blocking waits do not hold the per-open table mutex.

This ABI is experimental and may change before any deployment.
An incompatible layout or command change requires a new ABI version and ioctl
number rather than silently reinterpreting a v1 request. CI compiles the
layout assertions as ordinary userspace C and separately verifies the v1 query
and basic v2 and v3 object behavior inside a disposable guest.
