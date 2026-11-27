/* ============================================================================
 * hal_st7735.h - Giao diện (API) của driver màn hình ST7735.
 *
 * Tầng trên (display_render.c) chỉ cần gọi các hàm khai báo ở đây để vẽ.
 * Cài đặt nằm trong hal_st7735.c; hàm đó dùng hal_spi.c và hal_gpio.c bên dưới.
 * ========================================================================== */
#ifndef HAL_ST7735_H
#define HAL_ST7735_H
#include <stdbool.h>
#include <stdint.h>
#include "FreeRTOS.h"   /* cung cấp kiểu BaseType_t */

/* Kích thước màn hình (điểm ảnh), dạng dọc. */
#define HAL_ST7735_WIDTH   128
#define HAL_ST7735_HEIGHT  160
/* Hai màu dùng sẵn, viết theo định dạng RGB565 (16 bit cho mỗi điểm ảnh). */
#define HAL_ST7735_BLACK   0x0000U
#define HAL_ST7735_WHITE   0xFFFFU
/* Kích thước một ký tự của font: rộng 6 px, cao 8 px (đã tính cả khoảng trống). */
#define HAL_ST7735_FONT_W  6
#define HAL_ST7735_FONT_H  8

/* Kết quả trả về của các hàm vẽ. */
typedef enum {
    HAL_ST7735_OK,           /* thành công */
    HAL_ST7735_ERROR,        /* lỗi khi truyền SPI/DMA */
    HAL_ST7735_TIMEOUT,      /* hết thời gian chờ (mutex hoặc hạn chót vẽ) */
    HAL_ST7735_INVALID,      /* tham số sai (con trỏ NULL, chuỗi quá dài, bán kính quá lớn) */
    HAL_ST7735_NOT_READY,    /* chưa thể dùng: scheduler chưa chạy, gọi từ ngắt, hoặc LCD chưa init */
    HAL_ST7735_UNSUPPORTED   /* không hỗ trợ (bật/tắt đèn nền khi đèn nền không do GPIO điều khiển) */
} hal_st7735_status_t;

/* prepare: gọi tuần tự TRƯỚC scheduler, tạo Mutex/Semaphore và cấu hình BSP.
 * init/vẽ/BL: chỉ gọi từ task SAU scheduler. Mỗi API giữ Mutex trọn thao tác.
 * Mutex chờ <=200 ms; vẽ <=500 ms; init <=1800 ms (không tính thời gian CPU
 * bị task ưu tiên cao chiếm). Không dùng ISR, không portMAX_DELAY.
 */
/* Chuẩn bị: tạo mutex, cấu hình chân GPIO, chuẩn bị SPI/DMA. pdPASS = thành công. */
BaseType_t hal_st7735_prepare(void);
/* Khởi tạo LCD: reset, đánh thức, nạp thông số, tô trắng màn hình, bật hiển thị. */
hal_st7735_status_t hal_st7735_init(void);
/* Tô hình chữ nhật đặc: góc (x, y), rộng w, cao h, màu rgb565. */
hal_st7735_status_t hal_st7735_fill_rect(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t rgb565);
/* Vẽ một điểm ảnh. */
hal_st7735_status_t hal_st7735_draw_pixel(int16_t x, int16_t y, uint16_t rgb565);
/* Vẽ đoạn ngang dài length điểm ảnh, bắt đầu từ (x, y). */
hal_st7735_status_t hal_st7735_draw_hline(int16_t x, int16_t y, uint16_t length, uint16_t rgb565);
/* Vẽ đoạn dọc dài length điểm ảnh, bắt đầu từ (x, y). */
hal_st7735_status_t hal_st7735_draw_vline(int16_t x, int16_t y, uint16_t length, uint16_t rgb565);
/* Vẽ hình tròn đặc: tâm (x, y), bán kính radius. */
hal_st7735_status_t hal_st7735_draw_circle_filled(int16_t x, int16_t y, uint16_t radius, uint16_t rgb565);
/* Font tự tạo 5x7 + 1 cột/1 hàng trống; ASCII hoa, số và dấu cơ bản.
 * Chữ thường chuyển thành hoa; ký tự ngoài font thành '?'. Tối đa 64 byte.
 * Không hỗ trợ UTF-8 có dấu trên LCD. Báo cáo và comment vẫn tiếng Việt.
 */
/* Vẽ một ký tự tại (x, y): fg là màu nét chữ, bg là màu nền ô ký tự. */
hal_st7735_status_t hal_st7735_draw_char(int16_t x, int16_t y, char c, uint16_t fg, uint16_t bg);
/* Vẽ chuỗi tại (x, y), mỗi ký tự cách nhau 6 px. Chuỗi NULL hoặc dài quá 64 byte thì trả INVALID. */
hal_st7735_status_t hal_st7735_draw_text(int16_t x, int16_t y, const char *text, uint16_t fg, uint16_t bg);
/* Bật (true) hoặc tắt (false) đèn nền. Trả UNSUPPORTED nếu đèn nền không do GPIO điều khiển. */
hal_st7735_status_t hal_st7735_set_backlight(bool on);
#endif
