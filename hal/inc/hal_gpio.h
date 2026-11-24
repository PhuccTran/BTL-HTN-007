#ifndef HAL_GPIO_H
#define HAL_GPIO_H
#include <stdbool.h>

void hal_gpio_display_init(void);
void hal_gpio_display_cs(bool selected);
void hal_gpio_display_dc(bool data);
void hal_gpio_display_reset(bool asserted);
bool hal_gpio_display_backlight(bool on);
#endif
