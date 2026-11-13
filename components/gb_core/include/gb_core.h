#ifndef GB_CORE_H
#define GB_CORE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
	GB_OK = 0,
	GB_ERR_BAD_ARG,
	GB_ERR_CHECKSUM,
	GB_ERR_UNSUPPORTED,
	GB_ERR_NO_MEM,
	GB_ERR_SAVE,
	GB_ERR_DISABLED
} gb_err_t;

typedef struct gb_core gb_core_t;

/* Pressed bits for gb_core_set_pressed. 1 means held. Low nibble is
 * A/B/Select/Start, high nibble is Right/Left/Up/Down. */
enum {
	GB_BTN_A = 0x01,
	GB_BTN_B = 0x02,
	GB_BTN_SELECT = 0x04,
	GB_BTN_START = 0x08,
	GB_BTN_RIGHT = 0x10,
	GB_BTN_LEFT = 0x20,
	GB_BTN_UP = 0x40,
	GB_BTN_DOWN = 0x80
};

/* Cart RAM larger than this is refused by the firmware. The 24 KiB NVS
 * partition could theoretically hold more; 2048 is the chosen ceiling. */
#define GB_NVS_RAM_MAX 2048u

typedef void (*gb_line_fn)(void *ctx, const uint8_t *pixels160, uint8_t line);

typedef void (*gb_serial_tx_fn)(void *ctx, uint8_t byte);

typedef struct {
	uint8_t b, c, d, e, h, l, a;
	uint16_t pc;
	uint32_t ld_b_b_count;
	bool ld_b_b;
} gb_cpu_view_t;

gb_err_t gb_core_init(gb_core_t *core,
		      const uint8_t *rom, size_t rom_size,
		      uint8_t *cart_ram, size_t cart_ram_size);

gb_err_t gb_core_save_size(const uint8_t *rom, size_t rom_size, size_t *out);

void gb_core_set_pressed(gb_core_t *core, uint8_t pressed_mask);

/* Call after gb_core_init. Bytes written to SB with SC bit 7 set are delivered
 * here. A NULL fn disables the callback. */
void gb_core_set_serial_tx(gb_core_t *core, gb_serial_tx_fn fn, void *ctx);

/* Live registers, or the registers sampled at the most recent opcode 0x40
 * when the host opcode hook is compiled in and that opcode has executed. */
void gb_core_cpu_view(const gb_core_t *core, gb_cpu_view_t *out);

void gb_core_run_frame(gb_core_t *core, gb_line_fn draw, void *ctx);

void gb_core_set_frame_skip(gb_core_t *core, bool enable);
void gb_core_set_interlace(gb_core_t *core, bool enable);

uint8_t *gb_core_cart_ram(gb_core_t *core);
size_t gb_core_cart_ram_size(const gb_core_t *core);

size_t gb_core_gb_struct_size(void);

/* Opaque instance size for static/host allocation (not on embedded task stacks). */
size_t gb_core_instance_size(void);

#endif
