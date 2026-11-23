#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
temp=$1
run=$2
repo='Matts-lab69/CompatibilityOS'
export HOME="$temp/installer home" XDG_CURRENT_DESKTOP=COSMIC
export XDG_CONFIG_HOME="$HOME/config" XDG_DATA_HOME="$HOME/data"
mkdir -p -- "$HOME"
install() { bash 'installer/install.sh' "$@"; }
install --dry-run install "$run"
install --dry-run uninstall
[[ -z $(find "$HOME" -mindepth 1 -print -quit) ]]
echo 'ok write-free dry run'

mkdir -p -- "$XDG_CONFIG_HOME" "$XDG_DATA_HOME/applications" "$XDG_DATA_HOME/retained-app"
printf '%s\n' '[Default Applications]' 'application/x-msi=previous.desktop;' > "$XDG_CONFIG_HOME/mimeapps.list"
cp -- "$XDG_CONFIG_HOME/mimeapps.list" "$temp/original.defaults"
printf '%s\n' 'keep' > "$XDG_DATA_HOME/retained-app/data"
install install "$run"
echo 'ok attested installation'
desktop="$XDG_DATA_HOME/applications/compatibilityos.desktop"
desktop-file-validate "$desktop"
grep -F -- ' --desktop -- %f' "$desktop"
grep -F 'application/x-msi=compatibilityos.desktop' "$XDG_CONFIG_HOME/mimeapps.list"
root="$XDG_DATA_HOME/compatibilityos"
before=$(stat -c '%i:%Y' -- "$root/current" "$desktop" "$XDG_CONFIG_HOME/mimeapps.list")
install install "$run"
[[ $(stat -c '%i:%Y' -- "$root/current" "$desktop" "$XDG_CONFIG_HOME/mimeapps.list") == "$before" ]]
echo 'ok same-version idempotence'

# A changed file has no matching provenance, even from the right workflow.
printf '%s\n' 'not the attested manifest' > "$temp/SHA256SUMS"
if gh attestation verify "$temp/SHA256SUMS" -R "$repo" \
    --signer-workflow "$repo/.github/workflows/wine.yml" --source-ref 'refs/heads/main'; then
    printf 'Unexpected verification of altered manifest\n' >&2
    exit 1
fi
echo 'ok altered manifest rejected'

install uninstall
cmp -- "$temp/original.defaults" "$XDG_CONFIG_HOME/mimeapps.list"
[[ ! -e $desktop && ! -e $root/current && ! -e $root/versions ]]
grep -Fx 'keep' "$XDG_DATA_HOME/retained-app/data"
install uninstall
echo 'ok uninstall, restore and retain app data'
