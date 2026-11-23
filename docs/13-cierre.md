<!-- SPDX-License-Identifier: MIT -->
# Compatibility implementation plan

1. Audit synchronization patch provenance and require real backend detection. Retain the verified fallback when a compatible patch is not available.
2. Extend headless Windows API smoke probes for spool, fonts, Fusion and Plug and Play; distinguish API coverage from device or vendor-runtime compatibility.
3. Route desktop diagnostics to XDG state and add an optional failure notification.
4. Keep experimental PE execution limited to self-built AMD64 fixtures; exercise parser, relocations, import policy and TLS separately in CI.
5. Extend the isolated handle/object ABI and test layouts without opening a host device. Develop the per-thread queue in userspace with explicit cancellation and shutdown semantics.

No local compilation, module loading, benchmarking or release publication. CI remains the source of runtime evidence.

**HECHO VERIFICADO — CI infrastructure.** In [run 45165292614](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45165292614), APT was still downloading when the fixture and kernel jobs hit their 10- and 15-minute limits. Neither reached checkout or compilation. [The retry](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45167762949) uses a single MinGW compiler variant, trims kernel build dependencies, and bounds APT retries and HTTP waits per request. Both jobs reached compilation and passed; see [Phase 2 evidence and limits](12-fase2.md#follow-up-implementation).

**HECHO VERIFICADO — fsync provenance check.** `git apply --check` on Wine `7298a818` rejected the [Valve futex_waitv series](https://github.com/ValveSoftware/wine/pull/128): its target files `dlls/ntdll/unix/fsync.c` and `server/fsync.c` do not exist at that pin. This is an upgrade to an existing fsync implementation, not a standalone port. No fsync/esync source was imported; the existing native ntsync → wineserver path remains. **SIN ANALIZAR:** an esync port and real intermediate fallback paths.

## Phase 1 status

- **HECHO VERIFICADO:** [run 45165805273](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45165805273) built Wine and passed the desktop notification stub and the empty-printer spooler check. Fusion was the remaining failure (`LoadLibraryW` error 126). The earlier [run 45165209485](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45165209485) passed font enumeration and synthetic Plug and Play setup.
- **HECHO VERIFICADO — CI gate:** [attempt 1 at `e311d55`](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45171969671/attempts/1) built and uploaded the staging bundle. The test job exceeded its 15-minute limit while APT was downloading test dependencies, before the Xvfb suite ran. Attestation was skipped. [Static checks](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45171969718) passed.
- **SIN ANALIZAR:** [attempt 2](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45171969671/attempts/2) retries the failed jobs using the existing build. Loading the packaged Fusion DLL by path and attesting the tested bundle remain unverified.
- **Still unimplemented:** fsync/esync and tests for those fallback steps. CI previously verified the wineserver fallback, not active ntsync on a host with the device.
- **SIN ANALIZAR — runtime coverage:** font installation and registration, actual spool job submission, GAC installation and managed .NET execution, physical Plug and Play or driver installation, and real COSMIC double-click and graphical dialogs. Enumeration, synthetic device records and the notification stub do not cover those cases. See [subsystem probe scope](14-servicios.md).
