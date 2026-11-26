#include "app_led.h"
#include "stm32f10x.h"
#include "task.h"

static TaskHandle_t s_led_task;

static void app_led_task(void *argument)
{
    (void)argument;
    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(APP_LED_BLINK_PERIOD_MS);
    configASSERT(period > 0U);

    for (;;)
    {
        /* Toggle PC13 (Blue Pill on-board LED is active-low: 0 = ON, 1 = OFF) */
        GPIOC->ODR ^= GPIO_Pin_13;

        (void)xTaskDelayUntil(&last_wake, period);
    }
}

BaseType_t app_led_start(void)
{
    GPIO_InitTypeDef gpio;

    if (s_led_task != NULL)
    {
        return pdPASS;
    }

    /* Enable GPIOC peripheral clock */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    /* Configure PC13: output push-pull, max speed 2 MHz (RTC domain IO limit) */
    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = GPIO_Pin_13;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOC, &gpio);

    /* Start with LED turned off (PC13 active-low: High = OFF) */
    GPIO_SetBits(GPIOC, GPIO_Pin_13);

    return xTaskCreate(app_led_task, "LED_BLINK", configMINIMAL_STACK_SIZE, NULL,
                       tskIDLE_PRIORITY + 1U, &s_led_task);
}
