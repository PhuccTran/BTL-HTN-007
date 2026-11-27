#ifndef HAL_I2C_H
#define HAL_I2C_H

#include <stdbool.h>
#include <stdint.h>

/* Layer 1 (HAL): I2C1 (PB6: SCL, PB7: SDA) */
bool hal_i2c_init(void);
bool hal_i2c_read_byte(uint8_t addr, uint8_t reg, uint8_t *data);
bool hal_i2c_read(uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len);
bool hal_i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t val);

#endif
