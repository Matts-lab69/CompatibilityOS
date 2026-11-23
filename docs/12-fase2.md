<!-- SPDX-License-Identifier: MIT -->
# Phase 2 — experimental, isolated from Phase 1

Neither `cpos-run` nor the installer uses these components. CI may build an out-of-tree Linux module but never load it. No host deployment instructions are part of this phase.

## Userspace and loader

**HIPÓTESIS:** maintain process-local Windows handle tables that refer to Linux fds; scope named NT objects by user and do not accept a caller-supplied UID as authority. Broker policy, access masks, inheritance, duplication and namespace lifetime must be designed before exposing shared objects. For synchronous `SendMessage`, use a per-thread request queue and completion event; ntsync may signal both. Reentrancy, cancellation and thread exit are unresolved.

**HIPÓTESIS:** on x86-64, GS is the TEB base and the TEB points to the PEB. A future isolated execution runtime must preserve host TLS and restore state around Windows calls. `0x140000000` is a common preferred PE32+ image base, not a required address. Treat ImageBase as untrusted metadata; use `MAP_FIXED_NOREPLACE` if attempting fixed placement and verify the returned address. If unavailable, reserve a separate anonymous region and apply validated relocations or reject the image. Do not grant executable permissions to parser/fuzz mappings.

The first loader milestone validates x86-64 PE32+ layout, sections, relocation and import metadata, then builds a non-executable staging image. Resolving DLLs, executing entry points, setting GS and emulating Windows services are separate compatibility milestones. Sanitized unit tests and a bounded libFuzzer run use synthetic inputs; CI never executes bytes from those inputs.

**HECHO VERIFICADO:** [Phase 2 run 45147536564](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45147536564) compiled the staging loader with warnings as errors, passed the unit checks and completed 5,814,076 fuzz iterations in 21 seconds with a synthetic seed. This is not a compatibility or performance benchmark.

## Kernel boundary

**HIPÓTESIS:** `/dev/compat` starts with ABI discovery and bounded per-open handle bookkeeping only. No cross-user object namespace or message queues are implemented by the skeleton. The device remains experimental and owner-only if loaded outside this project; this project does not load it. See [ABI v1](../cpos-kernel/ABI.md) for supported requests, layouts and errors. Shared NT objects and cross-process signaling require a separate review.

**SIN ANALIZAR:** Windows ABI compatibility, deployment permissions, loader TLS and imports at execution time, drivers, messaging, and cross-process handle semantics. No performance claims.
