#include "gb_video.h"
#include "gb_video_lut.h"

#include "bsp_display.h"
#include "bsp_pins.h"

#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <stdio.h>
#include <string.h>

#define GB_W 160
#define GB_H 144
#define GB_X0 40
#define GB_Y0 88
#define ROW_BYTES (GB_W * 2)
#define CLEAR_LINES 40
#define CLEAR_BYTES (BSP_LCD_W * CLEAR_LINES * 2)

static esp_lcd_panel_handle_t s_panel;
static uint8_t *s_row[2];
static SemaphoreHandle_t s_done;
static int s_submitted;
static int s_completed;
static int s_which;
static bool s_ready;
static bool s_timeout_logged;

static bool IRAM_ATTR color_trans_done(esp_lcd_panel_io_handle_t io,
				       esp_lcd_panel_io_event_data_t *edata,
				       void *ctx)
{
	BaseType_t wake = pdFALSE;

	(void)io;
	(void)edata;
	(void)ctx;
	if (s_done != NULL)
		xSemaphoreGiveFromISR(s_done, &wake);
	return wake == pdTRUE;
}

static void log_timeout(void)
{
	if (s_timeout_logged)
		return;
	s_timeout_logged = true;
	printf("gb_video: color transfer done timed out\n");
	fflush(stdout);
}

static void wait_completed(int target)
{
	while (s_completed < target) {
		if (xSemaphoreTake(s_done, pdMS_TO_TICKS(200)) != pdTRUE) {
			log_timeout();
			/* Only a real completion releases the borrowed DMA buffer. */
			continue;
		}
		s_completed++;
	}
}

static void submit(int x, int y, int x_end, int y_end, const void *buf)
{
	esp_err_t err = esp_lcd_panel_draw_bitmap(s_panel, x, y, x_end, y_end, buf);

	if (err != ESP_OK) {
		printf("gb_video: draw_bitmap %s\n", esp_err_to_name(err));
		fflush(stdout);
		/* The caller has rotated rows; drain before either is reused. */
		wait_completed(s_submitted);
		return;
	}
	s_submitted++;
}

static void draw_wait(int x, int y, int x_end, int y_end, const void *buf)
{
	int mark;

	submit(x, y, x_end, y_end, buf);
	mark = s_submitted;
	wait_completed(mark);
}

static void paint_shade_rect(int x, int y, int w, int h, uint8_t shade)
{
	uint8_t pair[2];
	uint8_t *buf;
	size_t n = (size_t)w * (size_t)h * 2;

	gb_video_shade_bytes(shade, pair);
	buf = heap_caps_aligned_alloc(16, n, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
	if (buf == NULL) {
		printf("gb_video: shade rect alloc failed\n");
		fflush(stdout);
		return;
	}
	for (size_t i = 0; i < n; i += 2) {
		buf[i] = pair[0];
		buf[i + 1] = pair[1];
	}
	draw_wait(x, y, x + w, y + h, buf);
	heap_caps_free(buf);
}

static void paint_black(void)
{
	uint8_t *buf = heap_caps_aligned_alloc(16, CLEAR_BYTES,
					       MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

	if (buf == NULL) {
		printf("gb_video: clear alloc failed\n");
		fflush(stdout);
		return;
	}
	memset(buf, 0, CLEAR_BYTES);
	for (int y = 0; y < BSP_LCD_H; y += CLEAR_LINES)
		draw_wait(0, y, BSP_LCD_W, y + CLEAR_LINES, buf);
	heap_caps_free(buf);
}

esp_err_t gb_video_init(const gb_video_config_t *cfg)
{
	esp_err_t err;
	esp_lcd_panel_io_callbacks_t cbs = {
		.on_color_trans_done = color_trans_done,
	};
	static const uint8_t swatches[4] = { 0, 1, 2, 3 };

	if (cfg == NULL)
		return ESP_ERR_INVALID_ARG;
	if (cfg->scale != GB_SCALE_1X_CENTER)
		return ESP_ERR_NOT_SUPPORTED;
	if (s_ready)
		return ESP_OK;

	err = bsp_display_init();
	if (err != ESP_OK)
		return err;
	s_panel = bsp_display_panel();
	if (s_panel == NULL || bsp_display_io() == NULL)
		return ESP_ERR_INVALID_STATE;

	s_done = xSemaphoreCreateCounting(8, 0);
	if (s_done == NULL)
		return ESP_ERR_NO_MEM;
	err = esp_lcd_panel_io_register_event_callbacks(bsp_display_io(), &cbs, NULL);
	if (err != ESP_OK)
		return err;

	for (int i = 0; i < 2; i++) {
		s_row[i] = heap_caps_aligned_alloc(16, ROW_BYTES,
						   MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
		if (s_row[i] == NULL)
			return ESP_ERR_NO_MEM;
	}

	bsp_display_backlight(50);
	paint_black();
	for (int i = 0; i < 4; i++)
		paint_shade_rect(0, GB_Y0 + i * 8, 8, 8, swatches[i]);

	s_ready = true;
	return ESP_OK;
}

void gb_video_push_line(const uint8_t *pixels160, uint8_t line)
{
	uint8_t *dst;

	if (!s_ready || pixels160 == NULL || line >= GB_H)
		return;
	/* Leave one transfer in flight so the other row can be filled. */
	if (s_submitted - s_completed >= 2)
		wait_completed(s_submitted - 1);

	dst = s_row[s_which];
	s_which ^= 1;
	for (int x = 0; x < GB_W; x++) {
		uint8_t pair[2];

		gb_video_shade_bytes((uint8_t)(pixels160[x] & 0x03u), pair);
		dst[x * 2] = pair[0];
		dst[x * 2 + 1] = pair[1];
	}
	submit(GB_X0, GB_Y0 + line, GB_X0 + GB_W, GB_Y0 + line + 1, dst);
}

void gb_video_flush(void)
{
	if (!s_ready)
		return;
	wait_completed(s_submitted);
}
