#ifndef TEST_QUEUE_H
#define TEST_QUEUE_H
#include "FreeRTOS.h"
typedef void *QueueHandle_t;
QueueHandle_t xQueueCreate(UBaseType_t n,UBaseType_t item);
BaseType_t xQueueSend(QueueHandle_t q,const void *item,TickType_t timeout);
BaseType_t xQueueReceive(QueueHandle_t q,void *item,TickType_t timeout);
void vQueueDelete(QueueHandle_t q);
#endif
