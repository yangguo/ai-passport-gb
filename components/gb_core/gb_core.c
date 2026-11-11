#include "gb_core.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ESP_PLATFORM)
#include "esp_system.h"
#endif

#define ENABLE_SOUND 0
#define ENABLE_LCD 1
#define PEANUT_GB_12_COLOUR 1
#define PEANUT_GB_HIGH_LCD_ACCURACY 1
#define PEANUT_GB_USE_INTRINSICS 1

#ifdef GB_CORE_OPCODE_HOOK
struct gb_s;
static void gb_on_opcode(struct gb_s *gb, uint8_t opcode);
#define PEANUT_GB_OPCODE_HOOK gb_on_opcode
#endif

#include "peanut_gb.h"

typedef struct {
	const uint8_t *rom;
	size_t rom_size;
	uint8_t *cart_ram;
	size_t cart_ram_size;
} gb_rom_ctx_t;

struct gb_core {
	struct gb_s gb;
	gb_rom_ctx_t rom_ctx;
	gb_line_fn draw;
	void *draw_ctx;
	gb_serial_tx_fn serial_fn;
	void *serial_ctx;
	uint8_t break_b, break_c, break_d, break_e, break_h, break_l;
	uint16_t break_pc;
	uint32_t ld_b_b_count;
	bool saw_break;
};

static gb_rom_ctx_t *rom_ctx(struct gb_s *gb)
{
	return (gb_rom_ctx_t *)gb->direct.priv;
}

static uint8_t rom_read(struct gb_s *gb, const uint_fast32_t addr)
{
	gb_rom_ctx_t *ctx = rom_ctx(gb);
	if (addr >= ctx->rom_size)
		return 0xFF;
	return ctx->rom[addr];
}

static uint8_t cart_ram_read(struct gb_s *gb, const uint_fast32_t addr)
{
	gb_rom_ctx_t *ctx = rom_ctx(gb);
	if (ctx->cart_ram_size == 0 || addr >= ctx->cart_ram_size)
		return 0xFF;
	return ctx->cart_ram[addr];
}

static void cart_ram_write(struct gb_s *gb, const uint_fast32_t addr,
			   const uint8_t val)
{
	gb_rom_ctx_t *ctx = rom_ctx(gb);
	if (ctx->cart_ram_size == 0 || addr >= ctx->cart_ram_size)
		return;
	ctx->cart_ram[addr] = val;
}

static void gb_error_cb(struct gb_s *gb, const enum gb_error_e err,
			const uint16_t addr)
{
	(void)gb;
	(void)err;
#if defined(ESP_PLATFORM)
	printf("gb_error addr=0x%04x\n", (unsigned)addr);
	fflush(stdout);
	esp_restart();
#else
	fprintf(stderr, "gb_error addr=0x%04x\n", (unsigned)addr);
	abort();
#endif
}

static struct gb_core *core_from_gb(struct gb_s *gb)
{
	return (struct gb_core *)((uint8_t *)gb - offsetof(struct gb_core, gb));
}

#ifdef GB_CORE_OPCODE_HOOK
static void gb_on_opcode(struct gb_s *gb, uint8_t opcode)
{
	struct gb_core *core;

	if (opcode != 0x40)
		return;
	core = core_from_gb(gb);
	core->saw_break = true;
	core->ld_b_b_count++;
	core->break_b = gb->cpu_reg.bc.bytes.b;
	core->break_c = gb->cpu_reg.bc.bytes.c;
	core->break_d = gb->cpu_reg.de.bytes.d;
	core->break_e = gb->cpu_reg.de.bytes.e;
	core->break_h = gb->cpu_reg.hl.bytes.h;
	core->break_l = gb->cpu_reg.hl.bytes.l;
	/* PC already points at the next instruction. The breakpoint address
	 * is the byte that held opcode 0x40. */
	core->break_pc = (uint16_t)(gb->cpu_reg.pc.reg - 1u);
}
#endif

static void serial_tx_cb(struct gb_s *gb, const uint8_t tx)
{
	struct gb_core *core = core_from_gb(gb);

	if (core->serial_fn != NULL)
		core->serial_fn(core->serial_ctx, tx);
}

static void lcd_draw_line(struct gb_s *gb, const uint8_t *pixels,
			  const uint_fast8_t line)
{
	struct gb_core *core = core_from_gb(gb);
	if (line >= 144 || core->draw == NULL)
		return;
	core->draw(core->draw_ctx, pixels, (uint8_t)line);
}

static gb_err_t init_from_rom(struct gb_s *gb, gb_rom_ctx_t *ctx)
{
	gb->direct.priv = ctx;
	enum gb_init_error_e err =
		gb_init(gb, rom_read, cart_ram_read, cart_ram_write, gb_error_cb,
			ctx);
	switch (err) {
	case GB_INIT_NO_ERROR:
		return GB_OK;
	case GB_INIT_INVALID_CHECKSUM:
		return GB_ERR_CHECKSUM;
	case GB_INIT_CARTRIDGE_UNSUPPORTED:
		return GB_ERR_UNSUPPORTED;
	default:
		return GB_ERR_BAD_ARG;
	}
}

gb_err_t gb_core_save_size(const uint8_t *rom, size_t rom_size, size_t *out)
{
	if (rom == NULL || out == NULL || rom_size < 0x150)
		return GB_ERR_BAD_ARG;

	/* struct gb_s is larger than the ESP32-C3 app_main stack. */
	struct gb_s *gb = calloc(1, sizeof(*gb));
	if (gb == NULL)
		return GB_ERR_NO_MEM;
	gb_rom_ctx_t ctx = { .rom = rom, .rom_size = rom_size, .cart_ram = NULL,
			     .cart_ram_size = 0 };

	gb_err_t mapped = init_from_rom(gb, &ctx);
	if (mapped != GB_OK) {
		free(gb);
		return mapped;
	}

	size_t ram_size = 0;
	if (gb_get_save_size_s(gb, &ram_size) != 0) {
		free(gb);
		return GB_ERR_UNSUPPORTED;
	}

	*out = ram_size;
	free(gb);
	return GB_OK;
}

gb_err_t gb_core_init(gb_core_t *core, const uint8_t *rom, size_t rom_size,
		      uint8_t *cart_ram, size_t cart_ram_size)
{
	if (core == NULL || rom == NULL)
		return GB_ERR_BAD_ARG;

	memset(core, 0, sizeof(*core));
	core->rom_ctx.rom = rom;
	core->rom_ctx.rom_size = rom_size;
	core->rom_ctx.cart_ram = cart_ram;
	core->rom_ctx.cart_ram_size = cart_ram_size;

	gb_err_t mapped = init_from_rom(&core->gb, &core->rom_ctx);
	if (mapped != GB_OK)
		return mapped;

	size_t need = 0;
	if (gb_get_save_size_s(&core->gb, &need) != 0)
		return GB_ERR_UNSUPPORTED;
	if (need != cart_ram_size)
		return GB_ERR_BAD_ARG;

	gb_init_lcd(&core->gb, lcd_draw_line);

	return GB_OK;
}

void gb_core_set_pressed(gb_core_t *core, uint8_t pressed_mask)
{
	if (core == NULL)
		return;
	core->gb.direct.joypad = (uint8_t)~pressed_mask;
}

void gb_core_set_serial_tx(gb_core_t *core, gb_serial_tx_fn fn, void *ctx)
{
	if (core == NULL)
		return;
	core->serial_fn = fn;
	core->serial_ctx = ctx;
	if (fn == NULL)
		gb_init_serial(&core->gb, NULL, NULL);
	else
		gb_init_serial(&core->gb, serial_tx_cb, NULL);
}

void gb_core_cpu_view(const gb_core_t *core, gb_cpu_view_t *out)
{
	if (out == NULL)
		return;
	memset(out, 0, sizeof(*out));
	if (core == NULL)
		return;
	out->ld_b_b = core->saw_break;
	out->ld_b_b_count = core->ld_b_b_count;
	if (core->saw_break) {
		out->b = core->break_b;
		out->c = core->break_c;
		out->d = core->break_d;
		out->e = core->break_e;
		out->h = core->break_h;
		out->l = core->break_l;
		out->pc = core->break_pc;
		return;
	}
	out->b = core->gb.cpu_reg.bc.bytes.b;
	out->c = core->gb.cpu_reg.bc.bytes.c;
	out->d = core->gb.cpu_reg.de.bytes.d;
	out->e = core->gb.cpu_reg.de.bytes.e;
	out->h = core->gb.cpu_reg.hl.bytes.h;
	out->l = core->gb.cpu_reg.hl.bytes.l;
	out->a = core->gb.cpu_reg.a;
	out->pc = core->gb.cpu_reg.pc.reg;
}

void gb_core_run_frame(gb_core_t *core, gb_line_fn draw, void *ctx)
{
	if (core == NULL)
		return;
	core->draw = draw;
	core->draw_ctx = ctx;
	gb_run_frame(&core->gb);
}

void gb_core_set_frame_skip(gb_core_t *core, bool enable)
{
	if (core == NULL)
		return;
	core->gb.direct.frame_skip = enable;
}

void gb_core_set_interlace(gb_core_t *core, bool enable)
{
	if (core == NULL)
		return;
	core->gb.direct.interlace = enable;
}

uint8_t *gb_core_cart_ram(gb_core_t *core)
{
	return core == NULL ? NULL : core->rom_ctx.cart_ram;
}

size_t gb_core_cart_ram_size(const gb_core_t *core)
{
	return core == NULL ? 0 : core->rom_ctx.cart_ram_size;
}

size_t gb_core_gb_struct_size(void)
{
	return sizeof(struct gb_s);
}

size_t gb_core_instance_size(void)
{
	return sizeof(struct gb_core);
}
