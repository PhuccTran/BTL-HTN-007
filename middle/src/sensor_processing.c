/* Layer 2 (Middle): BMI160 driver + Chuyen doi don vi & huong */
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

_Static_assert(GAME_X_SIGN == 1 || GAME_X_SIGN == -1, "Invalid X sign");
_Static_assert(GAME_Y_SIGN == 1 || GAME_Y_SIGN == -1, "Invalid Y sign");

static uint8_t s_device_addr = BMI160_I2C_ADDR_LOW;

void sensor_processing_init(SensorProcessing_t *state)
{
    memset(state, 0, sizeof(*state));
    state->swap_xy = (GAME_SWAP_AXES != 0);
    state->sign_x = GAME_X_SIGN;
    state->sign_y = GAME_Y_SIGN;
}

void sensor_processing_set_orientation(SensorProcessing_t *state, bool swap_xy,
                                       int8_t sign_x, int8_t sign_y)
{
    if (state == NULL) { return; }
    state->swap_xy = swap_xy;
    state->sign_x = (sign_x < 0) ? -1 : 1;
    state->sign_y = (sign_y < 0) ? -1 : 1;
}

void sensor_processing_update(SensorProcessing_t *state, int16_t raw_x,
                              int16_t raw_y, SensorSample_t *sample)
{
    if (state == NULL || sample == NULL) { return; }

    int16_t ox = (int16_t)((int32_t)raw_x * 1000 / 16384);
    int16_t oy = (int16_t)((int32_t)raw_y * 1000 / 16384);
    if (state->swap_xy) {
        int16_t swap = ox;
        ox = oy;
        oy = swap;
    }
    sample->ax_mg = (int16_t)(ox * state->sign_x);
    sample->ay_mg = (int16_t)(oy * state->sign_y);
    sample->valid = true;
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
