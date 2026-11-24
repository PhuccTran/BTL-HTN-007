#ifndef HAL_ST7735_H
#define HAL_ST7735_H
#include <stdbool.h>
#include <stdint.h>
#include "FreeRTOS.h"

#define HAL_ST7735_WIDTH   128
#define HAL_ST7735_HEIGHT  160
#define HAL_ST7735_BLACK   0x0000U
#define HAL_ST7735_WHITE   0xFFFFU
#define HAL_ST7735_FONT_W  6
#define HAL_ST7735_FONT_H  8

typedef enum {
    HAL_ST7735_OK, HAL_ST7735_ERROR, HAL_ST7735_TIMEOUT,
    HAL_ST7735_INVALID, HAL_ST7735_NOT_READY, HAL_ST7735_UNSUPPORTED
} hal_st7735_status_t;

/* prepare trước scheduler; init/vẽ/BL chỉ gọi từ task sau scheduler.
 * Mutex chờ 200 ms; deadline vẽ 500 ms, init 1800 ms sau khi lấy khóa.
 */
BaseType_t hal_st7735_prepare(void);
hal_st7735_status_t hal_st7735_init(void);
hal_st7735_status_t hal_st7735_fill_rect(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t rgb565);
hal_st7735_status_t hal_st7735_draw_pixel(int16_t x, int16_t y, uint16_t rgb565);
hal_st7735_status_t hal_st7735_draw_hline(int16_t x, int16_t y, uint16_t length, uint16_t rgb565);
hal_st7735_status_t hal_st7735_draw_vline(int16_t x, int16_t y, uint16_t length, uint16_t rgb565);
hal_st7735_status_t hal_st7735_draw_circle_filled(int16_t x, int16_t y, uint16_t radius, uint16_t rgb565);
/* Font 6x8 ASCII; chuỗi tối đa 64 byte, không hỗ trợ UTF-8. */
hal_st7735_status_t hal_st7735_draw_char(int16_t x, int16_t y, char c, uint16_t fg, uint16_t bg);
hal_st7735_status_t hal_st7735_draw_text(int16_t x, int16_t y, const char *text, uint16_t fg, uint16_t bg);
hal_st7735_status_t hal_st7735_set_backlight(bool on);
#endif
