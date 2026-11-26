#ifndef TASK_BUTTON_H
#define TASK_BUTTON_H
#include <stdbool.h>
#include <stdint.h>

#include "button_logic.h"
#include "FreeRTOS.h"
typedef struct {
    uint32_t short_presses, long_presses, queue_retries;
    UBaseType_t stack_free_words;
} GameButtonStats_t;
BaseType_t task_button_create(void);
void task_button_get_stats(GameButtonStats_t *out);
#endif
