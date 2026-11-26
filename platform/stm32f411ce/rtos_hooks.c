#include "FreeRTOS.h"
#include "task.h"

/* Inspect these variables in GDB after an assert. */
const char * volatile g_assert_file;
volatile unsigned long g_assert_line;

void vAssertCalled(const char *file, unsigned long line)
{
    taskDISABLE_INTERRUPTS();
    g_assert_file = file;
    g_assert_line = line;
    for (;;) { }
}

void vApplicationMallocFailedHook(void)
{
    vAssertCalled("FreeRTOS heap allocation failed", 0UL);
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    vAssertCalled("FreeRTOS task stack overflow", 0UL);
}

