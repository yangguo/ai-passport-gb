#!/bin/sh
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

CC="${CC:-cc}"
CFLAGS="-std=c11 -Wall -Wextra -Werror -O2 -DENABLE_SOUND=0"
INC="-I components/gb_core/include -I components/gb_video/include -I third_party/peanut-gb -I tools"

mkdir -p build

"$CC" $CFLAGS $INC \
	-o build/gb-host \
	tools/gb_host.c tools/sha256.c \
	components/gb_core/gb_core.c components/gb_video/gb_video_lut.c

"$CC" $CFLAGS $INC -o build/test-lut tools/test_lut.c components/gb_video/gb_video_lut.c
"$CC" $CFLAGS $INC -o build/test-core tools/test_core.c components/gb_core/gb_core.c

./build/test-lut

if [ ! -f roms/homebrew/pattern.gb ]; then
	./tools/build_rom.sh
fi

./build/test-core roms/homebrew/pattern.gb

./build/gb-host --rom roms/homebrew/pattern.gb --frames 3 \
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
x=0
for i in range(0x134, 0x14D):
    x=(x - d[i] - 1) & 0xFF
d[0x14D]=x
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
printf '%s\n' "$err" | grep -qx "gb_core_save_size: 3"

echo "test_host: ok"
