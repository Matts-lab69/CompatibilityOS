# Gentoo Wine build plan — documentation only

**VERIFIED FACT.** Gentoo's [`wine.eclass` at `9d05103`:43–75,348–355](https://github.com/gentoo/gentoo/blob/9d05103/eclass/wine.eclass#L43-L75) separates Linux ABI selection from cross-compiling PE targets with new WoW64 and emits `--enable-archs` in the new mode ([lines 348–355](https://github.com/gentoo/gentoo/blob/9d05103/eclass/wine.eclass#L348-L355)). The [Wine 11.20 ebuild](https://github.com/gentoo/gentoo/blob/9d05103/app-emulation/wine-vanilla/wine-vanilla-11.20.ebuild#L69-L90) lists X11, PulseAudio, Vulkan and FreeType dependencies subject to feature flags; [lines 124–136](https://github.com/gentoo/gentoo/blob/9d05103/app-emulation/wine-vanilla/wine-vanilla-11.20.ebuild#L124-L136) list Linux headers, Bison, Flex, pkg-config and NASM. Build headers do not enable a feature in the running kernel.

**HYPOTHESIS — planned sequence.**

1. Pin Wine 11.20 (`7298a818`) and build vanilla first in isolated CI. Use a native amd64 toolchain and MinGW-w64 for x86-64 and i686 PE targets; configure new WoW64 with `--enable-archs=x86_64,i386`.
2. Stage artifacts separately from the system. Verify PE32 and PE32+ artifacts and the absence of a 32-bit ELF Wine dependency in this build. Keep classic WoW64 with host multilib as a contingency, not a requirement. Validate Vulkan and audio dependencies on supported targets separately.
3. In a memory-constrained build environment, I start with at most `-j2` and raise it only after measuring peak memory, swap pressure and headroom. I add candidate patches in small groups after the vanilla baseline.

**NOT ANALYZED.** Build success, peak memory, binary portability and runtime compatibility. No compilation or tests on the user's host; no change to its kernel, configuration or modules.
