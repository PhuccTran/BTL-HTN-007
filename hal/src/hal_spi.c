#include "hal_spi.h"
#include "bsp_display.h"
#include "semphr.h"
#include "task.h"
#include <string.h>

static bool s_prepared;
static bool s_faulted;
#if BSP_DISPLAY_USE_DMA
static SemaphoreHandle_t s_done;
static volatile uint32_t s_dma_error;
/* DMA chỉ đọc buffer tĩnh này. Timeout không để DMA đọc stack đã hết lifetime. */
static uint8_t s_dma_buffer[256];
#endif

static TickType_t prvLeft(TickType_t start, TickType_t budget)
{
    TickType_t used = xTaskGetTickCount() - start;
    return used >= budget ? 0U : budget - used;
}

static hal_spi_status_t prvWait(uint32_t mask, bool set, TickType_t start, TickType_t budget)
{
    uint32_t spins=0U;
    while (((BSP_DISPLAY_SPI->SR & mask) != 0U) != set)
    {
        if (prvLeft(start, budget) == 0U) { return HAL_SPI_TIMEOUT; }
        /* Byte SPI chỉ cần khoảng microsecond. Không ngủ 1 ms cho từng byte;
         * chỉ nhường CPU khi cờ thực sự kẹt qua nhiều lần đọc. */
        if (++spins>=64U) { spins=0U; vTaskDelay(1U); }
    }
    return HAL_SPI_OK;
}

BaseType_t hal_spi_display_prepare(void)
{
    if (s_prepared) { return s_faulted ? pdFAIL : pdPASS; }
#if BSP_DISPLAY_USE_DMA
    s_done = xSemaphoreCreateBinary();
    if (s_done == NULL) { return pdFAIL; }
    BSP_DISPLAY_DMA_ENABLE();
    (void)RCC->AHB1ENR;
    /* Dành riêng Stream3. Không chiếm controller DMA2 của ngoại vi khác. */
    if ((BSP_DISPLAY_DMA_STREAM->CR & DMA_SxCR_EN) != 0U)
    {
        vSemaphoreDelete(s_done);
        s_done = NULL;
        return pdFAIL;
    }
    BSP_DISPLAY_DMA->LIFCR = BSP_DISPLAY_DMA_CLEAR;
    NVIC_ClearPendingIRQ(BSP_DISPLAY_DMA_IRQ);
    NVIC_SetPriority(BSP_DISPLAY_DMA_IRQ, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1U);
    NVIC_EnableIRQ(BSP_DISPLAY_DMA_IRQ);
#endif
    BSP_DISPLAY_SPI_ENABLE();
    (void)RCC->APB2ENR;
    BSP_DISPLAY_SPI_RESET();
    /* 1-line TX: không sinh RX overrun, CPOL=CPHA=0, 8 bit, software NSS. */
    BSP_DISPLAY_SPI->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI |
                          SPI_CR1_BIDIMODE | SPI_CR1_BIDIOE | BSP_DISPLAY_SPI_BR;
    BSP_DISPLAY_SPI->CR2 = 0U;
    BSP_DISPLAY_SPI->CR1 |= SPI_CR1_SPE;
    s_prepared = true;
    return pdPASS;
}

#if BSP_DISPLAY_USE_DMA
static bool prvStopDma(void)
{
    /* Chặn IRQ trước cleanup để completion cũ không đánh thức lần truyền mới. */
    NVIC_DisableIRQ(BSP_DISPLAY_DMA_IRQ);
    BSP_DISPLAY_SPI->CR2 &= ~SPI_CR2_TXDMAEN;
    BSP_DISPLAY_DMA_STREAM->CR &= ~DMA_SxCR_EN;
    /* Chờ phần cứng bằng số vòng hữu hạn, kể cả khi tick không tiến. */
    uint32_t guard = 100000U;
    while ((BSP_DISPLAY_DMA_STREAM->CR & DMA_SxCR_EN) != 0U && guard != 0U) { --guard; }
    bool stopped = (BSP_DISPLAY_DMA_STREAM->CR & DMA_SxCR_EN) == 0U;
    BSP_DISPLAY_DMA->LIFCR = BSP_DISPLAY_DMA_CLEAR;
    NVIC_ClearPendingIRQ(BSP_DISPLAY_DMA_IRQ);
    (void)xSemaphoreTake(s_done, 0U);
    if (!stopped) { s_faulted = true; } /* Không bao giờ ghi lại DMA buffer lúc này. */
    else { NVIC_EnableIRQ(BSP_DISPLAY_DMA_IRQ); }
    return stopped;
}

void BSP_DISPLAY_DMA_HANDLER(void)
{
    uint32_t flags = BSP_DISPLAY_DMA->LISR;
    if ((flags & (BSP_DISPLAY_DMA_TC | BSP_DISPLAY_DMA_ERRORS)) != 0U)
    {
        BaseType_t higher_woken = pdFALSE;
        s_dma_error = flags & BSP_DISPLAY_DMA_ERRORS;
        BSP_DISPLAY_DMA->LIFCR = BSP_DISPLAY_DMA_CLEAR;
        /* ISR không chờ SPI BSY và không gọi API task thường. */
        (void)xSemaphoreGiveFromISR(s_done, &higher_woken);
        portYIELD_FROM_ISR(higher_woken);
    }
}
#endif

hal_spi_status_t hal_spi_display_write(const uint8_t *data, size_t length, TickType_t timeout)
{
    if (data == NULL || length == 0U || timeout == 0U || timeout == portMAX_DELAY ||
        __get_IPSR() != 0U || xTaskGetSchedulerState() != taskSCHEDULER_RUNNING)
    { return HAL_SPI_INVALID; }
    if (!s_prepared || s_faulted) { return HAL_SPI_ERROR; }
    TickType_t start = xTaskGetTickCount();
    hal_spi_status_t result = HAL_SPI_OK;
    while (length != 0U)
    {
        if (prvLeft(start, timeout) == 0U) { result = HAL_SPI_TIMEOUT; break; }
#if BSP_DISPLAY_USE_DMA
        if (length >= 16U)
        {
            size_t n = length > sizeof(s_dma_buffer) ? sizeof(s_dma_buffer) : length;
            memcpy(s_dma_buffer, data, n);
            (void)xSemaphoreTake(s_done, 0U);
            s_dma_error = 0U;
            BSP_DISPLAY_DMA->LIFCR = BSP_DISPLAY_DMA_CLEAR;
            BSP_DISPLAY_DMA_STREAM->PAR = (uint32_t)(uintptr_t)&BSP_DISPLAY_SPI->DR;
            BSP_DISPLAY_DMA_STREAM->M0AR = (uint32_t)(uintptr_t)s_dma_buffer;
            BSP_DISPLAY_DMA_STREAM->NDTR = (uint32_t)n;
            BSP_DISPLAY_DMA_STREAM->FCR = DMA_SxFCR_FEIE;
            BSP_DISPLAY_DMA_STREAM->CR = BSP_DISPLAY_DMA_CHANNEL | DMA_SxCR_MINC |
                                         DMA_SxCR_DIR_0 | DMA_SxCR_TCIE |
                                         DMA_SxCR_TEIE | DMA_SxCR_DMEIE;
            __DMB();
            BSP_DISPLAY_DMA_STREAM->CR |= DMA_SxCR_EN;
            BSP_DISPLAY_SPI->CR2 |= SPI_CR2_TXDMAEN;
            if (xSemaphoreTake(s_done, prvLeft(start, timeout)) != pdPASS)
            { result = HAL_SPI_TIMEOUT; }
            else if (s_dma_error != 0U) { result = HAL_SPI_ERROR; }
            if (!prvStopDma()) { result = HAL_SPI_ERROR; }
            if (result != HAL_SPI_OK) { break; }
            data += n;
            length -= n;
            continue;
        }
#endif
        result = prvWait(SPI_SR_TXE, true, start, timeout);
        if (result != HAL_SPI_OK) { break; }
        *(__IO uint8_t *)&BSP_DISPLAY_SPI->DR = *data++;
        --length;
    }
    /* DMA TC chỉ báo hết buffer, phải đợi bit cuối ra dây trước khi đổi DC/CS. */
    if (result == HAL_SPI_OK) { result = prvWait(SPI_SR_TXE, true, start, timeout); }
    if (result == HAL_SPI_OK) { result = prvWait(SPI_SR_BSY, false, start, timeout); }
    if (result != HAL_SPI_OK)
    {
        /* Bỏ transfer lỗi, trả bus về mode đã biết; driver sẽ yêu cầu redraw. */
        BSP_DISPLAY_SPI_RESET();
        BSP_DISPLAY_SPI->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI |
                              SPI_CR1_BIDIMODE | SPI_CR1_BIDIOE | BSP_DISPLAY_SPI_BR | SPI_CR1_SPE;
    }
    return result;
}
