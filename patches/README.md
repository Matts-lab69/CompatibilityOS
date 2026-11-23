# Wine patch queue

Rebaseable patch series, not a divergent Wine fork. Keep a pinned upstream revision, small independently reviewable patches, file-level provenance, and a reason to carry each patch. Prefer submitting fixes upstream. See [Wine audit](../docs/04-auditoria-wine.md).

- [`series`](series) lists patches in application order against the commit in [`scripts/wine.lock`](../scripts/wine.lock). Use one relative path per line, with a globally increasing four-digit prefix. Blank lines and lines starting with `#` are ignored.
- [Paths](paths/README.md), [registry](registry/README.md), and [upstream candidates](upstream-candidates/README.md).
