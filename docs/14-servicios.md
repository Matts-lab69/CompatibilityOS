<!-- SPDX-License-Identifier: MIT -->
# Headless Windows subsystem probes

`tests/windows-subsystems.c` is built with MinGW and invoked by the Xvfb smoke job. It requires no physical GPU, printer or test device. Each subsystem prints `ok` or `FAIL`, and any failed check makes the process exit nonzero.

The font check creates a memory DC and calls `EnumFontFamiliesExW`; it verifies enumeration succeeds, not that any particular family exists or that fonts can be installed. The spool check exercises `EnumPrintersW`'s insufficient-buffer sizing contract and then performs the real enumeration; an empty printer list is acceptable. The Plug and Play check creates a SetupAPI device-information list and a generated-ID synthetic device-info record, then destroys the list without registering or installing hardware. It does not claim device enumeration, driver installation, service startup, or physical-device support.

Fusion is probed dynamically: `fusion.dll` must export `CreateAssemblyCache`, and creating/releasing the assembly-cache COM object must succeed. If the runtime does not provide that API, the check fails rather than treating absence as success. This is not proof of managed assembly binding, GAC install/uninstall, .NET runtime activation, or execution of managed code.

**SIN ANALIZAR:** actual font installation, printer-job submission, PnP driver/device behavior and Fusion assembly binding/runtime activation. These are outside this fixture-only probe.
