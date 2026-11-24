<!-- SPDX-License-Identifier: MIT -->
# EXPERIMENTAL — CompatibilityOS kernel scaffold

CI builds this out-of-tree Linux module against container headers and tests it only inside a disposable QEMU guest. It is not installed or loaded on the runner host and is not part of Wine, the installer, or the normal runner. See the [versioned ABI](ABI.md) and the [experimental design](../docs/12-fase2.md).
