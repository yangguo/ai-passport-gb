#!/bin/sh
# Add the notices and pinned Rubik sources required by the device pack.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT"
[ "$#" -eq 1 ] || { echo "usage: tools/package_notices.sh <pack-directory>" >&2; exit 2; }
mkdir -p "$1"
ART=$(CDPATH= cd -- "$1" && pwd)
NOTICES="$ART/notices"
SOURCES="$ART/sources"
CACHE="$ROOT/build/source-cache"

RUBIK_REV=e0bf62e069682273f8679f9297decd66079aa9c3
RUBIK_SHA=91020299ef3a8cb49021ede1355b46863ceb7db0c4e03a47e069f77875a37fb4
OWL_REV=9ac178632e99093b40d418148e82ca1c8392bb2f
OWL_SHA=65891a57fca4b22643c965356dbe3545ce80c067d1fd36313c37dbe615e9fd58

for file in LICENSE third_party/peanut-gb/LICENSE third_party/peanut-gb/SOURCE.md \
	third_party/peanut-gb/peanut_gb.h components/bsp/LICENSE components/bsp/IMPORT.md \
	roms/third_party/dmg-acid2/LICENSE roms/third_party/mooneye/LICENSE \
	roms/third_party/galactic-armada/LICENSE roms/third_party/rubik/LICENSE \
	roms/third_party/rubik/LICENSE-HomebrewOwl.GPL roms/third_party/rubik/SOURCE.md \
	docs/test-roms.md; do
	if [ ! -f "$file" ]; then
		echo "package_notices: missing $file" >&2
		exit 1
	fi
done

mkdir -p "$NOTICES" "$SOURCES" "$CACHE"
cp LICENSE "$ART/LICENSE"
cp third_party/peanut-gb/LICENSE "$NOTICES/Peanut-GB-LICENSE"
cp third_party/peanut-gb/SOURCE.md "$NOTICES/Peanut-GB-SOURCE.md"
# The header also contains the SameBoy/Lior Halphon attribution.
sed -n '1,/^ \*\/$/p' third_party/peanut-gb/peanut_gb.h >"$NOTICES/Peanut-GB-NOTICE"
cp components/bsp/LICENSE "$NOTICES/BSP-LICENSE"
cp components/bsp/IMPORT.md "$NOTICES/BSP-IMPORT.md"
cp roms/third_party/dmg-acid2/LICENSE "$NOTICES/dmg-acid2-LICENSE"
cp roms/third_party/mooneye/LICENSE "$NOTICES/mooneye-LICENSE"
cp roms/third_party/galactic-armada/LICENSE "$NOTICES/galactic-armada-LICENSE"
cp roms/third_party/rubik/LICENSE "$NOTICES/rubik-MIT-LICENSE"
cp roms/third_party/rubik/LICENSE-HomebrewOwl.GPL "$NOTICES/HomebrewOwl.GB-GPL-3.0-LICENSE"
cp docs/test-roms.md "$NOTICES/test-roms.md"
cp roms/third_party/rubik/SOURCE.md "$SOURCES/Rubik-SOURCE.md"

check_source() {
	actual=$(sha256sum "$1") || return
	actual=${actual%% *}
	if [ "$actual" != "$2" ]; then
		echo "package_notices: source SHA-256 mismatch for $1" >&2
		return 1
	fi
}

fetch_source() {
	name=$1
	url=$2
	expect=$3
	archive="$CACHE/$name"
	if [ ! -f "$archive" ]; then
		tmp=$(mktemp "$CACHE/.source.XXXXXX")
		if ! curl -fsSL --retry 2 "$url" -o "$tmp"; then
			rm -f "$tmp"
			return 1
		fi
		if ! check_source "$tmp" "$expect"; then
			rm -f "$tmp"
			return 1
		fi
		mv "$tmp" "$archive"
	fi
	check_source "$archive" "$expect"
	cp "$archive" "$SOURCES/$name"
}

fetch_source "rubik-$RUBIK_REV.tar.gz" \
	"https://codeload.github.com/NotImplementedLife/rubik/tar.gz/$RUBIK_REV" "$RUBIK_SHA"
fetch_source "HomebrewOwl.GB-$OWL_REV.tar.gz" \
	"https://codeload.github.com/NotImplementedLife/HomebrewOwl.GB/tar.gz/$OWL_REV" "$OWL_SHA"
echo "package_notices: ok"
