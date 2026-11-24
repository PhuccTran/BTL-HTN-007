#ifndef HAL_SPI_H
#define HAL_SPI_H
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "FreeRTOS.h"

typedef enum { HAL_SPI_OK, HAL_SPI_ERROR, HAL_SPI_TIMEOUT, HAL_SPI_INVALID } hal_spi_status_t;
/* prepare trước scheduler; write trong task và khi giữ Mutex ST7735. */
BaseType_t hal_spi_display_prepare(void);
hal_spi_status_t hal_spi_display_write(const uint8_t *data, size_t length, TickType_t timeout);
#endif
