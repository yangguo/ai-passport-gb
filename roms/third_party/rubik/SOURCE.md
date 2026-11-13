# Rubik source bundle

The device pack's `sources/` directory carries both unmodified upstream
archives used for the committed `rubik.gb`. GitHub's Rubik archive does not
include the contents of its HomebrewOwl.GB submodule, so both archives are
required. Game sources retain their MIT notice; the linked HomebrewOwl.GB
code uses GPL-3.0. The separate license texts are in `notices/`.

| Archive | Official source | SHA-256 |
| --- | --- | --- |
| `rubik-e0bf62e069682273f8679f9297decd66079aa9c3.tar.gz` | <https://codeload.github.com/NotImplementedLife/rubik/tar.gz/e0bf62e069682273f8679f9297decd66079aa9c3> | `91020299ef3a8cb49021ede1355b46863ceb7db0c4e03a47e069f77875a37fb4` |
| `HomebrewOwl.GB-9ac178632e99093b40d418148e82ca1c8392bb2f.tar.gz` | <https://codeload.github.com/NotImplementedLife/HomebrewOwl.GB/tar.gz/9ac178632e99093b40d418148e82ca1c8392bb2f> | `65891a57fca4b22643c965356dbe3545ce80c067d1fd36313c37dbe615e9fd58` |

`tools/package_notices.sh` checks these hashes before copying either archive.
Its download cache is `build/source-cache/`, which is gitignored.

To restore the source layout, run from the device pack's `sources/`:

```sh
mkdir -p rubik
tar -xzf rubik-e0bf62e069682273f8679f9297decd66079aa9c3.tar.gz \
  -C rubik --strip-components=1
mkdir -p rubik/src/HomebrewOwl.GB
tar -xzf HomebrewOwl.GB-9ac178632e99093b40d418148e82ca1c8392bb2f.tar.gz \
  -C rubik/src/HomebrewOwl.GB --strip-components=1
```

Build with GNU Make and **RGBDS 0.5.2** (`rgbasm`, `rgblink`, `rgbfix`),
available from the [official RGBDS release](https://github.com/gbdev/rgbds/releases/tag/v0.5.2).
The pinned Makefile reads `project.mk`, builds the assembly under `src/`
and `src/HomebrewOwl.GB/`, and writes `bin/rubik_1_3.gb`. For the recorded
binary, fix the build-date string with `SOURCE_DATE_EPOCH=1624428281`:

```sh
cd rubik
SOURCE_DATE_EPOCH=1624428281 make RGBDS=/path/to/rgbds-0.5.2/
sha256sum bin/rubik_1_3.gb
```

Expected ROM SHA-256:
`6372610166a73972ceefb80fd1488c8bb21bd6a09d48d15d212d8068fc7468fc`.
No source changes are applied to either archive.
