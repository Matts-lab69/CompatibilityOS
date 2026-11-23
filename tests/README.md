# Future test coverage

`bash "tests/check-series.sh"` checks the series structure: safe relative paths, strictly increasing patch numbers, existing files, and no unlisted patches. This is a static check, not a patch application or build. Lint the shell files with `shellcheck "scripts/fetch-wine.sh" "tests/check-series.sh"`.

Reserved for smoke and regression checks: PE32/PE32+ and MSI install, reopen and uninstall; route round trips for arguments, dialogs and drag-and-drop; known folders; session concurrency, crash recovery and cleanup; registry upgrades; recipe regressions; installation dry run and rollback. User-selected final app/version matrix and acceptance thresholds remain open. See [measurement protocol](../docs/05-medicion.md). No tests were written or run.
