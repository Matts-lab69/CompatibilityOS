#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail

uname -rm
if ! getconf GNU_LIBC_VERSION; then
    printf 'glibc not detected.\n' >&2
    exit 1
fi
if [[ ! -c /dev/ntsync ]] || ! ( : < '/dev/ntsync' ); then
    printf '/dev/ntsync is not an accessible character device.\n' >&2
    exit 1
fi
printf '/dev/ntsync opened read-only; verify effective sync in each build separately.\n'
if ! report=$(LC_ALL=C vulkaninfo --summary); then
    printf 'vulkaninfo --summary failed.\n' >&2
    exit 1
fi
printf '%s\n' "$report"
if ! grep -Eq 'driverID[[:space:]]*=[[:space:]]*DRIVER_ID_MESA_RADV([[:space:]]|$)' <<< "$report"; then
    printf 'No RADV device reported.\n' >&2
    exit 1
fi
