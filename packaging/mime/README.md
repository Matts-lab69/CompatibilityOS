<!-- SPDX-License-Identifier: MIT -->

# MIME integration

The desktop and shared MIME templates dispatch Windows EXE/MSI files to the internal runner. The installer registers them per user. **SIN ANALIZAR:** actual COSMIC MIME resolution and restoration of existing handlers.

**Decision:** keep desktop dispatch per user with `Terminal=false`, and keep one registration owner (`installer/install.sh uninstall`). The static CI job validates the desktop entry and compiles both MIME globs in a disposable database. A missing database tool stops removal before any app association is edited.
