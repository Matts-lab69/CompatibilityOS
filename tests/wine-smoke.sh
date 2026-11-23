#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
temp=$1
root=$(pwd -P)
export HOME="$temp/test-home" XDG_RUNTIME_DIR="$temp/test-runtime" CPOS_BUNDLE="$temp/bundle"
export XDG_CONFIG_HOME="$HOME/.config" XDG_DATA_HOME="$HOME/.local/share"
export WINEDLLOVERRIDES='mscoree,mshtml='
mkdir -p -- "$HOME" "$CPOS_BUNDLE" "$temp/fixtures"
mkdir -m 700 -- "$XDG_RUNTIME_DIR"
mkdir -p -- "$XDG_CONFIG_HOME" "$HOME/My Documents" "$HOME/My Desktop" "$HOME/My Downloads"
printf '%s\n' "XDG_DOCUMENTS_DIR=\"\$HOME/My Documents\"" "XDG_DESKTOP_DIR=\"\$HOME/My Desktop\"" \
    "XDG_DOWNLOAD_DIR=\"\$HOME/My Downloads\"" > "$XDG_CONFIG_HOME/user-dirs.dirs"
(cd -- "$temp/out"; sha256sum --check 'SHA256SUMS'; tar -xzf 'compatibilityos.tar.gz' -C "$CPOS_BUNDLE")
cd -- "$temp/fixtures"
x86_64-w64-mingw32-gcc "$root/tests/probe.c" -ladvapi32 -lshell32 -lole32 -o 'probe64.exe'
i686-w64-mingw32-gcc "$root/tests/probe.c" -ladvapi32 -lshell32 -lole32 -o 'probe32.exe'
cp -- "$root/tests/probe.wxs" 'probe.wxs'
wixl -o 'probe.msi' 'probe.wxs'
runner="$CPOS_BUNDLE/bin/cpos-run"
wine="$CPOS_BUNDLE/wine/bin/wine"
exe64="$PWD/probe64.exe"
exe32="$PWD/probe32.exe"
msi="$PWD/probe.msi"
app() { timeout 90 "$runner" --app 'smoke' -- "$@"; }
check_start() { "$wine" --version && app "$exe64" && grep -E 'cpos-sync: (ntsync|wineserver)' "$XDG_CONFIG_HOME/smoke/wine.log"; }
check_socket() {
    local pid found='' i
    app "$exe64" hold & pid=$!
    for ((i=0; i<60; i++)); do
        found=$(find "$XDG_RUNTIME_DIR" -type s -path '*/cpos-wine/server-*/socket' -print -quit)
        [[ -z $found ]] || break
        sleep 0.2
    done
    wait "$pid" || return
    [[ -n $found && ! -S $found ]] || return 1
    printf 'Ephemeral socket: %s\n' "$found"
}
check_msi() {
    timeout 100 "$runner" --app 'example-msi' -- "$msi" /qn /norestart &&
        find "$XDG_CONFIG_HOME/example-msi/wine/drive_c" -path '*/CPOSProbe/probe32.exe' | grep 'probe32.exe' &&
        test -f "$XDG_CONFIG_HOME/example-msi/wine/.cpos-migrations/0001.reg"
}
check_paths() {
    local target path unix dos
    app "$exe64" path || return
    target="$XDG_DATA_HOME/smoke/windows-installer"
    path="$target/cpos-probe.txt"
    [[ -f $path ]] || return 1
    export WINEPREFIX="$XDG_CONFIG_HOME/smoke/wine" CPOS_SESSION=1 CPOS_WINDOWS_INSTALLER="$target"
    unix=$("$wine" winepath.exe -u 'C:\Windows\Installer\cpos-probe.txt') || return
    dos=$("$wine" winepath.exe -w "$path") || return
    printf 'Round trip: %s <-> %s\n' "$unix" "$dos"
    [[ $unix == "$path" && ${dos,,} == 'c:\windows\installer\cpos-probe.txt' ]]
}
check_wow64() { app "$exe64" write && app "$exe32" write && app "$exe64" read && app "$exe32" read; }
check_known_folders() {
    app "$exe64" 'known-folders' &&
        [[ -f $HOME/My\ Documents/cpos-known-folder.txt && -f $HOME/My\ Desktop/cpos-known-folder.txt &&
           -f $HOME/My\ Downloads/cpos-known-folder.txt ]]
}
check_dry() {
    timeout 10 "$runner" --dry-run --app dry -- "$exe64" &&
        [[ ! -e $XDG_CONFIG_HOME/dry && ! -e $XDG_DATA_HOME/dry ]]
}
failed=0
report() { printf 'FAIL %s\n' "$1"; failed=1; }
if (check_dry); then printf 'ok dry\n'; else report dry; fi
if (check_start); then printf 'ok start\n'; else report start; fi
if (check_socket); then printf 'ok socket\n'; else report socket; fi
if (check_msi); then printf 'ok msi\n'; else report msi; fi
if (check_paths); then printf 'ok paths\n'; else report paths; fi
if (check_known_folders); then printf 'ok known-folders\n'; else report known-folders; fi
if (check_wow64); then printf 'ok wow64\n'; else report wow64; fi
exit "$failed"
