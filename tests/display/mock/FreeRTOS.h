#ifndef TEST_FREERTOS_H
#define TEST_FREERTOS_H
#include <stdint.h>
#include <stddef.h>
typedef int32_t BaseType_t;
typedef uint32_t UBaseType_t;
typedef uint32_t TickType_t;
typedef uint32_t StackType_t;
typedef struct { uint8_t dummy[104]; } StaticTask_t;
typedef struct { uint8_t dummy[80]; } StaticQueue_t;
typedef struct { size_t xAvailableHeapSpaceInBytes,xNumberOfSuccessfulAllocations; } HeapStats_t;
void vPortGetHeapStats(HeapStats_t *out);
void *pvPortMalloc(size_t size);
void vPortFree(void *block);
#define pdPASS 1
#define pdFAIL 0
#define pdTRUE 1
#define pdFALSE 0
#define portMAX_DELAY UINT32_MAX
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define tskIDLE_PRIORITY 0U
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 2U
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
#define portYIELD_FROM_ISR(woken) ((void)(woken))
size_t xPortGetFreeHeapSize(void);
#endif
