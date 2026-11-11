#!/bin/sh
# Build one firmware image per ROM and boot it under passportsim.
#
# The shade-buffer hash printed as gb_hash is compared with gb-host.
# passportsim time is the guest's virtual clock. emu_us is not a
# measurement of the real ESP32-C3.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT"

OUT=${1:-build/sim-roms}
mkdir -p "$OUT"
OUT=$(CDPATH= cd -- "$OUT" && pwd)
HOST=./build/gb-host

if [ ! -x "$HOST" ]; then
	echo "test_sim_roms: build gb-host first" >&2
	exit 1
fi
if ! command -v idf.py >/dev/null 2>&1; then
	# shellcheck disable=SC1091
	. "${IDF_PATH:-$HOME/esp/esp-idf}/export.sh"
fi
./tools/fetch_test_roms.sh

hash_of() {
	awk '/^sha256 /{print $2; exit}' "$1"
}

build_fw() {
	name=$1
	rom=$2
	frames=$3
	joy=$4
	input_at=$5
	echo "==== build $name frames=$frames joy=$joy input_at=$input_at"
	idf.py -B idf-build \
		-DGB_ROM="$rom" \
		-DGB_RUN_FRAMES="$frames" \
		-DGB_OK_JOY="$joy" \
		-DGB_INPUT_AT="$input_at" \
		build
	idf.py -B idf-build merge-bin
	cp idf-build/merged-binary.bin "$OUT/$name.bin"
	cp idf-build/ai-passport-gb.elf "$OUT/$name.elf"
}

# plain: boot and wait for gb_hash.
# press: wait for gb_input_at, press OK, then wait for gb_hash.
# limit: wait for the cart-RAM refusal. No hash.
write_commands() {
	mode=$1
	art=$2
	dest=$3
	boot_timeout=${4:-90s}
	run_timeout=${5:-180s}
	budget=${6:-300000}
	{
		echo "start gbrom --boot until_app_main --boot-timeout $boot_timeout --mode deterministic --artifacts $art"
		if [ "$mode" = "press" ]; then
			echo "run serial:/gb_input_at/ --timeout $run_timeout --wall-budget-ms $budget --artifacts $art"
			echo "input ok press"
		fi
		if [ "$mode" = "limit" ]; then
			# The regex is one shell word. A space makes passportsim
			# treat the rest as a second argument.
			echo "run serial:/NVS/ --timeout $run_timeout --wall-budget-ms $budget --artifacts $art"
		else
			echo "run serial:/gb_hash/ --timeout $run_timeout --wall-budget-ms $budget --artifacts $art"
		fi
		echo "screenshot raw --save-as panel --artifacts $art"
		echo "serial read --cursor 0 --max-bytes 262144 --artifacts $art"
		echo "status --output json --artifacts $art"
	} > "$dest"
}

run_sim() {
	name=$1
	mode=$2
	boot_timeout=$3
	run_timeout=$4
	budget=$5
	dir="$OUT/$name"
	mkdir -p "$dir"
	art="$dir/once/artifacts"
	cmds="$dir/commands.txt"
	write_commands "$mode" "$art" "$cmds" "$boot_timeout" "$run_timeout" "$budget"
	set +e
	./tools/passportsim once "$OUT/$name.bin" "$OUT/$name.elf" "$dir" "$cmds"
	rc=$?
	set -e
	echo "$rc" > "$dir/passportsim.exit"
	echo "sim $name mode=$mode passportsim_exit=$rc"
}

uart_text() {
	dir=$1
	# session.log is the CLI transcript. Keep the capture name off the
	# globs below so this function is not reading the file it writes.
	find "$dir" -type f \( -name 'session.log' -o -name '*.log' -o -name '*.json' \) \
		-print0 | xargs -0 cat
}

expect_hash() {
	name=$1
	expect=$2
	uart_text "$OUT/$name" > "$OUT/$name/uart.capture"
	got=$(sed -n 's/.*gb_hash sha256=\([0-9a-f]\{64\}\).*/\1/p' "$OUT/$name/uart.capture" | tail -1)
	echo "$got" > "$OUT/$name/gb_hash.txt"
	if [ "$got" = "$expect" ]; then
		echo "HASH OK $name"
		return 0
	fi
	echo "HASH MISMATCH $name" >&2
	echo "  expected $expect" >&2
	echo "  got      ${got:-<none>}" >&2
	return 1
}

fail=0
note_fail() {
	echo "FAIL $1" >&2
	fail=1
}

# name rom frames joy input_at timeout budget host-args...
run_case() {
	name=$1
	rom=$2
	frames=$3
	joy=$4
	input_at=$5
	run_timeout=$6
	budget=$7
	shift 7
	"$HOST" --rom "$rom" --frames "$frames" "$@" \
		--png "$OUT/$name-host.png" --shade "$OUT/$name-host.shade" \
		> "$OUT/$name-host.log"
	expect=$(hash_of "$OUT/$name-host.log")
	build_fw "$name" "$rom" "$frames" "$joy" "$input_at"
	if [ "$input_at" = "0" ]; then
		run_sim "$name" plain 120s "$run_timeout" "$budget"
	else
		run_sim "$name" press 120s "$run_timeout" "$budget"
	fi
	if ! expect_hash "$name" "$expect"; then
		note_fail "$name"
	fi
}

boot_idle() {
	# Second boot of an image that can press OK, with the key left up.
	name=$1
	src=$2
	rom=$3
	frames=$4
	"$HOST" --rom "$rom" --frames "$frames" \
		--png "$OUT/$name-host.png" > "$OUT/$name-host.log"
	cp "$OUT/$src.bin" "$OUT/$name.bin"
	cp "$OUT/$src.elf" "$OUT/$name.elf"
	run_sim "$name" plain 120s 240s 600000
	if ! expect_hash "$name" "$(hash_of "$OUT/$name-host.log")"; then
		note_fail "$name"
	fi
}

SHORT_T=240s
SHORT_B=600000

run_case acid2 "$ROOT/roms/third_party/dmg-acid2/dmg-acid2.gb" 10 GB_BTN_A 0 $SHORT_T $SHORT_B
run_case daa "$ROOT/roms/third_party/mooneye/daa.gb" 23 GB_BTN_A 0 $SHORT_T $SHORT_B
run_case mem_oam "$ROOT/roms/third_party/mooneye/mem_oam.gb" 11 GB_BTN_A 0 $SHORT_T $SHORT_B
run_case oam_dma "$ROOT/roms/third_party/mooneye/oam_dma_basic.gb" 11 GB_BTN_A 0 $SHORT_T $SHORT_B
run_case div_write "$ROOT/roms/third_party/mooneye/div_write.gb" 11 GB_BTN_A 0 $SHORT_T $SHORT_B
run_case hblank "$ROOT/roms/third_party/mooneye/hblank_ly_scx_timing-GS.gb" 12 GB_BTN_A 0 $SHORT_T $SHORT_B
run_case instr_timing "$ROOT/roms/third_party/blargg/instr_timing.gb" 38 GB_BTN_A 0 $SHORT_T $SHORT_B

run_case armada-held "$ROOT/roms/third_party/galactic-armada/GalacticArmada.gb" 90 GB_BTN_A 40 \
	$SHORT_T $SHORT_B --hold 40:90:01
boot_idle armada-idle armada-held \
	"$ROOT/roms/third_party/galactic-armada/GalacticArmada.gb" 90

run_case rubik-held "$ROOT/roms/third_party/rubik/rubik.gb" 160 GB_BTN_START 100 \
	$SHORT_T $SHORT_B --hold 100:160:08
boot_idle rubik-idle rubik-held "$ROOT/roms/third_party/rubik/rubik.gb" 160

# Pattern ROM, the original 3-frame contract, now with the button poll compiled in.
run_case pattern "$ROOT/roms/homebrew/pattern.gb" 3 GB_BTN_A 0 $SHORT_T $SHORT_B \
	--pattern-contract --check roms/homebrew/pattern.sha256

# 8 KiB cart RAM must refuse before any frame, with a clear line and no panic.
python3 - "$OUT/ram-over.gb" << 'PY'
import pathlib, sys
dest = pathlib.Path(sys.argv[1])
src = pathlib.Path("roms/homebrew/pattern.gb").read_bytes()
d = bytearray(src)
d[0x149] = 0x02
x = 0
for i in range(0x134, 0x14D):
    x = (x - d[i] - 1) & 0xFF
d[0x14D] = x
dest.write_bytes(d)
PY
build_fw ram-over "$OUT/ram-over.gb" 1 GB_BTN_A 0
run_sim ram-over limit 90s 120s 180000
uart_text "$OUT/ram-over" > "$OUT/ram-over/uart.capture"
if ! grep -q "exceeds 2048" "$OUT/ram-over/uart.capture"; then
	note_fail "ram-over missing limit message"
fi
if grep -E -q "Guru Meditation|panic" "$OUT/ram-over/uart.capture"; then
	note_fail "ram-over panicked"
fi

# cpu_instrs is 3173 frames. The timeout is guest time; the wall budget
# is the real clock. A missing hash stays a failure of this script, and
# the log is the record — it is not reported as a pass.
run_case cpu_instrs "$ROOT/roms/third_party/blargg/cpu_instrs.gb" 3173 GB_BTN_A 0 \
	3600s 1800000

if [ "$fail" -ne 0 ]; then
	echo "test_sim_roms: failures recorded" >&2
	exit 1
fi
echo "test_sim_roms: ok"
