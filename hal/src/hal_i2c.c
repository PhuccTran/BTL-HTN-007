/* STM32F411 I2C1, native CMSIS registers (ST RM0383, section 18).
 * PB6 = SCL, PB7 = SDA, AF4, open-drain; no F1 SPL compatibility API.
 */
#include "hal_i2c.h"
#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stddef.h>

static uint32_t s_clock_speed = 100000U;

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
    /* Same fast output speed and no internal pulls as the previous driver. */
    GPIOB->OSPEEDR = (GPIOB->OSPEEDR & ~mask) | (2U << 12U) | (2U << 14U);
    GPIOB->PUPDR &= ~mask;
    GPIOB->AFR[0] = (GPIOB->AFR[0] & ~((15U << 24U) | (15U << 28U))) |
                    (4U << 24U) | (4U << 28U);
    GPIOB->MODER = (GPIOB->MODER & ~mask) | (mode << 12U) | (mode << 14U);
}

hal_i2c_status_t hal_i2c_init(uint32_t clock_speed)
{
    if (clock_speed == 0U || clock_speed > 400000U) {
        return HAL_I2C_INVALID_ARGUMENT;
    }
    uint32_t pclk = i2c_pclk1_hz();
    uint32_t mhz = pclk / 1000000U;
    if (mhz < 2U || mhz > 50U || (clock_speed > 100000U && mhz < 4U)) {
        return HAL_I2C_INVALID_ARGUMENT;
    }
    uint32_t ccr = pclk / ((clock_speed <= 100000U ? 2U : 3U) * clock_speed);
    if (clock_speed <= 100000U && ccr < 4U) { ccr = 4U; }
    if (clock_speed > 100000U && ccr == 0U) { ccr = 1U; }
    if (ccr > I2C_CCR_CCR) { return HAL_I2C_INVALID_ARGUMENT; }
    s_clock_speed = clock_speed;

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    (void)RCC->AHB1ENR;
    (void)RCC->APB1ENR;
    __DSB();
    i2c_pins_mode(2U); /* Alternate function mode, selected after AF/type. */

    I2C1->CR1 |= I2C_CR1_SWRST;
    I2C1->CR1 &= ~I2C_CR1_SWRST;
    RCC->APB1RSTR |= RCC_APB1RSTR_I2C1RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C1RST;
    I2C1->CR1 &= ~I2C_CR1_PE;
    I2C1->CR2 = (I2C1->CR2 & ~I2C_CR2_FREQ) | mhz;
    if (clock_speed <= 100000U) {
        I2C1->CCR = ccr;
        I2C1->TRISE = mhz + 1U;
    } else {
        I2C1->CCR = I2C_CCR_FS | ccr; /* Fast mode, duty cycle 2. */
        I2C1->TRISE = (mhz * 300U) / 1000U + 1U;
    }
    I2C1->OAR1 = 1U << 14U; /* RM0383: bit 14 must be kept at one. */
    I2C1->CR1 = I2C_CR1_ACK | I2C_CR1_PE;
    return HAL_I2C_OK;
}

/* Short fixed pulse spacing, retained only for the GPIO bus-recovery sequence. */
static void i2c_recovery_delay(void)
{
    for (volatile uint32_t n = 0U; n < 50U; ++n) { __NOP(); }
}

void hal_i2c_bus_reset(void)
{
    const uint32_t scl = 1U << 6U, sda = 1U << 7U;
    I2C1->CR1 &= ~I2C_CR1_PE;
    RCC->APB1ENR &= ~RCC_APB1ENR_I2C1EN;
    (void)RCC->APB1ENR;
    __DSB();
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    (void)RCC->AHB1ENR;
    __DSB();
    i2c_pins_mode(1U); /* General-purpose open-drain during recovery. */
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
    (void)hal_i2c_init(s_clock_speed);
}

/* Keep the previously used bounded read/write sequence, including ACK/STOP. */
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
