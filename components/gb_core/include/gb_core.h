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

typedef void (*gb_line_fn)(void *ctx, const uint8_t *pixels160, uint8_t line);

gb_err_t gb_core_init(gb_core_t *core,
		      const uint8_t *rom, size_t rom_size,
		      uint8_t *cart_ram, size_t cart_ram_size);

gb_err_t gb_core_save_size(const uint8_t *rom, size_t rom_size, size_t *out);

void gb_core_set_pressed(gb_core_t *core, uint8_t pressed_mask);

void gb_core_run_frame(gb_core_t *core, gb_line_fn draw, void *ctx);

void gb_core_set_frame_skip(gb_core_t *core, bool enable);
void gb_core_set_interlace(gb_core_t *core, bool enable);

uint8_t *gb_core_cart_ram(gb_core_t *core);
size_t gb_core_cart_ram_size(const gb_core_t *core);

size_t gb_core_gb_struct_size(void);

/* Opaque instance size for static/host allocation (not on embedded task stacks). */
size_t gb_core_instance_size(void);

#endif
