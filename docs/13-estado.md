<!-- SPDX-License-Identifier: MIT -->
# Development status

## Closure follow-up — 2026-11-25

**HECHO VERIFICADO — resolution:** `62c2191` registered the synthetic COM server with an absolute DOS path. [Wine CI 45637774717](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45637774717) passed build, Xvfb and attestation. Xvfb logged `ok rpcss-com`, `ok persistent-font`, and `ok explorer-on-demand`. [Installer CI 45646058915](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45646058915) verified that exact bundle. The experimental jobs were skipped on this push; their separate green evidence is in [run 45604182888](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45604182888). See the [closure audit](14-cierre.md) for remaining gaps.

See the [criterion-by-criterion audit and CachyOS USB handoff](14-cierre.md). **HECHO VERIFICADO:** [run 45604182888](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45604182888/job/139198575163) passed persistent font discovery in a fresh session (`ok persistent-font`), but the COM probe failed with `ShellWindows COM activation failed: 0x80040152`. The synthetic replacement in [run 45614430285](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45614430285/job/139227228315) failed with `Synthetic COM activation failed: 0x80040154`. **HIPÓTESIS:** the pinned Wine lookup uses machine classes; `3298d44` registers the fixture under HKLM of the disposable prefix. **SIN ANALIZAR:** the outcome of [run 45622130216](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45622130216).

## Recovery and local checks

**HECHO VERIFICADO:** after the shutdown the checkout was clean, the stash was empty, and local `HEAD` and remote `main` agreed at `2fbbfd0`. The six active patches passed `git apply --check` in series order on a fresh external shallow checkout of `7298a818`; the checkout was removed. The optional fsync update fails on this pin; see [candidate notes](../patches/upstream-candidates/README.md). No local Wine or module build was run.

**HECHO VERIFICADO:** after the earlier `CloseDesktop failed: 6`, [Wine CI for `30fd688`](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45371042349/job/138395293858) logged `Explorer fixture exit: 0` and `ok explorer-on-demand`. This remains a regression check in the final run.

**HECHO VERIFICADO — local checks at `87dc4f5`:** ShellCheck 0.11.0, `git diff --check`, GCC syntax-only with `-Wall -Wextra -Werror` for the Linux userspace loader, queue and ABI probes, `xmllint` and `desktop-file-validate` passed. The repository had 53 GiB available on the home filesystem. Tracked non-Markdown physical lines: `patches/` 422, `scripts/` 197, `cpos-peload/` 1,077, `cpos-kernel/` 990 and `tests/` 1,127 (3,813 total); 920,346 apparent bytes including `.git`, 3.4 MiB allocated.

**Scope decision:** the supported chain is **ntsync → wineserver**; fsync and esync are excluded, not pending deliverables. **SIN ANALIZAR / future:** named kernel objects and general DLL loading in the experimental PE loader. Neither is a gate for the final CI. Active ntsync coverage remains unverified. The final manual Wine workflow also runs static and experimental jobs; installer verification follows a successful attested bundle.

**HECHO VERIFICADO — final CI at `083336c`:** [Wine CI 45427394757](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45427394757/attempts/1) passed all six jobs on its first attempt. The Xvfb job logged `Explorer fixture exit: 0`, `ok explorer-on-demand`, `ok sync-fallback`, `ok gac-fixture`, and `Verified one RAW job (66 bytes)`; `CloseDesktop failed: 6` did not recur. The experimental jobs passed the exact PE fixture output, 10,000,000 fuzz iterations, ABI layout checks and guest ioctls including wait sets and restricted file references. The guest event-backed queue remained enabled. The chained [installer verification 45435981928](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45435981928) verified this run's bundle and passed linked-registration rejection, attested installation, uninstall and user-data retention. No jobs needed a retry. Node.js 20 deprecation annotations were warnings, not failures.

## Execution inventory — 2026-11-24

| Block | Status | Verified scope and gaps |
| --- | --- | --- |
| 0 — Inventory | HECHO | This inventory records the revision and CI evidence below; it does not certify the missing work. |
| 1 — Sync | PARCIAL | `134e6d7`, [Wine CI](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45339755383): `ok sync-fallback` and server-reported `cpos-sync: wineserver` in the per-app XDG state log. Active ntsync is still unverified; fsync and esync are excluded. |
| 2 — Subsystems | PARCIAL | `083336c`, [Xvfb job](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45427394757/job/138589912314): private font, automatic PlugPlay, SCM-requested RpcSs, signed GAC fixture, RAW printing, and implicit Explorer launch and cleanup passed. Persistent font discovery and COM-triggered RpcSs start remain outside the test. |
| 3 — Desktop | PARCIAL | `08b6743`, [static CI](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45339758676): notifier stubs, desktop validation, `xmllint` and MIME glob checks. `134e6d7`, [Wine CI](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45339755383): `ok desktop-log` and `ok desktop-notification` under Xvfb. Real COSMIC behavior remains unverified. |
| 4 — PE loader | PARCIAL | `445bad7`, [Phase 2 CI](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45342439841): MinGW fixture, one static TLS slot, minimal TEB/PEB with mapped ImageBase, host GS restored and 10,000,000 fuzz iterations. Full process initialization and general imports are still missing. |
| 5 — Kernel objects | PARCIAL | `083336c`, [kernel job](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45427394757/job/138563022465): guest event, mutex, semaphore, per-open wait sets and restricted file-reference probes passed. Named objects are future work. |
| 6 — Messaging | HECHO (experimental queue scope) | `6cc0b78`, [kernel job](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45340644788/job/138263213730): owner dispatch, FIFO, synchronous reply and queued close passed with the guest event backend enabled. Not a general Win32 messaging implementation. |
| 7 — ABI and closeout | HECHO (experimental ABI scope) | `a269fc2`, [Phase 2 CI](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45341371745): v1/v2 layout assertions, guest v1 query/create/drop and basic v2 ioctl smoke passed. See [Phase 2 status](12-fase2.md) for missing behavior. |

**HECHO VERIFICADO:** [Wine CI run 45211101919](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45211101919) logged `Cache restored successfully`, `ok font-fixture`, and `ok subsystem-probes`; build, test and attestation passed. This supports the isolated lifecycle, not persistent font discovery or a definitive diagnosis of the earlier failure.

**HECHO VERIFICADO — static TLS corrections:** [45340397117](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45340397117) reported `error: call to undeclared function 'syscall'`; `02c8f0c` exposes the declaration. [45340517416](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45340517416) reported `error: cast to pointer from integer of different size [-Werror=int-to-pointer-cast]`; `407281a` corrected the cast. [45340644788](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45340644788) then reported `AddressSanitizer: SEGV on unknown address 0x000000000010`; `bd46bc8` fixed the TLS array element width as well. The [next full Phase 2 run](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45340819917) passed without disabling TLS, sanitizers, or `-Werror`.

**HECHO VERIFICADO — import fixture:** [45342788432](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45342788432) built the fixture but exited at `Makefile:19: check-exec` with `Error 1` before comparing output. `15bb1de` added a DIR64 relocation to permit loading away from the preferred base. [45343548109](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45343548109) passed the byte-for-byte comparison of `CPOS PE hello` and 10,000,000 parser fuzz iterations. The import resolves to an allowlisted host callback; no DLL is loaded.

**HECHO VERIFICADO — snapshot:** `c59b2a9`, 2026-11-23. HECHO means the specified deliverable passed CI; green checks on a subset do not close a partial milestone. NO INICIADO would mean no deliverable exists for the milestone, not merely that one of its requirements is missing.

| Milestone | Status | Evidence and remaining work |
| --- | --- | --- |
| A1 — Sync chain | PARCIAL | `e62c0b1`, `2e3c21e`, [Wine CI](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45171969671/attempts/2): native ntsync builds and wineserver fallback passes. Active ntsync remains unverified; fsync and esync are outside the final scope. |
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

- **HECHO VERIFICADO:** [Wine CI run 45186778110](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45186778110) built Wine, but APT exhausted the 15-minute test job before Xvfb ran. `1cd1fab` added five retries, per-request timeouts, one MinGW variant per architecture and `.deb` caching capped at 512 MiB per entry. [Run 45199095403](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45199095403) completed dependency setup after a cache miss and saved the cache. `c97fc90` raised the build/test job limits to 120/45 minutes. `300d4a9` isolates the test archives and checks canonical filename, architecture and SHA-256 against authenticated APT metadata. [Run 45341367121](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45341367121) restored the cache, reported `Need to get 0 B/212 MB of archives`, and passed build, Xvfb tests and attestation. This run predates the CUPS printing and Explorer probes.

- **HECHO VERIFICADO:** `347967f` adds a private font-load and registry fixture. Its [static checks](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45185941391) passed. In [run 45199095403](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45199095403), load and registry write reported success, but `RemoveFontResourceExW` returned false and attestation was skipped. `71f2cee` isolates Fontconfig and checks family visibility before load, after load, and after successful removal; [run 45211101919](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45211101919) passed. **SIN ANALIZAR:** printing, persistent font discovery, and GAC/managed activation.
- **HECHO VERIFICADO:** `da382a9`, `c68e077`, `4ba2b43` add early desktop errors and notifier fallback. The first two attempts failed on lint and a misplaced Wine stub, respectively; [static CI at `c69043e`](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45186319520) passed ShellCheck, stub-based tests, desktop and MIME validation. Real COSMIC behavior remains **SIN ANALIZAR**.
- **HECHO VERIFICADO:** `c69043e`, [Phase 2 CI](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45186319328): the MinGW fixture passed its entry-point assertion that the process-attach callback ran first. The job completed 4,065,316 fuzz iterations and passed messaging and ABI compile checks. Static TLS, TEB/PEB and general import loading are still absent.
- B2 remains **PARCIAL**: basic ioctls, per-open wait sets and restricted file references passed guest checks; per-user named objects and cross-open descriptor sharing are absent. B3 has guest coverage for the optional event-backed queue; full Win32 messaging semantics are still absent.

**HECHO VERIFICADO — footprint at `c69043e`:** 422 / 197 / 753 / 494 / 643 physical lines in the same directory order (2,509 total). `du -sb`: 436,763 apparent bytes, including 225,852 bytes of `.git`; `du -sh`: 952 KiB allocated. Git storage can change independently of source size.

**HECHO VERIFICADO — footprint at `71f2cee`:** 422 / 197 / 753 / 494 / 683 physical lines in the same directory order (2,549 total). `du -sb`: 480,823 apparent bytes, including 263,338 bytes of `.git`; `du -sh`: 1.2 MiB allocated. `df -h "$HOME"` reported 60 GiB available.

**HECHO VERIFICADO — footprint at `bd46bc8` with pending documentation edits:** 422 / 197 / 1,015 / 666 / 747 physical lines in the same directory order (3,047 total). `du -sb`: 575,204 apparent bytes; `du -sh`: 1.8 MiB allocated. `df -h "$HOME"` reported 60 GiB available.

**HECHO VERIFICADO — footprint at `f4832bf`:** 422 / 197 / 1,035 / 670 / 1,090 physical lines in the same directory order (3,414 total). `du -sb`: 689,262 apparent bytes; `du -sh`: 2.3 MiB allocated. `df -h "$HOME"` reported 59 GiB available.
