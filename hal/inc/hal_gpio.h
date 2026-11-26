#ifndef HAL_GPIO_H
#define HAL_GPIO_H
#include <stdbool.h>

/* Lớp mỏng TẠM của phần màn hình. Thay bằng HAL GPIO chung khi nhóm merge.
 * Chỉ driver màn hình được sở hữu các pin này; không dùng từ ISR.
 */
void hal_gpio_display_init(void);
void hal_gpio_display_cs(bool selected);
void hal_gpio_display_dc(bool data);
void hal_gpio_display_reset(bool asserted);
bool hal_gpio_display_backlight(bool on);
#endif
