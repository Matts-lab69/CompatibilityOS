#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
export LC_ALL=C

root=$(cd -- "${BASH_SOURCE[0]%/*}/.." && pwd -P)
shopt -s nullglob globstar dotglob
declare -A listed=()
previous=''
count=0
series_entry='^(paths|registry|upstream-candidates)/([0-9]{4})-[a-z0-9][a-z0-9-]*\.patch$'

fail() {
    printf '%s\n' "$1" >&2
    exit 1
}

while IFS= read -r entry || [[ -n $entry ]]; do
    [[ -z $entry || $entry == \#* ]] && continue
    [[ $entry =~ $series_entry ]] || fail "Invalid patch entry: $entry"
    number=${BASH_REMATCH[2]}
    [[ -z $previous || $number > $previous ]] || fail "Patch numbers must increase: $entry"
    patch="$root/patches/$entry"
    [[ -f $patch && ! -L $patch && ! -L ${patch%/*} ]] || fail "Missing or linked patch: $entry"
    listed[$entry]=1
    previous=$number
    count=$((count + 1))
done < "$root/patches/series"

for patch in "$root/patches/"**/*.patch; do
    entry=${patch#"$root/patches/"}
    [[ ${listed[$entry]:-} == 1 ]] || fail "Unlisted patch: $entry"
done
printf 'Patch series: %d entries verified.\n' "$count"
