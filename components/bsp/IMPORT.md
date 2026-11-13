# BSP import

- Upstream: https://github.com/FoloToy/ai-passport
- Pinned commit: `33d3d1d93a1125b356b47b6d83a7a60121be801e`
- This tree is that commit's `components/bsp/` (headers, sources, and the
  original CMake/component manifest before the build edits below). Pin
  headers are unchanged.
- License: MIT, copyright 2026 FoloToy. Upstream keeps the text at the
  repository root, not inside `components/bsp/`. `LICENSE` here is that
  file, copied verbatim.
- Not imported: upstream `main/` (LVGL demo), assets, docs, CI.
- Anki snapshot `09847914533da79e05325365eff65c700c4633ec` was read and
  not used as the file source. Relative to this upstream tree it adds
  deep-sleep support that this import does not carry:
  - `src/bsp_power.c`
  - `include/bsp_power.h`
  - `BSP_BTN_GPIO` in `include/bsp_pins.h`
  Those files are listed in anki `components/bsp/IMPORT.md`. They are
  left out on purpose; P1 does not enter deep sleep.
- Build edits, so the app does not download or link LVGL:
  - `src/bsp_display_lvgl.c` and `src/bsp_display_rounding.c` stay in the
    tree and are omitted from `CMakeLists.txt` `SRCS`. Upstream's own
    comment in `bsp_display_lvgl.c` says a non-LVGL app drops that file
    and the two component-manager dependencies. `idf_component.yml`
    therefore does not list `lvgl/lvgl` or `espressif/esp_lvgl_port`.
  - `REQUIRES` drops `esp_lvgl_port` for the same reason.
- Display runtime edit: `src/bsp_display.c` replaces the driver's default
  ST7789 initialization with the same MADCTL/COLMOD setup and an explicit
  SLPOUT delay of 120 ms before DISPON. The display source therefore no
  longer matches upstream byte for byte.
- Audio sources are still compiled. Nothing in this firmware calls
  `bsp_audio_init()`, so I2S is not initialized.
