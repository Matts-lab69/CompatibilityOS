#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
fail() { printf 'CompatibilityOS: %s\n' "$*" >&2; exit 1; }
repo='Matts-lab69/CompatibilityOS'
dry=0
if [[ ${1:-} == --dry-run ]]; then dry=1; shift; fi
op=${1:-}
run=${2:-}
case "$op" in
    install) [[ $# == 2 && $run =~ ^[0-9]+$ ]] || fail 'expected an Actions run ID' ;;
    uninstall) [[ $# == 1 ]] || fail 'uninstall takes no arguments' ;;
    *) fail 'usage: install.sh [--dry-run] install RUN_ID | uninstall' ;;
esac
data=${XDG_DATA_HOME:-$HOME/.local/share}
config=${XDG_CONFIG_HOME:-$HOME/.config}
for path in "$data" "$config"; do [[ $path == /* ]] || fail 'expected absolute XDG paths'; done
root="$data/compatibilityos"
desktop="$data/applications/compatibilityos.desktop"
mime="$data/mime/packages/compatibilityos.xml"
defaults="$config/mimeapps.list"
state="$root/registration"
if (( dry )); then
    printf '%s run=%s bundle=%q desktop=%q mime=%q; preserves app data\n' "$op" "$run" "$root" "$desktop" "$mime"
    exit 0
fi
umask 077
if [[ $op == uninstall && ! -d $root ]]; then exit 0; fi
mkdir -p -- "$root"
exec 9>"$root/.lock"
flock -x 9
refresh() { update-mime-database "$data/mime"; update-desktop-database "$data/applications"; }
if [[ $op == uninstall ]]; then
    if [[ -d $state ]]; then
        if [[ -f $state/after ]] && cmp -s -- "$state/after" "$defaults"; then
            if [[ -f $state/before ]]; then cp -- "$state/before" "$defaults"; else rm -- "$defaults"; fi
        elif [[ -f $state/after ]]; then
            printf 'MIME defaults changed since installation; leaving them untouched.\n' >&2
        fi
        for item in 'desktop' 'mime'; do
            if [[ $item == desktop ]]; then target=$desktop; else target=$mime; fi
            if cmp -s -- "$target" "$state/$item"; then rm -- "$target"; fi
        done
        refresh
        rm -rf -- "$state"
    fi
    if [[ -L $root/current ]]; then rm -- "$root/current"; fi
    if [[ -d $root/versions ]]; then rm -rf -- "$root/versions"; fi
    exit 0
fi

command -v update-mime-database >/dev/null && command -v update-desktop-database >/dev/null &&
    command -v xdg-mime >/dev/null || fail 'missing MIME database tools'
sha=$(gh run view "$run" -R "$repo" --json headSha,headBranch,conclusion \
    --jq 'select(.headBranch == "main" and .conclusion == "success") | .headSha')
[[ $sha =~ ^[0-9a-f]{40}$ ]] || fail 'expected a successful main workflow'
version="$root/versions/$sha"
if [[ -L $root/current && $(readlink -- "$root/current") == "$version" && -f $state/after ]]; then exit 0; fi
for path in "$desktop" "$mime"; do
    if [[ -e $path || -L $path ]]; then
        if [[ $path == "$desktop" ]]; then item=desktop; else item=mime; fi
        cmp -s -- "$path" "$state/$item" || fail "refusing to overwrite $path"
    fi
done
[[ ! -L $defaults ]] || fail 'refusing symlinked MIME defaults'
tmp=$(mktemp -d -- "$root/.install.XXXXXXXX")
trap 'rm -rf -- "$tmp"' EXIT
gh run download "$run" -R "$repo" -n 'compatibilityos-linux-x86_64' -D "$tmp"
for file in 'SHA256SUMS' 'compatibilityos.tar.gz'; do
    gh attestation verify "$tmp/$file" -R "$repo" \
        --signer-workflow "$repo/.github/workflows/wine.yml" \
        --source-ref 'refs/heads/main' --source-digest "$sha"
done
read -r digest filename extra < "$tmp/SHA256SUMS"
[[ $digest =~ ^[0-9a-f]{64}$ && $filename == compatibilityos.tar.gz && -z $extra ]] || fail 'invalid hash manifest'
(cd -- "$tmp"; printf '%s  compatibilityos.tar.gz\n' "$digest" | sha256sum --check --strict -)
mkdir -p -- "$root/versions" "$data/applications" "$data/mime/packages" "$config" "$tmp/bundle"
if [[ ! -d $version ]]; then
    tar -xzf "$tmp/compatibilityos.tar.gz" --no-same-owner --no-same-permissions -C "$tmp/bundle"
    mv -T -- "$tmp/bundle" "$version"
fi
[[ -x $version/bin/cpos-run && -f $version/share/compatibilityos.xml ]] || fail 'incomplete bundle'
mkdir -p -- "$state"
if [[ ! -f $state/snapshot ]]; then
    if [[ -f $defaults ]]; then cp -- "$defaults" "$state/before"; fi
    touch -- "$state/snapshot"
fi
# Escape desktop-entry string and Exec quoting independently.
exe="$root/current/bin/cpos-run"
[[ $exe != *$'\n'* && $exe != *$'\r'* ]] || fail 'unsupported launcher path'
exe=${exe//\\/\\\\}; exe=${exe//\"/\\\"}; exe=${exe//\$/\\\$}; exe=${exe//\`/\\\`}; exe=${exe//%/%%}
exe=${exe//\\/\\\\}
sed -e "/^Exec=/d" -e '/^TryExec=/d' -- "$version/share/compatibilityos.desktop" > "$state/desktop"
printf 'Exec="%s" -- %%f\n' "$exe" >> "$state/desktop"
cp -- "$version/share/compatibilityos.xml" "$state/mime"
install -m 644 -- "$state/desktop" "$desktop"
install -m 644 -- "$state/mime" "$mime"
refresh
if [[ ! -f $state/after ]]; then
    xdg-mime default 'compatibilityos.desktop' 'application/x-ms-dos-executable' 'application/x-msi'
    cp -- "$defaults" "$state/after"
fi
ln -s -- "$version" "$tmp/current"
mv -Tf -- "$tmp/current" "$root/current"
