#ifndef BUTTON_LOGIC_H
#define BUTTON_LOGIC_H
#include <stdint.h>
#include <stdbool.h>
typedef struct { bool candidate, stable, armed; uint32_t changed_ms; } ButtonDebounce_t;

typedef enum { BUTTON_EVENT_NONE = 0, BUTTON_EVENT_NEXT_LEVEL, BUTTON_EVENT_RESTART } ButtonEvent_t;
typedef struct { ButtonDebounce_t debounce; uint32_t pressed_ms; bool tracking; } ButtonGesture_t;
void button_gesture_init(ButtonGesture_t *button, bool pressed, uint32_t now_ms);
/* Short press is emitted on release; long hold emits once and consumes release. */
ButtonEvent_t button_gesture_update(ButtonGesture_t *button, bool pressed, uint32_t now_ms);


#endif
