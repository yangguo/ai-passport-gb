# P1 build configuration

This file records the ESP-IDF configuration used to build the P1 image.
It is not a device measurement. Cells that need the board stay `待测`.

ESP-IDF v5.5.3, `idf.py -B idf-build set-target esp32c3`.
`CONFIG_COMPILER_OPTIMIZATION_DEBUG=y` (the IDF default, `-Og`). The
optimization level was not changed.

| Symbol | Value in `sdkconfig` | Kind |
| --- | --- | --- |
| `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ` | 160 | Build configuration, not a runtime measurement |
| `CONFIG_BT_ENABLED` | not set | Build configuration |
| `CONFIG_SPIRAM` | not set (PSRAM menu empty) | Build configuration |
| `CONFIG_ESP_WIFI_ENABLED` | `y` | IDF 5.5.3 gives this symbol no prompt and defaults it on for esp32c3. `sdkconfig.defaults` cannot clear it. `esp_wifi_init` is not in the link map |

Factory partition `0x10000/0x7f0000` (8323072 bytes), unchanged from
FoloToy `33d3d1d`.

`idf.py build` reported:

```text
ai-passport-gb.bin binary size 0x46780 bytes. Smallest app partition is 0x7f0000 bytes. 0x7a9880 bytes (97%) free.
```

That is 288640 bytes used and 8034432 bytes free.

| Quantity | Device | passportsim |
| --- | --- | --- |
| `gb_hash` of frame 3 | 待测 | matches `roms/homebrew/pattern.sha256` |
| `emu_us` / `present_us` | 待测 | guest virtual time only; not device performance |
| SPI timing, flicker, color order | 待测 | panel model accepted the bitmap; real glass still open |
