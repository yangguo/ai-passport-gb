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

static uint8_t shade_at(const uint8_t *shade, unsigned x, unsigned y)
{
	return shade[y * LCD_W + x];
}

static int check_pixel_contract(const uint8_t *shade)
{
	const struct {
		unsigned x, y;
		uint8_t want;
	} pts[] = {
		{ 16, 32, 3 },
		{ 24, 32, 2 },
		{ 32, 32, 1 },
		{ 40, 32, 0 },
		{ 0, 0, 3 },
		{ 8, 0, 0 },
		{ 59, 64, 3 },
		{ 159, 143, 0 },
	};

	for (size_t i = 0; i < sizeof(pts) / sizeof(pts[0]); ++i) {
		uint8_t got = shade_at(shade, pts[i].x, pts[i].y);

		if (got != pts[i].want) {
			fprintf(stderr,
				"pixel contract fail: (%u,%u)=%u want %u\n",
				pts[i].x, pts[i].y, got, pts[i].want);
			return 1;
		}
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

enum { HOLD_MAX = 16, SERIAL_MAX = 1024 * 1024 };

typedef struct {
	unsigned start;
	unsigned end;
	uint8_t mask;
} hold_t;

typedef struct {
	unsigned frames;
	const char *png_path;
	const char *shade_path;
	const char *check_path;
	const char *serial_path;
	int pattern_contract;
	int judge_mooneye;
	int judge_blargg;
	int stop_on_result;
	hold_t holds[HOLD_MAX];
	int nholds;
} run_opts_t;

typedef struct {
	char *data;
	size_t len;
	size_t cap;
} serial_buf_t;

static void serial_push(void *ctx, uint8_t byte)
{
	serial_buf_t *buf = ctx;

	if (buf->len + 1 >= buf->cap) {
		size_t next = buf->cap == 0 ? 4096 : buf->cap * 2;
		char *grown;

		if (next > SERIAL_MAX)
			next = SERIAL_MAX;
		if (buf->len + 1 >= next)
			return;
		grown = realloc(buf->data, next);
		if (grown == NULL)
			return;
		buf->data = grown;
		buf->cap = next;
	}
	buf->data[buf->len++] = (char)byte;
	buf->data[buf->len] = '\0';
}

static int mooneye_fibonacci(const gb_cpu_view_t *view)
{
	return view->b == 3 && view->c == 5 && view->d == 8 && view->e == 13 &&
	       view->h == 21 && view->l == 34;
}

static int mooneye_fail_sig(const gb_cpu_view_t *view)
{
	return view->b == 0x42 && view->c == 0x42 && view->d == 0x42 &&
	       view->e == 0x42 && view->h == 0x42 && view->l == 0x42;
}

static int blargg_status(const char *text)
{
	if (text != NULL && strstr(text, "Failed") != NULL)
		return 2;
	if (text != NULL && strstr(text, "Passed") != NULL)
		return 0;
	return 4;
}

static int run_rom(const uint8_t *rom, size_t rom_size, const run_opts_t *opt)
{
	size_t save_size = 0;
	unsigned frames = opt->frames;
	gb_err_t err = gb_core_save_size(rom, rom_size, &save_size);
	uint8_t *cart_ram = NULL;
	serial_buf_t serial = { 0 };
	int judge = 0;

	if (rom_size > 0x143 && rom[0x143] == 0xC0) {
		fprintf(stderr,
			"CGB-only ROM (header 0x143 = 0xC0) is not a DMG image\n");
		return 3;
	}
	if (err != GB_OK) {
		fprintf(stderr, "gb_core_save_size: %d\n", (int)err);
		return 1;
	}
	if (save_size > GB_NVS_RAM_MAX) {
		fprintf(stderr,
			"cart RAM %zu bytes exceeds %u byte NVS limit\n",
			save_size, (unsigned)GB_NVS_RAM_MAX);
		return 3;
	}
	if (save_size > 0) {
		cart_ram = calloc(1, save_size);
		if (cart_ram == NULL)
			return 1;
	}

	void *core_mem = calloc(1, gb_core_instance_size());
	if (core_mem == NULL) {
		free(cart_ram);
		return 1;
	}
	gb_core_t *core = core_mem;
	err = gb_core_init(core, rom, rom_size, cart_ram, save_size);
	if (err != GB_OK) {
		fprintf(stderr, "gb_core_init: %d\n", (int)err);
		free(cart_ram);
		free(core_mem);
		return 1;
	}
	gb_core_set_serial_tx(core, serial_push, &serial);

	capture_t cap;
	memset(&cap, 0, sizeof(cap));

	char hash_prev[65] = "";
	char hash_last[65] = "";
	unsigned ran = 0;
	int stopped = 0;

	for (unsigned f = 0; f < frames; ++f) {
		uint8_t mask = 0;
		unsigned frame_no = f + 1;

		for (int h = 0; h < opt->nholds; ++h) {
			if (frame_no >= opt->holds[h].start &&
			    frame_no <= opt->holds[h].end)
				mask = (uint8_t)(mask | opt->holds[h].mask);
		}
		gb_core_set_pressed(core, mask);

		memset(&cap.shade, 0, sizeof(cap.shade));
		memset(&cap.rgb565, 0, sizeof(cap.rgb565));
		g_lines_drawn = 0;
		gb_core_run_frame(core, capture_line, &cap);
		ran = frame_no;

		char hex[65];
		hash_shade_buffer(cap.shade, hex);
		memcpy(hash_last, hex, sizeof(hash_last));

		if (opt->pattern_contract && f == 1 && g_lines_drawn >= LCD_H) {
			memcpy(hash_prev, hex, sizeof(hash_prev));
			if (check_pixel_contract(cap.shade) != 0) {
				free(serial.data);
				free(cart_ram);
				free(core_mem);
				return 1;
			}
		}
		if (opt->pattern_contract && f == 2) {
			if (g_lines_drawn < LCD_H) {
				fprintf(stderr, "frame 3: only %u lcd lines\n",
					g_lines_drawn);
				free(serial.data);
				free(cart_ram);
				free(core_mem);
				return 1;
			}
			if (check_pixel_contract(cap.shade) != 0) {
				free(serial.data);
				free(cart_ram);
				free(core_mem);
				return 1;
			}
			if (hash_prev[0] != '\0' &&
			    memcmp(hash_prev, hash_last, 64) != 0) {
				fprintf(stderr, "frame 2/3 hash mismatch\n");
				free(serial.data);
				free(cart_ram);
				free(core_mem);
				return 1;
			}
		}

		if (opt->stop_on_result) {
			gb_cpu_view_t view;
			gb_core_cpu_view(core, &view);
			if (opt->judge_mooneye && view.ld_b_b)
				stopped = 1;
			if (opt->judge_blargg && serial.data != NULL &&
			    (strstr(serial.data, "Passed") != NULL ||
			     strstr(serial.data, "Failed") != NULL))
				stopped = 1;
		}
		if (stopped || frame_no == frames) {
			if (opt->png_path != NULL &&
			    rgb565_to_png(opt->png_path, cap.rgb565) != 0) {
				fprintf(stderr, "png write failed\n");
				free(serial.data);
				free(cart_ram);
				free(core_mem);
				return 1;
			}
			if (opt->shade_path != NULL) {
				FILE *sf = fopen(opt->shade_path, "wb");
				if (sf == NULL ||
				    fwrite(cap.shade, 1, SHADE_BYTES, sf) !=
					    SHADE_BYTES) {
					if (sf != NULL)
						fclose(sf);
					fprintf(stderr, "shade write failed\n");
					free(serial.data);
					free(cart_ram);
					free(core_mem);
					return 1;
				}
				fclose(sf);
			}
		}
		if (stopped)
			break;
	}

	gb_cpu_view_t view;
	gb_core_cpu_view(core, &view);

	printf("sha256 %s\n", hash_last);
	printf("sizeof_gb_s %zu\n", gb_core_gb_struct_size());
	printf("frames %u\n", ran);
	printf("cart_ram %zu\n", save_size);
	printf("mbc_header %02x\n", rom_size > 0x147 ? rom[0x147] : 0);
	printf("cgb_flag %02x\n", rom_size > 0x143 ? rom[0x143] : 0);
	printf("regs b=%u c=%u d=%u e=%u h=%u l=%u pc=%04x ld_b_b=%u count=%u\n",
	       view.b, view.c, view.d, view.e, view.h, view.l, view.pc,
	       view.ld_b_b ? 1u : 0u, view.ld_b_b_count);

	if (opt->serial_path != NULL) {
		FILE *sf = fopen(opt->serial_path, "wb");
		if (sf == NULL) {
			free(serial.data);
			free(cart_ram);
			free(core_mem);
			return 1;
		}
		if (serial.len > 0 &&
		    fwrite(serial.data, 1, serial.len, sf) != serial.len) {
			fclose(sf);
			free(serial.data);
			free(cart_ram);
			free(core_mem);
			return 1;
		}
		fclose(sf);
	}

	if (opt->check_path != NULL) {
		char expect[65];
		if (load_expect_file(opt->check_path, expect) != 0) {
			free(serial.data);
			free(cart_ram);
			free(core_mem);
			return 1;
		}
		if (memcmp(expect, hash_last, 64) != 0) {
			fprintf(stderr, "hash mismatch\nexpected %s\ngot      %s\n",
				expect, hash_last);
			free(serial.data);
			free(cart_ram);
			free(core_mem);
			return 1;
		}
	}

	if (opt->judge_mooneye) {
		if (!view.ld_b_b) {
			printf("mooneye result=incomplete\n");
			judge = 4;
		} else if (mooneye_fibonacci(&view)) {
			printf("mooneye result=pass\n");
			judge = 0;
		} else if (mooneye_fail_sig(&view)) {
			printf("mooneye result=fail\n");
			judge = 2;
		} else {
			printf("mooneye result=fail\n");
			judge = 2;
		}
	}
	if (opt->judge_blargg) {
		int st = blargg_status(serial.data);
		const char *name = st == 0 ? "pass" : st == 2 ? "fail" : "incomplete";
		printf("blargg result=%s\n", name);
		judge = st;
	}

	free(serial.data);
	free(cart_ram);
	free(core_mem);
	return judge;
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

static int parse_hold(const char *spec, hold_t *out)
{
	unsigned start = 0, end = 0, mask = 0;

	if (sscanf(spec, "%u:%u:%x", &start, &end, &mask) != 3 || start == 0 ||
	    end < start || mask > 0xFF)
		return -1;
	out->start = start;
	out->end = end;
	out->mask = (uint8_t)mask;
	return 0;
}

static int cmd_run(int argc, char **argv)
{
	run_opts_t opt;
	const char *rom_path = NULL;

	memset(&opt, 0, sizeof(opt));
	opt.frames = 3;

	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--rom") == 0 && i + 1 < argc)
			rom_path = argv[++i];
		else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc)
			opt.frames = (unsigned)atoi(argv[++i]);
		else if (strcmp(argv[i], "--check") == 0 && i + 1 < argc)
			opt.check_path = argv[++i];
		else if (strcmp(argv[i], "--png") == 0 && i + 1 < argc)
			opt.png_path = argv[++i];
		else if (strcmp(argv[i], "--shade") == 0 && i + 1 < argc)
			opt.shade_path = argv[++i];
		else if (strcmp(argv[i], "--serial-file") == 0 && i + 1 < argc)
			opt.serial_path = argv[++i];
		else if (strcmp(argv[i], "--pattern-contract") == 0)
			opt.pattern_contract = 1;
		else if (strcmp(argv[i], "--stop-on-result") == 0)
			opt.stop_on_result = 1;
		else if (strcmp(argv[i], "--judge") == 0 && i + 1 < argc) {
			const char *which = argv[++i];
			if (strcmp(which, "mooneye") == 0)
				opt.judge_mooneye = 1;
			else if (strcmp(which, "blargg") == 0)
				opt.judge_blargg = 1;
			else {
				fprintf(stderr, "unknown judge: %s\n", which);
				return 1;
			}
		} else if (strcmp(argv[i], "--hold") == 0 && i + 1 < argc) {
			if (opt.nholds >= HOLD_MAX ||
			    parse_hold(argv[++i], &opt.holds[opt.nholds]) != 0) {
				fprintf(stderr, "bad --hold START:END:MASK\n");
				return 1;
			}
			opt.nholds++;
		} else {
			fprintf(stderr, "unknown arg: %s\n", argv[i]);
			return 1;
		}
	}

	if (rom_path == NULL || opt.frames < 1) {
		fprintf(stderr,
			"usage: gb-host --rom PATH --frames N [--check FILE] [--png PATH]\n"
			"       [--shade PATH] [--serial-file PATH] [--pattern-contract]\n"
			"       [--judge mooneye|blargg] [--stop-on-result]\n"
			"       [--hold START:END:HEXMASK]\n");
		return 1;
	}
	if (opt.pattern_contract && opt.frames < 3) {
		fprintf(stderr, "--pattern-contract needs at least 3 frames\n");
		return 1;
	}

	uint8_t *rom = NULL;
	size_t rom_size = 0;
	if (read_file(rom_path, &rom, &rom_size) != 0) {
		fprintf(stderr, "read rom: %s\n", strerror(errno));
		return 1;
	}

	int rc = run_rom(rom, rom_size, &opt);
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
