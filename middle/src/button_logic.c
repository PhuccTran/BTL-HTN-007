#include "button_logic.h"
#include "game_config.h"

/* 1. Debounce: accept a stable edge after 30 ms. */
static void button_debounce_init(ButtonDebounce_t *button, bool pressed, uint32_t now_ms)
{
    button->candidate = pressed; button->stable = pressed;
    button->armed = !pressed; button->changed_ms = now_ms;
}
static bool button_debounce_update(ButtonDebounce_t *button, bool pressed, uint32_t now_ms)
{
    if (pressed != button->candidate) {
        button->candidate = pressed; button->changed_ms = now_ms;
    }
    if (button->stable == button->candidate ||
        now_ms - button->changed_ms < GAME_BUTTON_DEBOUNCE_MS) { return false; }
    button->stable = button->candidate;
    if (!button->stable) { button->armed = true; return false; }
    if (!button->armed) { return false; }
    button->armed = false; return true;
}

/* 2. Short release / one long hold event. Shared by MCU and host tests. */
void button_gesture_init(ButtonGesture_t *button, bool pressed, uint32_t now_ms)
{
    button_debounce_init(&button->debounce, pressed, now_ms);
    button->pressed_ms = now_ms; button->tracking = false;
}
ButtonEvent_t button_gesture_update(ButtonGesture_t *button, bool pressed, uint32_t now_ms)
{
    if (button_debounce_update(&button->debounce, pressed, now_ms)) {
        button->tracking = true; button->pressed_ms = now_ms;
    }
    if (!button->tracking) { return BUTTON_EVENT_NONE; }
    if (!button->debounce.stable) {
        button->tracking = false; return BUTTON_EVENT_NEXT_LEVEL;
    }
    if (now_ms - button->pressed_ms >= GAME_BUTTON_LONG_MS) {
        button->tracking = false; return BUTTON_EVENT_RESTART;
    }
    return BUTTON_EVENT_NONE;
}

