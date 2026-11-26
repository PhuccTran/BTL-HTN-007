#include "uart_service.h"
#include "hal_uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <string.h>

#define UART_SERVICE_BAUD_RATE       115200UL
#define UART_SERVICE_LOCK_TIMEOUT_MS 100UL
#define UART_SERVICE_TX_TIMEOUT_MS   100UL

static SemaphoreHandle_t s_uart_mutex;

uart_service_status_t uart_service_init(void)
{
    if (s_uart_mutex != NULL)
    {
        return UART_SERVICE_OK;
    }
    if (hal_uart_init(UART_SERVICE_BAUD_RATE) != HAL_UART_OK)
    {
        return UART_SERVICE_IO_ERROR;
    }
    s_uart_mutex = xSemaphoreCreateMutex();
    return (s_uart_mutex != NULL) ? UART_SERVICE_OK : UART_SERVICE_IO_ERROR;
}

uart_service_status_t uart_service_send(const char *text)
{
    if (text == NULL)
    {
        return UART_SERVICE_INVALID_ARGUMENT;
    }
    if (s_uart_mutex == NULL || xTaskGetSchedulerState() != taskSCHEDULER_RUNNING)
    {
        return UART_SERVICE_NOT_READY;
    }

    size_t length = strlen(text);
    if (length == 0U)
    {
        return UART_SERVICE_OK;
    }

    /* Đảm bảo an toàn thread-safe: chỉ 1 task được dùng UART tại 1 thời điểm */
    if (xSemaphoreTake(s_uart_mutex, pdMS_TO_TICKS(UART_SERVICE_LOCK_TIMEOUT_MS)) != pdTRUE)
    {
        return UART_SERVICE_BUSY;
    }

    hal_uart_status_t result = hal_uart_write((const uint8_t *)text, length, UART_SERVICE_TX_TIMEOUT_MS);
    (void)xSemaphoreGive(s_uart_mutex);

    if (result == HAL_UART_OK)
    {
        return UART_SERVICE_OK;
    }
    if (result == HAL_UART_TIMEOUT)
    {
        return UART_SERVICE_IO_TIMEOUT;
    }
    return UART_SERVICE_IO_ERROR;
}
