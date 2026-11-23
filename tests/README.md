# Test coverage

`bash "tests/check-series.sh"` checks the series structure: safe relative paths, strictly increasing patch numbers, existing files, and no unlisted patches. This is a static check, not a patch application or build. Lint the shell files with `shellcheck "scripts/fetch-wine.sh" "tests/check-series.sh"`.

**HECHO VERIFICADO:** under Xvfb, [run 45113410407](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45113410407/job/137502444929) logged `ok dry`, `ok start`, `ok socket`, `ok msi`, `ok paths`, and `ok wow64`. The route check covers `C:\Windows\Installer`, not all known folders or dialogs.

**SIN ANALIZAR:** RPCSS and Plug and Play behavior beyond smoke startup, font installation, printing, .NET, GUI file dialogs and drag-and-drop, concurrent session recovery, registry upgrades, and installer rollback. See [measurement protocol](../docs/05-medicion.md) for workloads and acceptance thresholds.
