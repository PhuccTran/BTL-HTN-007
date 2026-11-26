#include "task_display.h"
#include "display_render.h"
#include "queue.h"
#include "semphr.h"
#include "stm32f4xx.h"
#include <string.h>

static TaskHandle_t s_display_task;
static QueueHandle_t s_display_queue;
static DisplayRender_t s_render;
static DisplayStats_t s_stats;

static void prvDisplayTask(void *argument)
{
    (void)argument;
    DisplayMsg_t msg;
    bool initialized=false;
    task_display_render_reset(&s_render);
    for (;;)
    {
        if (!initialized)
        {
            initialized=hal_st7735_init()==HAL_ST7735_OK;
            taskENTER_CRITICAL(); s_stats.ready=initialized?pdTRUE:pdFALSE; taskEXIT_CRITICAL();
            if (!initialized)
            {
                taskENTER_CRITICAL(); ++s_stats.render_errors; taskEXIT_CRITICAL();
                vTaskDelay(pdMS_TO_TICKS(APP_DISPLAY_RETRY_MS));
                continue;
            }
            task_display_render_invalidate(&s_render);
        }
        /* Queue rỗng: ngủ tối đa 50 ms; có message thì producer đánh thức ngay.
         * Timeout cho phép retry redraw lỗi và cập nhật thống kê dù không có data.
         */
        BaseType_t received=xQueueReceive(s_display_queue,&msg,pdMS_TO_TICKS(APP_DISPLAY_QUEUE_WAIT_MS));
        TickType_t start=xTaskGetTickCount();
        hal_st7735_status_t result=received==pdPASS?
            task_display_render_apply(&s_render,&msg):task_display_render_refresh(&s_render);
        TickType_t elapsed=xTaskGetTickCount()-start;
        UBaseType_t hwm=uxTaskGetStackHighWaterMark(NULL);
        size_t heap=xPortGetFreeHeapSize();
        taskENTER_CRITICAL();
        if (received==pdPASS) { ++s_stats.messages; }
        s_stats.last_render_ticks=elapsed;
        if (elapsed>s_stats.max_render_ticks) { s_stats.max_render_ticks=elapsed; }
        s_stats.stack_free_words=hwm;
        s_stats.heap_free_bytes=heap;
        if (result!=HAL_ST7735_OK) { ++s_stats.render_errors; }
        taskEXIT_CRITICAL();
        if (result!=HAL_ST7735_OK)
        {
            task_display_render_invalidate(&s_render);
            vTaskDelay(pdMS_TO_TICKS(APP_DISPLAY_RETRY_MS));
        }
    }
}

BaseType_t task_display_create(void)
{
    if (__get_IPSR()!=0U || xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED) { return pdFAIL; }
    if (s_display_task!=NULL) { return pdPASS; }
    /* config hook malloc-failed của project dừng CPU. Kiểm tra trước khi cấp
     * phát, chừa chỗ cho Idle/Timer. Không tự ý sửa heap 8 KiB của file chung.
     * Startup phải tuần tự; nếu nhóm thêm task, gọi sau khi đã cân đối heap.
     */
    size_t needed=APP_DISPLAY_STACK_WORDS*sizeof(StackType_t)+sizeof(StaticTask_t)+
        APP_DISPLAY_QUEUE_LENGTH*sizeof(DisplayMsg_t)+3U*sizeof(StaticQueue_t)+
        128U+APP_DISPLAY_STARTUP_RESERVE;
    HeapStats_t heap_stats;
    vPortGetHeapStats(&heap_stats);
    if (heap_stats.xAvailableHeapSpaceInBytes==0U && heap_stats.xNumberOfSuccessfulAllocations==0U)
    {
        /* heap_4 khởi tạo lười: trước malloc đầu tiên, free-size trả 0 dù còn
         * nguyên 8 KiB. Cấp phát 1 byte rồi trả lại để khởi tạo đúng heap. */
        void *warmup=pvPortMalloc(1U);
        if (warmup==NULL) { return pdFAIL; }
        vPortFree(warmup);
    }
    if (xPortGetFreeHeapSize()<needed) { return pdFAIL; }
    s_display_queue=xQueueCreate(APP_DISPLAY_QUEUE_LENGTH,sizeof(DisplayMsg_t));
    if (s_display_queue==NULL) { return pdFAIL; }
    if (hal_st7735_prepare()!=pdPASS)
    { vQueueDelete(s_display_queue); s_display_queue=NULL; return pdFAIL; }
    BaseType_t status=xTaskCreate(prvDisplayTask,"DISPLAY",APP_DISPLAY_STACK_WORDS,NULL,
                                  APP_DISPLAY_PRIORITY,&s_display_task);
    if (status!=pdPASS) { vQueueDelete(s_display_queue); s_display_queue=NULL; }
    return status;
}

BaseType_t task_display_post(const DisplayMsg_t *msg,TickType_t timeout)
{
    if (__get_IPSR()!=0U || xTaskGetSchedulerState()!=taskSCHEDULER_RUNNING ||
        s_display_queue==NULL || timeout==portMAX_DELAY ||
        timeout>pdMS_TO_TICKS(APP_DISPLAY_POST_MAX_MS) || !task_display_render_validate(msg))
    { return pdFAIL; }
    BaseType_t result=xQueueSend(s_display_queue,msg,timeout);
    if (result!=pdPASS) { taskENTER_CRITICAL(); ++s_stats.rejected; taskEXIT_CRITICAL(); }
    return result;
}
void task_display_get_stats(DisplayStats_t *out)
{
    if (out!=NULL)
    { taskENTER_CRITICAL(); memcpy(out,&s_stats,sizeof(*out)); taskEXIT_CRITICAL(); }
}
