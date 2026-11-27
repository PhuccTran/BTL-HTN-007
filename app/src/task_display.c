/* ============================================================================
 * task_display.c - Task FreeRTOS của module màn hình.
 *
 * Tệp này giữ: task "DISPLAY", queue nhận message từ game, bộ số liệu thống kê.
 * Ba hàm công khai:
 *   - task_display_create()    : tạo queue + task (gọi trước khi scheduler chạy)
 *   - task_display_post()      : game gửi message vào queue (gọi từ task)
 *   - task_display_get_stats() : đọc số liệu thống kê
 * ========================================================================== */
#include "task_display.h"
#include "display_render.h"
#include "queue.h"
#include "semphr.h"
#include "stm32f4xx.h"
#include <string.h>

/* Bốn biến chỉ dùng trong tệp này. */
static TaskHandle_t s_display_task;     /* mã nhận dạng task; khác NULL = đã tạo task */
static QueueHandle_t s_display_queue;   /* hàng đợi message từ game sang màn hình */
static DisplayRender_t s_render;        /* trạng thái của bộ vẽ (chỉ task này đụng tới) */
static DisplayStats_t s_stats;          /* số liệu thống kê */

/* Hàm mà task DISPLAY chạy mãi mãi. */
static void prvDisplayTask(void *argument)
{
    (void)argument;                     /* tham số không dùng */
    DisplayMsg_t msg;                   /* chỗ chứa message nhận từ queue */
    bool initialized=false;             /* LCD đã khởi tạo xong chưa */
    task_display_render_reset(&s_render);
    for (;;)
    {
        /* --- Bước 1: khởi tạo LCD nếu chưa xong --- */
        if (!initialized)
        {
            initialized=hal_st7735_init()==HAL_ST7735_OK;
            /* Vùng găng (critical section): trong lúc ghi s_stats, task khác
             * và ngắt không chen vào. */
            taskENTER_CRITICAL(); s_stats.ready=initialized?pdTRUE:pdFALSE; taskEXIT_CRITICAL();
            if (!initialized)
            {
                /* Khởi tạo lỗi: tính một lỗi, đợi 500 ms rồi quay lại thử tiếp. */
                taskENTER_CRITICAL(); ++s_stats.render_errors; taskEXIT_CRITICAL();
                vTaskDelay(pdMS_TO_TICKS(APP_DISPLAY_RETRY_MS));
                continue;
            }
            /* Khởi tạo xong: coi như màn hình chưa vẽ gì, lần vẽ tới vẽ toàn bộ. */
            task_display_render_invalidate(&s_render);
        }

        /* --- Bước 2: chờ message --- */
        /* Queue rỗng: ngủ tối đa 50 ms; có message thì producer đánh thức ngay.
         * Timeout cho phép retry redraw lỗi và cập nhật thống kê dù không có data.
         */
        BaseType_t received=xQueueReceive(s_display_queue,&msg,pdMS_TO_TICKS(APP_DISPLAY_QUEUE_WAIT_MS));

        /* --- Bước 3: vẽ và đo thời gian --- */
        TickType_t start=xTaskGetTickCount();   /* tick của RTOS trước khi vẽ */
        /* Có message: xử lý nó. Không có: refresh (chỉ vẽ nếu màn hình đang lệch). */
        hal_st7735_status_t result=received==pdPASS?
            task_display_render_apply(&s_render,&msg):task_display_render_refresh(&s_render);
        TickType_t elapsed=xTaskGetTickCount()-start;   /* số tick đã trôi qua khi vẽ */
        UBaseType_t hwm=uxTaskGetStackHighWaterMark(NULL);  /* stack trống ít nhất từ trước đến nay (từ) */
        size_t heap=xPortGetFreeHeapSize();                 /* heap còn trống (byte) */

        /* --- Bước 4: ghi thống kê --- */
        taskENTER_CRITICAL();
        if (received==pdPASS) { ++s_stats.messages; }
        s_stats.last_render_ticks=elapsed;
        if (elapsed>s_stats.max_render_ticks) { s_stats.max_render_ticks=elapsed; }
        s_stats.stack_free_words=hwm;
        s_stats.heap_free_bytes=heap;
        if (result!=HAL_ST7735_OK) { ++s_stats.render_errors; }
        taskEXIT_CRITICAL();

        /* --- Bước 5: vẽ lỗi thì đánh dấu vẽ lại toàn bộ và nghỉ 500 ms --- */
        if (result!=HAL_ST7735_OK)
        {
            task_display_render_invalidate(&s_render);
            vTaskDelay(pdMS_TO_TICKS(APP_DISPLAY_RETRY_MS));
        }
    }
}

/* Tạo queue và task. Chỉ gọi được một lần, từ ngữ cảnh thường (không phải ngắt),
 * trước khi scheduler chạy. Trả pdPASS nếu thành công. */
BaseType_t task_display_create(void)
{
    /* Gọi từ ngắt, hoặc khi scheduler đã chạy: từ chối. */
    if (__get_IPSR()!=0U || xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED) { return pdFAIL; }
    /* Task đã được tạo rồi: trả thành công, không làm gì thêm. */
    if (s_display_task!=NULL) { return pdPASS; }
    /* config hook malloc-failed của project dừng CPU. Kiểm tra trước khi cấp
     * phát, chừa chỗ cho Idle/Timer. Không tự ý sửa heap 8 KiB của file chung.
     * Startup phải tuần tự; nếu nhóm thêm task, gọi sau khi đã cân đối heap.
     */
    /* Số byte heap cần: stack task + cấu trúc task + chỗ chứa message của queue
     * + 3 cấu trúc queue (queue này, mutex LCD, semaphore SPI DMA)
     * + 128 byte cộng thêm + phần chừa cho Idle/Timer. */
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
    /* Heap không đủ: trả lỗi thay vì để malloc-failed hook dừng CPU. */
    if (xPortGetFreeHeapSize()<needed) { return pdFAIL; }
    /* Tạo queue chứa 2 message (mỗi message là cả DisplayMsg_t). */
    s_display_queue=xQueueCreate(APP_DISPLAY_QUEUE_LENGTH,sizeof(DisplayMsg_t));
    if (s_display_queue==NULL) { return pdFAIL; }
    /* Chuẩn bị phần LCD (mutex, chân GPIO, SPI). Lỗi thì xóa queue vừa tạo. */
    if (hal_st7735_prepare()!=pdPASS)
    { vQueueDelete(s_display_queue); s_display_queue=NULL; return pdFAIL; }
    /* Tạo task "DISPLAY". Lỗi thì xóa queue. */
    BaseType_t status=xTaskCreate(prvDisplayTask,"DISPLAY",APP_DISPLAY_STACK_WORDS,NULL,
                                  APP_DISPLAY_PRIORITY,&s_display_task);
    if (status!=pdPASS) { vQueueDelete(s_display_queue); s_display_queue=NULL; }
    return status;
}

/* Game gọi hàm này để gửi message. Chỉ gọi từ task, sau khi scheduler chạy.
 * timeout: thời gian chờ tối đa nếu queue đầy, từ 0 đến 100 ms.
 * Trả pdPASS nếu đã vào queue; pdFAIL nếu không (producer tự quyết định retry). */
BaseType_t task_display_post(const DisplayMsg_t *msg,TickType_t timeout)
{
    /* Từ chối ngay nếu: gọi từ ngắt, scheduler chưa chạy, queue chưa tạo,
     * timeout là chờ vô hạn, timeout quá 100 ms, hoặc message không hợp lệ. */
    if (__get_IPSR()!=0U || xTaskGetSchedulerState()!=taskSCHEDULER_RUNNING ||
        s_display_queue==NULL || timeout==portMAX_DELAY ||
        timeout>pdMS_TO_TICKS(APP_DISPLAY_POST_MAX_MS) || !task_display_render_validate(msg))
    { return pdFAIL; }
    /* Chép message vào queue; queue đầy thì chờ tối đa timeout. */
    BaseType_t result=xQueueSend(s_display_queue,msg,timeout);
    /* Gửi không được: tính vào số bị từ chối (rejected). */
    if (result!=pdPASS) { taskENTER_CRITICAL(); ++s_stats.rejected; taskEXIT_CRITICAL(); }
    return result;
}

/* Chép số liệu thống kê sang biến của người gọi. Trong vùng găng để bản chép
 * không lẫn số cũ và số mới khi task Display đang ghi dở. */
void task_display_get_stats(DisplayStats_t *out)
{
    if (out!=NULL)
    { taskENTER_CRITICAL(); memcpy(out,&s_stats,sizeof(*out)); taskEXIT_CRITICAL(); }
}
