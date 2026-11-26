#ifndef UART_SERVICE_H
#define UART_SERVICE_H

#include <stdint.h>

/* Layer 2: Dịch vụ tài nguyên UART, đa nhiệm (Mutex) và gửi chuỗi */
typedef enum
{
    UART_SERVICE_OK = 0,
    UART_SERVICE_INVALID_ARGUMENT,
    UART_SERVICE_NOT_READY,
    UART_SERVICE_BUSY,
    UART_SERVICE_IO_TIMEOUT,
    UART_SERVICE_IO_ERROR
} uart_service_status_t;

/* Khởi tạo phần cứng UART và tạo Mutex bảo vệ */
uart_service_status_t uart_service_init(void);

/* Gửi một chuỗi ký tự kết thúc bằng '\0' qua UART (Thread-safe, có Mutex) */
uart_service_status_t uart_service_send(const char *text);

#endif /* UART_SERVICE_H */
