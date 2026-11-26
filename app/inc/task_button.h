#ifndef TASK_BUTTON_H
#define TASK_BUTTON_H
#include <stdbool.h>
#include <stdint.h>

typedef struct { bool candidate, stable, armed; uint32_t changed_ms; } ButtonDebounce_t;

typedef enum { BUTTON_EVENT_NONE = 0, BUTTON_EVENT_NEXT_LEVEL, BUTTON_EVENT_RESTART } ButtonEvent_t;
typedef struct { ButtonDebounce_t debounce; uint32_t pressed_ms; bool tracking; } ButtonGesture_t;
void button_gesture_init(ButtonGesture_t *button, bool pressed, uint32_t now_ms);
/* Short press is emitted on release; long hold emits once and consumes release. */
ButtonEvent_t button_gesture_update(ButtonGesture_t *button, bool pressed, uint32_t now_ms);

#ifndef GAME_HOST_TEST
#include "FreeRTOS.h"
typedef struct {
    uint32_t short_presses, long_presses, queue_retries;
    UBaseType_t stack_free_words;
} GameButtonStats_t;
BaseType_t task_button_create(void);
void task_button_get_stats(GameButtonStats_t *out);
#endif
#endif
