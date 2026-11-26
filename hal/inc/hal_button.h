#ifndef HAL_BUTTON_H
#define HAL_BUTTON_H
#include <stdbool.h>
void hal_button_init(void);
bool hal_button_pressed(void);
#endif
