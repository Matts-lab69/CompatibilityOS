<!-- SPDX-License-Identifier: MIT -->

# Application recipes

Each directory is selected by the runner's application ID. `recipe.conf` consists of an SPDX comment, `format=1`, `kind=exe` or `kind=msi` (descriptive), then `migration=NNNN.reg` entries starting at `0001` without gaps. Files use LF and the Registry Editor 5.00 header. The parser reads data, not shell code. The EXE and MSI examples only write a project-owned registry key.

Append migrations; never rewrite or remove published entries. Imports must be idempotent in case Wine or the host stops before the checksum is recorded. The static check validates structure, not registry semantics. Do not redistribute proprietary files.
