/* Layer 3: App logic - quyết định KHI NÀO gửi và GỬI CHUỖI GÌ */
#include "app_uart.h"
#include "uart_service.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>
#include <string.h>

#define MAX_TASKS_TO_TRACK 8U
#define SENSOR_QUEUE_LEN   8U

static TaskHandle_t s_app_task;
static QueueHandle_t s_sensor_queue = NULL;

BaseType_t app_uart_post_sensor_data(const bmi160_data_msg_t *msg)
{
    if (msg == NULL || s_sensor_queue == NULL)
    {
        return pdFAIL;
    }
    return xQueueSend(s_sensor_queue, msg, 0);
}

static uint32_t get_cpu_usage(void)
{
    static uint32_t s_prev_idle_time = 0;
    static uint32_t s_prev_total_time = 0;

    UBaseType_t task_count = uxTaskGetNumberOfTasks();
    TaskStatus_t task_status[MAX_TASKS_TO_TRACK];
    configRUN_TIME_COUNTER_TYPE total_time = 0;

    if (task_count > MAX_TASKS_TO_TRACK)
    {
        task_count = MAX_TASKS_TO_TRACK;
    }

    uxTaskGetSystemState(task_status, task_count, &total_time);

    uint32_t idle_time = 0;
    for (UBaseType_t i = 0; i < task_count; i++)
    {
        if (strcmp(task_status[i].pcTaskName, "IDLE") == 0)
        {
            idle_time = (uint32_t)task_status[i].ulRunTimeCounter;
            break;
        }
    }

    uint32_t total_diff = (uint32_t)total_time - s_prev_total_time;
    uint32_t idle_diff  = idle_time - s_prev_idle_time;

    s_prev_total_time = (uint32_t)total_time;
    s_prev_idle_time  = idle_time;

    if (total_diff == 0)
    {
        return 0;
    }

    uint32_t idle_percent = (idle_diff * 100U) / total_diff;
    if (idle_percent > 100U)
    {
        idle_percent = 100U;
    }
    return 100U - idle_percent;
}

static void get_ram_usage(size_t *used_bytes, size_t *total_bytes, uint32_t *ram_percent)
{
    size_t free_heap = xPortGetFreeHeapSize();
    *total_bytes = (size_t)configTOTAL_HEAP_SIZE;
    *used_bytes = (size_t)configTOTAL_HEAP_SIZE - free_heap;
    *ram_percent = (uint32_t)((*used_bytes * 100U) / (size_t)configTOTAL_HEAP_SIZE);
}

static void app_uart_task(void *argument)
{
    (void)argument;
    TickType_t last_sys_wake = xTaskGetTickCount();
    const TickType_t sys_period = pdMS_TO_TICKS(APP_UART_PERIOD_MS);
    configASSERT(sys_period > 0U);

    char tx_buffer[128];
    bmi160_data_msg_t sensor_msg;

    for (;;)
    {
        /* 1. Kiểm tra Queue dữ liệu cảm biến (chờ tối đa 100ms) */
        if (s_sensor_queue != NULL && xQueueReceive(s_sensor_queue, &sensor_msg, pdMS_TO_TICKS(100)) == pdPASS)
        {
            (void)snprintf(tx_buffer, sizeof(tx_buffer),
                           "[BMI160] Acc(mg): %ld,%ld,%ld | Gyr(dps): %ld,%ld,%ld | P:%d R:%d\r\n",
                           (long)sensor_msg.acc_x_mg, (long)sensor_msg.acc_y_mg, (long)sensor_msg.acc_z_mg,
                           (long)sensor_msg.gyr_x_dps, (long)sensor_msg.gyr_y_dps, (long)sensor_msg.gyr_z_dps,
                           (int)sensor_msg.pitch_deg, (int)sensor_msg.roll_deg);
            uart_service_send(tx_buffer);
        }

        /* 2. Định kỳ mỗi 1000ms gửi thông số tài nguyên hệ thống (CPU & RAM) */
        TickType_t now = xTaskGetTickCount();
        if ((TickType_t)(now - last_sys_wake) >= sys_period)
        {
            last_sys_wake = now;

            uint32_t cpu_usage = get_cpu_usage();

            size_t used_ram = 0;
            size_t total_ram = 0;
            uint32_t ram_percent = 0;
            get_ram_usage(&used_ram, &total_ram, &ram_percent);

            (void)snprintf(tx_buffer, sizeof(tx_buffer),
                           "[SYS] CPU: %lu%% | RAM: %u/%u B (%lu%%)\r\n",
                           (unsigned long)cpu_usage,
                           (unsigned int)used_ram,
                           (unsigned int)total_ram,
                           (unsigned long)ram_percent);

            uart_service_send(tx_buffer);
        }
    }
}

BaseType_t app_uart_start(void)
{
    if (s_app_task != NULL)
    {
        return pdPASS;
    }
    if (uart_service_init() != UART_SERVICE_OK)
    {
        return pdFAIL;
    }
    if (s_sensor_queue == NULL)
    {
        s_sensor_queue = xQueueCreate(SENSOR_QUEUE_LEN, sizeof(bmi160_data_msg_t));
        if (s_sensor_queue == NULL)
        {
            return pdFAIL;
        }
    }
    return xTaskCreate(app_uart_task, "UART_APP", 384U, NULL,
                       tskIDLE_PRIORITY + 1U, &s_app_task);
}
