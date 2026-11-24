# SPDX-License-Identifier: MIT
from pathlib import Path
import sys

corpus = Path(sys.argv[1])
image = (corpus / "minimal.pe").read_bytes()
pe = int.from_bytes(image[0x3C:0x40], "little")
optional = pe + 24
(corpus / "truncated-dos.pe").write_bytes(image[:32])
for name, offset, size in (
    ("bad-pe-offset", 0x3C, 4),
    ("bad-section-count", pe + 6, 2),
    ("bad-image-size", optional + 56, 4),
    ("bad-entry-point", optional + 16, 4),
):
    (corpus / f"{name}.pe").write_bytes(
        image[:offset] + b"\xff" * size + image[offset + size:]
    )
