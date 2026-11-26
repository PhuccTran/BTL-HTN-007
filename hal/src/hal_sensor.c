/* Layer 1 (HAL): I2C1 (PB6: SCL, PB7: SDA) + BMI160 Accelerometer driver */
#include "hal_sensor.h"
#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stddef.h>

#define I2C_CLOCK_HZ            100000U

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

static uint32_t i2c_pclk1_hz(void)
{
    SystemCoreClockUpdate();
    uint32_t prescaler = (RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;
    return SystemCoreClock >> (prescaler < 4U ? 0U : prescaler - 3U);
}

static void i2c_pins_mode(uint32_t mode)
{
    const uint32_t pins = (1U << 6U) | (1U << 7U);
    const uint32_t mask = (3U << 12U) | (3U << 14U);
    GPIOB->OTYPER |= pins;
    GPIOB->OSPEEDR = (GPIOB->OSPEEDR & ~mask) | (2U << 12U) | (2U << 14U);
    GPIOB->PUPDR &= ~mask;
    GPIOB->AFR[0] = (GPIOB->AFR[0] & ~((15U << 24U) | (15U << 28U))) |
                    (4U << 24U) | (4U << 28U);
    GPIOB->MODER = (GPIOB->MODER & ~mask) | (mode << 12U) | (mode << 14U);
}

static bool i2c_init(void)
{
    uint32_t pclk = i2c_pclk1_hz();
    uint32_t mhz = pclk / 1000000U;
    if (mhz < 2U || mhz > 50U) { return false; }

    uint32_t ccr = pclk / (2U * I2C_CLOCK_HZ);
    if (ccr < 4U) { ccr = 4U; }
    if (ccr > I2C_CCR_CCR) { return false; }

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    (void)RCC->AHB1ENR;
    (void)RCC->APB1ENR;
    __DSB();
    i2c_pins_mode(2U);

    I2C1->CR1 |= I2C_CR1_SWRST;
    I2C1->CR1 &= ~I2C_CR1_SWRST;
    RCC->APB1RSTR |= RCC_APB1RSTR_I2C1RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C1RST;
    I2C1->CR1 &= ~I2C_CR1_PE;
    I2C1->CR2 = (I2C1->CR2 & ~I2C_CR2_FREQ) | mhz;
    I2C1->CCR = ccr;
    I2C1->TRISE = mhz + 1U;
    I2C1->OAR1 = 1U << 14U;
    I2C1->CR1 = I2C_CR1_ACK | I2C_CR1_PE;
    return true;
}

static void i2c_recovery_delay(void)
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        vTaskDelay(pdMS_TO_TICKS(1U));
    }
}

static void i2c_bus_reset(void)
{
    const uint32_t scl = 1U << 6U, sda = 1U << 7U;
    I2C1->CR1 &= ~I2C_CR1_PE;
    RCC->APB1ENR &= ~RCC_APB1ENR_I2C1EN;
    (void)RCC->APB1ENR;
    __DSB();
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    (void)RCC->AHB1ENR;
    __DSB();
    i2c_pins_mode(1U);
    GPIOB->BSRR = scl | sda;
    i2c_recovery_delay();
    for (uint32_t n = 0U; n < 9U; ++n) {
        GPIOB->BSRR = scl << 16U;
        i2c_recovery_delay();
        GPIOB->BSRR = scl;
        i2c_recovery_delay();
    }
    GPIOB->BSRR = sda << 16U;
    i2c_recovery_delay();
    GPIOB->BSRR = scl;
    i2c_recovery_delay();
    GPIOB->BSRR = sda;
    i2c_recovery_delay();
    (void)i2c_init();
}

static bool i2c_wait_flag(uint32_t flag)
{
    for (uint32_t n = 0U; n < 100000U; ++n) {
        if ((I2C1->SR1 & (I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO)) != 0U) { return false; }
        if ((I2C1->SR1 & flag) == flag) { return true; }
    }
    return false;
}

static bool i2c_read_byte(uint8_t addr, uint8_t reg, uint8_t *data)
{
    i2c_bus_reset();
    uint32_t guard = 100000U;
    while ((I2C1->SR2 & I2C_SR2_BUSY) != 0U && --guard != 0U) { }
    if (guard == 0U) { return false; }

    I2C1->SR1 &= ~(I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO);
    I2C1->CR1 |= I2C_CR1_ACK;
    I2C1->CR1 &= ~I2C_CR1_POS;
    I2C1->CR1 |= I2C_CR1_START;
    if (!i2c_wait_flag(I2C_SR1_SB)) { goto fail; }
    I2C1->DR = (uint32_t)addr << 1U;
    if (!i2c_wait_flag(I2C_SR1_ADDR)) { goto fail; }
    (void)I2C1->SR2;
    if (!i2c_wait_flag(I2C_SR1_TXE)) { goto fail; }
    I2C1->DR = reg;
    if (!i2c_wait_flag(I2C_SR1_BTF)) { goto fail; }
    I2C1->CR1 |= I2C_CR1_START;
    if (!i2c_wait_flag(I2C_SR1_SB)) { goto fail; }
    I2C1->DR = ((uint32_t)addr << 1U) | 1U;
    if (!i2c_wait_flag(I2C_SR1_ADDR)) { goto fail; }

    taskENTER_CRITICAL();
    I2C1->CR1 &= ~I2C_CR1_ACK;
    (void)I2C1->SR2;
    I2C1->CR1 |= I2C_CR1_STOP;
    taskEXIT_CRITICAL();

    if (!i2c_wait_flag(I2C_SR1_RXNE)) { goto fail; }
    *data = (uint8_t)I2C1->DR;
    guard = 100000U;
    while ((I2C1->CR1 & I2C_CR1_STOP) != 0U && --guard != 0U) { }
    I2C1->CR1 |= I2C_CR1_ACK;
    return guard != 0U;

fail:
    I2C1->CR1 |= I2C_CR1_STOP | I2C_CR1_ACK;
    I2C1->SR1 &= ~(I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO);
    return false;
}

static bool i2c_read(uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0U; i < len; ++i) {
        if (!i2c_read_byte(addr, (uint8_t)(reg + i), &data[i])) { return false; }
    }
    return true;
}

static bool i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t val)
{
    i2c_bus_reset();
    I2C1->SR1 &= ~(I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO);
    I2C1->CR1 |= I2C_CR1_START;
    if (!i2c_wait_flag(I2C_SR1_SB)) { goto error; }
    I2C1->DR = (uint32_t)addr << 1U;
    if (!i2c_wait_flag(I2C_SR1_ADDR)) { goto error; }
    (void)I2C1->SR2;
    if (!i2c_wait_flag(I2C_SR1_TXE)) { goto error; }
    I2C1->DR = reg;
    if (!i2c_wait_flag(I2C_SR1_BTF)) { goto error; }
    I2C1->DR = val;
    if (!i2c_wait_flag(I2C_SR1_BTF)) { goto error; }
    I2C1->CR1 |= I2C_CR1_STOP;
    for (uint32_t guard = 100000U; guard != 0U; --guard) {
        if ((I2C1->CR1 & I2C_CR1_STOP) == 0U) { return true; }
    }
error:
    I2C1->CR1 |= I2C_CR1_STOP;
    return false;
}

bool hal_sensor_configure(void)
{
    if (!i2c_init()) { return false; }
    vTaskDelay(pdMS_TO_TICKS(10U));

    uint8_t id = 0U;
    s_device_addr = BMI160_I2C_ADDR_LOW;
    if (!i2c_read_byte(s_device_addr, BMI160_REG_CHIP_ID, &id) || id != BMI160_CHIP_ID_VAL) {
        s_device_addr = BMI160_I2C_ADDR_HIGH;
        if (!i2c_read_byte(s_device_addr, BMI160_REG_CHIP_ID, &id) || id != BMI160_CHIP_ID_VAL) {
            return false;
        }
    }

    if (!i2c_write_reg(s_device_addr, BMI160_REG_CMD, BMI160_CMD_SOFT_RESET)) { return false; }
    vTaskDelay(pdMS_TO_TICKS(25U));

    if (!i2c_write_reg(s_device_addr, BMI160_REG_CMD, BMI160_CMD_ACC_NORMAL)) { return false; }
    vTaskDelay(pdMS_TO_TICKS(10U));

    if (!i2c_write_reg(s_device_addr, BMI160_REG_ACC_CONF, 0x28U) ||
        !i2c_write_reg(s_device_addr, BMI160_REG_ACC_RANGE, 0x03U)) {
        return false;
    }

    uint8_t range = 0U, conf = 0U, pmu = 0U;
    if (!i2c_read_byte(s_device_addr, BMI160_REG_ACC_RANGE, &range) ||
        !i2c_read_byte(s_device_addr, BMI160_REG_ACC_CONF, &conf) ||
        !i2c_read_byte(s_device_addr, BMI160_REG_PMU_STATUS, &pmu)) {
        return false;
    }

    return range == 0x03U && (conf & 0x0FU) == 0x08U && (pmu & 0x30U) == 0x10U;
}

bool hal_sensor_read(int16_t *raw_x, int16_t *raw_y)
{
    if (raw_x == NULL || raw_y == NULL) { return false; }

    uint8_t buf[4];
    if (!i2c_read(s_device_addr, BMI160_REG_ACC_X_L, buf, sizeof(buf))) {
        return false;
    }

    *raw_x = (int16_t)(((uint16_t)buf[1] << 8U) | buf[0]);
    *raw_y = (int16_t)(((uint16_t)buf[3] << 8U) | buf[2]);
    return true;
}
