#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>
extern uint32_t SystemCoreClock;
void vAssertCalled(const char *file, unsigned long line);

/* platform/stm32f411ce/system.c configures HSI + PLL = 96 MHz. */
#define configCPU_CLOCK_HZ                       (SystemCoreClock)
#define configTICK_RATE_HZ                       1000U
#define configUSE_PREEMPTION                     1
#define configUSE_TIME_SLICING                   1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION  1
#define configMAX_PRIORITIES                     5
#define configMINIMAL_STACK_SIZE                 128U
#define configMAX_TASK_NAME_LEN                  16
#define configTICK_TYPE_WIDTH_IN_BITS            TICK_TYPE_WIDTH_32_BITS
#define configIDLE_SHOULD_YIELD                  1
#define configUSE_TASK_NOTIFICATIONS             1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES    1

#define configSUPPORT_STATIC_ALLOCATION          0
#define configSUPPORT_DYNAMIC_ALLOCATION         1
#define configTOTAL_HEAP_SIZE                    (8U * 1024U)
#define configAPPLICATION_ALLOCATED_HEAP         0
#define configUSE_MALLOC_FAILED_HOOK             1
#define configCHECK_FOR_STACK_OVERFLOW           2
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0

#define configUSE_MUTEXES                        1
#define configUSE_RECURSIVE_MUTEXES              1
#define configUSE_COUNTING_SEMAPHORES            1
#define configUSE_QUEUE_SETS                     0
#define configQUEUE_REGISTRY_SIZE                8
#define configUSE_TIMERS                         1
#define configTIMER_TASK_PRIORITY                2
#define configTIMER_QUEUE_LENGTH                 10
#define configTIMER_TASK_STACK_DEPTH             128U
#define configUSE_CO_ROUTINES                    0
#define configUSE_TRACE_FACILITY                 1
#define configUSE_STATS_FORMATTING_FUNCTIONS     0
#define configGENERATE_RUN_TIME_STATS            1

#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() do { \
    (*(volatile uint32_t *)0xE000EDFCUL) |= (1UL << 24); /* CoreDebug DEMCR |= TRCENA */ \
    (*(volatile uint32_t *)0xE0001004UL) = 0UL;          /* DWT CYCCNT = 0 */ \
    (*(volatile uint32_t *)0xE0001000UL) |= 1UL;          /* DWT CTRL |= CYCCNTENA */ \
} while (0)
#define portGET_RUN_TIME_COUNTER_VALUE()         (*(volatile uint32_t *)0xE0001004UL)

/* STM32F411 implements four NVIC priority bits. BSP selects PRIGROUP=3. */
#define configPRIO_BITS                          4U
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY  15U
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 2U
#define configKERNEL_INTERRUPT_PRIORITY          (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY     (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))
#define configASSERT(condition) do { if (!(condition)) { vAssertCalled(__FILE__, __LINE__); } } while (0)

/* Override the weak vector handlers supplied by ST's startup file. */
#define vPortSVCHandler                          SVC_Handler
#define xPortPendSVHandler                       PendSV_Handler
#define xPortSysTickHandler                      SysTick_Handler

#define INCLUDE_vTaskDelay                       1
#define INCLUDE_xTaskDelayUntil                  1
#define INCLUDE_vTaskDelete                      1
#define INCLUDE_vTaskSuspend                     1
#define INCLUDE_uxTaskPriorityGet                1
#define INCLUDE_vTaskPrioritySet                 1
#define INCLUDE_xTaskGetSchedulerState           1
#define INCLUDE_uxTaskGetStackHighWaterMark       1

#endif
