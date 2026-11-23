# Installer and artifact verification

**HECHO VERIFICADO — artifact creation.** [Run 45113410407](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45113410407) uploaded an Actions artifact and attested both the tarball and its SHA-256 manifest. This is not a release. See the [installer guide](../installer/README.md) for consumer verification.

**HYPOTHESIS — change and removal rules.** Register only owned files and associations; record any replaced associations so uninstall can restore them without overwriting subsequent user changes. Same-version installation is idempotent. `--dry-run` reports planned actions without writing files or registering anything. Uninstall leaves app data intact by default. The installer never changes kernel settings, recompiles the kernel, or loads modules.

**VERIFIED FACT.** Wine `7298a818`, [`server/inproc_sync.c:40–52`](https://github.com/wine-mirror/wine/blob/7298a818b79446bae153b8d5aa27235ab3703bf9/server/inproc_sync.c#L40-L52), guards ntsync at compile time and opens `/dev/ntsync` at runtime.

**HYPOTHESIS — runtime fallback.** The installer only reports availability; the runner prefers usable ntsync, then fsync, then esync if the selected build and environment support each backend. Otherwise use wineserver synchronization. Record the actual backend; do not compare benchmarks across backends.

**HIPÓTESIS — implementation.** `installer/install.sh` selects an explicit successful Actions run from `main`, verifies GitHub artifact attestations for both files and checks SHA-256 before extraction. It installs under the user's XDG paths, and restores an untouched MIME defaults file on uninstall. If the file has since changed it is left alone; owned MIME/desktop files are removed only if unchanged. Per-app data and registry are retained.

**SIN ANALIZAR.** Signature verification, COSMIC association behavior and binary portability on Gentoo/OpenRC. The installer does not modify the kernel.
