# CompatibilityOS

I'm building CompatibilityOS so Linux users can double-click a Windows `.exe` or `.msi` without invoking `wine` or managing prefixes. Phase 1 uses Wine. [Experimental Phase 2](docs/12-fase2.md) is separate from the installer and normal launcher; it is not a Wine replacement or a microkernel.

**HECHO VERIFICADO:** [GitHub Actions run 45155264148](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45155264148) built and attested Wine 11.20 for Linux x86-64. It passed 11 Xvfb checks, including desktop log capture, known folders, per-app writes and wineserver fallback. No releases have been published.

## Install and use

1. Check out the repository and choose a successful [Wine CI run](https://github.com/Matts-lab69/CompatibilityOS/actions/workflows/wine.yml) on `main` with an unexpired `compatibilityos-linux-x86_64` artifact. Artifacts expire after seven days. Use authenticated `gh` with attestation support and the XDG/MIME tools listed in the [installer guide](installer/README.md).
2. Preview with `bash "installer/install.sh" --dry-run install RUN_ID`. Remove `--dry-run` to install after the script verifies provenance and SHA-256. No compilation or privileges are required.
3. Open a trusted `.exe` or `.msi` through your file manager, or invoke `"${XDG_DATA_HOME:-$HOME/.local/share}/compatibilityos/current/bin/cpos-run" --app example -- "/path/to/program.exe"`. Use the same app ID for the installer and installed executable when they need to share state.
4. Remove the bundle with `bash "installer/install.sh" uninstall`. Per-app data and registry remain. If you change MIME defaults after installation, the installer leaves your modified list alone.

**SIN ANALIZAR:** installation and double-click on Gentoo/OpenRC with COSMIC, ntsync on a supported host, broad Windows app compatibility, and performance. The runner is not a security sandbox. Kernel changes, `binfmt_misc` registration, DXVK/VKD3D integration, and upstream proprietary DLL redistribution are not part of this CI artifact.

The launcher records per-app diagnostics under `"${XDG_STATE_HOME:-$HOME/.local/state}/cpos"`; desktop mode does not open a terminal. Native ntsync and the wineserver fallback are compiled; fsync and esync are not integrated. See the [verification inventory](docs/13-estado.md) and the separate [EXPERIMENTAL Phase 2](docs/12-fase2.md).

The manual [Wine CI workflow](.github/workflows/wine.yml) also schedules static and isolated Phase 2 checks as independent jobs. Installer verification follows a successful attested Wine CI run; neither workflow deploys to a user's host.

I license my standalone project code under [MIT](LICENSE) and my patches to Wine under `LGPL-2.1-or-later`.

- [Design, audit, and measurement documents](docs/README.md)
- [Patch series](patches/README.md), [internal runner](runner/README.md), and [application recipes](recipes/README.md)
- [Desktop and terminal integration](packaging/README.md), [installer](installer/README.md), and [CI](ci/README.md)
- [Test coverage](tests/README.md) and [licensing status](docs/06-licencias.md)
