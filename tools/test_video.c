/* Runs the real gb_video.c against an asynchronous LCD/FreeRTOS test driver.
 * Every accepted draw borrows the buffer until a completion is delivered. */
#include "gb_video.h"
#include "test_video_stubs.h"

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond) do { \
	if (!(cond)) { \
		fprintf(stderr, "test_video:%d: %s\n", __LINE__, #cond); \
		exit(1); \
	} \
} while (0)

static struct {
	const void *buf;
	uint8_t *snapshot;
	size_t size;
} pending[16];
static size_t pending_count;
static void *allocations[16];
static unsigned tokens;
static unsigned timeouts_left;
static unsigned timeout_count;
static unsigned missing_waits;
static bool missing_callback;
static bool fail_next_draw;
static bool rejected_draw;
static jmp_buf missing_checkpoint;
static esp_lcd_panel_io_callbacks_t lcd_callbacks;
static void *lcd_ctx;

static void check_borrowed_buffers(void)
{
	for (size_t i = 0; i < pending_count; i++)
		CHECK(memcmp(pending[i].buf, pending[i].snapshot,
			     pending[i].size) == 0);
}

static void complete_oldest_draw(void)
{
	CHECK(pending_count > 0);
	check_borrowed_buffers();
	free(pending[0].snapshot);
	pending_count--;
	memmove(pending, pending + 1, pending_count * sizeof(pending[0]));
	CHECK(lcd_callbacks.on_color_trans_done != NULL);
	lcd_callbacks.on_color_trans_done((void *)1, NULL, lcd_ctx);
}

esp_err_t bsp_display_init(void) { return ESP_OK; }
esp_lcd_panel_handle_t bsp_display_panel(void) { return (void *)1; }
esp_lcd_panel_io_handle_t bsp_display_io(void) { return (void *)1; }
void bsp_display_backlight(uint8_t percent) { (void)percent; }
const char *esp_err_to_name(esp_err_t err) { (void)err; return "ESP_FAIL"; }

void *heap_caps_aligned_alloc(size_t align, size_t size, unsigned caps)
{
	(void)align;
	(void)caps;
	void *ptr = malloc(size);
	CHECK(ptr != NULL);
	for (size_t i = 0; i < sizeof(allocations) / sizeof(allocations[0]); i++) {
		if (allocations[i] == NULL) {
			allocations[i] = ptr;
			return ptr;
		}
	}
	CHECK(false);
	return NULL;
}

void heap_caps_free(void *ptr)
{
	/* A draw completion, rather than elapsed time, releases DMA ownership. */
	for (size_t i = 0; i < pending_count; i++)
		CHECK(pending[i].buf != ptr);
	for (size_t i = 0; i < sizeof(allocations) / sizeof(allocations[0]); i++) {
		if (allocations[i] == ptr) {
			allocations[i] = NULL;
			free(ptr);
			return;
		}
	}
	CHECK(false);
}

SemaphoreHandle_t xSemaphoreCreateCounting(unsigned max, unsigned initial)
{
	CHECK(max >= 2);
	tokens = initial;
	return &tokens;
}

BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t sem, BaseType_t *wake)
{
	CHECK(sem == &tokens);
	*wake = pdFALSE;
	tokens++;
	return pdTRUE;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, unsigned ticks)
{
	CHECK(sem == &tokens);
	CHECK(ticks > 0);
	check_borrowed_buffers();
	if (missing_callback) {
		/* Bound the test without delivering a callback or hanging forever. */
		if (++missing_waits == 2)
			longjmp(missing_checkpoint, 1);
		return pdFALSE;
	}
	if (timeouts_left > 0) {
		timeouts_left--;
		timeout_count++;
		return pdFALSE;
	}
	if (tokens == 0)
		complete_oldest_draw();
	CHECK(tokens > 0);
	tokens--;
	return pdTRUE;
}

esp_err_t esp_lcd_panel_io_register_event_callbacks(
	esp_lcd_panel_io_handle_t io, const esp_lcd_panel_io_callbacks_t *callbacks,
	void *ctx)
{
	CHECK(io == (void *)1);
	lcd_callbacks = *callbacks;
	lcd_ctx = ctx;
	return ESP_OK;
}

esp_err_t esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t panel,
	int x, int y, int x_end, int y_end, const void *buf)
{
	CHECK(panel == (void *)1);
	check_borrowed_buffers();
	if (fail_next_draw) {
		fail_next_draw = false;
		rejected_draw = true;
		return ESP_FAIL;
	}
	CHECK(pending_count < sizeof(pending) / sizeof(pending[0]));
	size_t size = (size_t)(x_end - x) * (size_t)(y_end - y) * 2;
	uint8_t *snapshot = malloc(size);
	CHECK(snapshot != NULL);
	memcpy(snapshot, buf, size);
	pending[pending_count].buf = buf;
	pending[pending_count].snapshot = snapshot;
	pending[pending_count].size = size;
	pending_count++;
	return ESP_OK;
}

int main(int argc, char **argv)
{
	const gb_video_config_t cfg = { .scale = GB_SCALE_1X_CENTER };
	uint8_t line[160];
	CHECK(argc == 2);

	if (strcmp(argv[1], "missing-callback") == 0) {
		missing_callback = true;
		if (setjmp(missing_checkpoint) == 0) {
			gb_video_init(&cfg);
			CHECK(false);
		}
		/* The first clear draw must still own its unreleased buffer. */
		CHECK(pending_count == 1);
		check_borrowed_buffers();
		missing_callback = false;
		complete_oldest_draw();
	} else {
		if (strcmp(argv[1], "init-timeout") == 0)
			timeouts_left = 1;
		CHECK(gb_video_init(&cfg) == ESP_OK);
		CHECK(pending_count == 0);
		memset(line, 0, sizeof(line));
		gb_video_push_line(line, 0);
		memset(line, 1, sizeof(line));
		if (strcmp(argv[1], "rejected-draw") == 0)
			fail_next_draw = true;
		gb_video_push_line(line, 1);
		if (strcmp(argv[1], "row-timeout") == 0)
			timeouts_left = 1;
		memset(line, 2, sizeof(line));
		gb_video_push_line(line, 2);
		gb_video_flush();
		CHECK(pending_count == 0);
		if (strcmp(argv[1], "init-timeout") == 0 ||
		    strcmp(argv[1], "row-timeout") == 0)
			CHECK(timeout_count == 1);
		if (strcmp(argv[1], "rejected-draw") == 0)
			CHECK(rejected_draw);
	}

	for (size_t i = 0; i < sizeof(allocations) / sizeof(allocations[0]); i++) {
		if (allocations[i] != NULL)
			heap_caps_free(allocations[i]);
	}
	printf("test_video: %s ok\n", argv[1]);
	return 0;
}
