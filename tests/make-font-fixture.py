# SPDX-License-Identifier: MIT
"""Generate a project-owned TrueType font for the isolated Wine smoke test."""

import sys

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen


def glyph(points):
    pen = TTGlyphPen(None)
    if points:
        pen.moveTo(points[0])
        for point in points[1:]:
            pen.lineTo(point)
        pen.closePath()
    return pen.glyph()


font = FontBuilder(1000, isTTF=True)
font.setupGlyphOrder([".notdef", "A"])
font.setupCharacterMap({65: "A"})
font.setupGlyf({".notdef": glyph([]), "A": glyph([(50, 0), (300, 700), (550, 0)])})
font.setupHorizontalMetrics({".notdef": (600, 0), "A": (600, 50)})
font.setupHorizontalHeader(ascent=800, descent=-200)
font.setupNameTable({
    "familyName": "CPOS Fixture Sans",
    "styleName": "Regular",
    "uniqueFontIdentifier": "CPOS Fixture Sans Regular 1",
    "fullName": "CPOS Fixture Sans Regular",
    "psName": "CPOSFixtureSans-Regular",
})
font.setupOS2(sTypoAscender=800, sTypoDescender=-200, usWinAscent=800, usWinDescent=200)
font.setupPost()
font.setupMaxp()
font.save(sys.argv[1])
