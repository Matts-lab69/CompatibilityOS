<!-- SPDX-License-Identifier: MIT -->
# External references

- **HECHO VERIFICADO — deepin-wine-ubuntu:** the [pinned packaging README](https://github.com/wszqkzqk/deepin-wine-ubuntu/blob/b9c0a582ddc8e35dd469d22b4cd8bd35da8a6c79/README.md#L19) declares GPL (version unspecified there) for its author's non-binary files and disclaims rights over packaged binaries. This is not the official deepin-wine source. **HIPÓTESIS:** use the separation between applications and packaging as a design reference. **SIN ANALIZAR:** upstream deepin-wine and binary provenance; do not import those binaries or packages.
- **HECHO VERIFICADO — umu-protonfixes:** the [pinned license](https://github.com/Open-Wine-Components/umu-protonfixes/blob/4e4f13bc11cee73d0a340a5893a71d6c24f82afe/LICENSE) is BSD-2-Clause. **HIPÓTESIS:** use documented app-specific fixes as leads and consider individual fixes after reviewing file notices and reproducing their behavior. Do not adopt the launcher or bundle downloaded runtimes.
- **HECHO VERIFICADO — Bottles:** the [pinned copying text](https://github.com/bottlesdevs/Bottles/blob/1b16c736a8adb9c0e7827f82a8290da15407cd9e/COPYING.md) contains GPLv3. **HIPÓTESIS:** compare dependency and environment management as design references. Do not copy the implementation into MIT code or the deferred GPL-2.0-only module.

**SIN ANALIZAR:** file-by-file reuse, dependencies and redistribution terms. No material from these projects is imported here.
