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

**HECHO VERIFICADO — further A1 research:** the [`esync`](https://github.com/zfigura/wine/commit/4838aa7f5c21493e7c0504b7819827cd0f536923) and [`esync-rebased`](https://github.com/zfigura/wine/commit/ce793468b04f0fd6a3e5e2653885ab15803cecfe) branch tips both date from 2018, according to the commit metadata. Neither establishes compatibility with the fixed 11.20 source. **SIN ANALIZAR:** a forward-port and its file-level provenance. No environment flag can enable a backend absent from the current build.

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

## Follow-up after the snapshot

- **HECHO VERIFICADO:** [Wine CI run 45186778110](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45186778110) built Wine, but APT exhausted the 15-minute test job before Xvfb ran. `1cd1fab` allows 35 minutes for the job and 25 for dependency setup, configures five APT retries and per-request timeouts, installs one MinGW variant per architecture, and caches only downloaded `.deb` archives, with a 512 MiB cap per entry. Restored packages are copied into the APT archive directory only after their SHA-256 matches the authenticated indexes. [Run 45199095403](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45199095403) completed dependency setup after a cache miss, saved the cache, and ran the Xvfb tests. [Static checks](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45199095400) passed. **SIN ANALIZAR:** cache reuse on a subsequent run.

- **HECHO VERIFICADO:** `347967f` adds a private font-load and registry fixture. Its [static checks](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45185941391) passed. In [run 45199095403](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45199095403), load and registry write reported success, but `RemoveFontResourceExW` returned false and attestation was skipped. `71f2cee` isolates Fontconfig and checks family visibility before load, after load, and after successful removal. **SIN ANALIZAR:** the lifecycle with isolation ([run 45211101919](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45211101919)), printing, persistent font discovery, and GAC/managed activation.
- **HECHO VERIFICADO:** `da382a9`, `c68e077`, `4ba2b43` add early desktop errors and notifier fallback. The first two attempts failed on lint and a misplaced Wine stub, respectively; [static CI at `c69043e`](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45186319520) passed ShellCheck, stub-based tests, desktop and MIME validation. Real COSMIC behavior remains **SIN ANALIZAR**.
- **HECHO VERIFICADO:** `c69043e`, [Phase 2 CI](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45186319328): the MinGW fixture passed its entry-point assertion that the process-attach callback ran first. The job completed 4,065,316 fuzz iterations and passed messaging and ABI compile checks. Static TLS, TEB/PEB and general import loading are still absent.
- B2 and B3 remain **PARCIAL**: scoped kernel objects and a pthread queue exist, but named objects, descriptor transfer, runtime ioctl validation and module-backed messaging do not.

**HECHO VERIFICADO — footprint at `c69043e`:** 422 / 197 / 753 / 494 / 643 physical lines in the same directory order (2,509 total). `du -sb`: 436,763 apparent bytes, including 225,852 bytes of `.git`; `du -sh`: 952 KiB allocated. Git storage can change independently of source size.

**HECHO VERIFICADO — footprint at `71f2cee`:** 422 / 197 / 753 / 494 / 683 physical lines in the same directory order (2,549 total). `du -sb`: 480,823 apparent bytes, including 263,338 bytes of `.git`; `du -sh`: 1.2 MiB allocated. `df -h "$HOME"` reported 60 GiB available.
