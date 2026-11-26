/* Layer 1: Hardware Abstraction Layer - Cấu hình và giao tiếp I2C1 (STM32F411) */
#include "hal_i2c.h"
#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stddef.h>

static uint32_t s_clock_speed = 100000UL;

/* Tạo độ trễ ngắn cho bus recovery */
static void i2c_delay(void)
{
    for (volatile uint32_t i = 0; i < 50; i++)
    {
        __NOP();
    }
}

void hal_i2c_bus_reset(void)
{
    GPIO_InitTypeDef gpio;

    /* Tắt I2C1 tạm thời */
    I2C_Cmd(I2C1, DISABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, DISABLE);

    /* Cấu hình PB6, PB7 thành GPIO Open-Drain để nhả bus thủ công */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);

    GPIO_SetBits(GPIOB, GPIO_Pin_6 | GPIO_Pin_7);
    i2c_delay();

    /* Nhấp nháy SCL 9 xung để kích hoạt slave nhả SDA nếu đang bị kẹt */
    for (int i = 0; i < 9; i++)
    {
        GPIO_ResetBits(GPIOB, GPIO_Pin_6);
        i2c_delay();
        GPIO_SetBits(GPIOB, GPIO_Pin_6);
        i2c_delay();
    }

    /* Tạo STOP condition giả lập */
    GPIO_ResetBits(GPIOB, GPIO_Pin_7);
    i2c_delay();
    GPIO_SetBits(GPIOB, GPIO_Pin_6);
    i2c_delay();
    GPIO_SetBits(GPIOB, GPIO_Pin_7);
    i2c_delay();

    /* Khởi tạo lại I2C1 */
    hal_i2c_init(s_clock_speed);
}

hal_i2c_status_t hal_i2c_init(uint32_t clock_speed)
{
    GPIO_InitTypeDef gpio;
    I2C_InitTypeDef i2c;

    if (clock_speed == 0U || clock_speed > 400000UL)
    {
        return HAL_I2C_INVALID_ARGUMENT;
    }
    s_clock_speed = clock_speed;

    /* Bật clock cho GPIOB và I2C1 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);

    /* Cấu hình PB6 (SCL) và PB7 (SDA): Alternate Function Open Drain */
    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    gpio.GPIO_Mode = GPIO_Mode_AF_OD;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);

    /* Reset I2C peripheral */
    I2C_SoftwareResetCmd(I2C1, ENABLE);
    I2C_SoftwareResetCmd(I2C1, DISABLE);

    I2C_DeInit(I2C1);
    I2C_StructInit(&i2c);
    i2c.I2C_Mode = I2C_Mode_I2C;
    i2c.I2C_DutyCycle = I2C_DutyCycle_2;
    i2c.I2C_OwnAddress1 = 0x00;
    i2c.I2C_Ack = I2C_Ack_Enable;
    i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    i2c.I2C_ClockSpeed = clock_speed;

    I2C_Init(I2C1, &i2c);
    I2C_Cmd(I2C1, ENABLE);

    return HAL_I2C_OK;
}

/* F411 register-level I2C transfer: ACK must be disabled BEFORE clearing ADDR on 1-byte receive. */
static int wait_flag(uint32_t flag)
{
    for (uint32_t n = 0; n < 100000U; ++n)
    {
        if ((I2C1->SR1 & (I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO)) != 0U)
        {
            return 0;
        }
        if ((I2C1->SR1 & flag) == flag)
        {
            return 1;
        }
    }
    return 0;
}

static hal_i2c_status_t read_byte(uint8_t addr, uint8_t reg, uint8_t *data)
{
    hal_i2c_bus_reset();
    uint32_t guard = 100000U;
    while ((I2C1->SR2 & I2C_SR2_BUSY) != 0U && --guard != 0U) { }
    if (guard == 0U)
    {
        return HAL_I2C_TIMEOUT;
    }

    I2C1->SR1 &= ~(I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO);
    I2C1->CR1 |= I2C_CR1_ACK;
    I2C1->CR1 &= ~I2C_CR1_POS;
    I2C1->CR1 |= I2C_CR1_START;
    if (!wait_flag(I2C_SR1_SB)) { goto fail; }

    I2C1->DR = (uint32_t)addr << 1;
    if (!wait_flag(I2C_SR1_ADDR)) { goto fail; }
    (void)I2C1->SR2;
    if (!wait_flag(I2C_SR1_TXE)) { goto fail; }

    I2C1->DR = reg;
    if (!wait_flag(I2C_SR1_BTF)) { goto fail; }

    I2C1->CR1 |= I2C_CR1_START;
    if (!wait_flag(I2C_SR1_SB)) { goto fail; }

    I2C1->DR = ((uint32_t)addr << 1) | 1U;
    if (!wait_flag(I2C_SR1_ADDR)) { goto fail; }

    taskENTER_CRITICAL();
    I2C1->CR1 &= ~I2C_CR1_ACK;
    (void)I2C1->SR2;
    I2C1->CR1 |= I2C_CR1_STOP;
    taskEXIT_CRITICAL();

    if (!wait_flag(I2C_SR1_RXNE)) { goto fail; }
    *data = (uint8_t)I2C1->DR;

    guard = 100000U;
    while ((I2C1->CR1 & I2C_CR1_STOP) != 0U && --guard != 0U) { }
    I2C1->CR1 |= I2C_CR1_ACK;
    return guard != 0U ? HAL_I2C_OK : HAL_I2C_TIMEOUT;

fail:
    I2C1->CR1 |= I2C_CR1_STOP | I2C_CR1_ACK;
    I2C1->SR1 &= ~(I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO);
    return HAL_I2C_TIMEOUT;
}

hal_i2c_status_t hal_i2c_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0U)
    {
        return HAL_I2C_INVALID_ARGUMENT;
    }
    for (uint16_t i = 0; i < len; ++i)
    {
        hal_i2c_status_t status = read_byte(dev_addr, (uint8_t)(reg_addr + i), &data[i]);
        if (status != HAL_I2C_OK)
        {
            return status;
        }
    }
    return HAL_I2C_OK;
}

hal_i2c_status_t hal_i2c_read_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t *val)
{
    return hal_i2c_read(dev_addr, reg_addr, val, 1U);
}

hal_i2c_status_t hal_i2c_write(uint8_t dev_addr, uint8_t reg_addr, const uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0U)
    {
        return HAL_I2C_INVALID_ARGUMENT;
    }
    hal_i2c_bus_reset();
    I2C1->SR1 &= ~(I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO);
    I2C1->CR1 |= I2C_CR1_START;
    if (!wait_flag(I2C_SR1_SB)) { goto error; }

    I2C1->DR = (uint32_t)dev_addr << 1;
    if (!wait_flag(I2C_SR1_ADDR)) { goto error; }
    (void)I2C1->SR2;
    if (!wait_flag(I2C_SR1_TXE)) { goto error; }

    I2C1->DR = reg_addr;
    if (!wait_flag(I2C_SR1_BTF)) { goto error; }

    for (uint16_t i = 0; i < len; ++i)
    {
        I2C1->DR = data[i];
        if (!wait_flag(I2C_SR1_BTF)) { goto error; }
    }

    I2C1->CR1 |= I2C_CR1_STOP;
    for (uint32_t guard = 100000U; guard != 0U; --guard)
    {
        if ((I2C1->CR1 & I2C_CR1_STOP) == 0U)
        {
            return HAL_I2C_OK;
        }
    }

error:
    I2C1->CR1 |= I2C_CR1_STOP;
    return HAL_I2C_TIMEOUT;
}

hal_i2c_status_t hal_i2c_write_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t val)
{
    return hal_i2c_write(dev_addr, reg_addr, &val, 1U);
}
