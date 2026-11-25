<!-- SPDX-License-Identifier: MIT -->
# Phase 2 — experimental, isolated from Phase 1

Neither Wine, `cpos-run` nor the installer uses these EXPERIMENTAL components. CI loads the out-of-tree module only inside a disposable QEMU guest, never on the runner host. No host deployment instructions are part of this phase.

| Area | Implemented and verified at `083336c` | Scaffolding / missing before replacing wineserver |
| --- | --- | --- |
| PE | [Synthetic PE fixtures and parser fuzzing](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45427394757/job/138563022265): x86-64 mapping, relocations, a minimal TEB/PEB, a TLS slot and an allowlisted import. | General DLL and process initialization, thread lifecycle and Windows ABI coverage. |
| Objects | [QEMU smoke](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45427394757/job/138563022465): file-local objects, atomic wait sets and restricted file references. | Named sharing, abandoned mutexes, access masks, inheritance and asynchronous I/O. |
| Messages and ABI | [QEMU smoke](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45427394757/job/138563022465): versioned ioctl checks and synchronous owner-thread queue with guest event signaling. | Win32 message pumping, reentrancy, cross-process dispatch and Wine client protocol compatibility. |

**SIN ANALIZAR — ntsync equivalence:** the [Linux 6.18 reference](https://docs.kernel.org/6.18/userspace-api/ntsync.html) supports explicit mutex abandonment, recursive ownership, and an optional alert object. The verified v2 configuration above has none of these. Its semaphore signal adds one, its wait timeout is relative, and duplicate handles are rejected in both wait-set modes. The historical guest checks cover acquisition and timeout without partial consumption, not equivalence under contention or owner death.

## Userspace and loader

**HIPÓTESIS:** maintain process-local Windows handle tables that refer to Linux fds; scope named NT objects by user and do not accept a caller-supplied UID as authority. Broker policy, access masks, inheritance, duplication and namespace lifetime must be designed before exposing shared objects. For synchronous `SendMessage`, use a per-thread request queue and completion event; ntsync may signal both. Reentrancy, cancellation and thread exit are unresolved.

**HIPÓTESIS:** on x86-64, GS is the TEB base and the TEB points to the PEB. A future isolated execution runtime must preserve host TLS and restore state around Windows calls. `0x140000000` is a common preferred PE32+ image base, not a required address. Treat ImageBase as untrusted metadata; use `MAP_FIXED_NOREPLACE` if attempting fixed placement and verify the returned address. If unavailable, reserve a separate anonymous region and apply validated relocations or reject the image. Do not grant executable permissions to parser/fuzz mappings.

The first loader milestone validates x86-64 PE32+ layout, sections, relocation and import metadata, then builds a non-executable staging image. Sanitized libFuzzer runs never execute corpus bytes. The separate execution test uses a self-built MinGW fixture, not fuzz inputs or third-party programs.

**HECHO VERIFICADO:** [Phase 2 run 45147536564](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45147536564) compiled the staging loader with warnings as errors, passed the unit checks and completed 5,814,076 fuzz iterations in 21 seconds with a synthetic seed. This is not a compatibility or performance benchmark.

**HECHO VERIFICADO:** `2784a5d`, [run 45339764134](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45339764134), completed 10,000,000 parser fuzz iterations with the expanded synthetic corpus and passed the MinGW entry fixture. This run predates the single-slot static TLS fixture.

**HECHO VERIFICADO:** `6cc0b78`, [kernel job 138263213730](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45340644788/job/138263213730), passed v1/v2 layout assertions and functional ioctl tests in a disposable QEMU guest. The guest also passed FIFO, synchronous reply, owner-dispatch and close checks with the experimental event backend still enabled. The separate loader job in that run failed; this is not a green result for the whole workflow.

## Kernel boundary

The experimental `/dev/compat` device has bounded per-open handle tables and no cross-user object namespace. See the [versioned ABI](../cpos-kernel/ABI.md) for requests, layouts, errors and access rules. Sharing named synchronization state does not imply support for arbitrary NT objects, cross-user signaling or transferring handles between opens.

**HECHO VERIFICADO:** [run 45147978579](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45147978579) linked `cpos.ko` in the Ubuntu container and compiled the userspace ABI v1 layout assertions. Nothing loaded the module or opened the device. Earlier checkout failed because the minimal container lacked CA certificates; the corrected run installed the CA bundle without disabling TLS verification.

**SIN ANALIZAR:** general Windows ABI compatibility, deployment permissions, general import loading, drivers, and cross-process handle semantics. No performance claims.

## Follow-up implementation

### Closure checks for the next revision

- The PE execution job must build its own MinGW fixtures and check direct DLL imports, forwarded exports, delay imports, and DLL attach/detach order. A delay-import directory alone does not prove lazy binding: verify that the slot is unresolved before the first call, that the first call reaches the expected fixture export, and that a second call reuses the binding. Fuzzing must remain non-executable and complete at least 10,000,000 iterations independently of the execution fixtures.
- The guest job must exercise independent opens, name and handle lifetime, mutex abandonment, semaphore bounds, and atomic waits without loading the module on the runner host. ABI v1 and v2 layout and behavior checks remain mandatory.
- **SIN ANALIZAR:** neither historical job success below nor successful compilation alone verifies this expanded scope. Record the tested commit and the jobs that actually ran before marking it verified.

**Pending CI — v3 synchronization scope:** named objects share state between independent opens in the same user namespace and filesystem UID; handles remain local to each open. Named mutex abandonment is tied to dropping or closing the acquiring binding, not to task exit while that binding remains open. A single wait reports `EOWNERDEAD` after acquiring the mutex; the v3 wait set reports abandonment by input index. This is not equivalent to ntsync's explicit mutex-kill operation or recursive ownership.

**Pending CI — DLL fixture scope:** the loader accepts only explicitly supplied fixture modules and does not search for DLLs. The MinGW checks exercise ordinary imports, a forwarded export and dependency-first attach. The delay fixture checks the import slot before and after the first call and verifies that a second call does not resolve it again. The module itself is initialized before the first call; this is not lazy module loading or a general Windows loader.

- **HECHO VERIFICADO:** [run 45342439841](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45342439841) passed the MinGW fixture (one static TLS slot, a minimal TEB and PEB exposing the mapped ImageBase, a process-attach callback, and restoration of host GS), 10,000,000 parser fuzz iterations, and the guest ioctl and messaging jobs. The loader attempts the preferred base without replacing existing mappings, applies AMD64 DIR64 relocations, and resolves only allowlisted fixture imports. It is not a sandbox. General DLL loading and process/thread initialization remain absent.
- **HECHO VERIFICADO:** The [final guest job](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45427394757/job/138563022465) passed ABI layout checks and functional tests for v1 tokens, v2 per-open objects, wait sets and restricted regular-file references. Cross-open descriptor sharing and ntsync integration are not implemented.
- **HECHO VERIFICADO:** The synchronous queue dispatches on the owning thread and wakes queued senders on close. It uses pthreads by default; the optional v2 event signals the owner, while pthreads still protect queue state and replies. There is no cross-process dispatch, Windows message pumping or deadlock resolution for cyclic sends.
- **HECHO VERIFICADO:** [run 45167762949](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45167762949) reported `MinGW fixture entry: 0`, passed parser unit tests and 6,155,392 fuzz iterations, and passed the queue tests for owner dispatch, synchronous reply and close. It linked `cpos.ko` and passed compile-time layout assertions for ABI v1 and v2 without loading the module or opening the device. The layout assertions do not verify runtime ioctl behavior.
- **SIN ANALIZAR — replacing wineserver:** object namespaces and access control, process/thread lifetimes, handle inheritance/duplication, abandoned mutexes, multi-object waits and APCs, async I/O, window ownership, registry state, and compatibility with Wine's client protocol.

**HECHO VERIFICADO — historical callback-only revision:** [CI at `c69043e`](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45186319328) passed the process-attach callback fixture before static TLS was added.

**HECHO VERIFICADO:** `15bb1de`, [run 45343548109](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45343548109), also passed a separate MinGW fixture calling an allowlisted import. CI compared the exact output bytes against `CPOS PE hello` plus a newline; this does not exercise Windows DLL loading.
