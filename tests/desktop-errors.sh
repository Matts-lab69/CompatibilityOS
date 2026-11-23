#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
export LC_ALL=C

root=$(cd -- "${BASH_SOURCE[0]%/*}/.." && pwd -P)
runner="$root/runner/cpos-run"
tmp=$(mktemp -d)
trap 'chmod -R u+rwX -- "$tmp"; rm -rf -- "$tmp"' EXIT

fail() {
    printf 'desktop error test: %s\n' "$1" >&2
    exit 1
}

mkdir -p "$tmp/bin" "$tmp/runtime" "$tmp/bundle/wine/bin" "$tmp/bundle/base" "$tmp/home" "$tmp/config" "$tmp/data" "$tmp/state"
chmod 700 "$tmp/runtime"
printf '1.0\n' > "$tmp/bundle/base/version"
for registry in system.reg userdef.reg user.reg; do : > "$tmp/bundle/base/$registry"; done
cat > "$tmp/bundle/wine/bin/wine" <<'EOF'
#!/usr/bin/env bash
case "${1:-}" in wineboot.exe|reg.exe) exit 0 ;; esac
exit 23
EOF
cat > "$tmp/bundle/wine/bin/wineserver" <<'EOF'
#!/usr/bin/env bash
exit 0
EOF
cat > "$tmp/bin/zenity" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' "$*" >> "$NOTICE_LOG"
exit "${ZENITY_STATUS:-0}"
EOF
cat > "$tmp/bin/kdialog" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' "$*" >> "$NOTICE_LOG"
exit "${KDIALOG_STATUS:-0}"
EOF
chmod +x "$tmp/bin/"* "$tmp/bundle/wine/bin/"*
printf 'MZ\n' > "$tmp/sample.exe"

export PATH="$tmp/bin:$PATH" HOME="$tmp/home" XDG_CONFIG_HOME="$tmp/config" XDG_DATA_HOME="$tmp/data" XDG_STATE_HOME="$tmp/state"
export XDG_RUNTIME_DIR="$tmp/runtime" CPOS_BUNDLE="$tmp/bundle" NOTICE_LOG="$tmp/notices"

# Early validation failures notify and retain the original status.
set +e
XDG_RUNTIME_DIR='' "$runner" --desktop --app test -- "$tmp/sample.exe" >/dev/null 2>&1
status=$?
set -e
[[ $status -ne 0 ]] || fail 'early error returned success'
[[ $(wc -l < "$NOTICE_LOG") == 1 ]] || fail 'early error did not notify exactly once'
grep -Fq 'CompatibilityOS' "$NOTICE_LOG" || fail 'notice omitted fixed application label'

# A failed primary notifier falls back to the secondary notifier.
: > "$NOTICE_LOG"
export ZENITY_STATUS=1
set +e
"$runner" --desktop --app test -- "$tmp/sample.exe" >/dev/null 2>&1
status=$?
set -e
[[ $status == 23 ]] || fail "runner status changed: $status"
[[ $(wc -l < "$NOTICE_LOG") == 2 ]] || fail 'kdialog fallback was not attempted'
grep -Fq -- '--error' "$NOTICE_LOG" || fail 'dialog invocation missing error mode'

# Dry runs must not display a dialog, even when validation fails.
: > "$NOTICE_LOG"
set +e
"$runner" --desktop --dry-run --app test -- "$tmp/missing.exe" >/dev/null 2>&1
status=$?
set -e
[[ $status -ne 0 ]] || fail 'invalid dry run returned success'
[[ ! -s $NOTICE_LOG ]] || fail 'dry run caused notification I/O'

printf 'Desktop error behavior verified.\n'
