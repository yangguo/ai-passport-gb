#include "gb_core.h"
#include "gb_video.h"

#include "bsp_button.h"

#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/sha256.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define LCD_W 160
#define LCD_H 144
#define SHADE_BYTES (LCD_W * LCD_H)

#ifndef GB_RUN_FRAMES
#define GB_RUN_FRAMES 3
#endif

#ifndef GB_OK_JOY
#define GB_OK_JOY GB_BTN_A
#endif

#ifndef GB_INPUT_AT
#define GB_INPUT_AT 0
#endif

extern const uint8_t _binary_embedded_rom_start[] asm("_binary_embedded_rom_start");
extern const uint8_t _binary_embedded_rom_end[] asm("_binary_embedded_rom_end");

static uint8_t s_shade[SHADE_BYTES];
static int s_buttons_ready;

typedef struct {
	const uint8_t *data;
	size_t size;
} gb_rom_t;

static gb_err_t gb_rom_open_embedded(gb_rom_t *rom)
{
	if (rom == NULL)
		return GB_ERR_BAD_ARG;
	rom->data = _binary_embedded_rom_start;
	rom->size = (size_t)(_binary_embedded_rom_end - _binary_embedded_rom_start);
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

static char s_serial[128];
static size_t s_serial_len;

static void serial_flush(void)
{
	if (s_serial_len == 0)
		return;
	s_serial[s_serial_len] = '\0';
	printf("gb_serial %s\n", s_serial);
	fflush(stdout);
	s_serial_len = 0;
}

static void on_serial(void *ctx, uint8_t byte)
{
	(void)ctx;
	if (byte == '\r')
		return;
	if (byte == '\n' || byte < 32 || byte > 126) {
		serial_flush();
		if (byte != '\n')
			printf("gb_serial_byte %02x\n", byte);
		fflush(stdout);
		return;
	}
	if (s_serial_len + 1 >= sizeof(s_serial))
		serial_flush();
	s_serial[s_serial_len++] = (char)byte;
}

static void idle_forever(void)
{
	while (1)
		vTaskDelay(pdMS_TO_TICKS(1000));
}

/* NVS keys are 15 characters. `r` plus 14 hex digits of the ROM SHA-256
 * keeps two cartridges with the same RAM size from sharing a slot. */
static void rom_nvs_key(const uint8_t *rom, size_t rom_size, char key[16])
{
	mbedtls_sha256_context ctx;
	uint8_t digest[32];
	int i;

	mbedtls_sha256_init(&ctx);
	mbedtls_sha256_starts(&ctx, 0);
	mbedtls_sha256_update(&ctx, rom, rom_size);
	mbedtls_sha256_finish(&ctx, digest);
	mbedtls_sha256_free(&ctx);
	key[0] = 'r';
	for (i = 0; i < 7; i++)
		sprintf(key + 1 + 2 * i, "%02x", digest[i]);
	key[15] = '\0';
}

static void save_load(const uint8_t *rom, size_t rom_size, uint8_t *ram,
		      size_t size)
{
	nvs_handle_t handle;
	esp_err_t err;
	size_t got;
	char key[16];

	rom_nvs_key(rom, rom_size, key);
	err = nvs_flash_init();
	if (err != ESP_OK) {
		printf("gb_save: nvs init %s\n", esp_err_to_name(err));
		fflush(stdout);
		printf("gb_save loaded byte0=00 missing\n");
		fflush(stdout);
		return;
	}
	err = nvs_open("gb", NVS_READONLY, &handle);
	if (err != ESP_OK) {
		printf("gb_save loaded byte0=00 missing\n");
		fflush(stdout);
		return;
	}
	got = size;
	err = nvs_get_blob(handle, key, ram, &got);
	nvs_close(handle);
	if (err != ESP_OK || got != size) {
		memset(ram, 0, size);
		printf("gb_save loaded byte0=00 missing\n");
		fflush(stdout);
		return;
	}
	printf("gb_save loaded byte0=%02x\n", ram[0]);
	fflush(stdout);
}

static void save_store(const uint8_t *rom, size_t rom_size, const uint8_t *ram,
		       size_t size)
{
	nvs_handle_t handle;
	esp_err_t err;
	char key[16];

	rom_nvs_key(rom, rom_size, key);
	err = nvs_flash_init();
	if (err != ESP_OK) {
		printf("gb_save: nvs init %s\n", esp_err_to_name(err));
		fflush(stdout);
		return;
	}
	err = nvs_open("gb", NVS_READWRITE, &handle);
	if (err != ESP_OK) {
		printf("gb_save: nvs open %s\n", esp_err_to_name(err));
		fflush(stdout);
		return;
	}
	err = nvs_set_blob(handle, key, ram, size);
	if (err == ESP_OK)
		err = nvs_commit(handle);
	nvs_close(handle);
	if (err != ESP_OK)
		printf("gb_save: write %s\n", esp_err_to_name(err));
	else
		printf("gb_save stored bytes=%u\n", (unsigned)size);
	fflush(stdout);
}

static uint8_t poll_joy(void)
{
	int mv;

	if (!s_buttons_ready)
		return 0;
	mv = bsp_button_read_mv();
	if (mv < 0)
		return 0;
	if (mv < 150)
		return GB_BTN_UP;
	if (mv < 447)
		return GB_BTN_DOWN;
	if (mv < 1900)
		return GB_OK_JOY;
	return 0;
}

void app_main(void)
{
	gb_rom_t rom;
	gb_core_t *core;
	uint8_t *cart_ram = NULL;
	size_t ram_size = 0;
	gb_err_t err;
	gb_video_config_t video = { .scale = GB_SCALE_1X_CENTER };
	esp_err_t verr;
	gb_cpu_view_t view;

	printf("ai-passport-gb boot\n");
	fflush(stdout);

	/* I2S stays untouched: bsp_audio_init() is not called. */
	err = gb_rom_open_embedded(&rom);
	if (err != GB_OK) {
		printf("gb_rom_open_embedded err=%d\n", (int)err);
		fflush(stdout);
		idle_forever();
	}
	if (rom.size > 0x143 && rom.data[0x143] == 0xC0) {
		printf("gb_rom: CGB-only (header 0x143 = 0xC0)\n");
		fflush(stdout);
		idle_forever();
	}
	err = gb_core_save_size(rom.data, rom.size, &ram_size);
	if (err != GB_OK) {
		printf("gb_core_save_size err=%d\n", (int)err);
		fflush(stdout);
		idle_forever();
	}
	printf("gb_rom bytes=%u cart_ram=%u\n", (unsigned)rom.size,
	       (unsigned)ram_size);
	fflush(stdout);
	if (ram_size > GB_NVS_RAM_MAX) {
		printf("gb_save: cart RAM %u exceeds %u byte NVS limit\n",
		       (unsigned)ram_size, (unsigned)GB_NVS_RAM_MAX);
		fflush(stdout);
		idle_forever();
	}
	if (ram_size > 0) {
		cart_ram = heap_caps_calloc(1, ram_size, MALLOC_CAP_INTERNAL);
		if (cart_ram == NULL) {
			printf("gb_save: cart RAM alloc failed\n");
			fflush(stdout);
			idle_forever();
		}
		save_load(rom.data, rom.size, cart_ram, ram_size);
	}

	core = heap_caps_malloc(gb_core_instance_size(), MALLOC_CAP_INTERNAL);
	if (core == NULL) {
		printf("gb_core alloc failed\n");
		fflush(stdout);
		idle_forever();
	}
	err = gb_core_init(core, rom.data, rom.size, cart_ram, ram_size);
	if (err != GB_OK) {
		printf("gb_core_init err=%d\n", (int)err);
		fflush(stdout);
		idle_forever();
	}
	gb_core_set_serial_tx(core, on_serial, NULL);

	verr = gb_video_init(&video);
	if (verr != ESP_OK) {
		printf("gb_video_init %s\n", esp_err_to_name(verr));
		fflush(stdout);
		idle_forever();
	}

	verr = bsp_button_init(NULL, NULL);
	if (verr != ESP_OK) {
		printf("bsp_button_init %s\n", esp_err_to_name(verr));
		fflush(stdout);
	} else {
		s_buttons_ready = 1;
	}

	for (int frame = 1; frame <= GB_RUN_FRAMES; frame++) {
		int64_t t0, emu_us, p0, present_us;
		uint8_t line;

		if (GB_INPUT_AT != 0 && frame == GB_INPUT_AT) {
			printf("gb_input_at frame=%d\n", frame);
			fflush(stdout);
		}
		gb_core_set_pressed(core, poll_joy());
		t0 = esp_timer_get_time();
		gb_core_run_frame(core, capture_line, NULL);
		emu_us = esp_timer_get_time() - t0;

		p0 = esp_timer_get_time();
		for (line = 0; line < LCD_H; line++)
			gb_video_push_line(s_shade + (size_t)line * LCD_W, line);
		gb_video_flush();
		present_us = esp_timer_get_time() - p0;

		if (frame == GB_RUN_FRAMES) {
			char hex[65];

			hash_shade(hex);
			printf("gb_hash sha256=%s emu_us=%" PRId64
			       " present_us=%" PRId64 "\n",
			       hex, emu_us, present_us);
			fflush(stdout);
		}
	}

	serial_flush();
	if (cart_ram != NULL && ram_size > 0)
		save_store(rom.data, rom.size, cart_ram, ram_size);
	gb_core_cpu_view(core, &view);
	printf("gb_regs b=%u c=%u d=%u e=%u h=%u l=%u pc=%04x\n", view.b, view.c,
	       view.d, view.e, view.h, view.l, view.pc);
	fflush(stdout);

	idle_forever();
}
