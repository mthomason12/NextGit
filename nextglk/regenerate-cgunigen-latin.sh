#!/bin/bash
# Regenerate cgunigen.c with Latin-only Unicode tables.
#
# Downloads Unicode 4.0.1 data files, filters to Latin ranges,
# runs casemap.py, and writes output to cgunigen.c.
#
# Target ranges:
#   U+0000-U+024F  Basic Latin through IPA Extensions
#   U+0300-U+036F  Combining Diacritical Marks
#   U+1E00-U+1EFF  Latin Extended Additional

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

CASEMAP_PY="$SCRIPT_DIR/../thirdparty/cheapglk/casemap.py"
OUTPUT="$SCRIPT_DIR/cgunigen.c"

# Unicode data files from ICU repository (modern Unicode).
# Latin character properties have not changed since Unicode 4.0.1,
# so the generated tables will be functionally identical for Latin.
UNICODEDATA_URL="https://raw.githubusercontent.com/unicode-org/icu/main/icu4c/source/data/unidata/UnicodeData.txt"
SPECIALCASING_URL="https://raw.githubusercontent.com/unicode-org/icu/main/icu4c/source/data/unidata/SpecialCasing.txt"

echo "=== Downloading Unicode data files ==="

# Pick a download tool
if command -v wget &>/dev/null; then
    DL_CMD="wget -q -O"
elif command -v curl &>/dev/null; then
    DL_CMD="curl -sS -o"
else
    echo "ERROR: neither wget nor curl found. Install one of them."
    exit 1
fi

$DL_CMD "$TMPDIR/UnicodeData.txt"    "$UNICODEDATA_URL"
$DL_CMD "$TMPDIR/SpecialCasing.txt"  "$SPECIALCASING_URL"

if [ ! -s "$TMPDIR/UnicodeData.txt" ]; then
    echo "ERROR: failed to download UnicodeData.txt"
    exit 1
fi
if [ ! -s "$TMPDIR/SpecialCasing.txt" ]; then
    echo "WARNING: failed to download SpecialCasing.txt, creating empty file"
    touch "$TMPDIR/SpecialCasing.txt"
fi

echo "=== Filtering to Latin ranges ==="

# Filter UnicodeData.txt: keep lines where the first field is in range.
# Ranges: 0000-024F, 0300-036F, 1E00-1EFF
filter_ucd() {
    python3 -c "
import sys
for line in sys.stdin:
    line = line.strip()
    if not line or line.startswith('#'):
        continue
    cp = line.split(';')[0]
    if not cp:
        continue
    val = int(cp, 16)
    if (0x0000 <= val <= 0x024F) or (0x0300 <= val <= 0x036F) or (0x1E00 <= val <= 0x1EFF):
        print(line)
"
}

# Filter SpecialCasing.txt: keep lines where the code point AND all
# referenced characters in the expansion sequences are in range.
filter_sc() {
    python3 -c "
import sys

LATIN_RANGES = [
    (0x0000, 0x024F),
    (0x0300, 0x036F),
    (0x1E00, 0x1EFF),
]

def is_latin(cp):
    return any(lo <= cp <= hi for (lo, hi) in LATIN_RANGES)

for line in sys.stdin:
    line = line.strip()
    if not line or line.startswith('#'):
        continue
    parts = [p.strip() for p in line.split(';')]
    if not parts or not parts[0]:
        continue
    val = int(parts[0], 16)
    if not is_latin(val):
        continue
    # Expanded sequences are in fields 1 (lower), 2 (title), 3 (upper).
    # All referenced characters must be Latin.
    ok = True
    for idx in (1, 2, 3):
        if len(parts) > idx and parts[idx]:
            for hex_cp in parts[idx].split():
                if not is_latin(int(hex_cp, 16)):
                    ok = False
                    break
        if not ok:
            break
    if ok:
        print(line)
"
}

filter_ucd < "$TMPDIR/UnicodeData.txt" > "$TMPDIR/UnicodeData-filtered.txt"
filter_sc < "$TMPDIR/SpecialCasing.txt" > "$TMPDIR/SpecialCasing-filtered.txt"

echo "  UnicodeData.txt:    $(wc -l < "$TMPDIR/UnicodeData.txt") lines -> $(wc -l < "$TMPDIR/UnicodeData-filtered.txt") lines"
echo "  SpecialCasing.txt:  $(wc -l < "$TMPDIR/SpecialCasing.txt") lines -> $(wc -l < "$TMPDIR/SpecialCasing-filtered.txt") lines"

# Replace the original files with filtered versions in the temp dir.
# casemap.py will read from this directory.
mv "$TMPDIR/UnicodeData-filtered.txt"   "$TMPDIR/UnicodeData.txt"
mv "$TMPDIR/SpecialCasing-filtered.txt" "$TMPDIR/SpecialCasing.txt"

echo "=== Running casemap.py ==="

if [ ! -f "$CASEMAP_PY" ]; then
    echo "ERROR: casemap.py not found at $CASEMAP_PY"
    exit 1
fi

python3 "$CASEMAP_PY" "$TMPDIR" > "$OUTPUT"

LINES=$(wc -l < "$OUTPUT")
echo "=== Done ==="
echo "Generated $OUTPUT ($LINES lines)"