#ifndef HAL_I2C_H
#define HAL_I2C_H

#include <stdint.h>

/* Layer 1: Hardware Abstraction Layer cho I2C1 (PB6: SCL, PB7: SDA) */

typedef enum
{
    HAL_I2C_OK = 0,
    HAL_I2C_ERROR,
    HAL_I2C_BUSY,
    HAL_I2C_TIMEOUT,
    HAL_I2C_INVALID_ARGUMENT
} hal_i2c_status_t;

/* Khởi tạo ngoại vi I2C1 và chân PB6/PB7 */
hal_i2c_status_t hal_i2c_init(uint32_t clock_speed);

/* Ghi một hoặc nhiều byte vào thanh ghi I2C */
hal_i2c_status_t hal_i2c_write(uint8_t dev_addr, uint8_t reg_addr, const uint8_t *data, uint16_t len);

/* Đọc một hoặc nhiều byte từ thanh ghi I2C */
hal_i2c_status_t hal_i2c_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len);

/* Ghi 1 byte vào thanh ghi */
hal_i2c_status_t hal_i2c_write_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t val);

/* Đọc 1 byte từ thanh ghi */
hal_i2c_status_t hal_i2c_read_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t *val);

/* Giải phóng và phục hồi đường truyền I2C nếu bus bị kẹt */
void hal_i2c_bus_reset(void);

#endif /* HAL_I2C_H */
