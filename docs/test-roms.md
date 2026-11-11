# Licensed DMG test ROMs

These runs use the Peanut-GB core already in this tree (upstream
`d0bcca771c83a2638c93a9ae61f3d51f226dc905`) on a DMG model: no boot ROM,
register A starts at `0x01`. CGB-only images (header `0x143 = 0xC0`) are
refused. Header `0x80` (CGB-enhanced, still DMG-bootable) is accepted.
No GBA ROM is used.

`tools/fetch_test_roms.sh` checks every SHA-256 below. Blargg binaries are
downloaded by that script and are gitignored. Everything else in the table
is committed, because its license allows redistribution.

RGBDS 0.9.0 cannot assemble dmg-acid2, the Mooneye suite, or Rubik: those
sources use the older `name : MACRO` / `SET` syntax, and Mooneye is a
wla-dx project. Galactic Armada does build with RGBDS 0.9.0.

## Provenance

| ROM | Upstream | Pin | License | Size | MBC | Cart RAM | SHA-256 | In git |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| dmg-acid2 | [mattcurrie/dmg-acid2](https://github.com/mattcurrie/dmg-acid2) release [v1.0](https://github.com/mattcurrie/dmg-acid2/releases/download/v1.0/dmg-acid2.gb) | release v1.0; reference PNG from `8a98ce731f96dde032ffb22ec36dc985d78fdb18` `img/reference-dmg.png` | MIT | 32768 | ROM only (`0x00`) | 0 | `464e14b7d42e7feea0b7ede42be7071dc88913f75b9ffa444299424b63d1dff1` | yes |
| mooneye `acceptance/instr/daa` | [Gekkio/mooneye-test-suite](https://github.com/Gekkio/mooneye-test-suite) | `31510e12eea6286d36eea060a6adde755e1067aa` | MIT | 32768 | ROM only | 0 | `1498d92d70592a07a2493ef764609916616f0b023b21408189e277201e6c14c1` | yes |
| mooneye `acceptance/bits/mem_oam` | same | same | MIT | 32768 | ROM only | 0 | `eba5d165aaa55e7d4a1d1d910ea312a55c78157923c29fbbc34031709390f1de` | yes |
| mooneye `acceptance/oam_dma/basic` | same | same | MIT | 32768 | ROM only | 0 | `326b747cac8cc96b62d6ee508e73b87eda24bfe29553d3d32e719f3b6d76c97c` | yes |
| mooneye `acceptance/timer/div_write` | same | same | MIT | 32768 | ROM only | 0 | `2be1e4da6fa24b9123d2a8bae47dd0d6f5e97e1855186c0c0f49e6d213eebfff` | yes |
| mooneye `acceptance/ppu/hblank_ly_scx_timing-GS` | same | same | MIT | 32768 | ROM only | 0 | `3adec9174d16b7a4cece42e5525e4363ff956c19070600aa9344de68b0885449` | yes |
| Blargg `cpu_instrs` | [retrio/gb-test-roms](https://github.com/retrio/gb-test-roms) `cpu_instrs/cpu_instrs.gb` | `c240dd7d700e5c0b00a7bbba52b53e4ee67b5f15` | no license grant in the upstream readme | 65536 | MBC1 (`0x01`) | 0 | `8c5e12f41e0ba5bbca796944f92ffe6de28809198682c4332e38d1b3cf56fcf2` | no |
| Blargg `instr_timing` | same tree, `instr_timing/instr_timing.gb` | same commit | no license grant | 32768 | MBC1 | 0 | `646067b3d6c79fda810e9c3f1cb7c0efd5abb0a7ac06437c54e65720c15d9925` | no |
| Rubik's Cube | [NotImplementedLife/rubik](https://github.com/NotImplementedLife/rubik) | `e0bf62e069682273f8679f9297decd66079aa9c3`; Owl library submodule `9ac178632e99093b40d418148e82ca1c8392bb2f` | game sources MIT; HomebrewOwl.GB is GPL-3.0, so the linked ROM is GPL-3.0. Built with RGBDS 0.5.2 and `SOURCE_DATE_EPOCH=1624428281` | 32768 | ROM only | 0 | `6372610166a73972ceefb80fd1488c8bb21bd6a09d48d15d212d8068fc7468fc` | yes |
| Galactic Armada | [gbdev/gb-asm-tutorial](https://github.com/gbdev/gb-asm-tutorial) `galactic-armada/` | `444ae578d4c74fa5f1fe94730f6cf4620f1996ba` | MIT. Built with RGBDS 0.9.0 (`rgbfix -v -p 0xFF`) | 32768 | ROM only | 0 | `e3132cc5aaf07acb5618f21e279fe4c20559b9d685a6c7a9fe63bb8253c0cc5a` | yes |

The dmg-acid2 reference PNG SHA-256 is
`ca966d50895c7efef05838590d148c2cbfd7fba57dab986f25b35b4da71abb57`.
It is a 160×144 2-bit greyscale image. Sample 0 is black; the host shade
buffer uses Game Boy colour 0 as white, so the compare stores `3 - sample`.

Mooneye was built with wla-dx `89a90a56be5c2b8cf19a9afa3e1b32384ddb1a97`,
the assembler that suite's CI pins. The old `Gekkio/mooneye-gb` emulator
repository is GPL-3.0 and does not contain these ROMs.

Blargg header byte `0x143` is `0x80`. With A = `0x01` the ROMs take the DMG
path. They were not committed: the retrio readme names Shay Green and does
not grant a license.

Galactic Armada's title bytes at `0x134` are zero. The header checksum is
valid and the ROM runs. CGB flag is `0x00`.

## How a result is decided

| Suite | Pass | Fail |
| --- | --- | --- |
| dmg-acid2 | Frame 10 shade buffer matches `reference-dmg.png` (0 differing pixels) | Any other pixel |
| Mooneye | At opcode `0x40` (`ld b, b`), B C D E H L = 3 5 8 13 21 34 | Those registers are `0x42`, or any other value |
| Blargg | Serial text contains `Passed` and does not contain `Failed` | Serial contains `Failed` |
| Homebrew | Not a pass/fail suite. The title frame and a later frame with a button held must differ, and each must match the host hash |  |

The host (`tools/gb_host.c`, built with `-DGB_CORE_OPCODE_HOOK`) records the
registers at `ld b, b`. The firmware does not compile that hook. After a
Mooneye test halts in `jr .`, the live registers still hold the signature.
The live PC is one byte after the breakpoint; the host prints the breakpoint
address. Both are reported below.

`tools/test_third_party.sh` is the host run. `tools/test_sim_roms.sh` builds
one firmware image per ROM (`-DGB_ROM`, `-DGB_RUN_FRAMES`, `-DGB_OK_JOY`,
`-DGB_INPUT_AT`) and boots it with `tools/passportsim once`.

## Results

Shade hash is SHA-256 of the 160×144 Game Boy shade buffer. "Sim hash" is
the `gb_hash` line from the firmware UART. A match means the panel contents
and the host frame are the same picture.

| ROM | Frames | Host | Sim | Shade hash |
| --- | --- | --- | --- | --- |
| dmg-acid2 | 10 | pass (0 pixels differ) | hash matches | `f844ea760a6f1fe137f7f992c7ab1c72d34c7fcd3a807b4174a78eb04a32a458` |
| mooneye daa | 23 | pass (3 5 8 13 21 34) | hash matches; UART `b=3 c=5 d=8 e=13 h=21 l=34` | `ae6bfbe8094f4162964e5c2fa4c04846546adff1ea97a0059946569c155dc885` |
| mooneye mem_oam | 11 | pass | hash matches; same Fibonacci registers | `ae6bfbe8094f4162964e5c2fa4c04846546adff1ea97a0059946569c155dc885` |
| mooneye oam_dma basic | 11 | pass | hash matches; same Fibonacci registers | `ae6bfbe8094f4162964e5c2fa4c04846546adff1ea97a0059946569c155dc885` |
| mooneye div_write | 11 | fail (`0x42` in B C D E H L) | hash matches; UART shows 66 in each of those registers | `b537adac063357549eb63e0e9383f7bf73b8cb5d70a1583f5aa53baa1a4999ff` |
| mooneye hblank_ly_scx_timing-GS | 12 | fail (`0x42`) | hash matches; UART shows 66 | `596c7bad14e0a2c128164b01a2a5eb3b06e5cbe2c13935d82730eef0b8fb3af4` |
| Blargg cpu_instrs | 3173 | pass (`01:ok` … `11:ok`, then `Passed`) | hash matches; UART has the same `Passed` line | `ed1dff8e21ce7ebf6ee3e349828581f7906ae1019bc4facb1d2005123929e204` |
| Blargg instr_timing | 38 | pass (`Passed`) | hash matches; UART `gb_serial Passed` | `5fd21a582a4383fe56f8a705d5bcac95676871236a5ac0117adee124aa9211ef` |
| Galactic Armada, no button | 90 | title held still | hash matches | `6d89499e54e726b1aaa869cb890762572fe2a9695f78ab3bc736febef0cc4ba4` |
| Galactic Armada, OK held from frame 40 | 90 | picture changed (2170 shade bytes) | hash matches. OK is mapped to Game Boy A | `1465249ea7857719a735a361069c29ad661bdc1c6b963a90852aa6cd5b5d1dc0` |
| Rubik, no button | 160 | splash still on screen | hash matches | `3a32cb3e9f3257aea5a44078c96ccafb58e3f2f0642f59aab361a56828706786` |
| Rubik, OK held from frame 100 | 160 | picture changed (10190 shade bytes) | hash matches. OK is mapped to Start | `3c40d8633d138936589645dca803918ee5a51bde4b18e6a5e546fba241a95cc2` |

The two Mooneye failures are the timing-sensitive tests in this set. Peanut-GB
does not implement DIV-write timing or the HBlank LY/SCX case the `-GS` ROM
checks. They are recorded as failures. The picture of the failure still
matches between the host and the firmware.

`cpu_instrs` and `instr_timing` passed on this core, including all 11
cpu_instrs subtests. That is what the serial text said. It is not rounded up
from a partial log.

Screenshots from the host are 160×144. passportsim screenshots are the
240×320 panel with the Game Boy image centered 1:1. The PNGs are run
artifacts, not files in this repository. Names below are relative to that
artifact directory.

| Name | Host PNG | Panel PNG |
| --- | --- | --- |
| dmg-acid2 | `host/dmg-acid2.png` | `sim/dmg-acid2.png` |
| mooneye daa | `host/mooneye-daa.png` | `sim/mooneye-daa.png` |
| mooneye mem_oam | `host/mooneye-mem_oam.png` | `sim/mooneye-mem_oam.png` |
| mooneye oam_dma | `host/mooneye-oam_dma_basic.png` | `sim/mooneye-oam_dma.png` |
| mooneye div_write | `host/mooneye-div_write.png` | `sim/mooneye-div_write.png` |
| mooneye hblank | `host/mooneye-hblank.png` | `sim/mooneye-hblank.png` |
| cpu_instrs | `host/cpu_instrs.png` | `sim/cpu_instrs.png` |
| instr_timing | `host/instr_timing.png` | `sim/instr_timing.png` |
| Galactic Armada title | `host/armada-title.png` | `sim/armada-title.png` |
| Galactic Armada after A | `host/armada-held.png` | `sim/armada-held.png` |
| Rubik splash | `host/rubik-title.png` | `sim/rubik-title.png` |
| Rubik mode select | `host/rubik-held.png` | `sim/rubik-held.png` |

UART excerpts are under `logs/`. The homebrew pair is the check that a
button did something: the title says "PRESS A TO PLAY" and the held frame
says "THE GALACTIC EMPIRE RULE"; Rubik leaves the owl splash and shows the
cube mode menu.

## Cart RAM ceiling

Cart RAM larger than 2048 bytes is not stored. The host exits 3 with
`cart RAM N bytes exceeds 2048 byte NVS limit`. The firmware prints

```
gb_save: cart RAM 8192 exceeds 2048 byte NVS limit
```

and idles before `gb_video_init`, so the panel screenshot is black and there
is no panic. A header patched to the 2 KiB code (`0x149 = 0x01`) still
produces the pattern-ROM shade hash
`b272c10c352f8ae1793b177f5fe08eb6de29d6c77d4f251bf3b8416602642743`.
On the firmware that boot logs `gb_save loaded byte0=00 missing` and, after
the frames, `gb_save stored bytes=2048`.

No ROM in the table needs cart RAM. Games that need more than 2048 bytes
(MBC5 with an 8 KiB SRAM, for example) do not fit this image and were not
patched to fit.

A CGB-only header (`0x143 = 0xC0`) makes the host exit 3. The firmware
prints `gb_rom: CGB-only` and idles.

## What this does not claim

passportsim advances a virtual clock. `emu_us` and `present_us` on the
`gb_hash` line are durations on that clock, and `vt_us` in the simulator
status is the same kind of number. They are not measurements of a physical
ESP32-C3, and they are not a frame rate. The dmg-acid2 boot reached the hash
at guest `vt_us=450000` with the last frame reporting `emu_us=7334` and
`present_us=15548`. That only says the simulated core finished the frame.
