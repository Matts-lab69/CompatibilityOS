#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
temp=$1
root=$(pwd -P)
export HOME="$temp/test-home" XDG_RUNTIME_DIR="$temp/test-runtime" CPOS_BUNDLE="$temp/bundle"
export XDG_CONFIG_HOME="$HOME/.config" XDG_DATA_HOME="$HOME/.local/share" XDG_STATE_HOME="$HOME/.local/state"
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
x86_64-w64-mingw32-gcc "$root/tests/windows-subsystems.c" -lgdi32 -lsetupapi -lwinspool -o 'subsystems.exe'
cp -- "$root/tests/probe.wxs" 'probe.wxs'
wixl -o 'probe.msi' 'probe.wxs'
runner="$CPOS_BUNDLE/bin/cpos-run"
wine="$CPOS_BUNDLE/wine/bin/wine"
exe64="$PWD/probe64.exe"
exe32="$PWD/probe32.exe"
msi="$PWD/probe.msi"
app() { timeout 90 "$runner" --app 'smoke' -- "$@"; }
check_start() { "$wine" --version && app "$exe64" && grep -E 'cpos-sync: (ntsync|wineserver)' "$XDG_STATE_HOME/cpos/smoke/wine.log"; }
check_desktop_log() {
    local output log="$XDG_STATE_HOME/cpos/desktop-log/wine.log"
    if ! output=$(timeout 90 "$runner" --desktop --app 'desktop-log' -- "$exe64" 2>&1); then
        printf 'Desktop launch failed: %s\n' "$output" >&2
        if [[ -f $log ]]; then tail -n 30 -- "$log" >&2; fi
        return 1
    fi
    if [[ -n $output ]]; then
        printf 'Unexpected desktop console output: %s\n' "$output" >&2
        return 1
    fi
    # A Windows text-mode stdout redirected to a file can emit CRLF.
    if ! grep -Ex $'PE64 started\r?' "$log"; then
        printf 'Missing Windows startup marker in the desktop log\n' >&2
        if [[ -f $log ]]; then tail -n 30 -- "$log" >&2; fi
        return 1
    fi
}
check_desktop_notification() {
    local stub="$temp/notification-stub" marker="$temp/notification-called" status=0
    mkdir -p -- "$stub"
    cat > "$stub/zenity" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' "$*" > "$CPOS_NOTIFICATION_MARKER"
EOF
    chmod +x -- "$stub/zenity"
    export CPOS_NOTIFICATION_MARKER="$marker"
    PATH="$stub:$PATH" timeout 90 "$runner" --desktop --app 'desktop-error' -- "$exe64" read || status=$?
    [[ $status == 4 && -f $marker ]] || return 1
    grep -Fq 'CompatibilityOS could not run the application.' "$marker" &&
        [[ -s $XDG_STATE_HOME/cpos/desktop-error/wine.log ]]
}
check_sync_fallback() {
    CPOS_SYNC=wineserver timeout 90 "$runner" --app 'sync-fallback' -- "$exe64" &&
        grep -Fx 'cpos-sync: wineserver' "$XDG_STATE_HOME/cpos/sync-fallback/wine.log" &&
        ! grep -Fxq 'cpos-sync: ntsync' "$XDG_STATE_HOME/cpos/sync-fallback/wine.log"
}
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
check_windows_writes() {
    local directory
    app "$exe64" 'writable-windows' || return
    for directory in 'temp' 'spool' 'fonts' 'assembly' 'dotnet-assembly' 'winsxs'; do
        [[ -f $XDG_DATA_HOME/smoke/windows-$directory/cpos-probe.txt ]] || return 1
    done
}
check_dry() {
    timeout 10 "$runner" --dry-run --app dry -- "$exe64" &&
        [[ ! -e $XDG_CONFIG_HOME/dry && ! -e $XDG_DATA_HOME/dry ]]
}
failed=0
report() { printf 'FAIL %s\n' "$1"; failed=1; }
if (check_dry); then printf 'ok dry\n'; else report dry; fi
if (check_start); then printf 'ok start\n'; else report start; fi
if (check_desktop_log); then printf 'ok desktop-log\n'; else report desktop-log; fi
if (check_desktop_notification); then printf 'ok desktop-notification\n'; else report desktop-notification; fi
if (check_sync_fallback); then printf 'ok sync-fallback\n'; else report sync-fallback; fi
if (check_socket); then printf 'ok socket\n'; else report socket; fi
if (check_msi); then printf 'ok msi\n'; else report msi; fi
if (check_paths); then printf 'ok paths\n'; else report paths; fi
if (check_known_folders); then printf 'ok known-folders\n'; else report known-folders; fi
if (check_windows_writes); then printf 'ok windows-writes\n'; else report windows-writes; fi
if app "$exe64" 'services'; then printf 'ok services-config\n'; else report services-config; fi
fusion_dll="$CPOS_BUNDLE/wine/lib/wine/x86_64-windows/fusion.dll"
font_file="$PWD/cpos-font-fixture.ttf"
/usr/bin/python3 "$root/tests/make-font-fixture.py" "$font_file"
font_config="$PWD/private-fonts.conf"
mkdir -- 'empty-fonts' 'fontconfig-cache'
cat > "$font_config" <<'EOF'
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">
<fontconfig>
  <dir prefix="relative">empty-fonts</dir>
  <cachedir prefix="relative">fontconfig-cache</cachedir>
</fontconfig>
EOF
if [[ ! -f $fusion_dll ]]; then
    printf 'Packaged Fusion DLL missing: %s\n' "$fusion_dll" >&2
    report subsystem-probes
elif [[ ! -f $font_file ]]; then
    printf 'Missing CI font: %s\n' "$font_file" >&2
    report subsystem-probes
elif FONTCONFIG_FILE="$font_config" CPOS_TEST_FUSION_DLL="\\\\?\\unix${fusion_dll//\//\\}" \
    CPOS_TEST_FONT_FILE="\\\\?\\unix${font_file//\//\\}" \
    timeout 90 "$runner" --app 'subsystem-probes' -- "$PWD/subsystems.exe"; then
    printf 'ok subsystem-probes\n'
else
    report subsystem-probes
fi
if (check_wow64); then printf 'ok wow64\n'; else report wow64; fi
check_gac() {
    local fixture="$PWD/CompatibilityOS.GacFixture.dll" token identity
    sn -k 'gac-fixture.snk' || return
    mcs -nologo -target:library -keyfile:'gac-fixture.snk' -out:"$fixture" "$root/tests/gac-fixture.cs" || return
    token=$(LC_ALL=C sn -T "$fixture" | sed -nE 's/^Public Key Token: ([0-9a-fA-F]{16})$/\1/p') || return
    [[ $token =~ ^[0-9a-fA-F]{16}$ ]] || return 1
    identity="CompatibilityOS.GacFixture, Version=1.0.0.0, Culture=neutral, PublicKeyToken=$token"
    x86_64-w64-mingw32-gcc -municode "$root/tests/gac-probe.c" -o 'gac-probe.exe' || return
    timeout 90 "$runner" --app 'gac-fixture' -- "$PWD/gac-probe.exe" \
        "\\\\?\\unix${fusion_dll//\//\\}" "\\\\?\\unix${fixture//\//\\}" "$identity"
}
if (check_gac); then printf 'ok gac-fixture\n'; else report gac-fixture; fi
exit "$failed"
