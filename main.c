/* STM32F103C8T6: startup calls SystemInit() to configure HSE 8 MHz * 9 = 72 MHz. */
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f10x.h"
#include "app_uart.h"
#include "app_led.h"
#include "app_bmi160.h"

int main(void)
{
    /* Update SystemCoreClock to reflect the hardware frequency set in SystemInit. */
    SystemCoreClockUpdate();

    /* FreeRTOS requires priority bits configured for preemption. */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_3);

    /* The top app layer owns resource initialization and task creation. */
    BaseType_t status = app_uart_start();
    configASSERT(status == pdPASS);

    status = app_led_start();
    configASSERT(status == pdPASS);

    status = app_bmi160_start();
    configASSERT(status == pdPASS);

    vTaskStartScheduler();

    /* The scheduler should never return. If it returns, heap allocation failed. */
    configASSERT(pdFALSE);
    taskDISABLE_INTERRUPTS();
    for (;;)
    {
    }
}
