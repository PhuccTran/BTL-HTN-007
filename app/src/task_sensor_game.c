#include "task_sensor_game.h"
#include "task.h"
#include "game_config.h"
#include "app_game_ipc.h"
#include "sensor_processing.h"
#include "stm32f4xx.h"
#include <stdbool.h>

static TaskHandle_t s_sensor_task;

/* This adapter is the only owner of the inherited BMI service in this build.
 * The original UART demo and its second physics integrator are not started. */

static void prvPublish(int16_t ax, int16_t ay, bool valid)
{
    GameSensorMsg_t sample = {0};
    sample.ax_mg = ax; sample.ay_mg = ay;
    sample.sampled_at = xTaskGetTickCount(); sample.valid = valid ? 1U : 0U;
    configASSERT(app_game_sensor_post(&sample) == pdPASS); // gửi vào queue
}

static void prvSensorTask(void *argument)
{
    (void)argument;
    bool configured = false;
    SensorProcessing_t processing;
    sensor_processing_init(&processing);
    /* Config huong cam bien: (swap_xy, sign_x, sign_y) */
    sensor_processing_set_orientation(&processing, GAME_SWAP_AXES != 0,
                                      GAME_X_SIGN, GAME_Y_SIGN);
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        if (!configured) { // nếu chưa config thì config lại
            configured = bmi160_configure();
            if (!configured) { // nêu config lỗi thì gửi dữ liệu 0,0 và delay 1s để retry
                prvPublish(0, 0, false);
                vTaskDelay(pdMS_TO_TICKS(APP_SENSOR_RETRY_MS));
                wake = xTaskGetTickCount(); continue;
            }
            wake = xTaskGetTickCount();
        }
        int16_t raw_x, raw_y;
        SensorSample_t sample;
        if (!bmi160_read_raw(&raw_x, &raw_y)) { // nếu đọc lỗi thì gửi 0,0
            configured = false;
            prvPublish(0, 0, false);
        } else { // nếu đọc được thì đổi đơn vị, chỉnh hướng và gửi dữ liệu vào queue
            sensor_processing_update(&processing, raw_x, raw_y, &sample);
            prvPublish(sample.ax_mg, sample.ay_mg, sample.valid);
        }
        (void)xTaskDelayUntil(&wake, pdMS_TO_TICKS(APP_SENSOR_PERIOD_MS));
    }
}


BaseType_t task_sensor_game_create(void) 
{
    if (__get_IPSR() != 0U || xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) { // không được gọi từ ISR và scheduler chưa start
        return pdFAIL;
    }
    if (s_sensor_task != NULL) { return pdPASS; } // nếu task đã tạo thì return success luôn 
    return xTaskCreate(prvSensorTask, "SENSOR_GAME", APP_SENSOR_STACK_WORDS, NULL,
                       APP_SENSOR_PRIORITY, &s_sensor_task);
}
