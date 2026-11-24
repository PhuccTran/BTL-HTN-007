#ifndef BSP_DISPLAY_H
#define BSP_DISPLAY_H

#include "stm32f4xx.h"

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

/* BL mặc định cấp nguồn riêng; chỉ bật GPIO khi có tầng điều khiển phù hợp. */
#ifndef BSP_DISPLAY_BL_CONTROLLED
#define BSP_DISPLAY_BL_CONTROLLED    0
#endif
#define BSP_DISPLAY_BL_ACTIVE_HIGH   1U

/* SPI mode 0, MSB trước; SCK = APB2/16. */
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

/* Portrait 128x160: không bật bit MV (0x20). */
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

#ifndef BSP_DISPLAY_PANEL_PROFILE
#define BSP_DISPLAY_PANEL_PROFILE    1
#endif

#endif
