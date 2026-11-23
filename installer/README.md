# Installer

Use `bash "installer/install.sh" [--dry-run] install RUN_ID` or `bash "installer/install.sh" [--dry-run] uninstall`. Installation requires `gh` with artifact attestation support, Bash, GNU coreutils, tar, flock, xdg-mime, update-desktop-database and update-mime-database. Runtime also requires the system libraries for the Wine bundle and a private `XDG_RUNTIME_DIR`.

The installer writes per-user files only. It verifies both attested subjects against the signing workflow and commit, then checks the bundle's SHA-256. Uninstall preserves app data, restores the original MIME defaults file if the installer-written copy is still unchanged, and does not overwrite subsequent user edits. **SIN ANALIZAR:** end-to-end installation, restoration and portability outside the CI runner.
