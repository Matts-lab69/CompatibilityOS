<!-- SPDX-License-Identifier: MIT -->

# Measurement protocol

**Status: not run.** The host has no `/dev/ntsync` according to the user, so the intended ntsync baseline is blocked. A future authorized live environment that already supports it (for example a CachyOS or Fedora USB, if verified) can be evaluated. Keep both sides of each comparison in the same environment on the same machine. Do not change the Gentoo kernel or compare Gentoo results directly against live-USB results.

1. **Freeze the workload.** The user chooses apps and exact versions. Define the installer, first launch, steady-state action or repeatable game scene, crash criteria, warmup, repetition count, success thresholds, and stop criteria before testing. Separate cold and warm launches. Keep failed trials in the record.
2. **Pair the builds.** Measure unmodified Wine at the same revision against CompatibilityOS; include unmodified Proton when it can run the same workload. Record exact builds, recipe set, DXVK/VKD3D, OS/kernel, Mesa/driver, compositor, display settings, power profile and effective synchronization backend. Never compare trials with different backends. Hold other settings constant; explicitly label comparisons that isolate recipe changes from runner/patch changes.
3. **Measure without tracing.** Count crashes per attempt and distinguish hangs and normal exits; measure from launch to a defined ready event; report FPS and frame-time percentiles for a fixed scene; sample peak and steady PSS for the app, wineserver, and associated processes without double-counting shared memory. Use a clean desktop session for RAM and FPS; record background load, swap and memory pressure. Preserve raw results and report dispersion, not just a best run.
4. **Diagnose in separate runs.** Use `WINEDEBUG=+server` to count protocol requests (exclude replies), `strace -f` to see process and syscall activity, and `perf` to sample hotspots or counters if available. Use separate runs per instrument and never reuse traced timings as baseline data. Record diagnostic configuration and attributable failure logs.

**Resource guard.** I do not compile Wine on my reference machine. In an authorized build environment with limited memory, I start with at most two parallel jobs and raise the cap only after observing peak memory, swap pressure and headroom. I set CI concurrency per runner rather than inheriting the visible CPU count.

**HYPOTHESIS.** Recipes may improve crash rate; tracing may locate bottlenecks. Neither is a result. No improvement claim without paired measurements and an agreed threshold. No deadline commitment before baseline evidence.

**USB baseline checklist.** Use a live Linux image that already exposes an accessible `/dev/ntsync`, a writable external results location, and paired builds (unmodified Wine at the same pin and CompatibilityOS). Verify both builds use the same effective backend, GPU driver, display mode, app versions and workload on the same machine; agree on repetitions and acceptance thresholds first. A CI smoke pass is not a performance baseline.
