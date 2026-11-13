# Blargg ROMs are fetched, not committed

`cpu_instrs.gb` and `instr_timing.gb` come from
[retrio/gb-test-roms](https://github.com/retrio/gb-test-roms) commit
`c240dd7d700e5c0b00a7bbba52b53e4ee67b5f15`. That tree's readme names Shay
Green as the author and does not grant a license, so the binaries are
gitignored.

`tools/fetch_test_roms.sh` downloads those two files from the pinned commit
and checks SHA-256. Do not commit them.
