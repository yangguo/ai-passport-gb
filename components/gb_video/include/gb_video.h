#ifndef GB_VIDEO_H
#define GB_VIDEO_H

#include "esp_err.h"
#include <stdint.h>

typedef enum {
	GB_SCALE_1X_CENTER = 0,
	GB_SCALE_3_2 = 1
} gb_scale_t;

typedef struct {
	gb_scale_t scale;
} gb_video_config_t;

/* Calls bsp_display_init, backlight 50, paints the 240x320 border black,
 * then four 8x8 gray swatches on the left margin. GB_SCALE_3_2 is refused. */
esp_err_t gb_video_init(const gb_video_config_t *cfg);

/* One DMG scanline. pixels160 is 160 bytes; shade is the low 2 bits.
 * line >= 144 is ignored. The panel window is x [40,200), y [88+line, 89+line). */
void gb_video_push_line(const uint8_t *pixels160, uint8_t line);

/* Block until queued panel transfers have finished, so a following
 * screenshot sees this frame. */
void gb_video_flush(void);

#endif
