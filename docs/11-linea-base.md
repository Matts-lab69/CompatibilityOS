<!-- SPDX-License-Identifier: MIT -->
# Live USB baseline protocol

**SIN ANALIZAR:** no baseline has been run. The [Xvfb smoke test](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45113410407) used `wineserver` synchronization; it is not a hardware, performance, or COSMIC result.

## Prepare the live session

- Boot a Linux x86-64 live USB with a graphical session, a kernel that already exposes `/dev/ntsync`, and glibc plus runtime libraries compatible with the selected builds. The CI bundle was built on Ubuntu 24.04; the presence of glibc alone does not guarantee ABI compatibility. Use a live image that already provides Bash, GNU coreutils, `awk`, `gh`, `vulkaninfo`, `strace` and the necessary Vulkan libraries. No `sudo`, package installs, kernel changes or writes to internal disks. Use RAM-backed storage for the builds and prefixes, or an external test volume; save results on external storage. Do not mount internal disks read-write.
- Run `bash "scripts/medir/preflight.sh"` **inside the live session**. Its checks include `getconf GNU_LIBC_VERSION`, `test -c '/dev/ntsync' && ( : < '/dev/ntsync' )`, and `vulkaninfo --summary`. Save the output and confirm that RADV is attached to the intended AMD GPU, not a software renderer or a different GPU. Verify the actual runtime architecture for each workload (including DXVK's 32-bit DLLs for PE32 where applicable). A successful device open is not proof that Wine selected ntsync.
- For double-click tests, use the same COSMIC session and file manager, with disposable XDG directories. If the image cannot provide COSMIC, record other desktop tests separately; they cannot satisfy COSMIC acceptance.

## Freeze the comparison

1. Select a successful [Wine CI run](https://github.com/Matts-lab69/CompatibilityOS/actions/workflows/wine.yml) on `main` with an unexpired `compatibilityos-linux-x86_64` artifact. Use the [installer](../installer/README.md) in the disposable live session, or download with `gh run download "$run_id" -R 'Matts-lab69/CompatibilityOS' -n 'compatibilityos-linux-x86_64' -D "$artifact_dir"`. If downloading manually, set `head_sha` to the run's full head SHA and verify before extraction:

   ```bash
   (
       cd -- "$artifact_dir" || exit 1
       for subject in 'compatibilityos.tar.gz' 'SHA256SUMS'; do
           gh attestation verify "$subject" -R 'Matts-lab69/CompatibilityOS' \
               --signer-workflow 'Matts-lab69/CompatibilityOS/.github/workflows/wine.yml' \
               --source-ref 'refs/heads/main' --source-digest "$head_sha" || exit 1
       done
       sha256sum --strict --check 'SHA256SUMS'
   )
   ```

   Do not extract unless the subshell exits successfully. Record the run, commit and tarball hash. Actions artifacts expire after seven days; a new run must be recorded as a new input.
2. Primary control: obtain an unmodified Wine build at the [project pin](../scripts/wine.lock) with equivalent build settings/new WoW64. If none is available, obtain a separately produced vanilla Actions artifact before claiming a pinned comparison; do not compile locally. Optional second control: download an exact unmodified tag from [GE-Proton](https://github.com/GloriousEggroll/proton-ge-custom/releases), verify published checksums where available, and use its supported launch environment. Record its launcher/runtime and treat it as a whole-stack comparison, not a substitute for vanilla Wine. Do not use GE when equivalent execution, rendering and sync cannot be established.
3. Test on the same machine, live image, kernel, Mesa/RADV, GPU, compositor, resolution, refresh rate and power profile. Pin the exact same DXVK/VKD3D-Proton releases, architectures, hashes and settings when applicable; record `none` for unused components and verify the renderer actually loaded. The CI bundle does not ship DXVK/VKD3D-Proton. Do not compare a bundled runtime with different graphics versions under the same profile.
4. Record the **effective** sync backend of each build, saving evidence for each attempt (e.g. the `cpos-sync:` log for CompatibilityOS, or the build's own backend diagnostics; use a separate verification run if it requires tracing). Do not infer it from `/dev/ntsync`, environment flags or a requested backend. If equal effective backends cannot be confirmed, mark `unknown` or use separate profiles and exclude from paired conclusions.
5. Give each build independent disposable app state. Restore equivalent initial state per run; for clean installs use fresh state, and for warm launches use equivalent seeded state. Freeze recipe policy and document differences. Record all fixed inputs, including command/association per build, app hashes, window/scene duration and measurement tool versions, in the external run notes keyed by `profile`.

## App matrix (fill in before testing)

| Category | App | Version / SHA-256 | Ready event / fixed workload | Timeout |
| --- | --- | --- | --- | --- |
| MSI installer | | | | |
| Simple EXE | | | | |
| DirectX 9 | | | | |
| DirectX 11 | | | | |
| DirectX 12 | | | | |
| .NET | | | | |
| App that currently fails | | | | |

## Runs and metrics

- Run at least **five paired clean attempts per app, build and scenario**. Alternate which build goes first, reuse paired run IDs, separate cold and warm trials, and keep every failure. Before starting define the same ready event, timeout and observation window for both builds. An independent double-click scenario starts from a file-manager double-click; use the normal association for each build, or document that the control has none. Do not pool CLI and desktop results.
- `started`: reached the agreed ready event before timeout (for MSI, for example, completion and an expected installed file). `crashes`: count attributable unexpected process exits in the observation window, including before ready; do not count normal exits or test cleanup. `hang`: timed out or stopped responding, separate from a crash. `double_click`: dispatch reached the ready event with no terminal command or intermediate launcher; blank if not tested. Record startup in milliseconds from launch/click to ready using a monotonic clock or timestamped video; leave it blank if ready was never reached.
- For graphics runs, use the same frame capture tool and fixed scene after warmup. Report mean FPS as frames divided by elapsed seconds, and minimum FPS as the lowest count in complete one-second windows. No FPS data for runs without a comparable full scene. For RAM, sample the sum of `Pss:` from `/proc/<pid>/smaps_rollup` once per second for the isolated app, wineserver and associated processes (unique PIDs, excluding observer processes). Record peak sampled sum in MiB; if required processes cannot be sampled, report missing rather than zero.
- Repeat the same fixed workload in **separate** `server` and `strace` attempts; never use traced timing, FPS or RAM as baseline results. Run with no pre-existing wineserver for that prefix so logging covers startup. Set `run_cmd` to the same pre-recorded direct CLI invocation as a Bash array (wrapper and file as separate arguments), and `log_dir` to external test storage. Run on the live USB only:

  ```bash
  env WINEDEBUG=+server "${run_cmd[@]}" 2>"$log_dir/wineserver.log"
  env -u WINEDEBUG strace -f -tt -o "$log_dir/syscalls.log" -- "${run_cmd[@]}"
  ```

  Count incoming protocol requests in the `+server` log, excluding replies and diagnostics. Use one log stream: do not merge the runner's forwarded stderr with its stored copy, and do not deduplicate identical requests within a single stream. Check request/reply syntax in each pinned build before counting; retain raw logs and record the matching rule. If logging is incomplete or the syntax cannot be distinguished, leave `server_requests` blank. `strace -f` is for process/syscall diagnosis, not a proxy for wineserver request counts. Keep logs private if they contain file paths or application data.

## Acceptance and stop conditions

**HIPÓTESIS — proposed, not agreed:** for MSI and simple EXE, require 5/5 successful COSMIC double-click launches with no crashes or hangs. Across the agreed app set, require at least as many clean turns (ready, no crash or hang) per app as the control, and more clean turns for the agreed currently failing app. Report paired outcomes; FPS is descriptive, not the success criterion. Five runs detect obvious failures, not statistical significance.

**HIPÓTESIS — stop rule:** stop immediately on a write outside the test volume, data loss or a host crash. Stop the affected comparison as inconclusive if glibc/Vulkan preflight fails, the effective backend or graphics versions cannot be matched, or either arm cannot complete five recorded comparable attempts. If the completed matrix fails the agreed reliability gate, reject the candidate for this baseline rather than discarding failures or continuing until it passes.

## CSV and summary

Copy [`resultados.csv`](../scripts/medir/resultados.csv) to external storage. Use one row per attempt, `clean`, `server` or `strace` as the mode, and `ntsync`, `fsync`, `esync`, `wineserver` or `unknown` as `sync`. The first seven columns are ASCII identifiers (no spaces, commas or quoting); put names and details in the notes. Use `0`/`1` for outcomes, positive integer run IDs, and a decimal point for measurements. Blank means not measured; enter `0` only for an observed zero. Keep `server_requests` exclusive to `server` mode and startup/FPS/RAM/double-click metrics exclusive to `clean` mode.

Run `bash "scripts/medir/resumir.sh" "$results_dir/resultados.csv" > "$results_dir/resumen.csv"`. It groups by profile, app/version, scenario, build, backend and mode; reports runs, sample count, median, minimum, maximum and range per metric, plus totals for outcomes and crash counts. It warns for clean groups under five runs and does not certify pairing or acceptance: check matching run IDs, profile metadata and per-run combinations separately.
