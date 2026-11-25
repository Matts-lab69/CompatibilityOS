<!-- SPDX-License-Identifier: MIT -->

# Wine Source Audit and Open Risks

## Scope and evidence standard

This is a source audit of Wine commit [`7298a818b79446bae153b8d5aa27235ab3703bf9`](https://github.com/wine-mirror/wine/tree/7298a818b79446bae153b8d5aa27235ab3703bf9), limited to the cited code fragments. It is not a build, runtime, performance, or compatibility test.

- **VERIFIED FACT:** directly supported by the cited pinned source fragment.
- **HYPOTHESIS:** proposed CompatibilityOS design or expected effect, not verified in a running system.
- **NOT ANALYZED:** outside the inspected evidence; no conclusion is implied.

## Runtime directory and server socket

**VERIFIED FACT.** `ntdll/unix/loader.c` contains `set_config_dir`; see [lines 363–406](https://github.com/wine-mirror/wine/blob/7298a818b79446bae153b8d5aa27235ab3703bf9/dlls/ntdll/unix/loader.c#L363-L406).

**VERIFIED FACT.** Client initialization uses `init_server_dir` and `setup_config_dir`; see [`dlls/ntdll/unix/server.c:1304–1358`](https://github.com/wine-mirror/wine/blob/7298a818b79446bae153b8d5aa27235ab3703bf9/dlls/ntdll/unix/server.c#L1304-L1358). Server-side `create_server_dir` is in [`server/request.c:577–651`](https://github.com/wine-mirror/wine/blob/7298a818b79446bae153b8d5aa27235ab3703bf9/server/request.c#L577-L651). Both use the configuration directory's device and inode; on Linux the directory is `/tmp/.wine-<uid>/server-<dev>-<ino>`. `<config_dir>/.wineserver` is the Android branch.

**HYPOTHESIS.** Supporting `XDG_RUNTIME_DIR` for server socket placement requires coordinated client and server changes; changing only one side risks mismatched runtime paths. This is a design requirement inferred from the client/server responsibilities, not a tested patch.

**HIPÓTESIS — Phase 1 patch.** With `CPOS_SESSION=1`, `paths/0002-runtime-socket.patch` uses `$XDG_RUNTIME_DIR/cpos-wine/server-<dev>-<ino>` on Linux (normally under `/run/user/<uid>/`). Client and server require a private user-owned runtime directory, keep the existing device/inode key, and leave ordinary Wine launches unchanged. This is session storage, not a sandbox. Socket creation and cleanup require CI runtime evidence.

## DOS/Unix paths and known folders

**VERIFIED FACT.** DOS-to-Unix translation is in [`dlls/ntdll/unix/file.c:3404–3534`](https://github.com/wine-mirror/wine/blob/7298a818b79446bae153b8d5aa27235ab3703bf9/dlls/ntdll/unix/file.c#L3404-L3534); line 3470 calls [`get_dos_device`, defined at line 3013](https://github.com/wine-mirror/wine/blob/7298a818b79446bae153b8d5aa27235ab3703bf9/dlls/ntdll/unix/file.c#L3013) for a device without a subpath. Reverse translation can fall back to `\??\unix` ([lines 3919–4002](https://github.com/wine-mirror/wine/blob/7298a818b79446bae153b8d5aa27235ab3703bf9/dlls/ntdll/unix/file.c#L3919-L4002)); disabling `Z:` is not containment.

**VERIFIED FACT.** `SHGetKnownFolderPath` and known-folder resolution are in [`shell32/shellpath.c`, lines 3561–3596](https://github.com/wine-mirror/wine/blob/7298a818b79446bae153b8d5aa27235ab3703bf9/dlls/shell32/shellpath.c#L3561-L3596) and [lines 3980–4051](https://github.com/wine-mirror/wine/blob/7298a818b79446bae153b8d5aa27235ab3703bf9/dlls/shell32/shellpath.c#L3980-L4051). The inspected `User Shell Folders` reference is at [line 2147](https://github.com/wine-mirror/wine/blob/7298a818b79446bae153b8d5aa27235ab3703bf9/dlls/shell32/shellpath.c#L2147).

## Installer writes and mutability

**VERIFIED FACT.** MSI's `msi_create_empty_local_file` writes under `Windows/Installer`; see [`dlls/msi/package.c`, lines 1060–1076](https://github.com/wine-mirror/wine/blob/7298a818b79446bae153b8d5aa27235ab3703bf9/dlls/msi/package.c#L1060-L1076).

**HYPOTHESIS.** Back `Windows/Installer` with writable per-app storage that persists across session shutdown and cleanup, while retaining the immutable base. The source fragment does not establish that this captures every MSI write.

**VERIFIED FACT — additional write sites at `7298a818`:**

- `Windows\Installer\<ProductCode>`: MSI creates the machine-wide icon directory; the per-user branch starts from AppData instead (`dlls/msi/action.c:3715–3728`).
- `Windows\Temp`, `Help`, `Logs`, `Security`, `Tasks`, `Performance\WinSAT\DataStore`, and `System32` subdirectories such as `catroot`, `mui`, `tasks`, and `spool\printers`: bootstrap setup entries (`loader/wine.inf.in:812–833`; `dlls/setupapi/install.c:755–782`; `dlls/setupapi/fakedll.c:395,542`). The spool default is `%11%\spool\printers` (`loader/wine.inf.in:655`); this verifies configuration, not job-file creation.
- `Windows\assembly\GAC*` and `Windows\Microsoft.NET\assembly\GAC*`: Fusion creates directories and copies assemblies (`dlls/fusion/asmcache.c:104–121,405,471–484,502–508`). SetupAPI creates `Windows\WinSxS\manifests` content (`dlls/setupapi/fakedll.c:733–745,863–870`).
- `Fonts`: MSI obtains `FontsFolder` from `CSIDL_FONTS` (`dlls/msi/package.c:653–655`); this identifies a possible destination, not a write. `GetTempPathW` uses the environment and can fall back to the Windows directory, not necessarily `Windows\Temp`; `GetTempPath2W` leaves the SYSTEM-specific `SystemTemp` behavior unimplemented (`dlls/kernelbase/file.c:2549–2558,2613–2617`).

**NOT ANALYZED.** Exhaustive coverage of package-defined destinations, font copies, print-job files, and other .NET caches. Do not use this inventory as a complete list of writable paths.

## Boot, services, UI, and synchronization

**VERIFIED FACT.** `run_wineboot` invokes initialization with `--init`; see [`dlls/ntdll/unix/env.c`, lines 1541–1608](https://github.com/wine-mirror/wine/blob/7298a818b79446bae153b8d5aa27235ab3703bf9/dlls/ntdll/unix/env.c#L1541-L1608).

**VERIFIED FACT.** The inspected `wineboot` code starts `services.exe` at [`programs/wineboot/wineboot.c`, lines 1917–1938](https://github.com/wine-mirror/wine/blob/7298a818b79446bae153b8d5aa27235ab3703bf9/programs/wineboot/wineboot.c#L1917-L1938). Its UI-related code is at [lines 1697–1710](https://github.com/wine-mirror/wine/blob/7298a818b79446bae153b8d5aa27235ab3703bf9/programs/wineboot/wineboot.c#L1697-L1710).

**HYPOTHESIS.** Suppress the startup dialog without skipping required initialization. This is proposed behavior, not a verified upstream capability or implementation.

**HIPÓTESIS — Phase 1 patch.** `paths/0003-quiet-prefix-update.patch` skips only the progress window when `CPOS_QUIET_UPDATE=1`. It still waits for setup, reports wait errors and does not skip wineboot or service initialization. CI must confirm this behavior at runtime.

**Decision — desktop logging.** The desktop entry invokes `cpos-run --desktop` without a terminal. In desktop mode the runner redirects its input and captures output after the app context is prepared; native console invocation retains diagnostics on stderr. Normal app windows and required Wine error dialogs are not suppressed. The smoke test checks capture, not the set of windows displayed on a real desktop.

**VERIFIED FACT — startup dependencies at `7298a818`.** `wineboot --init` and `services.exe` have fixed `C:\windows\system32` executable paths (`dlls/ntdll/unix/env.c:1541–1549`; `programs/wineboot/wineboot.c:1465–1474`). The SCM loads its service database and initializes RPC and autostart (`programs/services/services.c:1312–1323`). Skipping `wineboot` also skips its hardware and volatile registry initialization, pending renames, `KnownDLLs`, and startup processing (`programs/wineboot/wineboot.c:1924–1950`).

**VERIFIED FACT — demand start at `7298a818`.** COM starts `RpcSs` through the SCM if needed (`dlls/combase/rpc.c:185–206`); its configured binary is `%11%\rpcss.exe` and its start type is demand. `PlugPlay` uses `%11%\plugplay.exe` with automatic start (`loader/wine.inf.in:977–982,1065–1070`). If there is no desktop window, `win32u` can launch `C:\windows\system32\explorer.exe /desktop` (`dlls/win32u/winstation.c:805–834,890–901`). The INF entries for Explorer are setup entries, not launch triggers (`loader/wine.inf.in:812–828`).

**NOT ANALYZED.** Whether launching the SCM independently is sufficient in a session that skips the bootstrap; a demand-start RPCSS still requires the SCM.

**Decision — keep upstream service policy.** Keep the SCM and Wine bootstrap unchanged: COM already starts RpcSs on demand, and the desktop window request already starts Explorer conditionally. PlugPlay is automatic at this pin; do not force demand-start without evidence that device setup and enumeration can tolerate it. The new smoke probe checks SCM connectivity and the configured RpcSs and PlugPlay start types. **SIN ANALIZAR:** real COM activation, device notifications and Explorer process lifetime across multiple desktops. Configuration alone does not establish any of these behaviors.

**VERIFIED FACT.** The inspected `ntsync` server/in-process synchronization path checks a guard header and device; see [`server/inproc_sync.c`, lines 40–52](https://github.com/wine-mirror/wine/blob/7298a818b79446bae153b8d5aa27235ab3703bf9/server/inproc_sync.c#L40-L52).

## Reference revisions and what they establish

The audit references previously checked with `git ls-remote` are:

| Component | Reference | Revision |
| --- | --- | --- |
| Wine | `wine-11.20` (peeled commit) | `7298a818b79446bae153b8d5aa27235ab3703bf9` |
| GE-Proton | `GE-Proton11-11` | `5de4b529` |
| DXVK | `v3.1.1` | `1e0f9231` |
| VKD3D-Proton | `v3.0.1` | `3b10bd7a` |

**VERIFIED FACT (reference only).** These revisions identify upstream references checked previously. They do **not** prove that CompatibilityOS builds, runs, improves compatibility, or benefits from adopting any of them.

## WoW64, compositor and rebase

**VERIFIED FACT — WoW64 at `7298a818`.** Classic redirection happens in `dlls/ntdll/unix/file.c:2952–2977,4091–4093`. New WoW64 translates PE32 file arguments in `dlls/wow64/file.c:179–190,271` before passing them to the native call. Both support the `System32` redirect with exceptions and resolve `sysnative` to native `System32` even when redirection is disabled. **HYPOTHESIS:** apply the path table after NT redirection, without redirecting twice; preserve views in both directions. See [build plan](07-compilacion.md).

**VERIFIED FACT — compositor paths.** Wine `7298a818` uses `wl_display_connect` in `dlls/winewayland.drv/wayland.c:290` and `XOpenDisplay` in `dlls/winex11.drv/x11drv_main.c:646`. COSMIC [`epoch-1.0.0`, `fa88002b`, `src/xwayland.rs:75`](https://github.com/pop-os/cosmic-comp/blob/fa88002ba41d2edec25dd7ffdee9719fbb928fc0/src/xwayland.rs#L75) launches XWayland. **NOT ANALYZED:** runtime color-management and tearing-control globals. **HYPOTHESIS:** start with XWayland SDR; compare native Wayland for focus, scaling, input, drag-and-drop, and presentation later.

**VERIFIED FACT — history, not conflicts.** The [upstream comparison](https://github.com/wine-mirror/wine/compare/wine-11.0...7298a818b79446bae153b8d5aa27235ab3703bf9) from stable `wine-11.0` (`db11d0fe`) reported 5,054 commits ahead for development `11.20`. Per-path GitHub commit history from `2026-01-20T00:00:00Z` through `7298a818` counted `dlls/ntdll/unix/file.c` 29, `env.c` 24, `server.c` 21; `server/registry.c` 19, `server/request.c` 18; `dlls/wow64/file.c` and `dlls/shell32/shellpath.c` one each. This counts commits touching files, not conflicts or movement within individual functions.

**HYPOTHESIS — under-three-month target.** Pin the audited 11.20 baseline; do not port to stable in parallel. Small path/socket patches are medium estimated rebase effort; bootstrap redesign or registry deltas are high. Separate paths, coordinated client/server socket changes, and boot UI in the rebaseable series. Prefer upstream fixes; avoid patching the registry server if the effective-copy design is chosen. **NOT ANALYZED:** actual rebase time, conflict counts, and overlap with GE/CachyOS patches.
