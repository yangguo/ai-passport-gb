#!/bin/sh
# Build per-ROM firmware images for real-device testers (DMG only).
# Blargg is fetched locally and never committed.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT"

ART=${1:-/opt/cursor/artifacts/device-pack}
mkdir -p "$ART"
ART=$(CDPATH= cd -- "$ART" && pwd)
HOST=./build/gb-host
PACK="$ART/pack"
SIM="$ART/sim"

if ! command -v idf.py >/dev/null 2>&1; then
	# shellcheck disable=SC1091
	. "${IDF_PATH:-$HOME/esp/esp-idf}/export.sh"
fi

./tools/fetch_test_roms.sh

mkdir -p build
if [ ! -x "$HOST" ]; then
	echo "build_device_pack: compiling gb-host"
	CC=${CC:-cc}
	CFLAGS="-std=c11 -Wall -Wextra -Werror -O2 -DENABLE_SOUND=0 -DGB_CORE_OPCODE_HOOK"
	INC="-I components/gb_core/include -I components/gb_video/include -I third_party/peanut-gb -I tools"
	"$CC" $CFLAGS $INC -o "$HOST" tools/gb_host.c tools/sha256.c \
		components/gb_core/gb_core.c components/gb_video/gb_video_lut.c
fi

if [ ! -x ./tools/passportsim ] ||
	[ ! -x "${PASSPORTSIM_BIN:-${HOME}/.cache/apsim/passportsim/src/target/release/passportsim}" ]; then
	./tools/passportsim setup
fi

sha256_file() {
	sha256sum "$1" | awk '{print $1}'
}

hash_of_host_log() {
	awk '/^sha256 /{print $2; exit}' "$1"
}

build_one() {
	name=$1
	rom=$2
	frames=$3
	joy=$4
	input_at=$5
	shift 5
	host_args=${*:-}

	out="$PACK/$name"
	mkdir -p "$out"

	echo "==== build $name rom=$(basename "$rom") frames=$frames"
	idf.py -B idf-build \
		-DGB_ROM="$rom" \
		-DGB_RUN_FRAMES="$frames" \
		-DGB_OK_JOY="$joy" \
		-DGB_INPUT_AT="$input_at" \
		build merge-bin

	cp idf-build/merged-binary.bin "$out/${name}-merged.bin"
	cp idf-build/bootloader/bootloader.bin "$out/${name}-bootloader.bin"
	cp idf-build/partition_table/partition-table.bin "$out/${name}-partition-table.bin"
	cp idf-build/ai-passport-gb.bin "$out/${name}-app.bin"
	cp idf-build/ai-passport-gb.elf "$out/${name}.elf"

	{
		echo "# Flash ai-passport-gb ($name)"
		echo "esptool.py --chip esp32c3 -p /dev/ttyACM0 -b 460800 write_flash \\"
		echo "  0x0 $out/${name}-merged.bin"
		echo "# Or write components:"
		echo "esptool.py --chip esp32c3 -p /dev/ttyACM0 -b 460800 write_flash \\"
		echo "  0x0 $out/${name}-bootloader.bin \\"
		echo "  0x8000 $out/${name}-partition-table.bin \\"
		echo "  0x10000 $out/${name}-app.bin"
	} >"$out/${name}-flash.cmd"

	{
		echo "$out/${name}-merged.bin $(sha256_file "$out/${name}-merged.bin")"
		echo "$out/${name}-bootloader.bin $(sha256_file "$out/${name}-bootloader.bin")"
		echo "$out/${name}-partition-table.bin $(sha256_file "$out/${name}-partition-table.bin")"
		echo "$out/${name}-app.bin $(sha256_file "$out/${name}-app.bin")"
	} >"$out/${name}.sha256"

	"$HOST" --rom "$rom" --frames 3 $host_args \
		> "$SIM/$name-host.log" 2>&1 || true
	expect=$(hash_of_host_log "$SIM/$name-host.log")
	echo "$expect" >"$SIM/$name-expected-gb_hash.txt"

	sdir="$SIM/$name"
	mkdir -p "$sdir"
	cmds="$sdir/commands.txt"
	art="$sdir/once/artifacts"
	cat >"$cmds" <<EOF
start gbrom --boot until_app_main --boot-timeout 120s --mode deterministic --artifacts $art
run serial:/gb_hash/ --timeout 600s --wall-budget-ms 900000 --artifacts $art
run serial:/gb_perf/ --timeout 600s --wall-budget-ms 900000 --artifacts $art
screenshot raw --save-as panel --artifacts $art
serial read --cursor 0 --max-bytes 524288 --artifacts $art
status --output json --artifacts $art
EOF
	set +e
	./tools/passportsim once "$out/${name}-merged.bin" "$out/${name}.elf" \
		"$sdir" "$cmds"
	sim_rc=$?
	set -e
	echo "$sim_rc" >"$sdir/passportsim.exit"

	find "$sdir" -type f \( -name 'session.log' -o -name '*.log' -o -name '*.json' \) \
		-print0 2>/dev/null | xargs -0 cat >"$SIM/$name-uart.log" || true
	got=$(sed -n 's/.*gb_hash sha256=\([0-9a-f]\{64\}\).*/\1/p' \
		"$SIM/$name-uart.log" | tail -1)
	if [ "$got" != "$expect" ]; then
		echo "HASH MISMATCH $name expected=$expect got=${got:-<none>}" >&2
		return 1
	fi
	if ! grep -q 'gb_perf ' "$SIM/$name-uart.log"; then
		echo "gb_perf missing for $name" >&2
		return 1
	fi
	cp "$SIM/$name-uart.log" "$ART/$name-uart.log"
	find "$sdir" -name 'panel*.png' -print -quit 2>/dev/null | while read -r png; do
		cp "$png" "$ART/$name-screenshot.png"
	done
	echo "OK $name gb_hash=$got sim_exit=$sim_rc"
}

mkdir -p "$PACK" "$SIM"
FAIL=0

# GB_RUN_FRAMES=0 → run until power-off (device testers); passportsim uses wall budget.
run_or_fail() {
	if ! build_one "$@"; then
		FAIL=1
	fi
}

run_or_fail pattern "$ROOT/roms/homebrew/pattern.gb" 0 GB_BTN_A 0 \
	--pattern-contract --check roms/homebrew/pattern.sha256
run_or_fail dmg-acid2 "$ROOT/roms/third_party/dmg-acid2/dmg-acid2.gb" 0 GB_BTN_A 0
run_or_fail galactic-armada "$ROOT/roms/third_party/galactic-armada/GalacticArmada.gb" \
	0 GB_BTN_A 40 --hold 40:90:01
run_or_fail rubik "$ROOT/roms/third_party/rubik/rubik.gb" 0 GB_BTN_START 100 \
	--hold 100:160:08
run_or_fail mooneye-daa "$ROOT/roms/third_party/mooneye/daa.gb" 0 GB_BTN_A 0

# Blargg: build and verify locally; merged image stays uncommitted and is not
# published under the device-pack artifacts directory.
blargg_tmp=$(mktemp -d)
if build_one blargg-cpu-instrs "$ROOT/roms/third_party/blargg/cpu_instrs.gb" \
	0 GB_BTN_A 0; then
	cp -a "$PACK/blargg-cpu-instrs" "$blargg_tmp/" 2>/dev/null || true
	rm -rf "$PACK/blargg-cpu-instrs"
	rm -f "$ART/blargg-cpu-instrs-"*
	echo "blargg-cpu-instrs: sim OK (images not published)"
else
	FAIL=1
fi
rm -rf "$blargg_tmp"

{
	echo "# Device pack artifact SHA-256 ($(date -u +%Y-%m-%dT%H:%M:%SZ))"
	find "$PACK" "$ART" -maxdepth 2 -type f \( -name '*.bin' -o -name '*.sha256' \
		-o -name '*-uart.log' -o -name '*-screenshot.png' \) 2>/dev/null |
		sort | while read -r f; do
		printf '%s %s\n' "$(sha256_file "$f")" "$f"
	done
} >"$ART/manifest.sha256"

if [ "$FAIL" -ne 0 ]; then
	echo "build_device_pack: failures" >&2
	exit 1
fi
echo "build_device_pack: ok → $ART"
