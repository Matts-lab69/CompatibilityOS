# Phase 1 architecture

**Design requirements and target architecture.** Linux remains monolithic and Wine is the engine. The integration changes the user experience; it is not evidence of faster graphics or broader compatibility. No divergent `ntdll`/`win32u` fork and no `umu-launcher`. The CI-tested subset is recorded in [the build notes](07-compilacion.md); the complete XDG folder map and graphics stack remain unimplemented.

**HIPÓTESIS — desktop integration.** `packaging/mime/compatibilityos.desktop` hands one local file to the runner; `compatibilityos.xml` adds the EXE and MSI globs. COSMIC behavior is **SIN ANALIZAR**; no `binfmt_misc` registration is planned for this delivery.

1. COSMIC dispatches `.exe` and `.msi` by MIME to the internal, non-interactive `cpos-run` (working name). Terminal integration uses `binfmt_misc` for `MZ` executables; MSI is a separate installer dispatch, not an `MZ` executable. See [packaging](../packaging/README.md).
2. The runner resolves app identity, prepares `~/.local/share/<app>` and `~/.config/<app>`, builds the [path table](02-rutas.md), prepares the [registry](03-registro.md), and applies the app recipe. Recipes may activate legitimate high-level vendor DLLs for personal use; none are redistributed.
3. Start wineserver automatically, with ephemeral session files under the user's `$XDG_RUNTIME_DIR` (normally `/run/user/<uid>`). Preserve background bootstrap without showing Wine's startup dialog. Shut down and clean up after the last app and its session processes finish, without removing persistent app data. See the [source audit](04-auditoria-wine.md) for the socket and boot dependencies.
4. Run the PE loader or dispatch an MSI to Wine's `msiexec` in the same app context. Route D3D9–11 via DXVK and D3D12 via VKD3D-Proton to Vulkan/Mesa/RADV. Initial display route: XWayland SDR on COSMIC. Initial audio: `winepulse.drv` on `pipewire-pulse`.
5. Detect whether `/dev/ntsync` is accessible and supported by the selected build. If unavailable, prefer fsync, then esync, **only if supported by that build and host**; otherwise use Wine's server synchronization. Record the effective backend, not just the requested one. No kernel changes.

**Reference setup, not measured here.** I use Gentoo with OpenRC and an AMD Vega integrated GPU. `/dev/ntsync` is unavailable in my reference environment, so I have not measured an ntsync baseline.

**Decision — sync availability.** `CPOS_SYNC=auto` leaves native ntsync selection to the patched wineserver; `CPOS_SYNC=wineserver` explicitly skips opening the device for the CompatibilityOS session. The current pin and reviewed patch series do not include fsync or esync, so both candidates are skipped rather than reported from environment switches alone. CI exercises explicit fallback in a fresh app context. **SIN ANALIZAR:** effective ntsync on a host with the device and compatible build headers.

**HYPOTHESIS.** App recipes may reduce crashes for specific workloads. D3D translation, Mesa and the driver are variables to control, not gains attributable to this integration without data. Compare against [the baseline protocol](05-medicion.md).

**NOT ANALYZED.** Actual fallback backends in the eventual pinned build, native Wine-Wayland versus XWayland, COSMIC's color-management and tearing-control protocol coverage, and the native PipeWire driver candidate from [Proton-CachyOS](https://github.com/CachyOS/proton-cachyos). SDR via XWayland does not depend on establishing those capabilities now.
