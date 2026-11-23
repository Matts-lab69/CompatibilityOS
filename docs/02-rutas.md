# Route and Mount Design

## Scope and evidence labels

- **VERIFIED FACT** means the cited pinned Wine source directly shows the behavior described.
- **HYPOTHESIS** means a proposed CompatibilityOS design, not an implemented or tested behavior.
- **NOT ANALYZED** means no conclusion is made here.

The source references below are pinned to Wine commit [`7298a818b79446bae153b8d5aa27235ab3703bf9`](https://github.com/wine-mirror/wine/tree/7298a818b79446bae153b8d5aa27235ab3703bf9). They document upstream behavior; they do not establish CompatibilityOS implementation or build compatibility.

## Proposed path map

| Windows-visible path | Host mapping (proposal) | Status / notes |
| --- | --- | --- |
| `C:\Windows` | Shared immutable base, with explicit writable subpaths backed by persistent per-app storage | **HYPOTHESIS.** Start with `C:\Windows\Installer`; the rest of the write inventory is pending. |
| `C:\Program Files\<app>` | Per-app writable subtree under `~/.local/share/<app>/` | **HYPOTHESIS.** Include both native and x86 program directories in the per-app layout; preserve the Windows-visible distinction. |
| `C:\Program Files (x86)\<app>` | Per-app writable subtree under `~/.local/share/<app>/` | **HYPOTHESIS.** Do not collapse the Windows-visible x86 path into a different app identity. |
| `C:\ProgramData\<app>` | Per-app writable subtree under `~/.local/share/<app>/` | **HYPOTHESIS.** Separate app data from the immutable base. |
| User `AppData\Local` | `~/.local/share/<app>/` | **HYPOTHESIS.** Per-app data location. |
| User `AppData\Roaming` | `~/.config/<app>/` | **HYPOTHESIS.** Per-app configuration location. |
| `Documents` | Actual XDG user Documents directory | **HYPOTHESIS.** Resolve the configured XDG location rather than assuming `~/Documents`. |
| `Desktop` | Actual XDG user Desktop directory | **HYPOTHESIS.** Resolve the configured XDG location. |
| `Downloads` | Actual XDG user Downloads directory | **HYPOTHESIS.** Resolve the configured XDG location. |
| `D:` and other configured letters | Explicitly defined mounts | **HYPOTHESIS.** No implicit host-wide mount. |
| `Z:` | Optional, explicitly enabled compatibility mapping | **HYPOTHESIS.** Never treat it as a sandbox boundary. |

The proposed mapping is intentionally app-scoped for writable application data while retaining real XDG locations for user libraries. The precise on-disk directory layout remains a design decision to implement and validate.

## Upstream path behavior

**VERIFIED FACT.** Wine's DOS-to-Unix file translation consults DOS devices, and its reverse translation can fall back to a Unix device. See pinned [`dlls/ntdll/unix/file.c`, lines 3404–3534](https://github.com/wine-mirror/wine/blob/7298a818b79446bae153b8d5aa27235ab3703bf9/dlls/ntdll/unix/file.c#L3404-L3534) and [lines 3919–4002](https://github.com/wine-mirror/wine/blob/7298a818b79446bae153b8d5aa27235ab3703bf9/dlls/ntdll/unix/file.c#L3919-L4002). A `Z:`-style mapping therefore must not be described as containment or sandboxing.

**VERIFIED FACT.** `SHGetKnownFolderPath` reaches known-folder lookup logic in `shell32`; the implementation also consults the `User Shell Folders` registry area. See pinned [`dlls/shell32/shellpath.c`, lines 3561–3596](https://github.com/wine-mirror/wine/blob/7298a818b79446bae153b8d5aa27235ab3703bf9/dlls/shell32/shellpath.c#L3561-L3596), [lines 3980–4051](https://github.com/wine-mirror/wine/blob/7298a818b79446bae153b8d5aa27235ab3703bf9/dlls/shell32/shellpath.c#L3980-L4051), and [line 2147](https://github.com/wine-mirror/wine/blob/7298a818b79446bae153b8d5aa27235ab3703bf9/dlls/shell32/shellpath.c#L2147). This supports configuring known folders through the appropriate Windows-visible settings; it does not prove the proposed XDG mappings work in COS.

## Reversibility and user access

**HIPÓTESIS — XDG folders.** When `xdg-user-dir` is installed, the runner resolves Documents, Desktop and Downloads from the user's configuration, maps them through Wine's native DOS-device path table (`W:`, `X:`, `Y:`), and records both modern GUID and applicable legacy names in `User Shell Folders`. It does not create host directories. An absent resolver leaves Wine defaults; an explicit root mapping or an existing conflicting drive is rejected. When multiple folders resolve to one host directory, a reverse DOS lookup can select any matching alias. This exposes the configured folders to the app; it is not containment.

**HYPOTHESIS.** Keep the mapping reversible and explicit through CLI arguments, file dialogs, and drag-and-drop (DnD). Each route should resolve to the same configured mount/path policy, and users should be able to inspect or change those mappings. Exact UX and permission semantics are not specified here.

**Design decision.** Implement a path-table patch in Wine. Discard per-app prefixes and per-app mounts with a minimal `drive_c`. Mapping in both directions must agree with Windows known-folder results.

## Open questions

**HIPÓTESIS — per-app Windows write routes.** `0005-map-windows-writes.patch` adds Temp, native `System32\spool`, Fonts, both Fusion assembly roots and WinSxS. Each maps to a separate persistent directory in the app data root. Existing WoW64 rules still apply before route matching. The smoke test checks file creation, not printing, font registration, COM activation or .NET installation. The route list is not exhaustive and is not a sandbox policy.

**HIPÓTESIS — Phase 1 patch.** `paths/0001-map-installer.patch` maps `C:\Windows\Installer` to the normalized absolute directory supplied in `CPOS_WINDOWS_INSTALLER`; without that variable Wine keeps its original lookup. Device-only names continue through `get_dos_device`. The forward lookup runs after the existing WoW64 redirect and the reverse path considers the route before generic drives at the same ancestor. This patch does not replace `User Shell Folders`: the runner must set those values consistently for user folders in a later milestone. The route is not a containment boundary.

- **NOT ANALYZED:** Exact Windows known-folder behavior for every application and XDG configuration, including unset or nonstandard XDG directories.
- **NOT ANALYZED:** Permission, symlink, case-sensitivity, and path-collision behavior for host mounts.
- **NOT ANALYZED:** Compatibility implications of application expectations around `Program Files`, `ProgramData`, or `Z:`.
