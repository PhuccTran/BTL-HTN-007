/* STM32F411CE at 3.3 V: HSI 16 MHz / 16 * 192 / 2 = 96 MHz.
 * AHB = 96 MHz, APB1 = 48 MHz, APB2 = 96 MHz, PLLQ = 48 MHz.
 * HSI avoids assuming a board-specific HSE crystal frequency.
 */
#include "stm32f4xx.h"

#define HSI_HZ 16000000UL
#define CLOCK_TIMEOUT 1000000UL
uint32_t SystemCoreClock = HSI_HZ;

static int wait_bits(volatile uint32_t *reg, uint32_t mask, uint32_t value)
{
    for (uint32_t count = 0U; count < CLOCK_TIMEOUT; ++count) {
        if ((*reg & mask) == value) { return 1; }
    }
    return 0;
}

/* Called before .data/.bss initialization: use registers and local data only. */
void SystemInit(void)
{
    /* Enable CP10/CP11 before any hard-float code or C library executes. */
    SCB->CPACR |= (0xfUL << 20U);
    __DSB();
    __ISB();
    SCB->VTOR = FLASH_BASE;

    RCC->CR |= RCC_CR_HSION;
    if (!wait_bits(&RCC->CR, RCC_CR_HSIRDY, RCC_CR_HSIRDY)) {
        for (;;) { } /* No usable clock source. */
    }
    RCC->CFGR &= ~RCC_CFGR_SW;
    if (!wait_bits(&RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_HSI)) {
        for (;;) { }
    }
    RCC->CR &= ~RCC_CR_PLLON;
    if (!wait_bits(&RCC->CR, RCC_CR_PLLRDY, 0U)) { return; }
    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);
    RCC->CIR = 0U;

    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC->APB1ENR;
    /* Scale 1 permits up to 100 MHz on F411. VOSRDY is checked after PLL lock. */
    PWR->CR |= PWR_CR_VOS;
    FLASH->ACR = FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN | FLASH_ACR_LATENCY_3WS;
    if (!wait_bits(&FLASH->ACR, FLASH_ACR_LATENCY, FLASH_ACR_LATENCY_3WS)) { return; }
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;
    RCC->PLLCFGR = 16U | (192U << RCC_PLLCFGR_PLLN_Pos) | (4U << RCC_PLLCFGR_PLLQ_Pos);
    RCC->CR |= RCC_CR_PLLON;
    if (!wait_bits(&RCC->CR, RCC_CR_PLLRDY, RCC_CR_PLLRDY) ||
        !wait_bits(&PWR->CSR, PWR_CSR_VOSRDY, PWR_CSR_VOSRDY)) {
        RCC->CR &= ~RCC_CR_PLLON;
        return; /* Remain on HSI; main.c recalculates the actual clock. */
    }
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    if (!wait_bits(&RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_PLL)) {
        RCC->CFGR &= ~RCC_CFGR_SW;
        if (!wait_bits(&RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_HSI)) {
            for (;;) { }
        }
        RCC->CR &= ~RCC_CR_PLLON;
    }
}

void SystemCoreClockUpdate(void)
{
    static const uint8_t ahb_shift[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};
    uint32_t sysclk = HSI_HZ;
    /* This platform configures only HSI or HSI-fed PLL. */
    if ((RCC->CFGR & RCC_CFGR_SWS) == RCC_CFGR_SWS_PLL) {
        const uint32_t pll = RCC->PLLCFGR;
        const uint32_t m = pll & RCC_PLLCFGR_PLLM;
        const uint32_t n = (pll & RCC_PLLCFGR_PLLN) >> RCC_PLLCFGR_PLLN_Pos;
        const uint32_t p = (((pll & RCC_PLLCFGR_PLLP) >> RCC_PLLCFGR_PLLP_Pos) + 1U) * 2U;
        sysclk = (HSI_HZ / m) * n / p;
    }
    SystemCoreClock = sysclk >> ahb_shift[(RCC->CFGR & RCC_CFGR_HPRE) >> RCC_CFGR_HPRE_Pos];
}

/* Newlib constructor/destructor hooks with -nostartfiles. */
void _init(void) { }
void _fini(void) { }
