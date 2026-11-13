# 2026-11-13 merge review

Reviewed the design, host core, ESP-IDF firmware, licensed ROM tests and
device-pack changes from PRs #1, #2, #4, #5 and #6. The work does not flash
a device or claim physical-board acceptance.

## Repairs

- Validate ROM/RAM size codes and the complete declared ROM length before
  entering Peanut-GB. Exercise both initialization APIs with invalid headers.
- Keep DMA buffers borrowed until a real completion callback arrives. A
  timeout no longer invents a completion; rejected submissions drain earlier
  accepted work before a buffer is reused.
- Reject failed simulator executions and discard earlier receipts for each
  case. Require a valid host SHA-256.
- Propagate device-pack build/copy/host errors, isolate Blargg artifacts from
  the published tree, and generate a verifiable checksum manifest.
- Include the project and third-party notices plus pinned Rubik and
  HomebrewOwl.GB source archives in the device pack. Downloads remain
  gitignored and are checked against fixed SHA-256 values.
- Correct the device download-mode instructions, document synthetic input
  on both simulator and device, and record the local BSP display changes.

## Validation

- `tools/test_host.sh`: passed, including malformed/truncated headers, LUT,
  unsupported cartridge, 2 KiB save ceiling and the unchanged pattern hash.
- `tools/test_video.sh`: five scenarios passed. AddressSanitizer and
  UndefinedBehaviorSanitizer also passed. These tests replace the hardware
  boundary and exercise the real display code, including late/missing DMA
  callbacks and a rejected transfer.
- `tools/test_pack_scripts.py`: all 14 normal and failure cases passed using
  isolated copies of the real scripts with controlled IDF/host/simulator
  boundaries. This is a shell-behaviour check, not an emulator run.
- `tools/test_third_party.sh`: dmg-acid2 had zero differing pixels; three
  Mooneye tests and both Blargg tests passed. The two documented Mooneye
  timing failures remain failures. Both homebrew input pairs changed the
  expected frames.
- ESP-IDF 5.5.3 / esp32c3 `build size merge-bin`: passed with the original
  partition table, 8 MiB image header and no I2S initialization.
- Fresh pinned passportsim (`c94b793`, runner patches from `8cdbe42`,
  Rust 1.93.0): `tools/test_sim_roms.sh` passed all 14 firmware scenarios,
  including the long Blargg CPU run, held/idle inputs and the 8 KiB refusal.
  Every requested shade hash matched its host counterpart. This does not
  turn the two known Mooneye timing failures into compatibility passes.
- Fresh `tools/build_device_pack.sh`: all five distributed ROM images passed
  their host hash, `gb_perf` and empty timing-lint checks. Blargg also passed
  from a private temporary directory and was removed. The complete manifest
  and all five per-ROM checksum lists passed `sha256sum -c`; no Blargg ROM,
  firmware or ELF remained in the published tree. The pattern and dmg-acid2
  virtual panel screenshots were also visually reviewed.
- GitHub Actions is configured for manual dispatch only; no automatic CI
  result was available for these PR heads.

## Remaining acceptance

Physical SPI stability, colours, input and performance are still untested.
Continuous-run device images currently have no manual save entry point.
The pattern ROM's `AIP0` tile data uses the wrong bit-plane ordering, so its
letter shapes are distorted; the four grey shades and checkerboard remain
useful, and the golden hash is retained for this merge.
