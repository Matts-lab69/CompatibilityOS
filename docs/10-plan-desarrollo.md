<!-- SPDX-License-Identifier: MIT -->
# Phase 1 development plan

## Fixed decisions and constraints

- Wine 11.20, one commit pin in `scripts/wine.lock`. Seed an effective registry copy per app under `~/.config/<app>/` from a versioned immutable base; apply `.reg` recipes once per migration.
- Standalone code and docs: MIT. Wine patches: LGPL-2.1-or-later. Add SPDX identifiers to new files.
- Do not modify the user's kernel or system or install local packages. Compile and run Wine only in GitHub Actions. Local verification is limited to reading, Git, GitHub CLI and ShellCheck.
- Run `df -h "$HOME"` before work and after each milestone; stop below 5 GiB available. Download Wine locally only in an external temporary shallow, blob-filtered checkout, capped at 2 GiB and removed after each milestone. Keep binaries, archives and caches out of Git. No local Docker, images or toolchains.
- Retain CI artifacts for seven days; cap cache at 5 GB if enabled and limit ccache. Clean up old artifacts with `gh`. Do not publish releases without authorization.
- Publish small first-person commits directly to `main`; verify each push with `git ls-remote`. Record diff stats, actual static checks, applicable CI output and free space at each milestone. Read failed CI logs with `gh run view --log-failed`; fix and retry up to five attempts per milestone, then stop and ask one question.
- Label claims **HECHO VERIFICADO** (source lines or actual output), **HIPÓTESIS**, or **SIN ANALIZAR**. Do not claim build or runtime success without CI output, or performance/crash improvements without a baseline.

## Milestones (in order)

**HECHO VERIFICADO — delivery record.** [Wine CI run 45113410407](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45113410407) built, smoke-tested and attested the bundle. The implemented routing subset is `C:\Windows\Installer`; the full XDG known-folder mapping, real COSMIC installation and the live USB baseline are not verified by this run. `fsync` and `esync` are not built from the pinned series.

1. [ ] Establish the ordered patch series, verified source download, static checks and artifact exclusions.
2. [ ] Add a bidirectional NT/Unix route table after WoW64 redirection, consistent with `User Shell Folders`; make `C:\Windows\Installer` writable per app first. Check patch applicability.
3. [ ] Place ephemeral Wine sockets under `/run/user/<uid>/`, coordinating `loader.c`, `server.c` and `server/request.c`.
4. [ ] Separate the wineboot update dialog from initialization; keep errors, wineboot and services initialization intact.
5. [ ] Implement `cpos-run`: write-free `--dry-run`, effective per-app registry, recipes, direct EXE and `msiexec` dispatch. Detect effective sync in order ntsync, fsync, esync, wineserver, considering runtime and build support.
6. [ ] Document and validate a versioned recipe schema; add MSI and EXE examples with versioned `.reg` migrations.
7. [ ] Add desktop and MIME integration for COSMIC. Document, but do not register, `binfmt_misc`.
8. [ ] Implement verifiable CI artifact installation: signature and hash checks, idempotence, write-free dry run, association restoration and preservation of user data on uninstall.
9. [ ] Build patched Wine in Actions with MinGW-w64, `--enable-archs=x86_64,i386` and initially `-j2`; upload artifacts and a hash manifest. In a separate Xvfb job report each result for startup, socket creation, MSI, EXE, bidirectional paths and both WoW64 views.
10. [ ] Document deepin-wine, umu-protonfixes and Bottles in `09-referencias.md`: licenses and candidates or exclusions for reuse; mark claims unverified until reviewed at source.
11. [ ] Document installation, usage and limits. List untested RPCSS, Plug and Play, fonts, printing and .NET behavior. Report CI outcomes, actual failures, live-USB baseline requirements and pending decisions.
