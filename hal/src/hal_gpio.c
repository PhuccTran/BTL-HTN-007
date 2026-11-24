#include "hal_gpio.h"
#include "bsp_display.h"

static void prvWrite(GPIO_TypeDef *port, uint32_t pin, bool high)
{

    port->BSRR = 1UL << (pin + (high ? 0U : 16U));
}

static void prvOutput(GPIO_TypeDef *port, uint32_t pin)
{
    port->OTYPER &= ~(1UL << pin);
    port->OSPEEDR &= ~(3UL << (2U * pin));
    port->PUPDR &= ~(3UL << (2U * pin));
    port->MODER = (port->MODER & ~(3UL << (2U * pin))) | (1UL << (2U * pin));
}

static void prvAlternate(GPIO_TypeDef *port, uint32_t pin)
{
    uint32_t shift = (pin % 8U) * 4U;
    port->AFR[pin / 8U] = (port->AFR[pin / 8U] & ~(15UL << shift)) |
                          ((uint32_t)BSP_DISPLAY_SPI_AF << shift);
    port->OTYPER &= ~(1UL << pin);
    port->OSPEEDR = (port->OSPEEDR & ~(3UL << (pin * 2U))) | (2UL << (pin * 2U));
    port->PUPDR &= ~(3UL << (pin * 2U));
    port->MODER = (port->MODER & ~(3UL << (pin * 2U))) | (2UL << (pin * 2U));
}

void hal_gpio_display_init(void)
{
    BSP_DISPLAY_GPIO_ENABLE();
    (void)RCC->AHB1ENR;
    hal_gpio_display_cs(false);
    hal_gpio_display_dc(true);
    hal_gpio_display_reset(false);
    prvOutput(BSP_DISPLAY_CS_PORT, BSP_DISPLAY_CS_PIN);
    prvOutput(BSP_DISPLAY_DC_PORT, BSP_DISPLAY_DC_PIN);
    prvOutput(BSP_DISPLAY_RST_PORT, BSP_DISPLAY_RST_PIN);
#if BSP_DISPLAY_BL_CONTROLLED
    (void)hal_gpio_display_backlight(false);
    prvOutput(BSP_DISPLAY_BL_PORT, BSP_DISPLAY_BL_PIN);
#endif
    prvAlternate(BSP_DISPLAY_SCK_PORT, BSP_DISPLAY_SCK_PIN);
    prvAlternate(BSP_DISPLAY_MOSI_PORT, BSP_DISPLAY_MOSI_PIN);
}

void hal_gpio_display_cs(bool selected) { prvWrite(BSP_DISPLAY_CS_PORT, BSP_DISPLAY_CS_PIN, !selected); }
void hal_gpio_display_dc(bool data) { prvWrite(BSP_DISPLAY_DC_PORT, BSP_DISPLAY_DC_PIN, data); }
void hal_gpio_display_reset(bool asserted) { prvWrite(BSP_DISPLAY_RST_PORT, BSP_DISPLAY_RST_PIN, !asserted); }
bool hal_gpio_display_backlight(bool on)
{
#if BSP_DISPLAY_BL_CONTROLLED
    prvWrite(BSP_DISPLAY_BL_PORT, BSP_DISPLAY_BL_PIN,
             BSP_DISPLAY_BL_ACTIVE_HIGH ? on : !on);
    return true;
#else
    (void)on;
    return false;
#endif
}
