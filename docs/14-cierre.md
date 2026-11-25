<!-- SPDX-License-Identifier: MIT -->
# Closure audit

Evidence is revision-specific. The original [full run][full] passed at `083336c`; the latest [Wine run][com-retry] passed at `62c2191`, with separate [static][latest-static] and [installer][latest-installer] checks. The experimental jobs in the latest Wine run were skipped by design; their latest evidence here is from the closure dispatch at `6fa11f2`.

| Criterion | Status | Evidence and limits |
| --- | --- | --- |
| A — Wine patches | HECHO | `f455f13` records that all six applied in order to `7298a818`. `083336c`: [build][build] and [smoke][wine] passed; path round trips and ephemeral sockets were exercised. Quiet Wine initialization retains error handling. |
| B — Writable application routes | HECHO | `083336c`, [Xvfb][wine]: `ok paths` and `ok windows-writes` cover Installer, Temp, spool, Fonts, both assembly roots and WinSxS. |
| C — Fonts | HECHO | `62c2191`, [Xvfb][latest-test]: installation, registration, discovery in a fresh session without `AddFontResource`, and cleanup (`ok persistent-font`). |
| D — Printing | HECHO | `b63b9cc`, [Xvfb][wine]: writable spool and exact 66-byte RAW job to a disposable CUPS queue. |
| E — .NET/GAC | HECHO | `b995273`, [Xvfb][wine]: signed synthetic assembly installed, enumerated through Fusion and removed. No managed-runtime activation claim. |
| F — Services | HECHO | `62c2191`, [Xvfb][latest-test]: PlugPlay, implicit Explorer and cleanup, and successful synthetic COM activation transitioning RpcSs from stopped to running. |
| G — Per-app registry | HECHO | `083336c`, [static][static] and [Xvfb][wine]: versioned seed, migration marker and WoW64 registry probes. |
| H — Runner | PARCIAL | `083336c`, [static][static] and [Xvfb][wine]: write-free dry run, recipe checks, EXE, MSI and logged wineserver fallback. Active ntsync is unverified; fsync/esync are excluded as recorded in [status](13-estado.md). |
| I — Desktop | PARCIAL | `083336c`, [static][static] and [Xvfb][wine]: desktop/MIME validation, log and notification tests; [binfmt_misc](../packaging/binfmt/README.md) is documentation only. Actual COSMIC double-click is unverified. |
| J — Installer | HECHO | `58d9322`, [installer][installer]: provenance, hash, tamper rejection, dry run, idempotence, association restoration and retained data under disposable XDG paths. |
| K — Repository and licensing | HECHO | `624cb9b`: SPDX inventory checked for tracked files except the license text itself. MIT project, LGPL-2.1-or-later Wine patches and GPL-2.0-only module; [static CI][closure-static] passed. The pre-existing local `.gitignore` edit was not included. |
| L — CI | HECHO | `095c960`: Node.js 24 pins; separate [experimental/static jobs][closure], [Wine build, tests and attestation][com-retry], and [installer][latest-installer] passed across their stated revisions. The published bundle contains a hash manifest and has seven-day retention. |

## COM activation follow-up

**HECHO VERIFICADO — COM follow-up:** [run 45614430285](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45614430285/job/139227228315) reported `Synthetic COM activation failed: 0x80040154` (`REGDB_E_CLASSNOTREG`), while persistent fonts and Explorer passed. **HIPÓTESIS:** Wine's pinned [COM class lookup](https://github.com/wine-mirror/wine/blob/7298a818b79446bae153b8d5aa27235ab3703bf9/dlls/combase/combase.c) uses the machine classes in the disposable Wine prefix, not the fixture's HKCU key. The next probe registers and removes the class in that prefix's HKLM. No host registry or system setting is changed.

**HECHO VERIFICADO:** [run 45622130216](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45622130216/job/139254670008) still returned `0x80040154` with the fixture in HKLM. That change alone did not resolve activation. The pinned [`rpc_get_local_class_object`](https://github.com/wine-mirror/wine/blob/7298a818b79446bae153b8d5aa27235ab3703bf9/dlls/combase/rpc.c) can try several activation paths before returning the last error, so this HRESULT does not identify the failing path. The follow-up adds registry readback and filtered OLE/RPC traces without relaxing the assertions.

**HECHO VERIFICADO:** [run 45630202444](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45630202444/job/139283281980) read back `LocalServer32` successfully and logged `create_server failed to run local server` for the relative `unix\home\...\probe64.exe` command, before the surrogate fallback returned `0x80040154`. The revised smoke test places the server in the application's routed Temp directory and registers an absolute DOS path. The [green Xvfb job][latest-test] logged the stopped-to-running transition and `ok rpcss-com` with that path.

## Experimental Phase 2

- **PARCIAL:** [PE job][pe] passed synthetic TLS, relocation and import fixtures plus 10 million fuzz iterations; general DLL loading, forwarders and delay imports are not verified.
- **PARCIAL:** [QEMU job][kernel] passed per-open objects, wait sets and restricted file references; named objects and abandonment are not covered.
- **HECHO VERIFICADO:** `083336c`, [QEMU job][kernel]: optional event-backed messaging and versioned ioctl smoke tests. This does not substitute for wineserver. See [Phase 2](12-fase2.md).

**HECHO VERIFICADO — historical scope:** the [experimental PE][closure-pe] and [kernel][closure-kernel] jobs passed at `6fa11f2`; the same workflow failed separately at the Wine COM probe. Those green jobs do not verify the expanded [Phase 2 closure checks](12-fase2.md#closure-checks-for-the-next-revision).

**SIN ANALIZAR:** active ntsync, real COSMIC, performance and crash rates. No baseline was run.

## CachyOS USB handoff

1. On the USB session, record the kernel version and whether `/dev/ntsync` exists as a character device and can be opened by the current user. Do not load modules or change permissions. A reachable device alone does not prove that Wine selected it.
2. Open [Wine run 45637774717][com-retry] and download `compatibilityos-linux-x86_64` (artifact `15036064671`, expires 2026-12-02 15:30 UTC). If it expired, use a fresh, successful and attested Wine CI run from `main`; do not use the staging artifact. The artifact contains the bundle and `SHA256SUMS`.
3. Start with the repository installer's `--dry-run install RUN_ID` mode, which prints the destination paths without writing. If installing in the USB environment is desired, normal installation fetches the selected run and checks the manifest, bundle hash and signed attestations before registering the launcher. See [installer documentation](08-instalador.md).
4. Record the exact run and commit, kernel, desktop session, launcher result, and the `cpos-sync:` marker in `${XDG_STATE_HOME:-$HOME/.local/state}/cpos/APP/wine.log`. Record absent logs or errors verbatim. Do not infer performance or crash-rate differences from this smoke check.

**Pending decisions:** confirm active ntsync and actual COSMIC behavior on the USB; no host changes or release are part of this audit.

[full]: https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45427394757
[closure]: https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45604182888
[closure-static]: https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45604182888/job/139170619297
[closure-pe]: https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45604182888/job/139170619274
[closure-kernel]: https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45604182888/job/139170619063
[closure-test]: https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45604182888/job/139198575163
[com-retry]: https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45637774717
[latest-test]: https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45637774717/job/139312761770
[latest-static]: https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45637774637
[latest-installer]: https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45646058915/job/139317247314
[build]: https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45427394757/job/138563021512
[static]: https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45427394757/job/138563021880
[pe]: https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45427394757/job/138563022265
[kernel]: https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45427394757/job/138563022465
[wine]: https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45427394757/job/138589912314
[installer]: https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45435981928/job/138593780854
