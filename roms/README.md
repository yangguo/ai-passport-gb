# ROMs in this repository

Only **homemade** ROM sources and build artifacts belong here. Commercial Game Boy ROMs must not be committed.

## Built-in test ROM

| File | Purpose |
| --- | --- |
| `homebrew/pattern.asm` | Source for `AIPATTERN` — static tile pattern for P0 frame hash |
| `homebrew/pattern.gb` | Output of `tools/build_rom.sh` (rgbfix -v) |
| `homebrew/pattern.sha256` | SHA-256 of frame 3 shade buffer (host test) |

Build tool: [RGBDS](https://rgbds.gbdev.io/) (MIT). The toolchain is not vendored; install a release binary or distro package in CI.

## Optional third-party test ROMs (not in tree by default)

Per design §15.2: dmg-acid2, mealybug-tearoom-tests, mooneye-test-suite, and age-test-roms may be used later with their MIT licenses recorded. **Blargg** test ROMs are not used (license unclear).
