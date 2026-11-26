#include "task_game.h"
#include "task.h"
#include "game_config.h"
#include "app_game_ipc.h"
#include "game_core.h"
#include "game_display.h"
#include "task_display.h"
#include "stm32f4xx.h"
#include <string.h>

static TaskHandle_t s_game_task;
static Game_t s_game;
static GamePresent_t s_present;
static GameRuntimeStats_t s_stats;

static bool prvFlush(uint32_t now_ms)
{
    const DisplayMsg_t *msg = game_present_peek(&s_present);
    while (msg != NULL) {
        if (task_display_post(msg, 0U) != pdPASS) {
            taskENTER_CRITICAL(); ++s_stats.display_retries; taskEXIT_CRITICAL();
            return false;
        }
        game_present_accept(&s_present); msg = game_present_peek(&s_present);
    }
    if (s_present.has_notice) {
        /* Start the hold time only after the complete notice transaction is queued. */
        s_game.state_since_ms = now_ms; s_present.has_notice = false;
    }
    return true;
}

static void prvSampleStats(bool fresh)
{
    UBaseType_t hwm = uxTaskGetStackHighWaterMark(NULL);
    size_t heap = xPortGetFreeHeapSize(), minimum = xPortGetMinimumEverFreeHeapSize();
    taskENTER_CRITICAL();
    s_stats.stack_free_words = hwm; s_stats.heap_free_bytes = heap;
    s_stats.heap_min_bytes = minimum; s_stats.level = s_game.level;
    s_stats.lives = s_game.lives; s_stats.state = (uint8_t)s_game.state;
    s_stats.sensor_fresh = fresh ? 1U : 0U;
    taskEXIT_CRITICAL();
}

static void prvGameTask(void *argument)
{
    (void)argument;
    TickType_t wake = xTaskGetTickCount();
    game_core_init(&s_game, wake * portTICK_PERIOD_MS);
    game_present_init(&s_present); game_present_prepare(&s_present, &s_game);
    for (;;) {
        TickType_t now = xTaskGetTickCount();
        uint32_t now_ms = now * portTICK_PERIOD_MS;
        GameCommand_t command;
        if (app_game_take_command(&command) == pdPASS) {
            if (command == GAME_COMMAND_RESTART) {
                game_core_restart(&s_game, now_ms);
                taskENTER_CRITICAL(); ++s_stats.restarts; taskEXIT_CRITICAL();
            } else {
                game_core_next_level(&s_game, now_ms);
                taskENTER_CRITICAL(); ++s_stats.manual_levels; taskEXIT_CRITICAL();
            }
            game_present_init(&s_present); game_present_prepare(&s_present, &s_game);
        }
        GameSensorMsg_t sample;
        bool fresh = app_game_sensor_latest(&sample) == pdPASS && sample.valid != 0U &&
                     now - sample.sampled_at <= pdMS_TO_TICKS(APP_SENSOR_STALE_MS);
        if (prvFlush(now_ms)) {
            if (!fresh) { game_core_stop(&s_game); }
            if (fresh || s_game.state != GAME_PLAYING) {
                game_core_step(&s_game, fresh ? sample.ax_mg : 0,
                               fresh ? sample.ay_mg : 0, now_ms);
            }
            game_present_prepare(&s_present, &s_game);
            (void)prvFlush(now_ms);
            taskENTER_CRITICAL(); ++s_stats.frames; taskEXIT_CRITICAL();
        } else {
            /* No movement until control messages are delivered; don't catch up later. */
            game_core_stop(&s_game);
        }
        prvSampleStats(fresh);
        if (xTaskGetTickCount() - wake >= pdMS_TO_TICKS(GAME_STEP_MS)) {
            wake = xTaskGetTickCount();
            taskENTER_CRITICAL(); ++s_stats.overruns; taskEXIT_CRITICAL();
        }
        (void)xTaskDelayUntil(&wake, pdMS_TO_TICKS(GAME_STEP_MS));
    }
}

BaseType_t task_game_create(void)
{
    if (__get_IPSR() != 0U || xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        return pdFAIL;
    }
    if (s_game_task != NULL) { return pdPASS; }
    return xTaskCreate(prvGameTask, "GAME", APP_GAME_STACK_WORDS, NULL,
                       APP_GAME_PRIORITY, &s_game_task);
}
void task_game_get_stats(GameRuntimeStats_t *out)
{
    if (out != NULL) { taskENTER_CRITICAL(); memcpy(out, &s_stats, sizeof(*out)); taskEXIT_CRITICAL(); }
}
