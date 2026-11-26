/* Layer 2 (Middle): Tính toán tải CPU và mức sử dụng RAM */
#include "sys_monitor.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

uint8_t sys_monitor_calc_cpu_load(void)
{
    static uint32_t s_prev_total = 0U;
    static uint32_t s_prev_idle = 0U;
    static uint8_t s_has_prev = 0U;

    uint32_t total_now = portGET_RUN_TIME_COUNTER_VALUE();
    uint32_t idle_now = ulTaskGetIdleRunTimeCounter();

    if (s_has_prev == 0U) {
        s_prev_total = total_now;
        s_prev_idle = idle_now;
        s_has_prev = 1U;
        return 0U;
    }

    uint32_t delta_total = total_now - s_prev_total;
    uint32_t delta_idle = idle_now - s_prev_idle;
    s_prev_total = total_now;
    s_prev_idle = idle_now;

    if (delta_total == 0U || delta_idle >= delta_total) {
        return 0U;
    }

    uint64_t busy = (uint64_t)(delta_total - delta_idle);
    uint8_t load = (uint8_t)((busy * 100ULL) / (uint64_t)delta_total);
    return load > 100U ? 100U : load;
}

void sys_monitor_calc_ram_usage(SysRamUsage_t *out)
{
    if (out == NULL) { return; }

    size_t total = configTOTAL_HEAP_SIZE;
    size_t free_now = xPortGetFreeHeapSize();
    size_t min_free = xPortGetMinimumEverFreeHeapSize();
    size_t used = (free_now <= total) ? (total - free_now) : 0U;

    out->total_bytes = total;
    out->used_bytes = used;
    out->free_bytes = free_now;
    out->min_free_bytes = min_free;
    out->used_percent = (total > 0U) ? (uint8_t)((used * 100U) / total) : 0U;
}
