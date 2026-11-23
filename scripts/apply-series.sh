#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail

if (( $# != 1 )); then
    printf 'Usage: bash %s WINE_SOURCE_DIRECTORY\n' "$0" >&2
    exit 2
fi
root=$(cd -- "${BASH_SOURCE[0]%/*}/.." && pwd -P)
source_dir=$1
pin=$(tail -n 1 -- "$root/scripts/wine.lock")
[[ $(git -C "$source_dir" rev-parse --verify 'HEAD^{commit}') == "$pin" ]]
[[ -z $(git -C "$source_dir" status --porcelain) ]]
bash "$root/tests/check-series.sh"
while IFS= read -r entry || [[ -n $entry ]]; do
    [[ -z $entry || $entry == \#* ]] && continue
    patch="$root/patches/$entry"
    git -C "$source_dir" apply --index --check "$patch"
    git -C "$source_dir" apply --index "$patch"
    printf 'Checked and applied %s\n' "$entry"
done < "$root/patches/series"
