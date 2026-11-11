#ifndef GB_VIDEO_LUT_H
#define GB_VIDEO_LUT_H

#include <stdint.h>

/* Returns two bytes in panel wire order (high byte first), per design §9.3. */
void gb_video_shade_bytes(uint8_t shade, uint8_t out[2]);

/* RGB565 value derived from the same LUT (high byte in bits 15..8). */
uint16_t gb_video_shade_rgb565(uint8_t shade);

#endif
