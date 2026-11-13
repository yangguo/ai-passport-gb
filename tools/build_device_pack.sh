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

./tools/package_notices.sh "$ART"

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

hash_of_host_log() {
	awk '/^sha256 /{print $2; exit}' "$1"
}

build_one() {
	name=$1
	rom=$2
	hash_frame=$3
	joy=$4
	input_at=$5
	shift 5
	host_args=${*:-}

	out="$PACK/$name"
	# A failed rebuild must not leave a previous image under the new pack name.
	rm -rf "$out" || return
	mkdir -p "$out" || return

	echo "==== build $name rom=$(basename "$rom") hash_frame=$hash_frame run=forever"
	idf.py -B idf-build \
		-DGB_ROM="$rom" \
		-DGB_RUN_FRAMES=0 \
		-DGB_HASH_FRAME="$hash_frame" \
		-DGB_OK_JOY="$joy" \
		-DGB_INPUT_AT="$input_at" \
		build merge-bin || return

	# build_one is called in a condition, which disables POSIX sh's set -e
	# inside the function. Propagate command failures explicitly.
	cp idf-build/merged-binary.bin "$out/${name}-merged.bin" || return
	cp idf-build/bootloader/bootloader.bin "$out/${name}-bootloader.bin" || return
	cp idf-build/partition_table/partition-table.bin "$out/${name}-partition-table.bin" || return
	cp idf-build/ai-passport-gb.bin "$out/${name}-app.bin" || return
	cp idf-build/ai-passport-gb.elf "$out/${name}.elf" || return

	{
		echo "# Flash ai-passport-gb ($name)"
		echo "esptool.py --chip esp32c3 -p /dev/ttyACM0 -b 460800 write_flash \\"
		echo "  0x0 $out/${name}-merged.bin"
		echo "# Or write components:"
		echo "esptool.py --chip esp32c3 -p /dev/ttyACM0 -b 460800 write_flash \\"
		echo "  0x0 $out/${name}-bootloader.bin \\"
		echo "  0x8000 $out/${name}-partition-table.bin \\"
		echo "  0x10000 $out/${name}-app.bin"
	} >"$out/${name}-flash.cmd" || return

	(
		cd "$out" || exit
		sha256sum "${name}-merged.bin" "${name}-bootloader.bin" \
			"${name}-partition-table.bin" "${name}-app.bin"
	) >"$out/${name}.sha256" || return

	"$HOST" --rom "$rom" --frames "$hash_frame" $host_args \
		> "$SIM/$name-host.log" 2>&1 || return
	expect=$(hash_of_host_log "$SIM/$name-host.log") || return
	if ! printf '%s\n' "$expect" | grep -Eq '^[0-9a-f]{64}$'; then
		echo "invalid host hash for $name" >&2
		return 1
	fi
	echo "$hash_frame" >"$SIM/$name-expected-gb_hash-frame.txt" || return
	echo "$expect" >"$SIM/$name-expected-gb_hash.txt" || return

	sdir="$SIM/$name"
	rm -rf "$sdir" || return
	mkdir -p "$sdir" || return
	cmds="$sdir/commands.txt"
	art="$sdir/once/artifacts"
	{
		echo "start gbrom --boot until_app_main --boot-timeout 120s --mode deterministic --artifacts $art"
		if [ "$input_at" != "0" ]; then
			echo "run serial:/gb_input_at/ --timeout 600s --wall-budget-ms 900000 --artifacts $art"
			echo "input ok press"
		fi
		echo "run serial:/gb_hash/ --timeout 600s --wall-budget-ms 900000 --artifacts $art"
		echo "run serial:/gb_perf/ --timeout 600s --wall-budget-ms 900000 --artifacts $art"
		echo "screenshot raw --save-as panel --artifacts $art"
		echo "serial read --cursor 0 --max-bytes 524288 --artifacts $art"
		echo "status --output json --artifacts $art"
	} >"$cmds" || return
	set +e
	./tools/passportsim once "$out/${name}-merged.bin" "$out/${name}.elf" \
		"$sdir" "$cmds"
	sim_rc=$?
	set -e
	echo "$sim_rc" >"$sdir/passportsim.exit" || return
	if [ "$sim_rc" -ne 0 ]; then
		echo "passportsim failed for $name (exit $sim_rc)" >&2
		return "$sim_rc"
	fi

	find "$sdir" -type f \( -name 'session.log' -o -name '*.log' -o -name '*.json' \) \
		-print0 2>/dev/null | xargs -0 cat >"$SIM/$name-uart.log" || return
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
	if ! python3 - "$SIM/$name-uart.log" <<'PY'
import json, sys
path = sys.argv[1]
found = False
for line in open(path):
    if not line.startswith("{"):
        continue
    lint = json.loads(line).get("receipt", {}).get("timing_lint", None)
    if lint is not None:
        found = True
        if lint != []:
            sys.exit(1)
sys.exit(0 if found else 1)
PY
	then
		echo "timing_lint not empty for $name" >&2
		return 1
	fi
	cp "$SIM/$name-uart.log" "$ART/$name-uart.log" || return
	png=$(find "$sdir" -name 'panel*.png' -print -quit) || return
	if [ -z "$png" ]; then
		echo "panel screenshot missing for $name" >&2
		return 1
	fi
	cp "$png" "$ART/$name-screenshot.png" || return
	echo "OK $name gb_hash=$got sim_exit=$sim_rc"
}

mkdir -p "$PACK" "$SIM"
# Remove images left by the old runner's failed Blargg check, if present.
rm -rf "$PACK/blargg-cpu-instrs"
rm -f "$ART/blargg-cpu-instrs-"*
FAIL=0

# GB_RUN_FRAMES=0 → run until power-off (device testers); passportsim uses wall budget.
run_or_fail() {
	if ! build_one "$@"; then
		FAIL=1
	fi
}

# Acceptance frame counts match tools/test_sim_roms.sh (shade hash at that frame).
run_or_fail pattern "$ROOT/roms/homebrew/pattern.gb" 3 GB_BTN_A 0 \
	--pattern-contract --check roms/homebrew/pattern.sha256
run_or_fail dmg-acid2 "$ROOT/roms/third_party/dmg-acid2/dmg-acid2.gb" 10 GB_BTN_A 0
run_or_fail galactic-armada "$ROOT/roms/third_party/galactic-armada/GalacticArmada.gb" \
	90 GB_BTN_A 40 --hold 40:90:01
run_or_fail rubik "$ROOT/roms/third_party/rubik/rubik.gb" 160 GB_BTN_START 100 \
	--hold 100:160:08
run_or_fail mooneye-daa "$ROOT/roms/third_party/mooneye/daa.gb" 23 GB_BTN_A 0

# Blargg: build and verify locally; merged image stays uncommitted and is not
# published under the device-pack artifacts directory.
blargg_tmp=$(mktemp -d)
if (
	# Never put the unlicensed image in the public artifact tree, even while
	# verification is running or if it fails.
	ART="$blargg_tmp"
	PACK="$ART/pack"
	SIM="$ART/sim"
	mkdir -p "$PACK" "$SIM" || exit
	build_one blargg-cpu-instrs "$ROOT/roms/third_party/blargg/cpu_instrs.gb" \
		3173 GB_BTN_A 0
); then
	echo "blargg-cpu-instrs: sim OK (images not published)"
else
	FAIL=1
fi
rm -rf "$blargg_tmp"

manifest_tmp=$(mktemp "$ART/.manifest.XXXXXX")
(
	cd "$ART"
	echo "# Device pack artifact SHA-256 ($(date -u +%Y-%m-%dT%H:%M:%SZ))"
	{
		find pack notices sources -type f
		find . -maxdepth 1 -type f \( -name LICENSE -o -name '*-uart.log' \
			-o -name '*-screenshot.png' \)
	} |
		sort | while read -r f; do
			sha256sum "$f"
	done
) >"$manifest_tmp"
mv "$manifest_tmp" "$ART/manifest.sha256"

if [ "$FAIL" -ne 0 ]; then
	echo "build_device_pack: failures" >&2
	exit 1
fi
echo "build_device_pack: ok → $ART"
