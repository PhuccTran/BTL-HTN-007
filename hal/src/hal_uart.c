/* Layer 1: configure hardware through ST Standard Peripheral Library. */
#include "hal_uart.h"
#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"

static uint8_t s_initialized;

hal_uart_status_t hal_uart_init(uint32_t baud_rate)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef usart;

    if (baud_rate < 1200UL || baud_rate > 115200UL)
    {
        return HAL_UART_INVALID_ARGUMENT;
    }

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);
    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = GPIO_Pin_9;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &gpio);

    USART_DeInit(USART1);
    USART_StructInit(&usart);
    usart.USART_BaudRate = baud_rate;
    usart.USART_WordLength = USART_WordLength_8b;
    usart.USART_StopBits = USART_StopBits_1;
    usart.USART_Parity = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode = USART_Mode_Tx;
    USART_Init(USART1, &usart);
    USART_Cmd(USART1, ENABLE);
    s_initialized = 1U;
    return HAL_UART_OK;
}

static hal_uart_status_t wait_flag(uint16_t flag, TickType_t start,
                                   TickType_t budget)
{
    while (USART_GetFlagStatus(USART1, flag) == RESET)
    {
        if ((TickType_t)(xTaskGetTickCount() - start) >= budget)
        {
            return HAL_UART_TIMEOUT;
        }
    }
    return HAL_UART_OK;
}

hal_uart_status_t hal_uart_write(const uint8_t *data, size_t length,
                                 uint32_t timeout_ms)
{
    TickType_t start;
    TickType_t budget;

    if (data == NULL || length == 0U || timeout_ms == 0UL || timeout_ms > 60000UL)
    {
        return HAL_UART_INVALID_ARGUMENT;
    }
    if (s_initialized == 0U || xTaskGetSchedulerState() != taskSCHEDULER_RUNNING)
    {
        return HAL_UART_NOT_READY;
    }

    budget = pdMS_TO_TICKS(timeout_ms);
    if (budget == 0U)
    {
        budget = 1U;
    }
    start = xTaskGetTickCount();
    for (size_t index = 0U; index < length; ++index)
    {
        hal_uart_status_t result = wait_flag(USART_FLAG_TXE, start, budget);
        if (result != HAL_UART_OK)
        {
            return result;
        }
        USART_SendData(USART1, data[index]);
    }

    /* TXE only means DR empty; TC confirms the last stop bit left the wire. */
    return wait_flag(USART_FLAG_TC, start, budget);
}
