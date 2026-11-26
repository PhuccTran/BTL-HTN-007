#ifndef APP_UART_H
#define APP_UART_H

#include "FreeRTOS.h"
#include "app_bmi160.h"

/* Layer 3: Chu kỳ gửi UART định kỳ (ms) */
#define APP_UART_PERIOD_MS 1000UL

/* Gọi một lần từ main trước khi vTaskStartScheduler() */
BaseType_t app_uart_start(void);

/* Hàm đẩy dữ liệu cảm biến từ các task khác vào Queue gửi UART */
BaseType_t app_uart_post_sensor_data(const bmi160_data_msg_t *msg);

#endif /* APP_UART_H */
