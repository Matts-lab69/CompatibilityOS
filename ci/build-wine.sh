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
uapi="$temp/linux-uapi"
mkdir -p -- "$uapi/linux"
curl --fail --silent --show-error --location --retry 3 \
    'https://raw.githubusercontent.com/torvalds/linux/7d0a66e4bb9081d75c82ec4957c50034cb0ea449/include/uapi/linux/ntsync.h' \
    -o "$uapi/linux/ntsync.h"
(cd -- "$uapi"; printf '%s\n' '006437ee52a3e04f921df77081eb5c21c44c71f598b10ac534c6ef9e78296262  linux/ntsync.h' | sha256sum --check --strict -)
export CCACHE_DISABLE=1
cd -- "$build"
CPPFLAGS="-I$uapi" "$src/configure" --prefix=/wine --enable-archs=x86_64,i386 --without-wayland --disable-tests
grep -Fx '#define HAVE_LINUX_NTSYNC_H 1' 'include/config.h'
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
cp -R -- "$uapi" "$bundle/source/"
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
