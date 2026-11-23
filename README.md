# CompatibilityOS

I'm building CompatibilityOS so Linux users can double-click a Windows `.exe` or `.msi` to run or install it without invoking `wine`, managing prefixes, or using a visible launcher. I use Wine for Phase 1; I'm not building a new Windows API implementation or a microkernel.

**Status: documentation only.** I haven't published binaries, CI workflows, installers, patches, registrations, or runnable tests. I'm targeting Gentoo/OpenRC with COSMIC first and plan to offer installation from GitHub. I'm aiming for under three months for Phase 1, but won't commit to a date without measurements.

I license my standalone project code under [MIT](LICENSE) and my patches to Wine under `LGPL-2.1-or-later`.

- [Design, audit, and measurement documents](docs/README.md)
- [Patch series](patches/README.md), [internal runner](runner/README.md), and [application recipes](recipes/README.md)
- [Desktop and terminal integration](packaging/README.md), [installer](installer/README.md), and [CI](ci/README.md)
- [Future test coverage](tests/README.md) and [licensing status](docs/06-licencias.md)
