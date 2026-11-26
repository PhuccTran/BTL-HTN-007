#ifndef APP_LED_H
#define APP_LED_H

#include "FreeRTOS.h"

/* LED blink toggle period in milliseconds. 500 ms toggle = 1 Hz blink. */
#define APP_LED_BLINK_PERIOD_MS 500UL

/* Call once from main before vTaskStartScheduler(). */
BaseType_t app_led_start(void);

#endif /* APP_LED_H */
