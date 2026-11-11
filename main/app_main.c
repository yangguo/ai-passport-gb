#include "gb_core.h"
#include "gb_video.h"

#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/sha256.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define LCD_W 160
#define LCD_H 144
#define SHADE_BYTES (LCD_W * LCD_H)

extern const uint8_t _binary_pattern_gb_start[] asm("_binary_pattern_gb_start");
extern const uint8_t _binary_pattern_gb_end[] asm("_binary_pattern_gb_end");

static uint8_t s_shade[SHADE_BYTES];

typedef struct {
	const uint8_t *data;
	size_t size;
} gb_rom_t;

static gb_err_t gb_rom_open_embedded(gb_rom_t *rom)
{
	if (rom == NULL)
		return GB_ERR_BAD_ARG;
	rom->data = _binary_pattern_gb_start;
	rom->size = (size_t)(_binary_pattern_gb_end - _binary_pattern_gb_start);
	return GB_OK;
}

static void capture_line(void *ctx, const uint8_t *pixels160, uint8_t line)
{
	(void)ctx;
	if (line >= LCD_H)
		return;
	uint8_t *row = s_shade + (size_t)line * LCD_W;
	for (int x = 0; x < LCD_W; x++)
		row[x] = (uint8_t)(pixels160[x] & 0x03u);
}

static void hash_shade(char hex[65])
{
	mbedtls_sha256_context ctx;
	uint8_t digest[32];

	mbedtls_sha256_init(&ctx);
	mbedtls_sha256_starts(&ctx, 0);
	mbedtls_sha256_update(&ctx, s_shade, SHADE_BYTES);
	mbedtls_sha256_finish(&ctx, digest);
	mbedtls_sha256_free(&ctx);
	for (int i = 0; i < 32; i++)
		sprintf(hex + i * 2, "%02x", digest[i]);
	hex[64] = '\0';
}

static void idle_forever(void)
{
	while (1)
		vTaskDelay(pdMS_TO_TICKS(1000));
}

void app_main(void)
{
	gb_rom_t rom;
	gb_core_t *core;
	size_t ram_size = 0;
	gb_err_t err;
	gb_video_config_t video = { .scale = GB_SCALE_1X_CENTER };
	esp_err_t verr;

	printf("ai-passport-gb boot\n");
	fflush(stdout);

	/* I2S stays untouched: bsp_audio_init() is not called. */
	err = gb_rom_open_embedded(&rom);
	if (err != GB_OK) {
		printf("gb_rom_open_embedded err=%d\n", (int)err);
		fflush(stdout);
		idle_forever();
	}
	err = gb_core_save_size(rom.data, rom.size, &ram_size);
	if (err != GB_OK || ram_size != 0) {
		printf("gb_core_save_size err=%d ram=%u\n", (int)err,
		       (unsigned)ram_size);
		fflush(stdout);
		idle_forever();
	}

	core = heap_caps_malloc(gb_core_instance_size(), MALLOC_CAP_INTERNAL);
	if (core == NULL) {
		printf("gb_core alloc failed\n");
		fflush(stdout);
		idle_forever();
	}
	err = gb_core_init(core, rom.data, rom.size, NULL, 0);
	if (err != GB_OK) {
		printf("gb_core_init err=%d\n", (int)err);
		fflush(stdout);
		idle_forever();
	}

	verr = gb_video_init(&video);
	if (verr != ESP_OK) {
		printf("gb_video_init %s\n", esp_err_to_name(verr));
		fflush(stdout);
		idle_forever();
	}

	for (int frame = 1; frame <= 3; frame++) {
		int64_t t0 = esp_timer_get_time();
		gb_core_run_frame(core, capture_line, NULL);
		int64_t emu_us = esp_timer_get_time() - t0;

		int64_t p0 = esp_timer_get_time();
		for (uint8_t line = 0; line < LCD_H; line++)
			gb_video_push_line(s_shade + (size_t)line * LCD_W, line);
		gb_video_flush();
		int64_t present_us = esp_timer_get_time() - p0;

		if (frame == 3) {
			char hex[65];

			hash_shade(hex);
			printf("gb_hash sha256=%s emu_us=%" PRId64 " present_us=%" PRId64
			       "\n",
			       hex, emu_us, present_us);
			fflush(stdout);
		}
	}

	idle_forever();
}
