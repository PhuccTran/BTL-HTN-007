/* ============================================================================
 * bsp_display.h - Cấu hình phần cứng của màn hình ST7735 (BSP = hỗ trợ bo mạch).
 *
 * Tệp này chỉ chứa #define, không có hàm. Nó cho biết:
 *   - màn hình nối vào chân nào, dùng SPI/DMA nào;
 *   - tốc độ SPI;
 *   - các thông số riêng của panel (độ lệch ảnh, hướng quét, đảo màu...).
 * Các tệp hal_gpio.c, hal_spi.c, hal_st7735.c đọc các macro này.
 * Nhiều macro có #ifndef: có thể ghi đè từ bên ngoài (ví dụ cờ -D khi biên dịch).
 * ========================================================================== */
#ifndef BSP_DISPLAY_H
#define BSP_DISPLAY_H

#include "stm32f4xx.h"

/* Sơ đồ đề xuất cho kit CHƯA đấu dây. Mọi pin mapping màn hình nằm ở đây.
 * Không trùng I2C PB6/PB7, UART PA9, LED PC13 và SWD PA13/PA14.
 * HAL tạm chỉ hỗ trợ SPI1 và DMA2 Stream3 Channel3; đổi bus phải sửa backend.
 */

/* --- Bộ SPI --- */
#define BSP_DISPLAY_SPI              SPI1   /* dùng bộ SPI1 của chip */
/* Bật xung clock cho SPI1 (bit SPI1EN trong thanh ghi RCC->APB2ENR). */
#define BSP_DISPLAY_SPI_ENABLE()     (RCC->APB2ENR |= RCC_APB2ENR_SPI1EN)
/* Reset SPI1 về trạng thái mặc định: bật rồi tắt bit reset trong RCC->APB2RSTR. */
#define BSP_DISPLAY_SPI_RESET()      do { RCC->APB2RSTR |= RCC_APB2RSTR_SPI1RST; \
                                         RCC->APB2RSTR &= ~RCC_APB2RSTR_SPI1RST; } while (0)
/* Bật xung clock cho cổng GPIOA và GPIOB (các chân màn hình nằm ở hai cổng này). */
#define BSP_DISPLAY_GPIO_ENABLE()    (RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN)

/* --- Chân nối --- */
#define BSP_DISPLAY_SCK_PORT         GPIOA  /* SCK  (xung nhịp SPI)     = PA5 */
#define BSP_DISPLAY_SCK_PIN          5U
#define BSP_DISPLAY_MOSI_PORT        GPIOA  /* MOSI (dữ liệu ra LCD)    = PA7 */
#define BSP_DISPLAY_MOSI_PIN         7U
/* Số chức năng thay thế (alternate function) để gắn chân SCK/MOSI vào SPI1: AF5. */
#define BSP_DISPLAY_SPI_AF           5U
#define BSP_DISPLAY_CS_PORT          GPIOA  /* CS  (chọn chip)          = PA4 */
#define BSP_DISPLAY_CS_PIN           4U
#define BSP_DISPLAY_DC_PORT          GPIOB  /* DC  (lệnh hay dữ liệu)   = PB0 */
#define BSP_DISPLAY_DC_PIN           0U
#define BSP_DISPLAY_RST_PORT         GPIOB  /* RST (reset phần cứng LCD) = PB1 */
#define BSP_DISPLAY_RST_PIN          1U
#define BSP_DISPLAY_BL_PORT          GPIOB  /* BL  (đèn nền)            = PB10 */
#define BSP_DISPLAY_BL_PIN           10U

/* --- Đèn nền --- */
/* Mặc định BL nối nguồn 3.3 V theo thông số module, KHÔNG nối tải LED vào PB10.
 * Nếu muốn bật/tắt: PB10 điều khiển tầng transistor/MOSFET hoặc ngõ BL logic
 * đã được xác minh. Khi đó đổi macro này thành 1; không có PWM dimming.
 */
/* 0 = chân PB10 không được cấu hình/điều khiển; 1 = PB10 điều khiển đèn nền. */
#ifndef BSP_DISPLAY_BL_CONTROLLED
#define BSP_DISPLAY_BL_CONTROLLED    0
#endif
/* Chỉ có tác dụng khi BL_CONTROLLED = 1: mức cao trên PB10 = bật đèn nền. */
#define BSP_DISPLAY_BL_ACTIVE_HIGH   1U

/* --- Tốc độ SPI và DMA --- */
/* SPI mode 0, MSB first; APB2 / 16 = 6 MHz khi CPU 96 MHz.
 * /16 giữ tốc độ thấp để thử dây breadboard. Không dùng MISO.
 */
/* Giá trị trường BR trong thanh ghi SPI_CR1: BR = 3 nghĩa là chia xung APB2 cho 16. */
#define BSP_DISPLAY_SPI_BR           (3UL << SPI_CR1_BR_Pos)
/* 1 = truyền SPI bằng DMA; 0 = truyền bằng vòng lặp trên CPU. Có thể ghi đè. */
#ifndef BSP_DISPLAY_USE_DMA
#define BSP_DISPLAY_USE_DMA          1
#endif
#define BSP_DISPLAY_DMA              DMA2           /* bộ DMA2 */
#define BSP_DISPLAY_DMA_STREAM       DMA2_Stream3   /* luồng (stream) 3 của DMA2 */
#define BSP_DISPLAY_DMA_IRQ          DMA2_Stream3_IRQn      /* số ngắt của luồng này */
/* Tên hàm xử lý ngắt. hal_spi.c định nghĩa hàm tên này, nên vector table của
 * project chính phải trỏ đúng tên DMA2_Stream3_IRQHandler. */
#define BSP_DISPLAY_DMA_HANDLER      DMA2_Stream3_IRQHandler
/* Bật xung clock cho DMA2. */
#define BSP_DISPLAY_DMA_ENABLE()     (RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN)
/* Chọn kênh (channel) 3 của luồng: đây là kênh gắn với SPI1 TX. */
#define BSP_DISPLAY_DMA_CHANNEL      (3UL << DMA_SxCR_CHSEL_Pos)
/* Cờ "truyền xong" (transfer complete) của luồng 3, đọc từ thanh ghi DMA_LISR. */
#define BSP_DISPLAY_DMA_TC           DMA_LISR_TCIF3
/* Ba cờ lỗi của luồng 3: lỗi truyền, lỗi direct mode, lỗi FIFO. */
#define BSP_DISPLAY_DMA_ERRORS       (DMA_LISR_TEIF3 | DMA_LISR_DMEIF3 | DMA_LISR_FEIF3)
/* Các bit để xóa cờ trong DMA_LIFCR: truyền xong, nửa chặng, lỗi truyền,
 * lỗi direct mode, lỗi FIFO. */
#define BSP_DISPLAY_DMA_CLEAR        (DMA_LIFCR_CTCIF3 | DMA_LIFCR_CHTIF3 | \
                                     DMA_LIFCR_CTEIF3 | DMA_LIFCR_CDMEIF3 | DMA_LIFCR_CFEIF3)

/* --- Thông số panel --- */
/* V1.1 là nhãn PCB, chưa xác định tab/panel thực tế. TODO: hiệu chỉnh trên kit.
 * Profile thử ban đầu: offset (0,0), portrait MX|MY|BGR = 0xC8.
 * Nếu lệch ảnh thử offset (2,1); nếu đỏ/xanh đổi chỗ đảo bit BGR (0x08).
 * Không bật MV (0x20), vì bố cục phần mềm cố định portrait 128x160.
 */
/* Độ lệch (pixel) cộng vào tọa độ khi đặt cửa sổ vẽ, theo trục X và Y. */
#ifndef BSP_DISPLAY_X_OFFSET
#define BSP_DISPLAY_X_OFFSET         0U
#endif
#ifndef BSP_DISPLAY_Y_OFFSET
#define BSP_DISPLAY_Y_OFFSET         0U
#endif
/* Giá trị gửi cho lệnh MADCTL (hướng quét và thứ tự màu). 0xC8 = 0x80 (MX) | 0x40 (MY) | 0x08 (BGR). */
#ifndef BSP_DISPLAY_MADCTL
#define BSP_DISPLAY_MADCTL           0xC8U
#endif
/* 0 = gửi lệnh tắt đảo màu (INVOFF); khác 0 = gửi lệnh bật đảo màu (INVON). */
#ifndef BSP_DISPLAY_INVERT
#define BSP_DISPLAY_INVERT           0
#endif
/* Profile tham chiếu ST component ST7735 cho panel 128x160 (frame/power/gamma).
 * Chưa xác nhận analog trên module thật; đặt 0 để thử mặc định sau reset. */
/* 1 = hal_st7735_init nạp bộ thông số frame/power/gamma kèm theo; 0 = bỏ qua. */
#ifndef BSP_DISPLAY_PANEL_PROFILE
#define BSP_DISPLAY_PANEL_PROFILE    1
#endif

#endif
