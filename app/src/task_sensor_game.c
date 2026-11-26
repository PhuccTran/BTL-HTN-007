#include "task_sensor_game.h"
#include "task.h"
#include "game_config.h"
#include "app_game_ipc.h"
#include "bmi160_service.h"
#include "hal_i2c.h"
#include "stm32f4xx.h"
#include <stdbool.h>
#include <string.h>

static TaskHandle_t s_sensor_task;
static GameSensorStats_t s_stats;

/* This adapter is the only owner of the inherited BMI service in this build.
 * The original UART demo and its second physics integrator are not started. */
#ifndef GAME_SENSOR_MOCK
static bool prvConfigure(void)
{
    if (bmi160_service_init() != BMI160_OK) { return false; }
    uint8_t addr = bmi160_service_get_i2c_addr();
    uint8_t id = 0U, range = 0U, conf = 0U, pmu = 0U;
    if (hal_i2c_read_reg(addr, 0x00U, &id) != HAL_I2C_OK || id != BMI160_CHIP_ID_VAL) {
        return false;
    }
    /* Check every write: the inherited demo intentionally discards write status.
     * Reapply accel settings so unplug/replug can recover without editing it. */
    if (hal_i2c_write_reg(addr, 0x7EU, 0x11U) != HAL_I2C_OK) { return false; }
    vTaskDelay(pdMS_TO_TICKS(10U));
    if (hal_i2c_write_reg(addr, 0x40U, 0x28U) != HAL_I2C_OK ||
        hal_i2c_write_reg(addr, 0x41U, 0x03U) != HAL_I2C_OK) { return false; }
    if (hal_i2c_read_reg(addr, 0x41U, &range) != HAL_I2C_OK ||
        hal_i2c_read_reg(addr, 0x40U, &conf) != HAL_I2C_OK ||
        hal_i2c_read_reg(addr, 0x03U, &pmu) != HAL_I2C_OK) { return false; }
    return range == 0x03U && (conf & 0x0FU) == 0x08U && (pmu & 0x30U) == 0x10U;
}
#endif

static void prvPublish(int16_t ax, int16_t ay, bool valid)
{
    GameSensorMsg_t sample = {0};
    sample.ax_mg = ax; sample.ay_mg = ay;
    sample.sampled_at = xTaskGetTickCount(); sample.valid = valid ? 1U : 0U;
    configASSERT(app_game_sensor_post(&sample) == pdPASS);
}

#ifndef GAME_SENSOR_MOCK
static void prvSensorTask(void *argument)
{
    (void)argument;
    bool configured = false;
    uint32_t count = 0U;
    int32_t sum_x = 0, sum_y = 0;
    int16_t min_x = 0, max_x = 0, min_y = 0, max_y = 0;
    int16_t offset_x = 0, offset_y = 0;
    bool calibrated = false;
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        if (!configured) {
            configured = prvConfigure();
            taskENTER_CRITICAL(); s_stats.ready = configured ? 1U : 0U; taskEXIT_CRITICAL();
            if (!configured) {
                prvPublish(0, 0, false);
                vTaskDelay(pdMS_TO_TICKS(APP_SENSOR_RETRY_MS));
                wake = xTaskGetTickCount(); continue;
            }
            wake = xTaskGetTickCount();
        }
        bmi160_raw_data_t raw;
        if (bmi160_service_read_raw(&raw) != BMI160_OK) {
            configured = false; count = 0U; sum_x = 0; sum_y = 0;
            /* Keep the original level calibration after a transient I2C fault. */
            taskENTER_CRITICAL(); ++s_stats.failures; s_stats.ready = 0U; taskEXIT_CRITICAL();
            prvPublish(0, 0, false);
        } else {
            int16_t ax = (int16_t)((int32_t)raw.acc_x * 1000 / 16384);
            int16_t ay = (int16_t)((int32_t)raw.acc_y * 1000 / 16384);
            if (!calibrated) {
                if (count == 0U) { min_x = ax; max_x = ax; min_y = ay; max_y = ay; }
                if (ax < min_x) { min_x = ax; }
                if (ax > max_x) { max_x = ax; }
                if (ay < min_y) { min_y = ay; }
                if (ay > max_y) { max_y = ay; }
                sum_x += ax; sum_y += ay; ++count;
                if (max_x - min_x > APP_SENSOR_CAL_SPREAD_MG ||
                    max_y - min_y > APP_SENSOR_CAL_SPREAD_MG) {
                    count = 0U; sum_x = 0; sum_y = 0;
                    taskENTER_CRITICAL(); ++s_stats.calibration_restarts; taskEXIT_CRITICAL();
                }
                if (count >= APP_SENSOR_CAL_SAMPLES) {
                    offset_x = (int16_t)(sum_x / (int32_t)count);
                    offset_y = (int16_t)(sum_y / (int32_t)count);
                    calibrated = true;
                }
            }
            prvPublish((int16_t)(ax - offset_x), (int16_t)(ay - offset_y), calibrated);
            taskENTER_CRITICAL();
            ++s_stats.samples; s_stats.offset_x_mg = offset_x; s_stats.offset_y_mg = offset_y;
            s_stats.calibrated = calibrated ? 1U : 0U;
            taskEXIT_CRITICAL();
        }
        UBaseType_t hwm = uxTaskGetStackHighWaterMark(NULL);
        taskENTER_CRITICAL(); s_stats.stack_free_words = hwm; taskEXIT_CRITICAL();
        (void)xTaskDelayUntil(&wake, pdMS_TO_TICKS(APP_SENSOR_PERIOD_MS));
    }
}

#else
/* Independent kit test: same mailbox/API, but no BMI or I2C calls.
 * The current mounting uses sensor Y for horizontal screen movement. */
static void prvSensorTask(void *argument)
{
    (void)argument;
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        uint32_t phase = (xTaskGetTickCount() * portTICK_PERIOD_MS / 1600U) % 4U;
        int16_t screen_x = phase == 0U ? 350 : (phase == 2U ? -350 : 0);
        int16_t ax = (int16_t)(screen_x * GAME_X_SIGN), ay = 0;
#if GAME_SWAP_AXES
        ay = ax; ax = 0;
#endif
        prvPublish(ax, ay, true);
        UBaseType_t hwm = uxTaskGetStackHighWaterMark(NULL);
        taskENTER_CRITICAL();
        ++s_stats.samples; s_stats.ready = 1U; s_stats.calibrated = 1U;
        s_stats.stack_free_words = hwm;
        taskEXIT_CRITICAL();
        (void)xTaskDelayUntil(&wake, pdMS_TO_TICKS(APP_SENSOR_PERIOD_MS));
    }
}
#endif

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
