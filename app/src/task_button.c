#include "task_button.h"
/* 3. The MCU task reads PA0 every 5 ms and sends commands through IPC. */
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
