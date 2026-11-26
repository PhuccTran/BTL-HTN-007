#ifndef TASK_SENSOR_GAME_H
#define TASK_SENSOR_GAME_H
#include "FreeRTOS.h"
#include <stdint.h>
typedef struct {
    uint32_t samples, failures, calibration_restarts;
    int16_t offset_x_mg, offset_y_mg;
    UBaseType_t stack_free_words;
    uint8_t ready, calibrated;
} GameSensorStats_t;
BaseType_t task_sensor_game_create(void);
void task_sensor_game_get_stats(GameSensorStats_t *out);
#endif
