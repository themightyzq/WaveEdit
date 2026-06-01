#!/usr/bin/env python3
"""
check_rendered_ascii.py — guard against non-ASCII glyphs in rendered strings.

CLAUDE.md §6.12 requires that every string DRAWN to the user is ASCII-only.
Non-ASCII glyphs (•, →, ✓, ✗, ▲, ▼, …, —, emoji) render as missing-glyph
boxes in some typefaces and in remote / streamed sessions.

This catches BOTH forms that bit us before:
  1. literal UTF-8 bytes in a string literal      ("Working…")
  2. byte-escape literals                          ("Working\xe2\x80\xa6")
The byte-escape form is the dangerous one — a plain "non-ASCII byte" grep
cannot see it, which is exactly how earlier sweeps missed the menu glyphs.

Exempt (per §6.12): comments, and DBG(...) / juce::Logger lines (never drawn).
Also exempt: generated data files (*_generated.*), which carry match-input
data, not display text, and are regenerated from source assets.

Exit code 0 = clean, 1 = offending lines found (prints them).
Run from the repo root:  python3 scripts/check_rendered_ascii.py
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "Source"

# A line is a pure comment (never drawn).
COMMENT_LINE = re.compile(r"^\s*(//|\*|/\*)")
# Start of a debug/log call whose argument is never drawn. May span lines.
LOG_START = re.compile(r"\bDBG\s*\(|\bwriteToLog\b|\bLogger\b")
# UTF-8 lead-byte escape: \xc2..\xf4 (start of a multi-byte sequence).
ESCAPE_GLYPH = re.compile(r'\\x[c-f][0-9a-fA-F]')
# Any raw non-ASCII byte.
RAW_NON_ASCII = re.compile(r'[^\x00-\x7f]')


def has_glyph(line: str) -> bool:
    if '"' not in line:  # only string literals can be drawn text
        return False
    return bool(RAW_NON_ASCII.search(line) or ESCAPE_GLYPH.search(line))


def scan(text: str):
    """Yield (line_no, line) for offending lines, skipping comments and the
    full extent of multi-line DBG(...) / Logger calls (tracked by paren depth)."""
    in_log = False
    depth = 0
    for n, line in enumerate(text.splitlines(), 1):
        if in_log:
            depth += line.count("(") - line.count(")")
            if depth <= 0:
                in_log = False
            continue  # continuation of a log call — exempt
        if COMMENT_LINE.search(line):
            continue
        m = LOG_START.search(line)
        if m:
            depth = line[m.start():].count("(") - line[m.start():].count(")")
            in_log = depth > 0
            continue  # the log line itself — exempt
        if has_glyph(line):
            yield n, line


def main() -> int:
    offenders = []
    for path in sorted(SRC.rglob("*")):
        if path.suffix not in (".cpp", ".h", ".mm"):
            continue
        if "_generated" in path.name:
            continue
        try:
            text = path.read_text(encoding="utf-8", errors="surrogateescape")
        except OSError:
            continue
        for n, line in scan(text):
            offenders.append(f"{path.relative_to(ROOT)}:{n}: {line.strip()}")

    if offenders:
        print("Non-ASCII glyphs found in rendered strings (CLAUDE.md §6.12):\n")
        print("\n".join(offenders))
        print(f"\n{len(offenders)} offending line(s). "
              "Use ASCII (-> ... -- etc.); convey status with colour/words, not glyphs.")
        return 1

    print("OK: no non-ASCII glyphs in rendered strings.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
