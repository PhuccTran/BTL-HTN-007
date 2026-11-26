#include "task_button.h"
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

/* 3. The MCU task reads PA0 every 5 ms and sends commands through IPC. */
#ifndef GAME_HOST_TEST
#include "task.h"
#include "game_config.h"
#include "app_game_ipc.h"
#include "hal_button.h"
#include "stm32f4xx.h"
#include <string.h>

static TaskHandle_t s_button_task;
static GameButtonStats_t s_stats;
static void prvButtonTask(void *argument)
{
    (void)argument;
    ButtonGesture_t button;
    ButtonEvent_t pending = BUTTON_EVENT_NONE;
    TickType_t wake = xTaskGetTickCount();
    button_gesture_init(&button, hal_button_pressed(), wake * portTICK_PERIOD_MS);
    for (;;) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (pending == BUTTON_EVENT_NONE) {
            pending = button_gesture_update(&button, hal_button_pressed(), now);
            taskENTER_CRITICAL();
            if (pending == BUTTON_EVENT_NEXT_LEVEL) { ++s_stats.short_presses; }
            if (pending == BUTTON_EVENT_RESTART) { ++s_stats.long_presses; }
            taskEXIT_CRITICAL();
        }
        if (pending != BUTTON_EVENT_NONE) {
            GameCommand_t command = pending == BUTTON_EVENT_RESTART ?
                GAME_COMMAND_RESTART : GAME_COMMAND_NEXT_LEVEL;
            if (app_game_command_post(command) == pdPASS) { pending = BUTTON_EVENT_NONE; }
            else { taskENTER_CRITICAL(); ++s_stats.queue_retries; taskEXIT_CRITICAL(); }
        }
        UBaseType_t hwm = uxTaskGetStackHighWaterMark(NULL);
        taskENTER_CRITICAL(); s_stats.stack_free_words = hwm; taskEXIT_CRITICAL();
        (void)xTaskDelayUntil(&wake, pdMS_TO_TICKS(APP_BUTTON_PERIOD_MS));
    }
}
void task_button_get_stats(GameButtonStats_t *out)
{
    if (out != NULL) { taskENTER_CRITICAL(); memcpy(out, &s_stats, sizeof(*out)); taskEXIT_CRITICAL(); }
}
BaseType_t task_button_create(void)
{
    if (__get_IPSR() != 0U || xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        return pdFAIL;
    }
    if (s_button_task != NULL) { return pdPASS; }
    hal_button_init();
    return xTaskCreate(prvButtonTask, "BUTTON", APP_BUTTON_STACK_WORDS, NULL,
                       APP_BUTTON_PRIORITY, &s_button_task);
}

#endif
