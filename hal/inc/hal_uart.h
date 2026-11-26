#ifndef HAL_UART_H
#define HAL_UART_H

#include <stdbool.h>
#include <stdint.h>

/* Layer 1 (HAL): USART1 TX trên chân PA9 (AF7), chế độ polling */
bool hal_uart_init(uint32_t baudrate);
void hal_uart_write(const char *str);

#endif /* HAL_UART_H */
