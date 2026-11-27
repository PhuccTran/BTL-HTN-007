/* Layer 1 (HAL): STM32F411 USART1 TX trên PA9 (AF7) */
#include "hal_uart.h"
#include "stm32f4xx.h"
#include <stddef.h>

#define UART_TX_TIMEOUT_GUARD 100000U

static uint32_t uart_pclk2_hz(void)
{
    SystemCoreClockUpdate();
    uint32_t prescaler = (RCC->CFGR & RCC_CFGR_PPRE2) >> RCC_CFGR_PPRE2_Pos;
    return SystemCoreClock >> (prescaler < 4U ? 0U : prescaler - 3U);
}

bool hal_uart_init(uint32_t baudrate)
{
    if (baudrate == 0U) { return false; }

    uint32_t pclk2 = uart_pclk2_hz();
    uint32_t brr = (pclk2 + (baudrate / 2U)) / baudrate;
    if (brr == 0U || brr > 0xFFFFU) { return false; }

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    (void)RCC->AHB1ENR;
    (void)RCC->APB2ENR;
    __DSB();

    /* PA9: Alternate Function (2), Push-Pull (0), High Speed (2), Pull-up (1), AF7 */
    const uint32_t pin = 9U;
    GPIOA->OTYPER &= ~(1U << pin); // Push-Pull
    GPIOA->OSPEEDR = (GPIOA->OSPEEDR & ~(3U << (pin * 2U))) | (2U << (pin * 2U)); // High Speed
    GPIOA->PUPDR = (GPIOA->PUPDR & ~(3U << (pin * 2U))) | (1U << (pin * 2U)); // Pull-up
    GPIOA->AFR[1] = (GPIOA->AFR[1] & ~(0xFU << ((pin - 8U) * 4U))) | (7U << ((pin - 8U) * 4U)); // AF7
    GPIOA->MODER = (GPIOA->MODER & ~(3U << (pin * 2U))) | (2U << (pin * 2U)); // Alternate Function
    // setup gpio 

    USART1->CR1 = 0U; // disable uart
    USART1->CR2 = 0U; // 1 stop bit, no parity
    USART1->CR3 = 0U; // no flow control
    USART1->BRR = brr; // set baudrate
    USART1->CR1 = USART_CR1_TE | USART_CR1_UE; // enable transmitter and uart
    return true;
}

static void uart_write_char(char c) 
{
    for (uint32_t guard = UART_TX_TIMEOUT_GUARD; guard != 0U; --guard) {
        if ((USART1->SR & USART_SR_TXE) != 0U) {
            USART1->DR = (uint8_t)c;
            return;
        }
    }
}

void hal_uart_write(const char *str) // ham này dùng để gửi chuỗi ra UART
{
    if (str == NULL) { return; }
    while (*str != '\0') {
        uart_write_char(*str++);
    }
    for (uint32_t guard = UART_TX_TIMEOUT_GUARD; guard != 0U; --guard) {
        if ((USART1->SR & USART_SR_TC) != 0U) { break; }
    }
}
