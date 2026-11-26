#include "hal_button.h"
#include "bsp_button.h"
void hal_button_init(void)
{
    BSP_BUTTON_CLOCK_ENABLE(); (void)RCC->AHB1ENR;
    BSP_BUTTON_PORT->MODER &= ~(3UL << (BSP_BUTTON_PIN * 2U));
    BSP_BUTTON_PORT->PUPDR = (BSP_BUTTON_PORT->PUPDR & ~(3UL << (BSP_BUTTON_PIN * 2U))) |
                            (1UL << (BSP_BUTTON_PIN * 2U));
    /* Polling only: no EXTI enable, no button ISR. */
}
bool hal_button_pressed(void)
{ return (BSP_BUTTON_PORT->IDR & (1UL << BSP_BUTTON_PIN)) == 0U; }
