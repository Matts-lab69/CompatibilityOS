# Objective and boundaries

**Requirement.** Double-click to run `.exe` and install `.exe`/`.msi` on Linux; no `wine` invocation in daily use, visible prefixes, or visible intermediary launcher. Deliver signed or verifiably hashed binaries from GitHub through an installer. Start with Gentoo/OpenRC and COSMIC; keep host-specific registration separate from the portable installer core.

**Target, not a commitment.** Aim for Phase 1 in under three months. Target fewer crashes on common Windows apps, including Adobe apps and games, without unacceptable startup, FPS, or memory regressions. No improvement or delivery date is established. Agree on the final app/version matrix and measurable success/stop thresholds before acceptance; the user selects the apps and runs final checks.

**Out of scope.** Kernel anti-cheat, DRM, Windows drivers, rewriting Wine's low-level DLLs, a new kernel architecture, and native DirectX in Mesa. Adobe launch failures are handled manually; other unsupported applications are deferred. Phase 2 is not authorized.

**Current scope.** Repository text only. No deployment, compilation, application tests, or system changes. The user's Gentoo kernel, configuration, and modules must not be changed or proposed for change.
