#!/bin/sh
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

CC="${CC:-cc}"
CFLAGS="-std=c11 -Wall -Wextra -Werror -O2 -DENABLE_SOUND=0 -DGB_CORE_OPCODE_HOOK"
INC="-I components/gb_core/include -I components/gb_video/include -I third_party/peanut-gb -I tools"

mkdir -p build

"$CC" $CFLAGS $INC \
	-o build/gb-host \
	tools/gb_host.c tools/sha256.c \
	components/gb_core/gb_core.c components/gb_video/gb_video_lut.c

"$CC" $CFLAGS $INC -o build/test-lut tools/test_lut.c components/gb_video/gb_video_lut.c

./build/test-lut

if [ ! -f roms/homebrew/pattern.gb ]; then
	./tools/build_rom.sh
fi

./build/gb-host --rom roms/homebrew/pattern.gb --frames 3 \
	--pattern-contract \
	--check roms/homebrew/pattern.sha256 \
	--png build/pattern-frame3.png

./build/gb-host --expect-bad roms/homebrew/pattern.gb

TMPDIR="${TMPDIR:-/tmp}"
TRUNC="$TMPDIR/gb-trunc.gb"
head -c 256 roms/homebrew/pattern.gb >"$TRUNC"
set +e
./build/gb-host --rom "$TRUNC" --frames 3
trunc_rc=$?
set -e
if [ "$trunc_rc" -eq 0 ]; then
	echo "truncated ROM should fail" >&2
	exit 1
fi

UNSUPPORTED="$TMPDIR/gb-unsupported.gb"
cp roms/homebrew/pattern.gb "$UNSUPPORTED"
python3 -c "
import sys
p=sys.argv[1]
d=bytearray(open(p,'rb').read())
d[0x147]=0xFE
open(p,'wb').write(d)
" "$UNSUPPORTED"

set +e
err=$(./build/gb-host --rom "$UNSUPPORTED" --frames 3 2>&1)
bad_rc=$?
set -e
if [ "$bad_rc" -eq 0 ]; then
	echo "unsupported MBC ROM should fail: $err" >&2
	exit 1
fi

# Header RAM-size code 0x02 is 8 KiB, above the 2048 byte NVS ceiling.
# The header checksum covers 0x134..0x14C, so it has to be recomputed.
OVER="$TMPDIR/gb-ram-over.gb"
cp roms/homebrew/pattern.gb "$OVER"
python3 -c "
import sys
p=sys.argv[1]
d=bytearray(open(p,'rb').read())
d[0x149]=0x02
x=0
for i in range(0x134, 0x14D):
    x=(x - d[i] - 1) & 0xFF
d[0x14D]=x
open(p,'wb').write(d)
" "$OVER"
set +e
err=$(./build/gb-host --rom "$OVER" --frames 1 2>&1)
over_rc=$?
set -e
if [ "$over_rc" -eq 0 ]; then
	echo "8 KiB cart RAM should fail: $err" >&2
	exit 1
fi
printf '%s\n' "$err" | grep -q "exceeds 2048 byte NVS limit"

# 2 KiB (code 0x01) is accepted. The pattern ROM never touches cart RAM,
# so the frame-3 hash stays the same.
OKRAM="$TMPDIR/gb-ram-2k.gb"
cp roms/homebrew/pattern.gb "$OKRAM"
python3 -c "
import sys
p=sys.argv[1]
d=bytearray(open(p,'rb').read())
d[0x149]=0x01
x=0
for i in range(0x134, 0x14D):
    x=(x - d[i] - 1) & 0xFF
d[0x14D]=x
open(p,'wb').write(d)
" "$OKRAM"
./build/gb-host --rom "$OKRAM" --frames 3 --pattern-contract \
	--check roms/homebrew/pattern.sha256

echo "test_host: ok"
