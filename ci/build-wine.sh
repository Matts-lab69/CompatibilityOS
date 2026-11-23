#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
temp=$1
root=$(pwd -P)
src="$temp/wine-src"
build="$temp/wine-build"
bundle="$temp/bundle"
out="$temp/out"
bash 'scripts/fetch-wine.sh' "$src"
bash 'scripts/apply-series.sh' "$src"
mkdir -p -- "$build" "$bundle" "$out"
export CCACHE_DISABLE=1
cd -- "$build"
"$src/configure" --prefix=/wine --enable-archs=x86_64,i386 --without-wayland --disable-tests
make -j2
make -j2 install DESTDIR="$bundle"
mkdir -p -- "$bundle/bin" "$bundle/libexec" "$bundle/share" "$bundle/source" "$temp/base-home"
install -m 755 -- "$root/runner/cpos-run" "$bundle/bin/cpos-run"
install -m 755 -- "$root/scripts/migrate-recipes" "$bundle/libexec/migrate-recipes"
install -m 644 -- "$root/scripts/recipe-schema.sh" "$bundle/libexec/recipe-schema.sh"
cp -R -- "$root/recipes" "$bundle/recipes"
cp -- "$root/packaging/mime/compatibilityos.desktop" "$root/packaging/mime/compatibilityos.xml" "$bundle/share/"
cp -- "$root/LICENSE" "$src/COPYING.LIB" "$bundle/share/"
git -C "$src" archive --format=tar.gz --prefix=wine/ -o "$bundle/source/wine.tar.gz" HEAD
cp -R -- "$root/patches" "$root/ci" "$root/scripts" "$bundle/source/"
export HOME="$temp/base-home" WINEPREFIX="$temp/seed" WINESERVER="$bundle/wine/bin/wineserver"
export WINEDLLOVERRIDES='mscoree,mshtml=' CPOS_QUIET_UPDATE=1
xvfb-run -a timeout 120 "$bundle/wine/bin/wine" wineboot.exe --init
timeout 60 "$WINESERVER" -w
mkdir -- "$bundle/base"
cp -- "$WINEPREFIX/system.reg" "$WINEPREFIX/userdef.reg" "$WINEPREFIX/user.reg" "$bundle/base/"
git -C "$src" rev-parse HEAD > "$bundle/base/version"
chmod a-w -- "$bundle/base/"*
tar -czf "$out/compatibilityos.tar.gz" -C "$bundle" .
cd -- "$out"
sha256sum 'compatibilityos.tar.gz' > 'SHA256SUMS'
