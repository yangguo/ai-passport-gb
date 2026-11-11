#include <stdio.h>
#include <stdlib.h>

#include "gb_video_lut.h"

static void expect_bytes(uint8_t shade, uint8_t hi, uint8_t lo)
{
	uint8_t out[2];

	gb_video_shade_bytes(shade, out);
	if (out[0] != hi || out[1] != lo) {
		fprintf(stderr, "shade %u: got %02X %02X want %02X %02X\n", shade,
			out[0], out[1], hi, lo);
		exit(1);
	}
}

int main(void)
{
	expect_bytes(0, 0xFF, 0xFF);
	expect_bytes(1, 0xAD, 0x55);
	expect_bytes(2, 0x52, 0xAA);
	expect_bytes(3, 0x00, 0x00);
	return 0;
}
