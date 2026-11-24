
#include "hal_spi.h"
#include "bsp_display.h"
#include "task.h"
#include "semphr.h"
#include <string.h>

volatile uint32_t test_failure,test_checks,test_stuck_dma;
uint32_t SystemCoreClock=96000000U;
static TickType_t s_tick;
static bool s_available;
static uint8_t s_capture[1024];
static size_t s_count;
static uint32_t s_mode,s_irq_count;
static int s_scheduler=taskSCHEDULER_RUNNING;
void BSP_DISPLAY_DMA_HANDLER(void);
__attribute__((noinline,noreturn)) void test_done(void) { for(;;) { __asm volatile("nop"); } }
#define CHECK(e) do { ++test_checks; if(!(e)) { test_failure=__LINE__; test_done(); } } while(0)
TickType_t xTaskGetTickCount(void) { return s_tick++; }
BaseType_t xTaskGetSchedulerState(void) { return s_scheduler; }
void vTaskDelay(TickType_t t) { CHECK(t>0U&&t!=portMAX_DELAY); s_tick+=t; }
SemaphoreHandle_t xSemaphoreCreateBinary(void) { return &s_available; }
void vSemaphoreDelete(SemaphoreHandle_t s) { (void)s; }
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t s,BaseType_t *w)
{ CHECK(s==&s_available); *w=pdFALSE; s_available=true; ++s_irq_count; return pdPASS; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t s,TickType_t t)
{
    CHECK(s==&s_available && t!=portMAX_DELAY);
    if(s_available) { s_available=false; return pdPASS; }
    if(t==0U) { return pdFAIL; }
    CHECK((BSP_DISPLAY_DMA_STREAM->CR&DMA_SxCR_EN)!=0U);
    if(s_mode==1U || s_mode==4U) { s_tick+=t; return pdFAIL; }
    size_t n=BSP_DISPLAY_DMA_STREAM->NDTR;
    CHECK(n<=256U && n+s_count<=sizeof(s_capture));
    memcpy(s_capture+s_count,(void *)(uintptr_t)BSP_DISPLAY_DMA_STREAM->M0AR,n); s_count+=n;
    CHECK(BSP_DISPLAY_DMA_STREAM->PAR==(uint32_t)(uintptr_t)&SPI1->DR);
    CHECK((BSP_DISPLAY_DMA_STREAM->CR&DMA_SxCR_CHSEL)==BSP_DISPLAY_DMA_CHANNEL);
    BSP_DISPLAY_DMA_STREAM->CR&=~DMA_SxCR_EN;
    BSP_DISPLAY_DMA->LISR=s_mode==2U?DMA_LISR_TEIF3:DMA_LISR_TCIF3;
    if(s_mode==3U) { SPI1->SR=SPI_SR_TXE|SPI_SR_BSY; }
    BSP_DISPLAY_DMA_HANDLER();
    CHECK(s_available); s_available=false;
    return pdPASS;
}
void test_entry(void)
{
    uint8_t data[600]; for(unsigned i=0;i<sizeof(data);++i) { data[i]=(uint8_t)i; }
    CHECK(hal_spi_display_prepare()==pdPASS && hal_spi_display_prepare()==pdPASS);
    CHECK((SPI1->CR1&(SPI_CR1_BIDIMODE|SPI_CR1_BIDIOE))==(SPI_CR1_BIDIMODE|SPI_CR1_BIDIOE));
    SPI1->SR=SPI_SR_TXE;
    CHECK(hal_spi_display_write(data,1U,1000U)==HAL_SPI_OK && (SPI1->DR&255U)==data[0]);
    CHECK(hal_spi_display_write(data,600U,1000U)==HAL_SPI_OK);
    CHECK(s_irq_count==3U && s_count==600U && memcmp(data,s_capture,600U)==0);
    CHECK((BSP_DISPLAY_DMA_STREAM->CR&DMA_SxCR_EN)==0U && (SPI1->CR2&SPI_CR2_TXDMAEN)==0U);
    CHECK(hal_spi_display_write(NULL,1U,10U)==HAL_SPI_INVALID);
    CHECK(hal_spi_display_write(data,0U,10U)==HAL_SPI_INVALID);
    CHECK(hal_spi_display_write(data,1U,0U)==HAL_SPI_INVALID);
    CHECK(hal_spi_display_write(data,1U,portMAX_DELAY)==HAL_SPI_INVALID);
    s_scheduler=0; CHECK(hal_spi_display_write(data,1U,10U)==HAL_SPI_INVALID); s_scheduler=2;
    s_mode=1; CHECK(hal_spi_display_write(data,64U,100U)==HAL_SPI_TIMEOUT);
    CHECK((BSP_DISPLAY_DMA_STREAM->CR&DMA_SxCR_EN)==0U && (SPI1->CR2&SPI_CR2_TXDMAEN)==0U);
    s_mode=0; s_available=true; s_count=0;
    CHECK(hal_spi_display_write(data,64U,100U)==HAL_SPI_OK && s_count==64U);
    s_mode=2; CHECK(hal_spi_display_write(data,64U,100U)==HAL_SPI_ERROR);
    s_mode=3; CHECK(hal_spi_display_write(data,64U,100U)==HAL_SPI_TIMEOUT); SPI1->SR=SPI_SR_TXE;
    s_mode=0; s_tick=UINT32_MAX-10U; CHECK(hal_spi_display_write(data,64U,100U)==HAL_SPI_OK);

    test_stuck_dma=1U; s_mode=4; CHECK(hal_spi_display_write(data,64U,100U)==HAL_SPI_ERROR);
    s_mode=0; CHECK(hal_spi_display_write(data,64U,100U)==HAL_SPI_ERROR);
    test_done();
}
