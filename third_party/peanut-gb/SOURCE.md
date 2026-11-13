# Peanut-GB source record

- Upstream: https://github.com/deltabeard/Peanut-GB
- Commit: `d0bcca771c83a2638c93a9ae61f3d51f226dc905`
- Commit date: 2026-09-21
- Vendored file: `peanut_gb.h`
- Local change: one optional `PEANUT_GB_OPCODE_HOOK` after the opcode fetch.
  The firmware build does not define it. With the macro unset, the CPU path
  matches this commit. The host defines it to read registers at opcode `0x40`
  (`ld b, b`), the Mooneye breakpoint.
- SameBoy fragment copyright remains in the header file

Host `sizeof(struct gb_s)` (Linux x86_64, P0 macros): **16984** bytes (`gb-host` prints `sizeof_gb_s`).
