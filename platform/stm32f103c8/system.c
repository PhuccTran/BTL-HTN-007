/* 72 MHz PLL clock setup from HSE (8 MHz) with HSI fallback.
 * SPL queries the same RCC registers for UART baud and peripheral clocks. */
#include <stdint.h>
#include "stm32f10x.h"

#define REG32(address) (*(volatile uint32_t *)(address))
#define SCB_VTOR     REG32(0xE000ED08UL)

uint32_t SystemCoreClock = 72000000UL;

/* Called before .data/.bss initialization: do not access C globals here. */
void SystemInit(void)
{
    /* Reset RCC clock configuration to default reset state */
    /* Set HSION bit */
    RCC->CR |= (uint32_t)0x00000001UL;

    /* Reset SW, HPRE, PPRE1, PPRE2, ADCPRE and MCO bits */
    RCC->CFGR &= (uint32_t)0xF8FF0000UL;

    /* Reset HSEON, CSSON and PLLON bits */
    RCC->CR &= (uint32_t)0xFEF6FFFFUL;

    /* Reset HSEBYP bit */
    RCC->CR &= (uint32_t)0xFFFBFFFFUL;

    /* Reset PLLSRC, PLLXTPRE, PLLMUL and USBPRE bits */
    RCC->CFGR &= (uint32_t)0xFF80FFFFUL;

    /* Disable all RCC interrupts */
    RCC->CIR = 0x009F0000UL;

    /* Configure 72 MHz from HSE (8 MHz) via PLL */
    /* 1. Enable HSE */
    RCC->CR |= (uint32_t)RCC_CR_HSEON;

    /* 2. Wait until HSE is ready or timeout */
    volatile uint32_t startup_counter = 0;
    uint32_t hse_status = 0;
    do
    {
        hse_status = RCC->CR & RCC_CR_HSERDY;
        startup_counter++;
    } while ((hse_status == 0UL) && (startup_counter < HSE_STARTUP_TIMEOUT));

    if ((RCC->CR & RCC_CR_HSERDY) != 0UL)
    {
        /* Enable Prefetch Buffer and set 2 Flash Latency wait states (required for 48MHz < SYSCLK <= 72MHz) */
        FLASH->ACR = (uint32_t)(FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2);

        /* HCLK = SYSCLK (72 MHz) */
        RCC->CFGR |= (uint32_t)RCC_CFGR_HPRE_DIV1;

        /* PCLK2 = HCLK (72 MHz max) */
        RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE2_DIV1;

        /* PCLK1 = HCLK / 2 (36 MHz max - APB1 limit on STM32F103) */
        RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE1_DIV2;

        /* PLL configuration: PLLCLK = HSE (8 MHz) * 9 = 72 MHz */
        RCC->CFGR &= (uint32_t)(~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL));
        RCC->CFGR |= (uint32_t)(RCC_CFGR_PLLSRC_HSE | RCC_CFGR_PLLMULL9);

        /* Enable PLL */
        RCC->CR |= (uint32_t)RCC_CR_PLLON;

        /* Wait till PLL is ready */
        while ((RCC->CR & RCC_CR_PLLRDY) == 0UL)
        {
        }

        /* Select PLL as system clock source */
        RCC->CFGR &= (uint32_t)(~RCC_CFGR_SW);
        RCC->CFGR |= (uint32_t)RCC_CFGR_SW_PLL;

        /* Wait till PLL is used as system clock source */
        while ((RCC->CFGR & (uint32_t)RCC_CFGR_SWS) != (uint32_t)0x08UL)
        {
        }
    }
    else
    {
        /* HSE startup failed: fall back to HSI (8 MHz) */
        FLASH->ACR = (uint32_t)FLASH_ACR_PRFTBE;
    }

    /* Set Vector Table base location */
    SCB_VTOR = 0x08000000UL;
}

/* Empty C runtime init/fini hooks used by newlib with -nostartfiles. */
void _init(void) { }
void _fini(void) { }

void SystemCoreClockUpdate(void)
{
    RCC_ClocksTypeDef clocks;
    RCC_GetClocksFreq(&clocks);
    SystemCoreClock = clocks.HCLK_Frequency;
}
