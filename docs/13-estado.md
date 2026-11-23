<!-- SPDX-License-Identifier: MIT -->
# Development status

**HECHO VERIFICADO — snapshot:** `c59b2a9`, 2026-11-23. HECHO means the specified deliverable passed CI; green checks on a subset do not close a partial milestone. NO INICIADO would mean no deliverable exists for the milestone, not merely that one of its requirements is missing.

| Milestone | Status | Evidence and remaining work |
| --- | --- | --- |
| A1 — Sync chain | PARCIAL | `e62c0b1`, `2e3c21e`, [Wine CI](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45171969671/attempts/2): native ntsync builds and wineserver fallback passes. The evaluated fsync update did not apply (`fee7f83`); neither fsync nor esync is integrated. Missing active ntsync coverage and intermediate fallback tests. |
| A2 — Windows subsystems | PARCIAL | `e311d55`, [Wine CI](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45171969671/attempts/2): per-app write routes, font and spool enumeration, synthetic SetupAPI and Fusion cache creation pass. Missing font installation/registration, printing a job, GAC install and managed activation, and real-device PnP. |
| A3 — Desktop launch | PARCIAL | `feec84a`, `0c94cbd`, [Wine CI](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45171969671/attempts/2) and [static validation](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45171969718): XDG logs, no terminal, notification stub, desktop and MIME tests pass. Missing real COSMIC validation and coverage for early failures and fallback after a dialog tool fails. |
| A4 — README and final docs | PARCIAL | `4a2fb20`, [static CI](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45165547815): documentation separates experimental work. Final consistency depends on remaining implementation and verification. |
| B1 — PE loader | PARCIAL | `ea82bb5`, `6f9d2fb`, [Phase 2 CI](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45167762949): MinGW fixture, parser tests and fuzz passed. Preferred mapping, DIR64 and allowlisted fixture imports exist; complete TLS, TEB/PEB, and general Windows import and process initialization do not. |
| B2 — Kernel objects | PARCIAL | `83f7783`, `6f9d2fb`, [Phase 2 CI](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45167762949): module compiled without loading. Per-open objects exist, but fd transfer is disabled; named per-user objects, ntsync and runtime tests are missing. |
| B3 — Messaging | PARCIAL | `ea82bb5`, [Phase 2 CI](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45167762949): pthread owner dispatch, synchronous response and close tests passed. Missing module-backed signaling and Windows message semantics. |
| B4 — Versioned ABI | HECHO (layout scope) | `f34695b`, `83f7783`, [Phase 2 CI](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45167762949): documented v1/v2 and compile-time userspace layout checks passed. Runtime behavior remains part of B2. |
| B5 — Phase 2 status documentation | HECHO (snapshot) | `9c3c320`: [design and limitations](12-fase2.md) record the green [Phase 2 run](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45167762949). Refresh when experimental coverage changes. |

## Repository footprint

**HECHO VERIFICADO:** `wc -l` on tracked non-Markdown files, including comments, blank lines, configuration and diff headers. This is physical line count, not executable code count. `du -sb` measured 586,443 apparent bytes including `.git` (390,593 bytes); `du -sh` reported 3.6 MiB allocated. Before the snapshot commit, `df -h "$HOME"` reported 58 GiB available.

| Directory | Physical lines |
| --- | ---: |
| `patches/` | 422 |
| `scripts/` | 197 |
| `cpos-peload/` | 671 |
| `cpos-kernel/` | 494 |
| `tests/` | 467 |
| Total | 2,251 |
