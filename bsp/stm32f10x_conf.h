#ifndef STM32F10X_CONF_H
#define STM32F10X_CONF_H

/* The F1 API subset is implemented in bsp/stm32f411_compat.c. */

/* Keep SPL parameter checks active; the hook is in platform/rtos_hooks.c. */
void assert_failed(uint8_t *file, uint32_t line);
#define assert_param(expression) \
    ((expression) ? (void)0 : assert_failed((uint8_t *)__FILE__, __LINE__))

#endif
