# Installer design

Future script: download a versioned binary release from GitHub, authenticate it with a trusted signature or verify a pinned trusted checksum, and register launch integration for the chosen target. Do not compile locally. Be idempotent, support `--dry-run` without making changes, show planned changes, preserve previous registrations, and uninstall only owned artifacts. Preserve user app data by default.

The installer never edits kernel settings, recompiles the kernel, or loads modules. It only checks and reports whether `/dev/ntsync` is available and accessible; documentation can point to upstream ntsync requirements for a separate already-enabled environment, not propose changing the user's Gentoo kernel. No installer or system registration exists.
