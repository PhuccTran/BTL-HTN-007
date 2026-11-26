#include "sensor_processing.h"
#include "game_config.h"
#include <string.h>

void sensor_processing_init(SensorProcessing_t *state)
{ memset(state, 0, sizeof(*state)); }

void sensor_processing_reset_window(SensorProcessing_t *state)
{ state->count = 0U; state->sum_x = 0; state->sum_y = 0; }

void sensor_processing_update(SensorProcessing_t *state, int16_t raw_x,
                              int16_t raw_y, SensorSample_t *sample)
{
    int16_t ax = (int16_t)((int32_t)raw_x * 1000 / 16384);
    int16_t ay = (int16_t)((int32_t)raw_y * 1000 / 16384);
    if (!state->calibrated) {
        if (state->count == 0U) {
            state->min_x = ax; state->max_x = ax;
            state->min_y = ay; state->max_y = ay;
        }
        if (ax < state->min_x) { state->min_x = ax; }
        if (ax > state->max_x) { state->max_x = ax; }
        if (ay < state->min_y) { state->min_y = ay; }
        if (ay > state->max_y) { state->max_y = ay; }
        state->sum_x += ax; state->sum_y += ay; ++state->count;
        if (state->max_x - state->min_x > APP_SENSOR_CAL_SPREAD_MG ||
            state->max_y - state->min_y > APP_SENSOR_CAL_SPREAD_MG) {
            sensor_processing_reset_window(state); ++state->calibration_restarts;
        }
        if (state->count >= APP_SENSOR_CAL_SAMPLES) {
            state->offset_x_mg = (int16_t)(state->sum_x / (int32_t)state->count);
            state->offset_y_mg = (int16_t)(state->sum_y / (int32_t)state->count);
            state->calibrated = true;
        }
    }
    sample->ax_mg = (int16_t)(ax - state->offset_x_mg);
    sample->ay_mg = (int16_t)(ay - state->offset_y_mg);
    sample->valid = state->calibrated;
}

#include "hal_sensor.h"

bool sensor_game_read(SensorProcessing_t *state, SensorSample_t *sample)
{
    int16_t raw_x, raw_y;
    if (!hal_sensor_read(&raw_x, &raw_y)) {
        sensor_processing_reset_window(state);
        sample->ax_mg = 0; sample->ay_mg = 0; sample->valid = false;
        return false;
    }
    sensor_processing_update(state, raw_x, raw_y, sample);
    return true;
}
