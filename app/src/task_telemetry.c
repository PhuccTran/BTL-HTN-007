/* Layer 3 (App): Đọc tải CPU & RAM usage từ Middle và gửi qua UART1 */
#include "task_telemetry.h"
#include "task.h"
#include "game_config.h"
#include "sys_monitor.h"
#include "hal_uart.h"
#include "stm32f4xx.h"
#include <stdio.h>

static TaskHandle_t s_telemetry_task;

static void prvTelemetryTask(void *argument)
{
    (void)argument;
    (void)hal_uart_init(APP_UART_BAUDRATE);
    hal_uart_write("\r\n[SYS] UART1 Telemetry started\r\n");

    TickType_t wake = xTaskGetTickCount();
    char buf[96];

    for (;;) {
        uint8_t cpu_load = sys_monitor_calc_cpu_load();
        SysRamUsage_t ram;
        sys_monitor_calc_ram_usage(&ram);

        (void)snprintf(buf, sizeof(buf),
                       "CPU: %u%% | RAM: %u/%u B (%u%%, free: %u, min: %u)\r\n",
                       (unsigned)cpu_load,
                       (unsigned)ram.used_bytes,
                       (unsigned)ram.total_bytes,
                       (unsigned)ram.used_percent,
                       (unsigned)ram.free_bytes,
                       (unsigned)ram.min_free_bytes);
        hal_uart_write(buf);

        (void)xTaskDelayUntil(&wake, pdMS_TO_TICKS(APP_TELEMETRY_PERIOD_MS)); // delay 1s
    }
}

BaseType_t task_telemetry_create(void)
{
    if (__get_IPSR() != 0U || xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        return pdFAIL;
    }
    if (s_telemetry_task != NULL) { return pdPASS; }
    return xTaskCreate(prvTelemetryTask, "TELEMETRY", APP_TELEMETRY_STACK_WORDS, NULL,
                       APP_TELEMETRY_PRIORITY, &s_telemetry_task);
} // ham tao task
