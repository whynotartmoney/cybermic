#!/usr/bin/env python3
"""Check Cyber Mick fonts cover the on-screen copy, and reject a missing glyph."""

from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COPY = ROOT / "main" / "mick_copy.h"
UI_SOURCES = (ROOT / "main" / "mick_ui.c", ROOT / "main" / "main.c")
FONT_16 = ROOT / "assets" / "fonts" / "mick_latin_16.c"
FONT_28 = ROOT / "assets" / "fonts" / "mick_latin_28.c"
MISSING = 0x9F98


def string_literals(text: str) -> list[str]:
    return re.findall(r'"((?:\\.|[^"\\])*)"', text)


def defines(text: str) -> list[tuple[str, str]]:
    return re.findall(r'#define\s+(\w+)\s+"((?:\\.|[^"\\])*)"', text)


def copy_defines() -> list[tuple[str, str]]:
    return defines(COPY.read_text(encoding="utf-8"))


def cmap_codepoints(text: str) -> set[int]:
    codes: set[int] = set()
    blocks = re.findall(
        r"\{[^{}]*?\.range_start = (\d+), \.range_length = (\d+),"
        r".*?\.unicode_list = (NULL|unicode_list_\d+),"
        r".*?\.list_length = (\d+), \.type = (LV_FONT_FMT_TXT_CMAP_\w+)",
        text,
        flags=re.S,
    )
    if not blocks:
        raise AssertionError("font cmap not found")
    for start_s, length_s, unicode_list, list_len_s, kind in blocks:
        start = int(start_s)
        length = int(length_s)
        list_len = int(list_len_s)
        if kind == "LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY":
            if unicode_list != "NULL" or list_len != 0:
                raise AssertionError(f"unexpected tiny cmap {start:#x}")
            codes.update(range(start, start + length))
            continue
        if "SPARSE" not in kind:
            raise AssertionError(f"unsupported cmap {kind}")
        array = re.search(
            rf"static const uint16_t {unicode_list}\[\] = \{{([^}}]+)\}}",
            text,
        )
        if array is None:
            raise AssertionError(f"missing {unicode_list}")
        offsets = [int(item, 16) for item in re.findall(r"0x[0-9a-fA-F]+", array.group(1))]
        if len(offsets) != list_len:
            raise AssertionError(f"{unicode_list} length {len(offsets)} != {list_len}")
        codes.update(start + offset for offset in offsets)
    return codes


class MickFontCoverageTest(unittest.TestCase):
    def test_ui_sources_keep_non_ascii_in_the_copy_header(self) -> None:
        for path in UI_SOURCES:
            for literal in string_literals(path.read_text(encoding="utf-8")):
                extra = [ch for ch in literal if ord(ch) > 127]
                self.assertEqual(extra, [], f"{path.name} has an untracked string {literal!r}")

    def test_copy_is_ascii_and_covered_by_latin_16(self) -> None:
        covered = cmap_codepoints(FONT_16.read_text(encoding="utf-8"))
        required = set(range(0x20, 0x7F))
        required.discard(0x5E)
        missing = sorted(cp for cp in required if cp not in covered)
        self.assertEqual(missing, [], " ".join(f"U+{cp:04X}" for cp in missing))
        self.assertNotIn(0x5E, covered)
        self.assertNotIn(MISSING, covered)
        for _name, literal in copy_defines():
            codes = {ord(ch) for ch in literal}
            self.assertTrue(all(cp < 128 for cp in codes), literal)
            self.assertNotIn(0x5E, codes)
            self.assertTrue(codes.issubset(covered), literal)

    def test_orbitron_28_covers_the_wordmark(self) -> None:
        covered = cmap_codepoints(FONT_28.read_text(encoding="utf-8"))
        needed = {ord(ch) for ch in "CYBER"}
        self.assertTrue(needed.issubset(covered))
        self.assertNotIn(ord("A"), covered)
        self.assertNotIn(ord("M"), covered)
        self.assertNotIn(MISSING, covered)


if __name__ == "__main__":
    unittest.main()
