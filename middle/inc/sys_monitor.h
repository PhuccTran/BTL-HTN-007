#ifndef SYS_MONITOR_H
#define SYS_MONITOR_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t total_bytes;
    size_t used_bytes;
    size_t free_bytes;
    size_t min_free_bytes;
    uint8_t used_percent;
} SysRamUsage_t;

/* Layer 2 (Middle): Tính toán tải CPU (%) và mức sử dụng RAM (FreeRTOS heap) */
uint8_t sys_monitor_calc_cpu_load(void);
void sys_monitor_calc_ram_usage(SysRamUsage_t *out);

#endif /* SYS_MONITOR_H */
