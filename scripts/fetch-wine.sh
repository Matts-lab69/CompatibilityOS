#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail

if (( $# != 1 )); then
    printf 'Usage: bash %s NEW_DIRECTORY\n' "$0" >&2
    exit 2
fi

script_dir=$(cd -- "${BASH_SOURCE[0]%/*}" && pwd -P)
mapfile -t pin_lines < "$script_dir/wine.lock"
if (( ${#pin_lines[@]} != 2 )) || [[ ! ${pin_lines[1]} =~ ^[0-9a-f]{40}$ ]]; then
    printf 'Invalid Wine commit pin.\n' >&2
    exit 1
fi
commit=${pin_lines[1]}
destination=$1
if [[ -z $destination || -e $destination || -L $destination ]]; then
    printf 'Expected a new destination: %s\n' "$destination" >&2
    exit 2
fi

git -c core.hooksPath=/dev/null -c fetch.fsckObjects=true clone \
    --no-checkout --no-tags --depth=1 --filter=blob:none --revision="$commit" -- \
    'https://github.com/wine-mirror/wine.git' "$destination"
actual=$(git -C "$destination" rev-parse --verify 'HEAD^{commit}')
if [[ $actual != "$commit" ]]; then
    printf 'Wine commit mismatch: expected %s, got %s\n' "$commit" "$actual" >&2
    exit 1
fi
git -C "$destination" -c core.hooksPath=/dev/null checkout --detach "$commit"
git -C "$destination" fsck --full --no-dangling
printf 'Verified Wine commit: %s\n' "$commit"
