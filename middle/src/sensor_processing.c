/* Layer 2 (Middle): BMI160 driver + Chuyen doi don vi & hieu chuan */
#include "sensor_processing.h"
#include "game_config.h"
#include "hal_i2c.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stddef.h>
#include <string.h>

#define BMI160_I2C_ADDR_LOW     0x68U
#define BMI160_I2C_ADDR_HIGH    0x69U
#define BMI160_CHIP_ID_VAL      0xD1U

#define BMI160_REG_CHIP_ID      0x00U
#define BMI160_REG_PMU_STATUS   0x03U
#define BMI160_REG_ACC_X_L      0x12U /* 0x12..0x15: ACC_X_L, ACC_X_H, ACC_Y_L, ACC_Y_H */
#define BMI160_REG_ACC_CONF     0x40U
#define BMI160_REG_ACC_RANGE    0x41U
#define BMI160_REG_CMD          0x7EU

#define BMI160_CMD_SOFT_RESET   0xB6U
#define BMI160_CMD_ACC_NORMAL   0x11U

static uint8_t s_device_addr = BMI160_I2C_ADDR_LOW;

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

bool bmi160_configure(void)
{
    if (!hal_i2c_init()) { return false; }
    vTaskDelay(pdMS_TO_TICKS(10U));

    uint8_t id = 0U;
    s_device_addr = BMI160_I2C_ADDR_LOW;
    if (!hal_i2c_read_byte(s_device_addr, BMI160_REG_CHIP_ID, &id) || id != BMI160_CHIP_ID_VAL) {
        s_device_addr = BMI160_I2C_ADDR_HIGH;
        if (!hal_i2c_read_byte(s_device_addr, BMI160_REG_CHIP_ID, &id) || id != BMI160_CHIP_ID_VAL) {
            return false;
        }
    }

    if (!hal_i2c_write_reg(s_device_addr, BMI160_REG_CMD, BMI160_CMD_SOFT_RESET)) { return false; }
    vTaskDelay(pdMS_TO_TICKS(25U));

    if (!hal_i2c_write_reg(s_device_addr, BMI160_REG_CMD, BMI160_CMD_ACC_NORMAL)) { return false; }
    vTaskDelay(pdMS_TO_TICKS(10U));

    if (!hal_i2c_write_reg(s_device_addr, BMI160_REG_ACC_CONF, 0x28U) ||
        !hal_i2c_write_reg(s_device_addr, BMI160_REG_ACC_RANGE, 0x03U)) {
        return false;
    }

    uint8_t range = 0U, conf = 0U, pmu = 0U;
    if (!hal_i2c_read_byte(s_device_addr, BMI160_REG_ACC_RANGE, &range) ||
        !hal_i2c_read_byte(s_device_addr, BMI160_REG_ACC_CONF, &conf) ||
        !hal_i2c_read_byte(s_device_addr, BMI160_REG_PMU_STATUS, &pmu)) {
        return false;
    }

    return range == 0x03U && (conf & 0x0FU) == 0x08U && (pmu & 0x30U) == 0x10U;
}

bool bmi160_read_raw(int16_t *raw_x, int16_t *raw_y)
{
    if (raw_x == NULL || raw_y == NULL) { return false; }

    uint8_t buf[4];
    if (!hal_i2c_read(s_device_addr, BMI160_REG_ACC_X_L, buf, sizeof(buf))) {
        return false;
    }

    *raw_x = (int16_t)(((uint16_t)buf[1] << 8U) | buf[0]);
    *raw_y = (int16_t)(((uint16_t)buf[3] << 8U) | buf[2]);
    return true;
}
