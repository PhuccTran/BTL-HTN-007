#ifndef HAL_UART_H
#define HAL_UART_H

#include <stddef.h>
#include <stdint.h>

/* Layer 1: USART1 hardware only; PA9 TX, 8 data bits, no parity, 1 stop bit. */
typedef enum
{
    HAL_UART_OK = 0,
    HAL_UART_INVALID_ARGUMENT,
    HAL_UART_NOT_READY,
    HAL_UART_TIMEOUT
} hal_uart_status_t;

/* Call once before starting the scheduler. Supported baud: 1200..115200. */
hal_uart_status_t hal_uart_init(uint32_t baud_rate);

/* Task context only, scheduler running. The caller must serialize writers.
 * timeout_ms bounds the entire frame, including the final TC flag.
 * A timeout can leave a partial frame on the wire; data is not retried here.
 * The data buffer remains owned by the caller; the call is synchronous.
 */
hal_uart_status_t hal_uart_write(const uint8_t *data, size_t length,
                                 uint32_t timeout_ms);

#endif
