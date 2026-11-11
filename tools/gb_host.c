#include "gb_core.h"
#include "gb_video_lut.h"
#include "sha256.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LCD_W 160
#define LCD_H 144
#define SHADE_BYTES (LCD_W * LCD_H)
#define RGB565_BYTES (SHADE_BYTES * 2)

enum { ROM_HEADER_CHECKSUM_LOC = 0x014D, ROM_CART_TYPE_LOC = 0x0147 };

typedef struct {
	uint8_t shade[SHADE_BYTES];
	uint8_t rgb565[RGB565_BYTES];
	uint32_t frame_index;
} capture_t;

static int load_expect_file(const char *path, char hex_out[65]);

static unsigned g_lines_drawn;

static void capture_line(void *ctx, const uint8_t *pixels160, uint8_t line)
{
	capture_t *cap = ctx;
	if (line >= LCD_H)
		return;
	g_lines_drawn++;

	for (int x = 0; x < LCD_W; ++x) {
		uint8_t s = (uint8_t)(pixels160[x] & 0x03u);
		size_t idx = (size_t)line * LCD_W + (size_t)x;
		cap->shade[idx] = s;
		uint8_t pair[2];
		gb_video_shade_bytes(s, pair);
		cap->rgb565[idx * 2] = pair[0];
		cap->rgb565[idx * 2 + 1] = pair[1];
	}
}

static int read_file(const char *path, uint8_t **out, size_t *out_len)
{
	FILE *f = fopen(path, "rb");
	if (f == NULL)
		return -1;
	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return -1;
	}
	long sz = ftell(f);
	if (sz < 0) {
		fclose(f);
		return -1;
	}
	rewind(f);
	uint8_t *buf = malloc((size_t)sz);
	if (buf == NULL) {
		fclose(f);
		return -1;
	}
	if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
		free(buf);
		fclose(f);
		return -1;
	}
	fclose(f);
	*out = buf;
	*out_len = (size_t)sz;
	return 0;
}

static void hash_shade_buffer(const uint8_t *shade, char hex_out[65])
{
	sha256_ctx_t ctx;
	uint8_t digest[SHA256_DIGEST_LENGTH];

	sha256_init(&ctx);
	sha256_update(&ctx, shade, SHADE_BYTES);
	sha256_final(&ctx, digest);
	sha256_hex(digest, hex_out);
}

static int check_pixel_contract(const uint8_t *shade)
{
	uint8_t a = shade[0];
	uint8_t b = shade[8];
	uint8_t c = shade[143 * LCD_W + 159];

	if (a != 3 || b != 0 || c != 0) {
		fprintf(stderr,
			"pixel contract fail: (0,0)=%u (8,0)=%u (159,143)=%u\n", a,
			b, c);
		return 1;
	}
	return 0;
}

static int rgb565_to_png(const char *path, const uint8_t *rgb565)
{
	uint8_t *rgb = malloc(SHADE_BYTES * 3);
	if (rgb == NULL)
		return 1;

	for (size_t i = 0; i < SHADE_BYTES; ++i) {
		uint16_t v = (uint16_t)(((uint16_t)rgb565[i * 2] << 8) |
					rgb565[i * 2 + 1]);
		uint8_t r = (uint8_t)(((v >> 11) & 0x1F) * 255 / 31);
		uint8_t g = (uint8_t)(((v >> 5) & 0x3F) * 255 / 63);
		uint8_t b = (uint8_t)((v & 0x1F) * 255 / 31);
		rgb[i * 3] = r;
		rgb[i * 3 + 1] = g;
		rgb[i * 3 + 2] = b;
	}

	int ok = stbi_write_png(path, LCD_W, LCD_H, 3, rgb, LCD_W * 3);
	free(rgb);
	return ok ? 0 : 1;
}

static int run_rom(const uint8_t *rom, size_t rom_size, unsigned frames,
		   const char *png_path, const char *check_path)
{
	size_t save_size = 0;
	gb_err_t err = gb_core_save_size(rom, rom_size, &save_size);
	if (err != GB_OK) {
		fprintf(stderr, "gb_core_save_size: %d\n", (int)err);
		return 1;
	}
	if (save_size != 0) {
		fprintf(stderr, "expected cart RAM size 0, got %zu\n", save_size);
		return 1;
	}

	void *core_mem = calloc(1, gb_core_instance_size());
	if (core_mem == NULL)
		return 1;
	gb_core_t *core = core_mem;
	err = gb_core_init(core, rom, rom_size, NULL, 0);
	if (err != GB_OK) {
		fprintf(stderr, "gb_core_init: %d\n", (int)err);
		free(core_mem);
		return 1;
	}

	capture_t cap;
	memset(&cap, 0, sizeof(cap));

	char hash_prev[65] = "";
	char hash_last[65] = "";
	unsigned lines_drawn = 0;

	for (unsigned f = 0; f < frames; ++f) {
		memset(&cap.shade, 0, sizeof(cap.shade));
		memset(&cap.rgb565, 0, sizeof(cap.rgb565));
		g_lines_drawn = 0;
		gb_core_run_frame(core, capture_line, &cap);
		lines_drawn = g_lines_drawn;

		char hex[65];
		hash_shade_buffer(cap.shade, hex);

		if (f == 1 && lines_drawn >= LCD_H) {
			memcpy(hash_prev, hex, sizeof(hash_prev));
			if (check_pixel_contract(cap.shade) != 0) {
				free(core_mem);
				return 1;
			}
		}
		if (f == 2) {
			memcpy(hash_last, hex, sizeof(hash_last));
			if (lines_drawn < LCD_H) {
				fprintf(stderr, "frame 3: only %u lcd lines\n",
					lines_drawn);
				free(core_mem);
				return 1;
			}
			if (check_pixel_contract(cap.shade) != 0) {
				free(core_mem);
				return 1;
			}
			if (hash_prev[0] != '\0' &&
			    memcmp(hash_prev, hash_last, 64) != 0) {
				fprintf(stderr, "frame 2/3 hash mismatch\n");
				free(core_mem);
				return 1;
			}
			if (png_path != NULL &&
			    rgb565_to_png(png_path, cap.rgb565) != 0) {
				fprintf(stderr, "png write failed\n");
				free(core_mem);
				return 1;
			}
		}
	}

	printf("sha256 %s\n", hash_last);
	printf("sizeof_gb_s %zu\n", gb_core_gb_struct_size());

	if (check_path != NULL) {
		char expect[65];
		if (load_expect_file(check_path, expect) != 0) {
			free(core_mem);
			return 1;
		}
		if (memcmp(expect, hash_last, 64) != 0) {
			fprintf(stderr, "hash mismatch\nexpected %s\ngot      %s\n",
				expect, hash_last);
			free(core_mem);
			return 1;
		}
	}

	free(core_mem);
	return 0;
}

static int load_expect_file(const char *path, char hex_out[65])
{
	FILE *f = fopen(path, "r");
	if (f == NULL)
		return 1;
	if (fscanf(f, "%64s", hex_out) != 1) {
		fclose(f);
		return 1;
	}
	fclose(f);
	return 0;
}

static int cmd_run(int argc, char **argv)
{
	const char *rom_path = NULL;
	const char *check_path = NULL;
	const char *png_path = NULL;
	unsigned frames = 3;

	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--rom") == 0 && i + 1 < argc)
			rom_path = argv[++i];
		else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc)
			frames = (unsigned)atoi(argv[++i]);
		else if (strcmp(argv[i], "--check") == 0 && i + 1 < argc)
			check_path = argv[++i];
		else if (strcmp(argv[i], "--png") == 0 && i + 1 < argc)
			png_path = argv[++i];
		else {
			fprintf(stderr, "unknown arg: %s\n", argv[i]);
			return 1;
		}
	}

	if (rom_path == NULL || frames < 3) {
		fprintf(stderr, "usage: gb-host --rom PATH --frames N [--check FILE] [--png PATH]\n");
		return 1;
	}

	uint8_t *rom = NULL;
	size_t rom_size = 0;
	if (read_file(rom_path, &rom, &rom_size) != 0) {
		fprintf(stderr, "read rom: %s\n", strerror(errno));
		return 1;
	}

	int rc = run_rom(rom, rom_size, frames, png_path, check_path);
	free(rom);
	return rc;
}

static int cmd_expect_bad(const char *rom_path)
{
	uint8_t *rom = NULL;
	size_t rom_size = 0;
	if (read_file(rom_path, &rom, &rom_size) != 0)
		return 1;
	if (rom_size <= ROM_HEADER_CHECKSUM_LOC) {
		free(rom);
		return 1;
	}

	rom[ROM_HEADER_CHECKSUM_LOC] = (uint8_t)(rom[ROM_HEADER_CHECKSUM_LOC] + 1u);

	size_t save = 0;
	gb_err_t err = gb_core_save_size(rom, rom_size, &save);
	if (err != GB_ERR_CHECKSUM) {
		fprintf(stderr, "expected GB_ERR_CHECKSUM from save_size, got %d\n",
			(int)err);
		free(rom);
		return 1;
	}

	void *core_mem = calloc(1, gb_core_instance_size());
	gb_core_t *core = core_mem;
	err = gb_core_init(core, rom, rom_size, NULL, 0);
	free(core_mem);
	if (err != GB_ERR_CHECKSUM) {
		fprintf(stderr, "expected GB_ERR_CHECKSUM from init, got %d\n",
			(int)err);
		free(rom);
		return 1;
	}

	free(rom);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr,
			"usage: gb-host --rom PATH --frames N | --expect-bad PATH\n");
		return 1;
	}

	if (strcmp(argv[1], "--expect-bad") == 0) {
		if (argc != 3)
			return 1;
		return cmd_expect_bad(argv[2]);
	}

	if (strcmp(argv[1], "--rom") != 0) {
		fprintf(stderr, "first option must be --rom or --expect-bad\n");
		return 1;
	}

	return cmd_run(argc, argv);
}
