/* F1 SPL calls used by app/, hal/ and main.c, implemented for STM32F411.
 * See ST RM0383: RCC, GPIO, USART and I2C register descriptions.
 */
#include "stm32f10x.h"

static void set_bits(volatile uint32_t *reg, uint32_t bits, FunctionalState state)
{
    assert_param(state == ENABLE || state == DISABLE);
    if (state == ENABLE) { *reg |= bits; }
    else { *reg &= ~bits; }
}

void NVIC_PriorityGroupConfig(uint32_t group)
{
    assert_param(group == NVIC_PriorityGroup_3);
    /* PRIGROUP=3 gives all four implemented bits to preemption on F411.
     * Keeping the F1 raw value (PRIGROUP=4) would fail the CM4F port check.
     */
    NVIC_SetPriorityGrouping(3U);
}

void RCC_APB2PeriphClockCmd(uint32_t mask, FunctionalState state)
{
    const uint32_t gpios = RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC;
    assert_param((mask & ~(gpios | RCC_APB2Periph_AFIO | RCC_APB2Periph_USART1)) == 0U);
    set_bits(&RCC->AHB1ENR, (mask & gpios) >> 2U, state);
    set_bits(&RCC->APB2ENR, (mask & RCC_APB2Periph_USART1) ? RCC_APB2ENR_USART1EN : 0U, state);
    /* F411 has no AFIO clock; pin alternate functions are in GPIOx->AFR. */
    (void)RCC->AHB1ENR;
    (void)RCC->APB2ENR;
    __DSB();
}

void RCC_APB1PeriphClockCmd(uint32_t mask, FunctionalState state)
{
    assert_param(mask == RCC_APB1Periph_I2C1);
    set_bits(&RCC->APB1ENR, mask, state);
    (void)RCC->APB1ENR;
    __DSB();
}

void GPIO_StructInit(GPIO_InitTypeDef *init)
{
    init->GPIO_Pin = 0xffffU;
    init->GPIO_Speed = GPIO_Speed_2MHz;
    init->GPIO_Mode = GPIO_Mode_IN_FLOATING;
}

void GPIO_Init(GPIO_TypeDef *gpio, GPIO_InitTypeDef *init)
{
    assert_param(gpio == GPIOA || gpio == GPIOB || gpio == GPIOC);
    assert_param(init->GPIO_Speed >= GPIO_Speed_10MHz && init->GPIO_Speed <= GPIO_Speed_50MHz);
    uint32_t mode, pull = 0U, od = 0U;
    switch (init->GPIO_Mode) {
    case GPIO_Mode_AIN: mode = 3U; break;
    case GPIO_Mode_IN_FLOATING: mode = 0U; break;
    case GPIO_Mode_IPU: mode = 0U; pull = 1U; break;
    case GPIO_Mode_IPD: mode = 0U; pull = 2U; break;
    case GPIO_Mode_Out_PP: mode = 1U; break;
    case GPIO_Mode_Out_OD: mode = 1U; od = 1U; break;
    case GPIO_Mode_AF_PP: mode = 2U; break;
    case GPIO_Mode_AF_OD: mode = 2U; od = 1U; break;
    default: assert_param(0); return;
    }
    const uint32_t speed = init->GPIO_Speed == GPIO_Speed_2MHz ? 0U :
                           init->GPIO_Speed == GPIO_Speed_10MHz ? 1U : 2U;
    for (uint32_t pin = 0U; pin < 16U; ++pin) {
        if ((init->GPIO_Pin & (1U << pin)) == 0U) { continue; }
        const uint32_t shift = pin * 2U;
        gpio->OTYPER = (gpio->OTYPER & ~(1U << pin)) | (od << pin);
        gpio->OSPEEDR = (gpio->OSPEEDR & ~(3U << shift)) | (speed << shift);
        gpio->PUPDR = (gpio->PUPDR & ~(3U << shift)) | (pull << shift);
        if (mode == 2U) {
            uint32_t af;
            if (gpio == GPIOA && pin == 9U && od == 0U) { af = 7U; }
            else if (gpio == GPIOB && (pin == 6U || pin == 7U) && od != 0U) { af = 4U; }
            else { assert_param(0); return; }
            const uint32_t afshift = (pin % 8U) * 4U;
            gpio->AFR[pin / 8U] = (gpio->AFR[pin / 8U] & ~(15U << afshift)) | (af << afshift);
        }
        /* Apply mode last, after output type and AF routing are ready. */
        gpio->MODER = (gpio->MODER & ~(3U << shift)) | (mode << shift);
    }
}

void GPIO_SetBits(GPIO_TypeDef *gpio, uint16_t pins) { gpio->BSRR = pins; }
void GPIO_ResetBits(GPIO_TypeDef *gpio, uint16_t pins) { gpio->BSRR = (uint32_t)pins << 16U; }

static uint32_t peripheral_clock(uint32_t shift)
{
    SystemCoreClockUpdate();
    const uint32_t ppre = (RCC->CFGR >> shift) & 7U;
    return SystemCoreClock >> (ppre < 4U ? 0U : ppre - 3U);
}

void USART_DeInit(USART_TypeDef *uart)
{
    assert_param(uart == USART1);
    RCC->APB2RSTR |= RCC_APB2RSTR_USART1RST;
    RCC->APB2RSTR &= ~RCC_APB2RSTR_USART1RST;
}

void USART_StructInit(USART_InitTypeDef *init)
{
    *init = (USART_InitTypeDef){9600U, 0U, 0U, 0U, USART_CR1_RE | USART_CR1_TE, 0U};
}

void USART_Init(USART_TypeDef *uart, USART_InitTypeDef *init)
{
    assert_param(uart == USART1);
    assert_param(init->USART_BaudRate >= 1200U && init->USART_BaudRate <= 115200U);
    assert_param(init->USART_WordLength == USART_WordLength_8b &&
                 init->USART_StopBits == USART_StopBits_1 &&
                 init->USART_Parity == USART_Parity_No &&
                 init->USART_HardwareFlowControl == USART_HardwareFlowControl_None);
    uart->CR1 = init->USART_Mode;
    uart->CR2 = init->USART_StopBits;
    uart->CR3 = init->USART_HardwareFlowControl;
    /* Oversampling by 16: BRR encodes PCLK / baud as a rounded integer. */
    uart->BRR = (peripheral_clock(13U) + init->USART_BaudRate / 2U) / init->USART_BaudRate;
}

void USART_Cmd(USART_TypeDef *uart, FunctionalState state) { set_bits(&uart->CR1, USART_CR1_UE, state); }
FlagStatus USART_GetFlagStatus(USART_TypeDef *uart, uint16_t flag) { return (uart->SR & flag) ? SET : RESET; }
void USART_SendData(USART_TypeDef *uart, uint16_t data) { uart->DR = data & 0x1ffU; }

void I2C_DeInit(I2C_TypeDef *i2c)
{
    assert_param(i2c == I2C1);
    RCC->APB1RSTR |= RCC_APB1RSTR_I2C1RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C1RST;
}

void I2C_StructInit(I2C_InitTypeDef *init)
{
    *init = (I2C_InitTypeDef){5000U, I2C_Mode_I2C, I2C_DutyCycle_2, 0U, 0U, I2C_AcknowledgedAddress_7bit};
}

void I2C_Init(I2C_TypeDef *i2c, I2C_InitTypeDef *init)
{
    assert_param(i2c == I2C1);
    assert_param(init->I2C_ClockSpeed > 0U && init->I2C_ClockSpeed <= 400000U);
    assert_param(init->I2C_Mode == I2C_Mode_I2C && init->I2C_DutyCycle == I2C_DutyCycle_2);
    assert_param(init->I2C_AcknowledgedAddress == I2C_AcknowledgedAddress_7bit);
    const uint32_t pclk = peripheral_clock(10U);
    const uint32_t mhz = pclk / 1000000U;
    assert_param(mhz >= 2U && mhz <= 50U);
    i2c->CR1 &= ~I2C_CR1_PE;
    i2c->CR2 = (i2c->CR2 & ~I2C_CR2_FREQ) | mhz;
    uint32_t ccr;
    if (init->I2C_ClockSpeed <= 100000U) {
        ccr = pclk / (2U * init->I2C_ClockSpeed);
        if (ccr < 4U) { ccr = 4U; }
        i2c->CCR = ccr;
        i2c->TRISE = mhz + 1U;
    } else {
        assert_param(mhz >= 4U);
        ccr = pclk / (3U * init->I2C_ClockSpeed);
        if (ccr == 0U) { ccr = 1U; }
        i2c->CCR = I2C_CCR_FS | ccr;
        i2c->TRISE = (mhz * 300U) / 1000U + 1U;
    }
    i2c->OAR1 = init->I2C_AcknowledgedAddress | init->I2C_OwnAddress1;
    i2c->CR1 = init->I2C_Ack | I2C_CR1_PE;
}

void I2C_Cmd(I2C_TypeDef *i2c, FunctionalState state) { set_bits(&i2c->CR1, I2C_CR1_PE, state); }
void I2C_SoftwareResetCmd(I2C_TypeDef *i2c, FunctionalState state) { set_bits(&i2c->CR1, I2C_CR1_SWRST, state); }
void I2C_GenerateSTART(I2C_TypeDef *i2c, FunctionalState state) { set_bits(&i2c->CR1, I2C_CR1_START, state); }
void I2C_GenerateSTOP(I2C_TypeDef *i2c, FunctionalState state) { set_bits(&i2c->CR1, I2C_CR1_STOP, state); }
void I2C_AcknowledgeConfig(I2C_TypeDef *i2c, FunctionalState state) { set_bits(&i2c->CR1, I2C_CR1_ACK, state); }
void I2C_Send7bitAddress(I2C_TypeDef *i2c, uint8_t address, uint8_t direction) { i2c->DR = (address & 0xfeU) | (direction & 1U); }
void I2C_SendData(I2C_TypeDef *i2c, uint8_t data) { i2c->DR = data; }
uint8_t I2C_ReceiveData(I2C_TypeDef *i2c) { return (uint8_t)i2c->DR; }

FlagStatus I2C_GetFlagStatus(I2C_TypeDef *i2c, uint32_t flag)
{
    /* Only BUSY is used by the preserved HAL; read SR2 alone to avoid
     * accidentally clearing ADDR via an SR1->SR2 sequence. */
    assert_param(flag == I2C_FLAG_BUSY);
    return (i2c->SR2 & I2C_SR2_BUSY) ? SET : RESET;
}

ErrorStatus I2C_CheckEvent(I2C_TypeDef *i2c, uint32_t event)
{
    const uint32_t sr1 = i2c->SR1;
    const uint32_t sr2 = i2c->SR2;
    /* Same read order and ADDR-clearing side effect as ST SPL. */
    return (((sr1 | (sr2 << 16U)) & event) == event) ? SUCCESS : ERROR;
}
