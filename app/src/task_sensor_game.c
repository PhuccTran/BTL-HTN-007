#include "task_sensor_game.h"
#include "task.h"
#include "game_config.h"
#include "app_game_ipc.h"
#include "sensor_processing.h"
#include "stm32f4xx.h"
#include <stdbool.h>
#include <string.h>

static TaskHandle_t s_sensor_task;
static GameSensorStats_t s_stats;

/* This adapter is the only owner of the inherited BMI service in this build.
 * The original UART demo and its second physics integrator are not started. */

static void prvPublish(int16_t ax, int16_t ay, bool valid)
{
    GameSensorMsg_t sample = {0};
    sample.ax_mg = ax; sample.ay_mg = ay;
    sample.sampled_at = xTaskGetTickCount(); sample.valid = valid ? 1U : 0U;
    configASSERT(app_game_sensor_post(&sample) == pdPASS);
}

static void prvSensorTask(void *argument)
{
    (void)argument;
    bool configured = false;
    SensorProcessing_t processing;
    sensor_processing_init(&processing);
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        if (!configured) {
            configured = sensor_game_configure();
            taskENTER_CRITICAL(); s_stats.ready = configured ? 1U : 0U; taskEXIT_CRITICAL();
            if (!configured) {
                prvPublish(0, 0, false);
                vTaskDelay(pdMS_TO_TICKS(APP_SENSOR_RETRY_MS));
                wake = xTaskGetTickCount(); continue;
            }
            wake = xTaskGetTickCount();
        }
        SensorSample_t sample;
        if (!sensor_game_read(&processing, &sample)) {
            configured = false;
            taskENTER_CRITICAL(); ++s_stats.failures; s_stats.ready = 0U; taskEXIT_CRITICAL();
            prvPublish(0, 0, false);
        } else {
            prvPublish(sample.ax_mg, sample.ay_mg, sample.valid);
            taskENTER_CRITICAL();
            ++s_stats.samples;
            s_stats.offset_x_mg = processing.offset_x_mg;
            s_stats.offset_y_mg = processing.offset_y_mg;
            s_stats.calibration_restarts = processing.calibration_restarts;
            s_stats.calibrated = processing.calibrated ? 1U : 0U;
            taskEXIT_CRITICAL();
        }
        UBaseType_t hwm = uxTaskGetStackHighWaterMark(NULL);
        taskENTER_CRITICAL(); s_stats.stack_free_words = hwm; taskEXIT_CRITICAL();
        (void)xTaskDelayUntil(&wake, pdMS_TO_TICKS(APP_SENSOR_PERIOD_MS));
    }
}


BaseType_t task_sensor_game_create(void)
{
    if (__get_IPSR() != 0U || xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        return pdFAIL;
    }
    if (s_sensor_task != NULL) { return pdPASS; }
    return xTaskCreate(prvSensorTask, "SENSOR_GAME", APP_SENSOR_STACK_WORDS, NULL,
                       APP_SENSOR_PRIORITY, &s_sensor_task);
}
void task_sensor_game_get_stats(GameSensorStats_t *out)
{
    if (out != NULL) { taskENTER_CRITICAL(); memcpy(out, &s_stats, sizeof(*out)); taskEXIT_CRITICAL(); }
}
