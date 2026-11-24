<!-- SPDX-License-Identifier: MIT -->
# Disposable QEMU ioctl functional tests

These tests exercise only the documented successful event, semaphore, mutex,
and ABI query behavior. They do not probe malformed inputs, race conditions,
or unsupported operations.

The runner accepts a kernel, a module built for that exact guest kernel, and a
static userspace test executable. It packages those artifacts into a temporary
initramfs. The only `insmod` call is in `init`, PID 1 inside the guest; the host
runner never invokes module-management commands. QEMU uses TCG, no network
device, no host disk, and no host filesystem passthrough. Use a disposable guest
kernel configured for initramfs, devtmpfs, modules, and the module's required
features. The static test executable and guest `insmod`, `rmmod`, and `poweroff`
utilities must be compatible with that guest architecture.

Run `run-qemu.sh <guest-kernel> <matching-cpos.ko> <static-test-binary>
<static-busybox> <static-message-test>`. The static BusyBox must include `sh`, `mount`, `mknod`,
`insmod`, `rmmod`, and `poweroff` applets. Artifacts are inputs only; this script does not compile, install, or load a
module on the host. A guest setup, insertion, or assertion failure is reported
as `GUEST TEST FAILURE`; successful assertions print `GUEST TEST PASS`.
The generated initramfs remains in the printed temporary workspace for review.
