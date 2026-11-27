/* Layer 1 (HAL): I2C1 (PB6: SCL, PB7: SDA) */
#include "hal_i2c.h"
#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stddef.h>

#define I2C_CLOCK_HZ 100000U

/* --- a) Clock, GPIO va khoi tao I2C1 --- */

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

static bool i2c_hw_init(void)
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

/* --- b) Phuc hoi bus & Polling co loi --- */

static void i2c_recovery_delay(void) // hàm delay ngắn 
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        vTaskDelay(pdMS_TO_TICKS(1U));
    }
}

static void i2c_bus_reset(void) // hàm khôi phục bus i2c nếu bị treo
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
    (void)i2c_hw_init();
}

bool hal_i2c_init(void) // hàm khởi tạo i2c
{
    if (!i2c_hw_init()) { return false; }
    if ((I2C1->SR2 & I2C_SR2_BUSY) != 0U) {
        i2c_bus_reset();
    }
    return (I2C1->SR2 & I2C_SR2_BUSY) == 0U;
}

static bool i2c_wait_flag(uint32_t flag) // hàm chờ cờ I2C, nếu timeout thì return false
{
    for (uint32_t n = 0U; n < 100000U; ++n) {
        if ((I2C1->SR1 & (I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO)) != 0U) { return false; }
        if ((I2C1->SR1 & flag) == flag) { return true; }
    }
    return false;
}

static bool i2c_wait_stop_clear(void) // hàm chờ stop bit clear
{
    for (uint32_t guard = 100000U; guard != 0U; --guard) {
        if ((I2C1->CR1 & I2C_CR1_STOP) == 0U) { return true; }
    }
    return false;
}

static bool i2c_wait_idle(void) // hàm chờ bus i2c
{
    for (uint32_t guard = 100000U; guard != 0U; --guard) {
        if ((I2C1->SR2 & I2C_SR2_BUSY) == 0U) { return true; }
    }
    i2c_bus_reset();
    for (uint32_t guard = 100000U; guard != 0U; --guard) {
        if ((I2C1->SR2 & I2C_SR2_BUSY) == 0U) { return true; }
    }
    return false;
}

static void i2c_abort(void) // ham nay dung de phuc hoi bus I2C khi co loi xay ra
{
    uint32_t sr1 = I2C1->SR1;
    I2C1->CR1 |= I2C_CR1_STOP | I2C_CR1_ACK;
    I2C1->SR1 &= ~(I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO);
    if (!i2c_wait_stop_clear() ||
        (I2C1->SR2 & I2C_SR2_BUSY) != 0U ||
        (sr1 & (I2C_SR1_BERR | I2C_SR1_ARLO)) != 0U) {
        i2c_bus_reset();
    }
}

/* --- c) Doc / ghi thanh ghi I2C --- */

bool hal_i2c_read_byte(uint8_t addr, uint8_t reg, uint8_t *data)
{
    if (data == NULL) { return false; }
    if (!i2c_wait_idle()) { return false; }

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
    if (!i2c_wait_stop_clear()) { goto fail; }
    I2C1->CR1 |= I2C_CR1_ACK;
    return true;

fail:
    i2c_abort();
    return false;
} // ham nay dung de doc 1 byte tu thanh ghi cua thiet bi I2C

bool hal_i2c_read(uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len) // ham nay dung de doc nhieu byte tu thanh ghi cua thiet bi I2C
{
    if (data == NULL || len == 0U) { return false; }

    for (uint16_t i = 0U; i < len; ++i) {
        if (!hal_i2c_read_byte(addr, (uint8_t)(reg + i), &data[i])) { return false; }
    }
    return true;
}

bool hal_i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t val) // ham nay dung de ghi 1 byte vao thanh ghi cua thiet bi I2C
{
    if (!i2c_wait_idle()) { return false; }

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
    if (i2c_wait_stop_clear()) { return true; }

error:
    i2c_abort();
    return false;
}
