#ifndef BMI160_SERVICE_H
#define BMI160_SERVICE_H

#include <stdint.h>
#include "FreeRTOS.h"

/* Layer 2: Dịch vụ cảm biến BMI160 - Quản lý cấu hình, giao tiếp và đọc raw data */

#define BMI160_I2C_ADDR_LOW     0x68U
#define BMI160_I2C_ADDR_HIGH    0x69U
#define BMI160_CHIP_ID_VAL      0xD1U

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

typedef enum
{
    BMI160_OK = 0,
    BMI160_ERR_INIT,
    BMI160_ERR_COMM,
    BMI160_ERR_NOT_FOUND,
    BMI160_ERR_TIMEOUT,
    BMI160_ERR_MUTEX
} bmi160_status_t;

/* Cấu trúc lưu trữ dữ liệu thô (raw) từ 6 trục của BMI160 */
typedef struct
{
    int16_t acc_x;
    int16_t acc_y;
    int16_t acc_z;
    int16_t gyr_x;
    int16_t gyr_y;
    int16_t gyr_z;
} bmi160_raw_data_t;

/* Khởi tạo giao tiếp phần cứng I2C1, cấu hình BMI160 vào Normal Mode */
bmi160_status_t bmi160_service_init(void);

/* Đọc 6 trục dữ liệu thô (Accelerometer + Gyroscope) */
bmi160_status_t bmi160_service_read_raw(bmi160_raw_data_t *raw_data);

/* Lấy địa chỉ I2C đang hoạt động (0x68 hoặc 0x69) */
uint8_t bmi160_service_get_i2c_addr(void);

#endif /* BMI160_SERVICE_H */
