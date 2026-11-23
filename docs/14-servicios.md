<!-- SPDX-License-Identifier: MIT -->
# Headless Windows subsystem probes

`tests/windows-subsystems.c` is built with MinGW and invoked by the Xvfb smoke job. It requires no physical GPU, printer or test device. Each subsystem prints `ok` or `FAIL`, and any failed check makes the process exit nonzero.

The font check uses `EnumFontFamiliesExW`. The Xvfb job also supplies DejaVu Sans as a fixture: the probe copies it into the app's routed `C:\Windows\Fonts`, creates a per-user registry value, loads it with `AddFontResourceExW(FR_PRIVATE)`, then removes all three. This does not test persistent font discovery in a new process. The spool check accepts empty printer lists and tests buffer sizing when printers exist. The Plug and Play probe creates and discards a synthetic device record without registering hardware or installing a driver.

Fusion is probed dynamically: `fusion.dll` must export `CreateAssemblyCache`, and creating/releasing the assembly-cache COM object must succeed. If the runtime does not provide that API, the check fails rather than treating absence as success. This is not proof of managed assembly binding, GAC install/uninstall, .NET runtime activation, or execution of managed code.

**HECHO VERIFICADO:** [run 45165209485](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45165209485) passed the font and synthetic Plug and Play checks. It failed the empty-printer case in the original probe and failed bare-name `LoadLibraryW` for Fusion with error 126. Wine's pinned `loader/wine.inf.in` skips fake Fusion DLLs in the Mono package; the build log shows that Wine's builtin was built. The updated probe loads the packaged builtin by explicit path rather than requiring registration under a runtime that this test does not install. This does not establish the cause of every error 126 or verify framework discovery.

**SIN ANALIZAR:** actual font installation, printer-job submission, PnP driver/device behavior and Fusion assembly binding/runtime activation. These are outside this fixture-only probe.

**HECHO VERIFICADO:** [attempt 2](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45171969671/attempts/2) logged `ok fonts`, `ok spool`, `ok plug-and-play-fixture`, `ok fusion`, and `ok subsystem-probes`. Fusion reported `CreateAssemblyCache succeeded (runtime activation not tested)`.
