<!-- SPDX-License-Identifier: MIT -->

# Upstream patch candidates

Review [GE-Proton](https://github.com/GloriousEggroll/proton-ge-custom) and [Proton-CachyOS](https://github.com/CachyOS/proton-cachyos) by exact revision before importing anything. Candidates include syscall dispatch, video, controllers and native PipeWire; presence and value in the selected revisions are not assumed. Record affected files, provenance, dependency order, rebase cost, and measured evidence. No import or change to the accepted anti-cheat limits is authorized.

**HECHO VERIFICADO:** `git apply --check` of [ValveSoftware/wine#128](https://github.com/ValveSoftware/wine/pull/128) on the unmodified `7298a818` checkout fails because `dlls/ntdll/unix/fsync.c` and `server/fsync.c` are absent. This patch updates an existing fsync implementation; it is not a standalone import. The [Wine Staging patch inventory at `e91fe63`](https://github.com/wine-staging/wine-staging/tree/e91fe639122b69d6dab50d923ee65edd37bd0516/patches) has no esync series. The historical esync branches cited in [the status inventory](../../docs/13-estado.md) do not establish applicability to this pin. Neither backend is included in the series; the runner does not enable either via environment flags. **SIN ANALIZAR:** a compatible forward-port and active ntsync runtime coverage.
