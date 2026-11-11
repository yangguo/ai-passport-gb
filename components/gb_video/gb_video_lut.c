#include "gb_video_lut.h"

void gb_video_shade_bytes(uint8_t shade, uint8_t out[2])
{
	static const uint8_t lut[4][2] = {
		{ 0xFF, 0xFF },
		{ 0xAD, 0x55 },
		{ 0x52, 0xAA },
		{ 0x00, 0x00 },
	};

	uint8_t s = (uint8_t)(shade & 3u);
	out[0] = lut[s][0];
	out[1] = lut[s][1];
}

uint16_t gb_video_shade_rgb565(uint8_t shade)
{
	uint8_t bytes[2];

	gb_video_shade_bytes(shade, bytes);
	return (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
}
