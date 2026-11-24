<!-- SPDX-License-Identifier: MIT -->
# Phase 2 — experimental, isolated from Phase 1

Neither Wine, `cpos-run` nor the installer uses these EXPERIMENTAL components. CI loads the out-of-tree module only inside a disposable QEMU guest, never on the runner host. No host deployment instructions are part of this phase.

## Userspace and loader

**HIPÓTESIS:** maintain process-local Windows handle tables that refer to Linux fds; scope named NT objects by user and do not accept a caller-supplied UID as authority. Broker policy, access masks, inheritance, duplication and namespace lifetime must be designed before exposing shared objects. For synchronous `SendMessage`, use a per-thread request queue and completion event; ntsync may signal both. Reentrancy, cancellation and thread exit are unresolved.

**HIPÓTESIS:** on x86-64, GS is the TEB base and the TEB points to the PEB. A future isolated execution runtime must preserve host TLS and restore state around Windows calls. `0x140000000` is a common preferred PE32+ image base, not a required address. Treat ImageBase as untrusted metadata; use `MAP_FIXED_NOREPLACE` if attempting fixed placement and verify the returned address. If unavailable, reserve a separate anonymous region and apply validated relocations or reject the image. Do not grant executable permissions to parser/fuzz mappings.

The first loader milestone validates x86-64 PE32+ layout, sections, relocation and import metadata, then builds a non-executable staging image. Sanitized libFuzzer runs never execute corpus bytes. The separate execution test uses a self-built MinGW fixture, not fuzz inputs or third-party programs.

**HECHO VERIFICADO:** [Phase 2 run 45147536564](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45147536564) compiled the staging loader with warnings as errors, passed the unit checks and completed 5,814,076 fuzz iterations in 21 seconds with a synthetic seed. This is not a compatibility or performance benchmark.

**HECHO VERIFICADO:** `2784a5d`, [run 45339764134](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45339764134), completed 10,000,000 parser fuzz iterations with the expanded synthetic corpus and passed the MinGW entry fixture. This run predates the single-slot static TLS fixture.

**HECHO VERIFICADO:** `6cc0b78`, [kernel job 138263213730](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45340644788/job/138263213730), passed v1/v2 layout assertions and functional ioctl tests in a disposable QEMU guest. The guest also passed FIFO, synchronous reply, owner-dispatch and close checks with the experimental event backend still enabled. The separate loader job in that run failed; this is not a green result for the whole workflow.

## Kernel boundary

**HIPÓTESIS:** `/dev/compat` starts with ABI discovery and bounded per-open handle bookkeeping only. No cross-user object namespace or message queues are implemented by the skeleton. The device remains experimental and owner-only if loaded outside this project; this project does not load it. See [ABI v1](../cpos-kernel/ABI.md) for supported requests, layouts and errors. Shared NT objects and cross-process signaling require a separate review.

**HECHO VERIFICADO:** [run 45147978579](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45147978579) linked `cpos.ko` in the Ubuntu container and compiled the userspace ABI v1 layout assertions. Nothing loaded the module or opened the device. Earlier checkout failed because the minimal container lacked CA certificates; the corrected run installed the CA bundle without disabling TLS verification.

**SIN ANALIZAR:** general Windows ABI compatibility, deployment permissions, general import loading, drivers, and cross-process handle semantics. No performance claims.

## Follow-up implementation

- **HECHO VERIFICADO:** [run 45340819917](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45340819917) passed the MinGW fixture (one static TLS slot through a minimal TEB, process-attach callback, and restoration of host GS), 10,000,000 parser fuzz iterations, and the guest ioctl and messaging jobs. The loader attempts the preferred base without replacing existing mappings, applies AMD64 DIR64 relocations, and resolves only allowlisted fixture imports. It is not a sandbox. A PEB, general DLL loading, and process/thread initialization remain absent.
- **HECHO VERIFICADO:** ABI v2 has per-open events, mutexes, semaphores and bounded single-object waits. ABI v1 remains available. Descriptor transfer remains disabled; named per-user objects, wait-any/all, and ntsync integration are not implemented.
- **HIPÓTESIS:** The initial synchronous queue dispatches on the owning thread and wakes queued senders on close. It uses pthread primitives, not the device; there is no cross-process dispatch, Windows message pumping or deadlock resolution for cyclic sends.
- **HECHO VERIFICADO:** [run 45167762949](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45167762949) reported `MinGW fixture entry: 0`, passed parser unit tests and 6,155,392 fuzz iterations, and passed the queue tests for owner dispatch, synchronous reply and close. It linked `cpos.ko` and passed compile-time layout assertions for ABI v1 and v2 without loading the module or opening the device. The layout assertions do not verify runtime ioctl behavior.
- **SIN ANALIZAR — replacing wineserver:** object namespaces and access control, process/thread lifetimes, handle inheritance/duplication, abandoned mutexes, multi-object waits and APCs, async I/O, window ownership, registry state, and compatibility with Wine's client protocol.

**HECHO VERIFICADO — historical callback-only revision:** [CI at `c69043e`](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45186319328) passed the process-attach callback fixture before static TLS was added.
