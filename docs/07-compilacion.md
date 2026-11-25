<!-- SPDX-License-Identifier: MIT -->

# Wine build and test plan

**VERIFIED FACT.** Gentoo's [`wine.eclass` at `9d05103`:43–75,348–355](https://github.com/gentoo/gentoo/blob/9d05103/eclass/wine.eclass#L43-L75) separates Linux ABI selection from cross-compiling PE targets with new WoW64 and emits `--enable-archs` in the new mode ([lines 348–355](https://github.com/gentoo/gentoo/blob/9d05103/eclass/wine.eclass#L348-L355)). The [Wine 11.20 ebuild](https://github.com/gentoo/gentoo/blob/9d05103/app-emulation/wine-vanilla/wine-vanilla-11.20.ebuild#L69-L90) lists X11, PulseAudio, Vulkan and FreeType dependencies subject to feature flags; [lines 124–136](https://github.com/gentoo/gentoo/blob/9d05103/app-emulation/wine-vanilla/wine-vanilla-11.20.ebuild#L124-L136) list Linux headers, Bison, Flex, pkg-config and NASM. Build headers do not enable a feature in the running kernel.

**HYPOTHESIS — planned sequence.**

1. Pin Wine 11.20 (`7298a818`) and build vanilla first in isolated CI. Use a native amd64 toolchain and MinGW-w64 for x86-64 and i686 PE targets; configure new WoW64 with `--enable-archs=x86_64,i386`.
2. Stage artifacts separately from the system. Verify PE32 and PE32+ artifacts and the absence of a 32-bit ELF Wine dependency in this build. Keep classic WoW64 with host multilib as a contingency, not a requirement. Validate Vulkan and audio dependencies on supported targets separately.
3. In a memory-constrained build environment, I start with at most `-j2` and raise it only after measuring peak memory, swap pressure and headroom. I add candidate patches in small groups after the vanilla baseline.

**SIN ANALIZAR.** Peak memory and binary portability beyond the hosted Ubuntu runner. No compilation or tests on the user's host; no change to its kernel, configuration or modules.

**Decision — build-time ntsync.** Previous Ubuntu CI logged `checking for linux/ntsync.h... no`. The build now fetches and checksums only the [pinned Linux 6.18 userspace header](https://github.com/torvalds/linux/blob/7d0a66e4bb9081d75c82ec4957c50034cb0ea449/include/uapi/linux/ntsync.h), preserves its original SPDX notice and includes it with the corresponding source. It fails if Wine does not configure support. Neither this header nor the fallback test demonstrates that the runner's kernel supports ntsync.

**HECHO VERIFICADO — hosted CI.** [Run 45113410407](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45113410407) built on Ubuntu 24.04 with new WoW64 and MinGW-w64 at `-j2` and passed the separate Xvfb job. It logged `Attestation created for 2 subjects` and uploaded `compatibilityos-linux-x86_64`. The previous run reported `wine/cpos_runtime.h: No such file or directory`; commit `3d2914b` registered the header in Wine's build system. See [test coverage](../tests/README.md).

**HECHO VERIFICADO — expanded suite.** [Run 45155264148](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45155264148) rebuilt with ntsync headers, passed the 11 Xvfb checks, and attested the bundle and checksum manifest. This tests fallback to wineserver, not actual ntsync on a host with an accessible device. No release was published.
