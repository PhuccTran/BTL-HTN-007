#include "hal_i2c.h"
#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"

/* F411 single-byte receive: ACK must be disabled BEFORE clearing ADDR.
 * Original driver is preserved; this integration backend replaces its read API. */
static int wait_flag(uint32_t flag)
{
    for (uint32_t n=0; n<100000U; ++n) {
        if ((I2C1->SR1 & (I2C_SR1_AF|I2C_SR1_BERR|I2C_SR1_ARLO)) != 0U) { return 0; }
        if ((I2C1->SR1 & flag) == flag) { return 1; }
    }
    return 0;
}
static hal_i2c_status_t read_byte(uint8_t addr,uint8_t reg,uint8_t *data)
{
    hal_i2c_bus_reset();
    uint32_t guard=100000U;
    while ((I2C1->SR2&I2C_SR2_BUSY)!=0U && --guard!=0U) { }
    if (guard==0U) { return HAL_I2C_TIMEOUT; }
    I2C1->SR1 &= ~(I2C_SR1_AF|I2C_SR1_BERR|I2C_SR1_ARLO);
    I2C1->CR1 |= I2C_CR1_ACK;
    I2C1->CR1 &= ~I2C_CR1_POS;
    I2C1->CR1 |= I2C_CR1_START;
    if (!wait_flag(I2C_SR1_SB)) { goto fail; }
    I2C1->DR=(uint32_t)addr<<1;
    if (!wait_flag(I2C_SR1_ADDR)) { goto fail; }
    (void)I2C1->SR2;
    if (!wait_flag(I2C_SR1_TXE)) { goto fail; }
    I2C1->DR=reg;
    if (!wait_flag(I2C_SR1_BTF)) { goto fail; }
    I2C1->CR1 |= I2C_CR1_START;
    if (!wait_flag(I2C_SR1_SB)) { goto fail; }
    I2C1->DR=((uint32_t)addr<<1)|1U;
    if (!wait_flag(I2C_SR1_ADDR)) { goto fail; }
    taskENTER_CRITICAL();
    I2C1->CR1 &= ~I2C_CR1_ACK;
    (void)I2C1->SR2;
    I2C1->CR1 |= I2C_CR1_STOP;
    taskEXIT_CRITICAL();
    if (!wait_flag(I2C_SR1_RXNE)) { goto fail; }
    *data=(uint8_t)I2C1->DR;
    guard=100000U;
    while ((I2C1->CR1&I2C_CR1_STOP)!=0U && --guard!=0U) { }
    I2C1->CR1 |= I2C_CR1_ACK;
    return guard!=0U ? HAL_I2C_OK : HAL_I2C_TIMEOUT;
fail:
    I2C1->CR1 |= I2C_CR1_STOP|I2C_CR1_ACK;
    I2C1->SR1 &= ~(I2C_SR1_AF|I2C_SR1_BERR|I2C_SR1_ARLO);
    return HAL_I2C_TIMEOUT;
}
hal_i2c_status_t hal_i2c_read(uint8_t addr,uint8_t reg,uint8_t *data,uint16_t len)
{
    if (data==NULL || len==0U) { return HAL_I2C_INVALID_ARGUMENT; }
    for (uint16_t i=0;i<len;++i) {
        hal_i2c_status_t status=read_byte(addr,(uint8_t)(reg+i),&data[i]);
        if (status!=HAL_I2C_OK) { return status; }
    }
    return HAL_I2C_OK;
}
hal_i2c_status_t hal_i2c_read_reg(uint8_t addr,uint8_t reg,uint8_t *data)
{ return hal_i2c_read(addr,reg,data,1U); }
hal_i2c_status_t hal_i2c_write(uint8_t addr,uint8_t reg,const uint8_t *data,uint16_t len)
{
    if (data==NULL || len==0U) { return HAL_I2C_INVALID_ARGUMENT; }
    hal_i2c_bus_reset();
    I2C1->SR1 &= ~(I2C_SR1_AF|I2C_SR1_BERR|I2C_SR1_ARLO);
    I2C1->CR1 |= I2C_CR1_START;
    if (!wait_flag(I2C_SR1_SB)) { goto error; }
    I2C1->DR=(uint32_t)addr<<1;
    if (!wait_flag(I2C_SR1_ADDR)) { goto error; }
    (void)I2C1->SR2;
    if (!wait_flag(I2C_SR1_TXE)) { goto error; }
    I2C1->DR=reg;
    if (!wait_flag(I2C_SR1_BTF)) { goto error; }
    for (uint16_t i=0;i<len;++i) {
        I2C1->DR=data[i];
        if (!wait_flag(I2C_SR1_BTF)) { goto error; }
    }
    I2C1->CR1 |= I2C_CR1_STOP;
    for (uint32_t guard=100000U;guard!=0U;--guard) {
        if ((I2C1->CR1&I2C_CR1_STOP)==0U) { return HAL_I2C_OK; }
    }
error:
    I2C1->CR1 |= I2C_CR1_STOP;
    return HAL_I2C_TIMEOUT;
}
hal_i2c_status_t hal_i2c_write_reg(uint8_t addr,uint8_t reg,uint8_t value)
{ return hal_i2c_write(addr,reg,&value,1U); }
