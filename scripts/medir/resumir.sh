#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail

if (( $# != 1 )); then
    printf 'Usage: bash %s RESULTS.csv\n' "$0" >&2
    exit 2
fi
export LC_ALL=C
awk -F, '
function fail(message) {
    print "Line " NR ": " message > "/dev/stderr"
    invalid = 1
    exit 2
}
{ sub(/\r$/, "") }
/^#/ { next }
!header {
    if ($0 != "profile,app,version,scenario,build,sync,mode,run,started,crashes,hang,double_click,startup_ms,fps_mean,fps_min,peak_pss_mib,server_requests") fail("unexpected header")
    for (i = 9; i <= NF; i++) metric[i] = $i
    header = 1
    next
}
/^[[:space:]]*$/ { next }
{
    if (NF != 17) fail("expected 17 columns")
    for (i = 1; i <= 7; i++) if ($i !~ /^[[:alnum:]_][[:alnum:]_.+-]*$/) fail("invalid group field: " i)
    if ($6 !~ /^(ntsync|fsync|esync|wineserver|unknown)$/) fail("invalid sync backend")
    if ($7 !~ /^(clean|server|strace)$/) fail("invalid mode")
    if ($8 !~ /^[1-9][0-9]*$/) fail("invalid run number")
    if ($9 !~ /^[01]$/ || $10 !~ /^[0-9]+$/ || $11 !~ /^[01]$/) fail("invalid outcome")
    if ($12 != "" && $12 !~ /^[01]$/) fail("invalid double-click outcome")
    for (i = 13; i <= 16; i++) if ($i != "" && $i !~ /^[0-9]+(\.[0-9]+)?$/) fail("invalid measurement: " i)
    if ($17 != "" && $17 !~ /^[0-9]+$/) fail("invalid request count")
    if (($7 != "clean" && ($12 != "" || $13 != "" || $14 != "" || $15 != "" || $16 != "")) ||
        ($7 != "server" && $17 != "")) fail("measurement does not belong to this mode")
    if (($9 == 0 && $13 != "") || ($14 != "" && $15 != "" && $15 + 0 > $14 + 0)) fail("inconsistent measurement")
    key = $1 "," $2 "," $3 "," $4 "," $5 "," $6 "," $7
    if (seen[key, $8]++) fail("duplicate run in group")
    if (!runs[key]++) groups[++total] = key
    for (i = 9; i <= 17; i++) if ($i != "") value[key, i, ++samples[key, i]] = $i + 0
}
END {
    if (invalid) exit 2
    if (!total) { print "No result rows." > "/dev/stderr"; exit 2 }
    print "profile,app,version,scenario,build,sync,mode,runs,metric,n,median,min,max,range,total"
    for (g = 1; g <= total; g++) {
        key = groups[g]
        split(key, fields, ",")
        if (fields[7] == "clean" && runs[key] < 5) print "Fewer than five runs: " key > "/dev/stderr"
        for (i = 9; i <= 17; i++) {
            n = samples[key, i]
            if (!n) continue
            sum = 0
            for (j = 1; j <= n; j++) {
                v = value[key, i, j]
                sum += v
                for (p = j; p > 1 && sorted[p-1] > v; p--) sorted[p] = sorted[p-1]
                sorted[p] = v
            }
            mid = int((n + 1) / 2)
            median = n % 2 ? sorted[mid] : (sorted[mid] + sorted[mid+1]) / 2
            printf "%s,%d,%s,%d,%.10g,%.10g,%.10g,%.10g,%s\n", key, runs[key], metric[i], n, median, sorted[1], sorted[n], sorted[n] - sorted[1], (i <= 12 ? sum : "")
        }
    }
}' < "$1"
