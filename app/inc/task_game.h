#ifndef TASK_GAME_H
#define TASK_GAME_H
#include "FreeRTOS.h"
#include <stdint.h>
typedef struct {
    uint32_t frames, display_retries, overruns, restarts, manual_levels;
    UBaseType_t stack_free_words;
    size_t heap_free_bytes, heap_min_bytes;
    uint8_t level, lives, state, sensor_fresh;
} GameRuntimeStats_t;
BaseType_t task_game_create(void);
void task_game_get_stats(GameRuntimeStats_t *out);
#endif
