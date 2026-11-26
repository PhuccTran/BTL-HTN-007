#ifndef TASK_DISPLAY_H
#define TASK_DISPLAY_H
#include <stddef.h>
#include "app_ipc.h"
#include "task.h"
typedef struct {
    uint32_t messages, render_errors, rejected, last_render_ticks, max_render_ticks;
    UBaseType_t stack_free_words;
    size_t heap_free_bytes;
    BaseType_t ready;
} DisplayStats_t;

/* Startup tuần tự trước scheduler; không tự gắn vào main.c của bạn BMI. */
BaseType_t task_display_create(void);
/* Task producer: timeout 0..100 ms, Queue đầy trả pdFAIL, không tự bỏ sự kiện.
 * Không được dùng từ ISR. Không dùng Queue cảm biến của UART để tránh tranh nhận.
 */
BaseType_t task_display_post(const DisplayMsg_t *msg,TickType_t timeout);
void task_display_get_stats(DisplayStats_t *out);
#endif
