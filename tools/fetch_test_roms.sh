#!/bin/sh
# Fetch or check the DMG ROMs used by tools/test_third_party.sh.
#
# Committed binaries are the ones whose license allows redistribution.
# Blargg's readme does not grant a license, so those two files are
# downloaded here and left gitignored. SHA-256 is checked either way.
#
# RGBDS 0.9.0 cannot assemble dmg-acid2, the Mooneye suite, or Rubik
# (they use the older `name : MACRO` / `SET` syntax, and Mooneye is
# built with wla-dx). Galactic Armada does build with RGBDS 0.9.0.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT"

ACID_ROM_URL=https://github.com/mattcurrie/dmg-acid2/releases/download/v1.0/dmg-acid2.gb
ACID_ROM_SHA=464e14b7d42e7feea0b7ede42be7071dc88913f75b9ffa444299424b63d1dff1
ACID_PNG_URL=https://github.com/mattcurrie/dmg-acid2/raw/8a98ce731f96dde032ffb22ec36dc985d78fdb18/img/reference-dmg.png
ACID_PNG_SHA=ca966d50895c7efef05838590d148c2cbfd7fba57dab986f25b35b4da71abb57

BLARGG_REV=c240dd7d700e5c0b00a7bbba52b53e4ee67b5f15
BLARGG_BASE=https://github.com/retrio/gb-test-roms/raw/$BLARGG_REV

sha_file() {
	sha256sum "$1" | awk '{print $1}'
}

need() {
	file=$1
	expect=$2
	if [ ! -f "$file" ]; then
		return 1
	fi
	got=$(sha_file "$file")
	if [ "$got" != "$expect" ]; then
		echo "fetch_test_roms: $file" >&2
		echo "  expected $expect" >&2
		echo "  got      $got" >&2
		return 1
	fi
	echo "ok $file"
	return 0
}

fetch() {
	url=$1
	dest=$2
	expect=$3
	tmp=$(mktemp)
	curl -fsSL "$url" -o "$tmp"
	got=$(sha_file "$tmp")
	if [ "$got" != "$expect" ]; then
		echo "fetch_test_roms: $url hashed to $got" >&2
		rm -f "$tmp"
		exit 1
	fi
	mkdir -p "$(dirname "$dest")"
	mv "$tmp" "$dest"
	echo "fetched $dest"
}

ensure() {
	file=$1
	expect=$2
	url=$3
	if need "$file" "$expect"; then
		return 0
	fi
	fetch "$url" "$file" "$expect"
}

ensure roms/third_party/dmg-acid2/dmg-acid2.gb "$ACID_ROM_SHA" "$ACID_ROM_URL"
ensure roms/third_party/dmg-acid2/reference-dmg.png "$ACID_PNG_SHA" "$ACID_PNG_URL"

# Committed ROMs. These are not re-downloaded; a mismatch is a hard error
# so a rebuilt binary cannot silently replace a pinned one.
check() {
	file=$1
	expect=$2
	need "$file" "$expect"
}

check roms/third_party/mooneye/daa.gb \
	1498d92d70592a07a2493ef764609916616f0b023b21408189e277201e6c14c1
check roms/third_party/mooneye/mem_oam.gb \
	eba5d165aaa55e7d4a1d1d910ea312a55c78157923c29fbbc34031709390f1de
check roms/third_party/mooneye/oam_dma_basic.gb \
	326b747cac8cc96b62d6ee508e73b87eda24bfe29553d3d32e719f3b6d76c97c
check roms/third_party/mooneye/div_write.gb \
	2be1e4da6fa24b9123d2a8bae47dd0d6f5e97e1855186c0c0f49e6d213eebfff
check roms/third_party/mooneye/hblank_ly_scx_timing-GS.gb \
	3adec9174d16b7a4cece42e5525e4363ff956c19070600aa9344de68b0885449
check roms/third_party/rubik/rubik.gb \
	6372610166a73972ceefb80fd1488c8bb21bd6a09d48d15d212d8068fc7468fc
check roms/third_party/galactic-armada/GalacticArmada.gb \
	e3132cc5aaf07acb5618f21e279fe4c20559b9d685a6c7a9fe63bb8253c0cc5a

ensure roms/third_party/blargg/cpu_instrs.gb \
	8c5e12f41e0ba5bbca796944f92ffe6de28809198682c4332e38d1b3cf56fcf2 \
	"$BLARGG_BASE/cpu_instrs/cpu_instrs.gb"
ensure roms/third_party/blargg/instr_timing.gb \
	646067b3d6c79fda810e9c3f1cb7c0efd5abb0a7ac06437c54e65720c15d9925 \
	"$BLARGG_BASE/instr_timing/instr_timing.gb"

echo "fetch_test_roms: ok"
