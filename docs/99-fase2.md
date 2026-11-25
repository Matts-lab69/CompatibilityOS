<!-- SPDX-License-Identifier: MIT -->

# Phase 2 — archived research notes

These notes predate the [authorized experimental design](12-fase2.md). No local module loading, installation, DKMS registration, or changes to the user's host are authorized. Linux remains monolithic; the notes below do not establish feasibility or benefit.

**VERIFIED FACT — starting references only.** In Wine 11.20, [`server/protocol.def:1127–1174`](https://github.com/wine-mirror/wine/blob/7298a818b79446bae153b8d5aa27235ab3703bf9/server/protocol.def#L1127-L1174) defines process and thread requests; [`dlls/ntdll/unix/server.c:743`](https://github.com/wine-mirror/wine/blob/7298a818b79446bae153b8d5aa27235ab3703bf9/dlls/ntdll/unix/server.c#L743) gives a `SERVER_START_REQ` call-site example. A future inventory would map `server/protocol.def` to `SERVER_START_REQ` call sites under `dlls/ntdll/unix` before considering any replacement.

**HYPOTHESES — archived design notes.**

- `/dev/compat`: Linux NT-object driver, not a new kernel. Keep handles in a user-space table pointing to `/dev/compat` or ntsync file descriptors; per-user NT object namespace. Candidate distribution via DKMS; OpenRC integration is unreviewed and the existing host remains out of scope.
- `peload` in C: separate `pe_parse`, `pe_map` and `peload`; validate offsets and fuzz the parser. For the x86-64 design, use `gs` for TEB access and reach the PEB via the TEB. `0x140000000` is the candidate preferred image base, not a universal PE rule; evaluate `MAP_FIXED_NOREPLACE` and conflicts before mapping.
- `SendMessage`: per-thread shared-memory queue and ntsync signaling; defer global hooks. Inventory hidden wineserver duties: session shared memory, crash recovery of locks (robust futex candidate), registry flushing, and cleanup. Removing wineserver alone is not a compatibility or performance claim.

**Conceptual references only.** [ReactOS](https://github.com/reactos/reactos) and [LiberNT](https://github.com/LiberNT/LiberNT) inform the NT model (`ntdll` → SSDT → `ntoskrnl`; CSRSS and `win32k`); they are not Linux implementations or code bases for this project. The [ReactOS-NT-Subsystem-for-Linux roadmap](https://github.com/splurf/ReactOS-NT-Subsystem-for-Linux#-project-roadmap--contribution-checklist) is a proposal, not a verified implementation. A micro-VM bridge is out of scope; its additional boundary is a latency risk, not a measurement.

**NOT ANALYZED:** protocol coverage, ABI, failure and recovery semantics, portability, maintenance cost, or performance.
