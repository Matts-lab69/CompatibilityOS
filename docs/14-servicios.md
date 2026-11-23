<!-- SPDX-License-Identifier: MIT -->
# Headless Windows subsystem probes

`tests/windows-subsystems.c` is built with MinGW and invoked by the Xvfb smoke job. It requires no physical GPU, printer or test device. Each subsystem prints `ok` or `FAIL`, and any failed check makes the process exit nonzero.

The font check creates a memory DC and calls `EnumFontFamiliesExW`; it verifies enumeration succeeds, not that any particular family exists or that fonts can be installed. The spool check exercises `EnumPrintersW`'s insufficient-buffer sizing contract and then performs the real enumeration; an empty printer list is acceptable. The Plug and Play check creates a SetupAPI device-information list and a generated-ID synthetic device-info record, then destroys the list without registering or installing hardware. It does not claim device enumeration, driver installation, service startup, or physical-device support.

Fusion is probed dynamically: `fusion.dll` must export `CreateAssemblyCache`, and creating/releasing the assembly-cache COM object must succeed. If the runtime does not provide that API, the check fails rather than treating absence as success. This is not proof of managed assembly binding, GAC install/uninstall, .NET runtime activation, or execution of managed code.

**HECHO VERIFICADO:** [run 45165209485](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45165209485) passed the font and synthetic Plug and Play checks. It failed the empty-printer case in the original probe and failed bare-name `LoadLibraryW` for Fusion with error 126. Wine's pinned `loader/wine.inf.in` skips fake Fusion DLLs in the Mono package; the build log shows that Wine's builtin was built. The updated probe loads the packaged builtin by explicit path rather than requiring registration under a runtime that this test does not install. This does not establish the cause of every error 126 or verify framework discovery.

**SIN ANALIZAR:** actual font installation, printer-job submission, PnP driver/device behavior and Fusion assembly binding/runtime activation. These are outside this fixture-only probe.
