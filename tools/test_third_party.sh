#!/bin/sh
# Run the licensed DMG set on gb-host and record how each one is judged.
#
# Exit status is the host status of a ROM that was expected to pass, or 1
# when dmg-acid2 does not match its reference image. div_write and the
# hblank timing test are recorded as Peanut-GB currently judges them; a
# failure there does not fail this script.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT"

OUT=${1:-build/third-party-host}
mkdir -p "$OUT"
HOST=./build/gb-host

if [ ! -x "$HOST" ]; then
	echo "test_third_party: build gb-host first (tools/test_host.sh)" >&2
	exit 1
fi
./tools/fetch_test_roms.sh

fail=0

host() {
	name=$1
	shift
	log="$OUT/$name.log"
	set +e
	"$HOST" "$@" > "$log" 2>"$OUT/$name.err"
	rc=$?
	set -e
	echo "$rc" > "$OUT/$name.exit"
	echo "$name exit=$rc"
	sed -n '1,20p' "$log"
}

# dmg-acid2 is stable by frame 10. Compare shade indices, not RGB:
# the reference PNG is 2-bit greyscale (0 = black) and the host buffer
# uses Game Boy shade 0 = white.
host acid2 \
	--rom roms/third_party/dmg-acid2/dmg-acid2.gb --frames 10 \
	--png "$OUT/dmg-acid2.png" --shade "$OUT/dmg-acid2.shade"
python3 - "$OUT" roms/third_party/dmg-acid2/reference-dmg.png << 'PY'
import struct, sys, zlib
out, ref_path = sys.argv[1], sys.argv[2]
data = open(ref_path, "rb").read()
assert data[:8] == b"\x89PNG\r\n\x1a\n"
i = 8
w = h = bit = None
raw = b""
while i < len(data):
    ln, typ = struct.unpack(">I4s", data[i:i+8])
    i += 8
    chunk = data[i:i+ln]
    i += ln + 4
    if typ == b"IHDR":
        w, h, bit = struct.unpack(">IIB", chunk[:9])
    elif typ == b"IDAT":
        raw += chunk
raw = zlib.decompress(raw)
stride = 1 + (w * bit + 7) // 8
expect = bytearray(w * h)
prev = bytearray(stride - 1)
pos = 0
for y in range(h):
    filt = raw[pos]
    pos += 1
    row = bytearray(raw[pos:pos+stride-1])
    pos += stride - 1
    if filt == 1:
        for x, b in enumerate(row):
            left = row[x-1] if x else 0
            row[x] = (b + left) & 0xFF
    elif filt == 2:
        for x, b in enumerate(row):
            row[x] = (b + prev[x]) & 0xFF
    elif filt != 0:
        raise SystemExit("unsupported png filter %s" % filt)
    prev = row
    for x in range(w):
        bitpos = x * bit
        byte = row[bitpos // 8]
        shift = 8 - bit - (bitpos % 8)
        sample = (byte >> shift) & ((1 << bit) - 1)
        expect[y * w + x] = 3 - sample
got = open(out + "/dmg-acid2.shade", "rb").read()
diff = sum(a != b for a, b in zip(expect, got))
print("acid2 shade_diff %d" % diff)
open(out + "/dmg-acid2.diff", "w").write("%d\n" % diff)
if diff != 0 or len(got) != len(expect):
    raise SystemExit(1)
PY

mooneye() {
	name=$1
	host "mooneye-$name" \
		--rom "roms/third_party/mooneye/$name.gb" --frames 300 \
		--judge mooneye --stop-on-result \
		--png "$OUT/mooneye-$name.png" --shade "$OUT/mooneye-$name.shade"
}

mooneye daa
mooneye mem_oam
mooneye oam_dma_basic
mooneye div_write
mooneye hblank_ly_scx_timing-GS

for name in daa mem_oam oam_dma_basic; do
	if ! grep -q 'mooneye result=pass' "$OUT/mooneye-$name.log"; then
		echo "expected pass: $name" >&2
		fail=1
	fi
done

host blargg-cpu_instrs \
	--rom roms/third_party/blargg/cpu_instrs.gb --frames 200000 \
	--judge blargg --stop-on-result \
	--serial-file "$OUT/cpu_instrs.serial" \
	--png "$OUT/cpu_instrs.png" --shade "$OUT/cpu_instrs.shade"
host blargg-instr_timing \
	--rom roms/third_party/blargg/instr_timing.gb --frames 20000 \
	--judge blargg --stop-on-result \
	--serial-file "$OUT/instr_timing.serial" \
	--png "$OUT/instr_timing.png" --shade "$OUT/instr_timing.shade"
for name in blargg-cpu_instrs blargg-instr_timing; do
	if ! grep -q 'blargg result=pass' "$OUT/$name.log"; then
		echo "expected pass: $name" >&2
		fail=1
	fi
done

host armada-title \
	--rom roms/third_party/galactic-armada/GalacticArmada.gb --frames 90 \
	--png "$OUT/armada-title.png" --shade "$OUT/armada-title.shade"
host armada-held \
	--rom roms/third_party/galactic-armada/GalacticArmada.gb --frames 90 \
	--hold 40:90:01 \
	--png "$OUT/armada-held.png" --shade "$OUT/armada-held.shade"
host rubik-title \
	--rom roms/third_party/rubik/rubik.gb --frames 160 \
	--png "$OUT/rubik-title.png" --shade "$OUT/rubik-title.shade"
host rubik-held \
	--rom roms/third_party/rubik/rubik.gb --frames 160 \
	--hold 100:160:08 \
	--png "$OUT/rubik-held.png" --shade "$OUT/rubik-held.shade"

python3 - "$OUT" << 'PY'
import sys
out = sys.argv[1]
def shade(name):
    return open(out + "/" + name, "rb").read()
pairs = [
    ("armada", "armada-title.shade", "armada-held.shade"),
    ("rubik", "rubik-title.shade", "rubik-held.shade"),
]
bad = 0
for label, a, b in pairs:
    diff = sum(x != y for x, y in zip(shade(a), shade(b)))
    print("%s input_diff %d" % (label, diff))
    if diff == 0:
        bad = 1
if bad:
    raise SystemExit(1)
PY

if [ "$fail" -ne 0 ]; then
	echo "test_third_party: a ROM that should pass did not" >&2
	exit 1
fi
echo "test_third_party: ok"
