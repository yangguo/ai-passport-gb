# ROMs in this repository

Commercial Game Boy ROMs must not be committed.

## Built-in test ROM

| File | Purpose |
| --- | --- |
| `homebrew/pattern.asm` | Source for `AIPATTERN` — static tile pattern for the frame hash |
| `homebrew/pattern.gb` | Output of `tools/build_rom.sh` (rgbfix -v) |
| `homebrew/pattern.sha256` | SHA-256 of frame 3 shade buffer (host test) |

Build tool: [RGBDS](https://rgbds.gbdev.io/) (MIT). The toolchain is not vendored.

## Licensed third-party ROMs

`third_party/` holds DMG ROMs whose license allows redistribution, plus the
dmg-acid2 reference image. Provenance, SHA-256, and pass/fail results are in
`docs/test-roms.md`. Blargg binaries are not in git; `tools/fetch_test_roms.sh`
downloads the pinned files and checks their hashes.
