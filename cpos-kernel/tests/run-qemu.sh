#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Packages prebuilt guest artifacts; the module is inserted only by guest PID 1.
set -euo pipefail

if [ "$#" -ne 5 ]; then
	echo "Usage: $0 <guest-kernel> <guest-module> <static-ioctl-test> <static-busybox> <static-message-test>" >&2
	exit 2
fi
kernel=$1
module=$2
test_binary=$3
busybox=$4
message_test=$5
for file in "$kernel" "$module" "$test_binary" "$busybox" "$message_test"; do
	[ -f "$file" ] || { echo "Missing input file: $file" >&2; exit 2; }
done
command -v qemu-system-x86_64 >/dev/null || { echo "qemu-system-x86_64 is required" >&2; exit 2; }
command -v cpio >/dev/null || { echo "cpio is required" >&2; exit 2; }

work=$(mktemp -d "${TMPDIR:-/tmp}/cpos-qemu.XXXXXX")
echo "Guest initramfs workspace: $work"
mkdir -p "$work/root/bin" "$work/root/sbin" "$work/root/dev" "$work/root/proc" "$work/root/sys"
cp "$(dirname "$0")/init" "$work/root/init"
chmod 0755 -- "$work/root/init"
cp "$module" "$work/root/cpos.ko"
cp "$test_binary" "$work/root/ioctl_functional"
cp "$message_test" "$work/root/message_test"
cp "$busybox" "$work/root/bin/busybox"
for utility in sh mount mknod insmod rmmod poweroff; do
	ln -s /bin/busybox "$work/root/bin/$utility"
done
for utility in insmod rmmod poweroff; do
	ln -s /bin/busybox "$work/root/sbin/$utility"
done
(
	cd "$work/root"
	find . -print0 | cpio --null -o --format=newc > "$work/initramfs.cpio"
)

set +e
qemu-system-x86_64 \
	-accel tcg,thread=single -machine q35 -m 512M -smp 1 \
	-kernel "$kernel" -initrd "$work/initramfs.cpio" \
	-append 'console=ttyS0 rdinit=/init panic=-1' \
	-display none -monitor none -no-reboot -nic none -nodefaults -serial stdio \
	>"$work/qemu.log" 2>&1
status=$?
set -e
cat "$work/qemu.log"
[ "$status" -eq 0 ] || { echo "QEMU exited with status $status" >&2; exit "$status"; }
grep -Eq $'^GUEST TEST PASS\r?$' "$work/qemu.log" || {
	echo "Guest did not report a successful test run" >&2
	exit 1
}
