<!-- SPDX-License-Identifier: MIT -->
# Compatibility implementation plan

1. Audit synchronization patch provenance and require real backend detection. Retain the verified fallback when a compatible patch is not available.
2. Extend headless Windows API smoke probes for spool, fonts, Fusion and Plug and Play; distinguish API coverage from device or vendor-runtime compatibility.
3. Route desktop diagnostics to XDG state and add an optional failure notification.
4. Keep experimental PE execution limited to self-built AMD64 fixtures; exercise parser, relocations, import policy and TLS separately in CI.
5. Extend the isolated handle/object ABI and test layouts without opening a host device. Develop the per-thread queue in userspace with explicit cancellation and shutdown semantics.

No local compilation, module loading, benchmarking or release publication. CI remains the source of runtime evidence.

**HECHO VERIFICADO — fsync provenance check.** `git apply --check` on Wine `7298a818` rejected the [Valve futex_waitv series](https://github.com/ValveSoftware/wine/pull/128): its target files `dlls/ntdll/unix/fsync.c` and `server/fsync.c` do not exist at that pin. This is an upgrade to an existing fsync implementation, not a standalone port. No fsync/esync source was imported; the existing native ntsync → wineserver path remains. **SIN ANALIZAR:** an esync port and real intermediate fallback paths.
