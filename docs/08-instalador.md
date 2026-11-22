# Installer and release design — documentation only

**HYPOTHESIS — future CI and installation.** Isolated CI builds pinned sources and patches, then publishes versioned binary bundles with a signed hash manifest and required source/notices. Before registration, match the selected artifact's hash against that manifest after verifying its signature with a trusted key, or against a checksum pinned via an independent trusted channel. No local compilation.

**HYPOTHESIS — change and removal rules.** Register only owned files and associations; record any replaced associations so uninstall can restore them without overwriting subsequent user changes. Same-version installation is idempotent. `--dry-run` reports planned actions without writing files or registering anything. Uninstall leaves app data intact by default. The installer never changes kernel settings, recompiles the kernel, or loads modules.

**VERIFIED FACT.** Wine `7298a818`, [`server/inproc_sync.c:40–52`](https://github.com/wine-mirror/wine/blob/7298a818b79446bae153b8d5aa27235ab3703bf9/server/inproc_sync.c#L40-L52), guards ntsync at compile time and opens `/dev/ntsync` at runtime.

**HYPOTHESIS — runtime fallback.** The installer only reports availability; the runner prefers usable ntsync, then fsync, then esync if the selected build and environment support each backend. Otherwise use wineserver synchronization. Record the actual backend; do not compare benchmarks across backends.

**NOT ANALYZED.** fsync/esync support or automatic fallback in the eventual GE/CachyOS patch combination; distribution-specific registration and binary portability. No installer or workflow is implemented.
