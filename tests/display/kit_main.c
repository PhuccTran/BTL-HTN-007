/* Entry point RIÊNG của firmware test. Main nhóm không được sửa ở đây.
 * Không gọi app_bmi160_start(), UART hay task nút nhấn.
 */
#include "stm32f4xx.h"
#include "task_display.h"
#include "hal_st7735.h"
#include "mock_display.h"

#if DISPLAY_TEST_BASIC
static TaskHandle_t s_basic_task;
volatile hal_st7735_status_t g_display_basic_result;
volatile UBaseType_t g_display_basic_stack_free;

#define BASIC(call) do { g_display_basic_result=(call); if(g_display_basic_result!=HAL_ST7735_OK) { return; } } while(0)
static void prvPattern(void)
{
    BASIC(hal_st7735_fill_rect(0,0,128U,160U,HAL_ST7735_WHITE));
    vTaskDelay(pdMS_TO_TICKS(1500U));
    BASIC(hal_st7735_fill_rect(0,0,128U,160U,HAL_ST7735_BLACK));
    vTaskDelay(pdMS_TO_TICKS(1500U));
    BASIC(hal_st7735_fill_rect(0,0,128U,160U,HAL_ST7735_WHITE));
    for (unsigned y=0U;y<8U;++y)
    { for (unsigned x=0U;x<8U;++x)
      { BASIC(hal_st7735_fill_rect((int16_t)(x*16U),(int16_t)(32U+y*12U),16U,12U,
                                   ((x+y)&1U)!=0U?HAL_ST7735_BLACK:HAL_ST7735_WHITE)); } }
    BASIC(hal_st7735_draw_hline(0,0,128U,HAL_ST7735_BLACK));
    BASIC(hal_st7735_draw_hline(0,159,128U,HAL_ST7735_BLACK));
    BASIC(hal_st7735_draw_vline(0,0,160U,HAL_ST7735_BLACK));
    BASIC(hal_st7735_draw_vline(127,0,160U,HAL_ST7735_BLACK));
    BASIC(hal_st7735_draw_text(4,5,"ST7735 128X160",HAL_ST7735_BLACK,HAL_ST7735_WHITE));
    BASIC(hal_st7735_draw_text(4,17,"TOP LEFT",HAL_ST7735_BLACK,HAL_ST7735_WHITE));
    BASIC(hal_st7735_draw_circle_filled(16,143,7U,HAL_ST7735_BLACK));
    BASIC(hal_st7735_draw_char(34,140,'A',HAL_ST7735_BLACK,HAL_ST7735_WHITE));
    BASIC(hal_st7735_draw_text(47,140,"0123456789",HAL_ST7735_BLACK,HAL_ST7735_WHITE));
    vTaskDelay(pdMS_TO_TICKS(4000U));
}
static void prvBasicTask(void *argument)
{
    (void)argument;
    for (;;)
    {
        g_display_basic_result=hal_st7735_init();
        if (g_display_basic_result==HAL_ST7735_OK) { prvPattern(); }
        g_display_basic_stack_free=uxTaskGetStackHighWaterMark(NULL);
        vTaskDelay(pdMS_TO_TICKS(1000U));
    }
}
#endif

int main(void)
{
    SystemCoreClockUpdate();
    NVIC_SetPriorityGrouping(3U);
#if DISPLAY_TEST_BASIC
    configASSERT(hal_st7735_prepare()==pdPASS);
    configASSERT(xTaskCreate(prvBasicTask,"DISPLAY_BASIC",APP_DISPLAY_STACK_WORDS,NULL,
                             APP_DISPLAY_PRIORITY,&s_basic_task)==pdPASS);
#else
    configASSERT(task_display_create()==pdPASS);
    configASSERT(task_display_mock_create()==pdPASS);
#endif
    vTaskStartScheduler();
    configASSERT(pdFALSE);
    for (;;) { }
}
