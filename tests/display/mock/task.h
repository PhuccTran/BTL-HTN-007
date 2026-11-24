#ifndef TEST_TASK_H
#define TEST_TASK_H
#include "FreeRTOS.h"
typedef void *TaskHandle_t;
#define taskSCHEDULER_NOT_STARTED 0
#define taskSCHEDULER_RUNNING 2
BaseType_t xTaskGetSchedulerState(void);
TickType_t xTaskGetTickCount(void);
void vTaskDelay(TickType_t ticks);
BaseType_t xTaskDelayUntil(TickType_t *wake,TickType_t period);
void vTaskDelete(TaskHandle_t task);
BaseType_t xTaskCreate(void (*fn)(void *),const char *name,uint32_t stack,void *arg,UBaseType_t priority,TaskHandle_t *handle);
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t task);
#endif
