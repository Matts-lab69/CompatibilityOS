<!-- SPDX-License-Identifier: MIT -->
# Closure audit

Evidence is revision-specific. The full [Wine CI][wine] and [installer CI][installer] ran at `083336c`. HECHO applies only to the stated scope.

| Criterion | Status | Evidence and limits |
| --- | --- | --- |
| A — Wine patches | HECHO | `f455f13` records that all six applied in order to `7298a818`. `083336c`: [build][build] and [smoke][wine] passed; path round trips and ephemeral sockets were exercised. Quiet Wine initialization retains error handling. |
| B — Writable application routes | HECHO | `083336c`, [Xvfb][wine]: `ok paths` and `ok windows-writes` cover Installer, Temp, spool, Fonts, both assembly roots and WinSxS. |
| C — Fonts | HECHO | `6fa11f2`, [Xvfb][closure-test]: absent before installation, copied and registered, enumerated in a fresh session without `AddFontResource`, and cleaned up (`ok persistent-font`). The job failed independently on the COM probe. |
| D — Printing | HECHO | `b63b9cc`, [Xvfb][wine]: writable spool and exact 66-byte RAW job to a disposable CUPS queue. |
| E — .NET/GAC | HECHO | `b995273`, [Xvfb][wine]: signed synthetic assembly installed, enumerated through Fusion and removed. No managed-runtime activation claim. |
| F — Services | PARCIAL | `30fd688`, [Xvfb][wine]: implicit Explorer and cleanup passed, as did PlugPlay and SCM-requested RpcSs. The separate COM probe in [closure CI][closure-test] observed RpcSs transition from stopped to running, but failed activation with `ShellWindows COM activation failed: 0x80040152` (`REGDB_E_KEYMISSING`). `02a2365` uses an explicitly registered synthetic local server; activation and cleanup await [follow-up CI][com-retry]. |
| G — Per-app registry | HECHO | `083336c`, [static][static] and [Xvfb][wine]: versioned seed, migration marker and WoW64 registry probes. |
| H — Runner | PARCIAL | `083336c`, [static][static] and [Xvfb][wine]: write-free dry run, recipe checks, EXE, MSI and logged wineserver fallback. Active ntsync is unverified; fsync/esync are excluded as recorded in [status](13-estado.md). |
| I — Desktop | PARCIAL | `083336c`, [static][static] and [Xvfb][wine]: desktop/MIME validation, log and notification tests; [binfmt_misc](../packaging/binfmt/README.md) is documentation only. Actual COSMIC double-click is unverified. |
| J — Installer | HECHO | `58d9322`, [installer][installer]: provenance, hash, tamper rejection, dry run, idempotence, association restoration and retained data under disposable XDG paths. |
| K — Repository and licensing | HECHO | `624cb9b`: SPDX inventory checked for tracked files except the license text itself. MIT project, LGPL-2.1-or-later Wine patches and GPL-2.0-only module; [static CI][closure-static] passed. The pre-existing local `.gitignore` edit was not included. |
| L — CI | PARCIAL | `083336c`, [full run][full]: separate jobs, cache, seven-day artifacts, hashes and attestation. `095c960` replaces the Node.js 20 action pins; end-to-end verification is tracked in [closure CI][closure]. |

## Experimental Phase 2

**HECHO VERIFICADO — COM follow-up:** [run 45614430285](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45614430285/job/139227228315) reported `Synthetic COM activation failed: 0x80040154` (`REGDB_E_CLASSNOTREG`), while persistent fonts and Explorer passed. **HIPÓTESIS:** Wine's pinned [COM class lookup](https://github.com/wine-mirror/wine/blob/7298a818b79446bae153b8d5aa27235ab3703bf9/dlls/combase/combase.c) uses the machine classes in the disposable Wine prefix, not the fixture's HKCU key. The next probe registers and removes the class in that prefix's HKLM. No host registry or system setting is changed.

- **PARCIAL:** [PE job][pe] passed synthetic TLS, relocation and import fixtures plus 10 million fuzz iterations; general DLL loading, forwarders and delay imports are not verified.
- **PARCIAL:** [QEMU job][kernel] passed per-open objects, wait sets and restricted file references; named objects and abandonment are not covered.
- **HECHO VERIFICADO:** `083336c`, [QEMU job][kernel]: optional event-backed messaging and versioned ioctl smoke tests. This does not substitute for wineserver. See [Phase 2](12-fase2.md).

**SIN ANALIZAR:** active ntsync, real COSMIC, performance and crash rates. No baseline was run.

## CachyOS USB handoff

1. On the USB session, record the kernel version and whether `/dev/ntsync` exists as a character device and can be opened by the current user. Do not load modules or change permissions. A reachable device alone does not prove that Wine selected it.
2. Open the [verified run's artifacts][full] and locate `compatibilityos-linux-x86_64`. If it expired, use a fresh, successful and attested Wine CI run from `main`; do not use the staging artifact. The artifact contains the bundle and `SHA256SUMS`.
3. Start with the repository installer's `--dry-run install RUN_ID` mode, which prints the destination paths without writing. If installing in the USB environment is desired, normal installation fetches the selected run and checks the manifest, bundle hash and signed attestations before registering the launcher. See [installer documentation](08-instalador.md).
4. Record the exact run and commit, kernel, desktop session, launcher result, and the `cpos-sync:` marker in `${XDG_STATE_HOME:-$HOME/.local/state}/cpos/APP/wine.log`. Record absent logs or errors verbatim. Do not infer performance or crash-rate differences from this smoke check.

**Pending decisions:** confirm active ntsync and actual COSMIC behavior on the USB; no host changes or release are part of this audit.

[full]: https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45427394757
[closure]: https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45604182888
[closure-static]: https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45604182888/job/139170619297
[closure-test]: https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45604182888/job/139198575163
[com-retry]: https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45614430285
[build]: https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45427394757/job/138563021512
[static]: https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45427394757/job/138563021880
[pe]: https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45427394757/job/138563022265
[kernel]: https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45427394757/job/138563022465
[wine]: https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45427394757/job/138589912314
[installer]: https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45435981928/job/138593780854
