#include "hal_sensor.h"
#include "bmi160_service.h"
#include "hal_i2c.h"
#include "FreeRTOS.h"
#include "task.h"

bool hal_sensor_configure(void)
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

bool hal_sensor_read(int16_t *raw_x, int16_t *raw_y)
{
    bmi160_raw_data_t raw;
    if (bmi160_service_read_raw(&raw) != BMI160_OK) { return false; }
    *raw_x = raw.acc_x; *raw_y = raw.acc_y;
    return true;
}
