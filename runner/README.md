<!-- SPDX-License-Identifier: MIT -->

# Internal runner

`cpos-run [--dry-run] [--desktop] [--app ID] -- FILE [ARG ...]` dispatches `.exe` and `.msi` files. Without `--app`, the canonical file path determines the application ID; use the same explicit ID for an installer and its applications. The runner expects `wine/bin`, `base/version` and three registry files in the installed bundle. `CPOS_BUNDLE` selects an alternate bundle. It does not silently switch registry base versions for an existing app.

Both modes write Wine diagnostics, including the server's `cpos-sync:` startup marker, to `${XDG_STATE_HOME:-$HOME/.local/state}/cpos/APP/wine.log`. Console mode also sends them to stderr. `--desktop` runs without terminal output. A failed launch tries an error dialog with `zenity`, then `kdialog`, then a `notify-send` notification when available. Notification failures do not replace the application's exit status.

The pinned build contains no fsync/esync implementation. The runner probes `/dev/ntsync` and the patched server logs `cpos-sync: ntsync` when its open succeeds or `cpos-sync: wineserver` otherwise; the result is in the per-app Wine log. Device accessibility alone is not a measured backend. **HECHO VERIFICADO:** the [Xvfb smoke test](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45113410407/job/137502444929) logged `cpos-sync: wineserver` and passed startup, MSI migration and WoW64 registry checks. **SIN ANALIZAR:** ntsync in a live environment.
