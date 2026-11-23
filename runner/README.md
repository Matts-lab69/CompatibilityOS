# Internal runner

`cpos-run [--dry-run] [--app ID] -- FILE [ARG ...]` dispatches `.exe` and `.msi` files. Without `--app`, the canonical file path determines the application ID; use the same explicit ID for an installer and its applications. The runner expects `wine/bin`, `base/version` and three registry files in the installed bundle. `CPOS_BUNDLE` selects an alternate bundle. It does not silently switch registry base versions for an existing app.

The pinned build contains no fsync/esync implementation. The runner probes `/dev/ntsync` and the patched server logs `cpos-sync: ntsync` when its open succeeds or `cpos-sync: wineserver` otherwise; the result is in the per-app Wine log. Device accessibility alone is not a measured backend. **SIN ANALIZAR:** Wine startup, registry initialization and backend logging in CI.
