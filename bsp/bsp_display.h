#ifndef BSP_DISPLAY_H
#define BSP_DISPLAY_H

#include "stm32f4xx.h"

/* Pin mapping riêng cho màn hình ST7735 trên STM32F411CE.
 * Không sử dụng hai chân SWD PA13/PA14.
 * HAL tạm chỉ hỗ trợ SPI1 và DMA2 Stream3 Channel3; đổi bus phải sửa backend.
 */
#define BSP_DISPLAY_SPI              SPI1
#define BSP_DISPLAY_SPI_ENABLE()     (RCC->APB2ENR |= RCC_APB2ENR_SPI1EN)
#define BSP_DISPLAY_SPI_RESET()      do { RCC->APB2RSTR |= RCC_APB2RSTR_SPI1RST; \
                                         RCC->APB2RSTR &= ~RCC_APB2RSTR_SPI1RST; } while (0)
#define BSP_DISPLAY_GPIO_ENABLE()    (RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN)
#define BSP_DISPLAY_SCK_PORT         GPIOA
#define BSP_DISPLAY_SCK_PIN          5U
#define BSP_DISPLAY_MOSI_PORT        GPIOA
#define BSP_DISPLAY_MOSI_PIN         7U
#define BSP_DISPLAY_SPI_AF           5U
#define BSP_DISPLAY_CS_PORT          GPIOA
#define BSP_DISPLAY_CS_PIN           4U
#define BSP_DISPLAY_DC_PORT          GPIOB
#define BSP_DISPLAY_DC_PIN           0U
#define BSP_DISPLAY_RST_PORT         GPIOB
#define BSP_DISPLAY_RST_PIN          1U
#define BSP_DISPLAY_BL_PORT          GPIOB
#define BSP_DISPLAY_BL_PIN           10U

/* Mặc định BL nối nguồn 3.3 V theo thông số module, KHÔNG nối tải LED vào PB10.
 * Nếu muốn bật/tắt: PB10 điều khiển tầng transistor/MOSFET hoặc ngõ BL logic
 * đã được xác minh. Khi đó đổi macro này thành 1; không có PWM dimming.
 */
#ifndef BSP_DISPLAY_BL_CONTROLLED
#define BSP_DISPLAY_BL_CONTROLLED    0
#endif
#define BSP_DISPLAY_BL_ACTIVE_HIGH   1U

/* SPI mode 0, MSB first; APB2 / 16 = 6 MHz khi CPU 96 MHz.
 * /16 giữ tốc độ thấp để thử dây breadboard. Không dùng MISO.
 */
#define BSP_DISPLAY_SPI_BR           (3UL << SPI_CR1_BR_Pos)
#ifndef BSP_DISPLAY_USE_DMA
#define BSP_DISPLAY_USE_DMA          1
#endif
#define BSP_DISPLAY_DMA              DMA2
#define BSP_DISPLAY_DMA_STREAM       DMA2_Stream3
#define BSP_DISPLAY_DMA_IRQ          DMA2_Stream3_IRQn
#define BSP_DISPLAY_DMA_HANDLER      DMA2_Stream3_IRQHandler
#define BSP_DISPLAY_DMA_ENABLE()     (RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN)
#define BSP_DISPLAY_DMA_CHANNEL      (3UL << DMA_SxCR_CHSEL_Pos)
#define BSP_DISPLAY_DMA_TC           DMA_LISR_TCIF3
#define BSP_DISPLAY_DMA_ERRORS       (DMA_LISR_TEIF3 | DMA_LISR_DMEIF3 | DMA_LISR_FEIF3)
#define BSP_DISPLAY_DMA_CLEAR        (DMA_LIFCR_CTCIF3 | DMA_LIFCR_CHTIF3 | \
                                     DMA_LIFCR_CTEIF3 | DMA_LIFCR_CDMEIF3 | DMA_LIFCR_CFEIF3)

/* V1.1 là nhãn PCB, chưa xác định tab/panel thực tế. TODO: hiệu chỉnh trên kit.
 * Profile thử ban đầu: offset (0,0), portrait MX|MY|BGR = 0xC8.
 * Nếu lệch ảnh thử offset (2,1); nếu đỏ/xanh đổi chỗ đảo bit BGR (0x08).
 * Không bật MV (0x20), vì bố cục phần mềm cố định portrait 128x160.
 */
#ifndef BSP_DISPLAY_X_OFFSET
#define BSP_DISPLAY_X_OFFSET         0U
#endif
#ifndef BSP_DISPLAY_Y_OFFSET
#define BSP_DISPLAY_Y_OFFSET         0U
#endif
#ifndef BSP_DISPLAY_MADCTL
#define BSP_DISPLAY_MADCTL           0xC8U
#endif
#ifndef BSP_DISPLAY_INVERT
#define BSP_DISPLAY_INVERT           0
#endif
/* Profile tham chiếu ST component ST7735 cho panel 128x160 (frame/power/gamma).
 * Chưa xác nhận analog trên module thật; đặt 0 để thử mặc định sau reset. */
#ifndef BSP_DISPLAY_PANEL_PROFILE
#define BSP_DISPLAY_PANEL_PROFILE    1
#endif

#endif
