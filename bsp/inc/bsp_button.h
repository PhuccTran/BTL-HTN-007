#ifndef BSP_BUTTON_H
#define BSP_BUTTON_H
#include "stm32f4xx.h"
/* On WeAct Black Pill boards with a KEY user button, KEY pulls PA0 to GND.
 * Confirm the board has KEY. NRST and BOOT0 are not gameplay buttons. */
#define BSP_BUTTON_PORT GPIOA
#define BSP_BUTTON_PIN  0U
#define BSP_BUTTON_CLOCK_ENABLE() (RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN)
#endif
