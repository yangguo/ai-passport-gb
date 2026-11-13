#ifndef TEST_VIDEO_STUBS_H
#define TEST_VIDEO_STUBS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define IRAM_ATTR
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 1
#define ESP_ERR_NOT_SUPPORTED 2
#define ESP_ERR_INVALID_STATE 3
#define ESP_ERR_NO_MEM 4
#define MALLOC_CAP_DMA 1
#define MALLOC_CAP_INTERNAL 2
#define BSP_LCD_W 240
#define BSP_LCD_H 320
#define pdFALSE 0
#define pdTRUE 1
#define pdMS_TO_TICKS(ms) (ms)

typedef int esp_err_t;
typedef int BaseType_t;
typedef void *esp_lcd_panel_handle_t;
typedef void *esp_lcd_panel_io_handle_t;
typedef void *SemaphoreHandle_t;
typedef struct { int unused; } esp_lcd_panel_io_event_data_t;
typedef struct {
	bool (*on_color_trans_done)(esp_lcd_panel_io_handle_t,
				    esp_lcd_panel_io_event_data_t *, void *);
} esp_lcd_panel_io_callbacks_t;

esp_err_t bsp_display_init(void);
esp_lcd_panel_handle_t bsp_display_panel(void);
esp_lcd_panel_io_handle_t bsp_display_io(void);
void bsp_display_backlight(uint8_t percent);
const char *esp_err_to_name(esp_err_t err);
void *heap_caps_aligned_alloc(size_t align, size_t size, unsigned caps);
void heap_caps_free(void *ptr);
SemaphoreHandle_t xSemaphoreCreateCounting(unsigned max, unsigned initial);
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t sem, BaseType_t *wake);
BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, unsigned ticks);
esp_err_t esp_lcd_panel_io_register_event_callbacks(
	esp_lcd_panel_io_handle_t io, const esp_lcd_panel_io_callbacks_t *callbacks,
	void *ctx);
esp_err_t esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t panel,
	int x, int y, int x_end, int y_end, const void *buf);

#endif
