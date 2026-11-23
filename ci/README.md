# CI design

[`wine.yml`](../.github/workflows/wine.yml) builds the pinned Wine source and ordered series with new WoW64 at `-j2`, runs tests under Xvfb, and uploads a seven-day artifact only after testing and attestation. There is no persistent build cache or release step. The bundle includes upstream source, our patch series, and license notices.

**HECHO VERIFICADO:** [run 45113410407](https://github.com/Matts-lab69/CompatibilityOS/actions/runs/45113410407) built, tested and attested the bundle. See [test coverage](../tests/README.md) for limits.
