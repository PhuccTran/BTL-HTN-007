/* Layer 2: BMI160 Service - Quản lý cấu hình, giao tiếp với cảm biến qua HAL I2C1 */
#include "bmi160_service.h"
#include "hal_i2c.h"
#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* Các thanh ghi quan trọng của cảm biến BMI160 */
#define BMI160_REG_CHIP_ID          0x00U
#define BMI160_REG_DATA_START       0x0CU /* 0x0C: GYR_X_L -> 0x17: ACC_Z_H (12 bytes) */
#define BMI160_REG_STATUS           0x1BU
#define BMI160_REG_ACC_CONF         0x40U
#define BMI160_REG_ACC_RANGE        0x41U
#define BMI160_REG_GYR_CONF         0x42U
#define BMI160_REG_GYR_RANGE        0x43U
#define BMI160_REG_CMD              0x7EU

/* Các mã lệnh ghi vào thanh ghi CMD */
#define BMI160_CMD_SOFT_RESET       0xB6U
#define BMI160_CMD_ACC_MODE_NORMAL  0x11U
#define BMI160_CMD_GYR_MODE_NORMAL  0x15U

#define BMI160_MUTEX_TIMEOUT_MS     100UL

static uint8_t s_device_addr = BMI160_I2C_ADDR_LOW;
static SemaphoreHandle_t s_bmi160_mutex = NULL;
static uint8_t s_is_initialized = 0U;

static void bmi160_delay_ms(uint32_t ms)
{
    /* Trong môi trường FreeRTOS, luôn dùng vTaskDelay để nhường CPU cho các task khác */
    vTaskDelay(pdMS_TO_TICKS(ms));
}

uint8_t bmi160_service_get_i2c_addr(void)
{
    return s_device_addr;
}

bmi160_status_t bmi160_service_init(void)
{
    uint8_t chip_id = 0;

    if (s_is_initialized != 0U)
    {
        return BMI160_OK;
    }

    if (s_bmi160_mutex == NULL)
    {
        s_bmi160_mutex = xSemaphoreCreateMutex();
    }

    /* 1. Khởi tạo tầng HAL I2C1 với tốc độ 100 kHz */
    if (hal_i2c_init(100000UL) != HAL_I2C_OK)
    {
        return BMI160_ERR_INIT;
    }

    /* Đợi cảm biến ổn định nguồn */
    bmi160_delay_ms(10);

    /* 2. Quét địa chỉ I2C: thử 0x68 trước, nếu không được thì thử 0x69 */
    s_device_addr = BMI160_I2C_ADDR_LOW;
    if (hal_i2c_read_reg(s_device_addr, BMI160_REG_CHIP_ID, &chip_id) != HAL_I2C_OK || chip_id != BMI160_CHIP_ID_VAL)
    {
        s_device_addr = BMI160_I2C_ADDR_HIGH;
        if (hal_i2c_read_reg(s_device_addr, BMI160_REG_CHIP_ID, &chip_id) != HAL_I2C_OK || chip_id != BMI160_CHIP_ID_VAL)
        {
            return BMI160_ERR_NOT_FOUND;
        }
    }

    /* 3. Thực hiện Soft-reset */
    (void)hal_i2c_write_reg(s_device_addr, BMI160_REG_CMD, BMI160_CMD_SOFT_RESET);
    bmi160_delay_ms(25);

    /* 4. Đặt Accel sang Normal Mode */
    (void)hal_i2c_write_reg(s_device_addr, BMI160_REG_CMD, BMI160_CMD_ACC_MODE_NORMAL);
    bmi160_delay_ms(10);

    /* 5. Đặt Gyro sang Normal Mode (cần khoảng 80ms khởi động) */
    (void)hal_i2c_write_reg(s_device_addr, BMI160_REG_CMD, BMI160_CMD_GYR_MODE_NORMAL);
    bmi160_delay_ms(90);

    /* 6. Cấu hình Dải đo và ODR (Output Data Rate = 100 Hz) */
    /* ACC_CONF: ODR 100Hz (0x08), normal bandwidth (0x20) -> 0x28 */
    (void)hal_i2c_write_reg(s_device_addr, BMI160_REG_ACC_CONF, 0x28U);
    /* ACC_RANGE: ±2g (0x03) */
    (void)hal_i2c_write_reg(s_device_addr, BMI160_REG_ACC_RANGE, 0x03U);

    /* GYR_CONF: ODR 100Hz (0x08), normal bandwidth (0x20) -> 0x28 */
    (void)hal_i2c_write_reg(s_device_addr, BMI160_REG_GYR_CONF, 0x28U);
    /* GYR_RANGE: ±2000 dps (0x00) */
    (void)hal_i2c_write_reg(s_device_addr, BMI160_REG_GYR_RANGE, 0x00U);

    s_is_initialized = 1U;
    return BMI160_OK;
}

bmi160_status_t bmi160_service_read_raw(bmi160_raw_data_t *raw_data)
{
    if (raw_data == NULL)
    {
        return BMI160_ERR_COMM;
    }

    if (s_is_initialized == 0U)
    {
        if (bmi160_service_init() != BMI160_OK)
        {
            return BMI160_ERR_NOT_FOUND;
        }
    }

    if (s_bmi160_mutex != NULL && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    {
        if (xSemaphoreTake(s_bmi160_mutex, pdMS_TO_TICKS(BMI160_MUTEX_TIMEOUT_MS)) != pdTRUE)
        {
            return BMI160_ERR_MUTEX;
        }
    }

    uint8_t buffer[12];
    /* Đọc 12 byte liên tiếp bắt đầu từ thanh ghi 0x0C */
    hal_i2c_status_t status = hal_i2c_read(s_device_addr, BMI160_REG_DATA_START, buffer, sizeof(buffer));

    if (s_bmi160_mutex != NULL && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    {
        (void)xSemaphoreGive(s_bmi160_mutex);
    }

    if (status != HAL_I2C_OK)
    {
        return BMI160_ERR_COMM;
    }

    /* Ghép dữ liệu Gyroscope (Little Endian: LSB trước, MSB sau) */
    raw_data->gyr_x = (int16_t)(((uint16_t)buffer[1] << 8) | buffer[0]);
    raw_data->gyr_y = (int16_t)(((uint16_t)buffer[3] << 8) | buffer[2]);
    raw_data->gyr_z = (int16_t)(((uint16_t)buffer[5] << 8) | buffer[4]);

    /* Ghép dữ liệu Accelerometer */
    raw_data->acc_x = (int16_t)(((uint16_t)buffer[7] << 8) | buffer[6]);
    raw_data->acc_y = (int16_t)(((uint16_t)buffer[9] << 8) | buffer[8]);
    raw_data->acc_z = (int16_t)(((uint16_t)buffer[11] << 8) | buffer[10]);

    return BMI160_OK;
}
