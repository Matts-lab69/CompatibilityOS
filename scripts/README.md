<!-- SPDX-License-Identifier: MIT -->
# Source preparation

`wine.lock` is the single machine-readable Wine commit pin. With Git supporting `clone --revision` (documented in Git 2.49), run `bash "scripts/fetch-wine.sh" "$temporary_parent/wine"` from the repository root, using an existing temporary parent outside the project and a new destination. The script fetches only the pinned revision at depth one, compares the full commit ID, checks Git objects and populates the detached checkout. This verifies source identity against the pin, not a release signature. The caller owns cleanup, including partial clones after errors. The script does not build or run Wine.
