#!/bin/sh
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if ! command -v rgbasm >/dev/null 2>&1; then
	echo "rgbasm not found; install RGBDS (https://rgbds.gbdev.io/)" >&2
	exit 1
fi

rgbasm -o roms/homebrew/pattern.o roms/homebrew/pattern.asm
rgblink -o roms/homebrew/pattern.gb roms/homebrew/pattern.o
rgbfix -v -p 0 -t AIPATTERN roms/homebrew/pattern.gb
rm -f roms/homebrew/pattern.o

echo "Built roms/homebrew/pattern.gb"
