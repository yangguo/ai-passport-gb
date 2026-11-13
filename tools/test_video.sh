#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
VIDEO_TMP=$(mktemp -d "${TMPDIR:-/tmp}/gb-video-test.XXXXXX")
trap 'rm -rf "$VIDEO_TMP"' EXIT HUP INT TERM
mkdir -p "$VIDEO_TMP/freertos"
# IDF headers are replaced at the hardware boundary; gb_video.c is unchanged.
for header in bsp_display.h bsp_pins.h esp_err.h esp_heap_caps.h \
    esp_lcd_panel_io.h esp_lcd_panel_ops.h freertos/FreeRTOS.h freertos/semphr.h; do
    : > "$VIDEO_TMP/$header"
done
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror ${CFLAGS:-} \
    -I "$VIDEO_TMP" -I "$ROOT/components/gb_video/include" -I "$ROOT/tools" \
    -include "$ROOT/tools/test_video_stubs.h" \
    "$ROOT/tools/test_video.c" "$ROOT/components/gb_video/gb_video.c" \
    "$ROOT/components/gb_video/gb_video_lut.c" -o "$VIDEO_TMP/test-video"
if [ "$#" -eq 0 ]; then
    set -- normal init-timeout row-timeout rejected-draw missing-callback
fi
for scenario do
    "$VIDEO_TMP/test-video" "$scenario"
done
