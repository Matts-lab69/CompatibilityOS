<!-- SPDX-License-Identifier: MIT -->

# Test coverage

`bash "tests/check-series.sh"` checks the series structure: safe relative paths, strictly increasing patch numbers, existing files, and no unlisted patches. This is a static check, not a patch application or build. Lint the shell files with `shellcheck "scripts/fetch-wine.sh" "tests/check-series.sh"`.

**HECHO VERIFICADO:** under Xvfb, [run 45155264148](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45155264148/job/137647184946) reported 11 passes: `dry`, `start`, `desktop-log`, `sync-fallback`, `socket`, `msi`, `paths`, `known-folders`, `windows-writes`, `services-config` and `wow64`. An earlier run failed only `desktop-log`; it passed after the check accepted Windows CRLF and Linux LF and added diagnostic output. These probes do not test COSMIC itself or real printing and font installation.

**SIN ANALIZAR:** RPCSS and Plug and Play behavior beyond smoke startup, font installation, printing, .NET, GUI file dialogs and drag-and-drop, concurrent session recovery, registry upgrades, and installer rollback. See [measurement protocol](../docs/05-medicion.md) for workloads and acceptance thresholds.
