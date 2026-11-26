/* Local integration harness only. The feature package does not edit group main.c. */
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f4xx.h"
#include "game_config.h"
#include "app_game_ipc.h"
#include "task_display.h"
#include "task_sensor_game.h"
#include "task_game.h"
#include "task_button.h"

_Static_assert(pdMS_TO_TICKS(GAME_STEP_MS) > 0U, "Game period below tick");
_Static_assert(pdMS_TO_TICKS(APP_BUTTON_PERIOD_MS) > 0U, "Button period below tick");

volatile size_t g_game_heap_before_scheduler;

int main(void)
{
    SystemCoreClockUpdate();
    NVIC_SetPriorityGrouping(3U);
    /* Firmware is built with BSP_DISPLAY_USE_DMA=0. No external IRQ is enabled
     * by the game, button or sensor tasks. FreeRTOS owns SysTick/PendSV/SVC. */
    configASSERT(task_display_create() == pdPASS);
    configASSERT(app_game_ipc_init() == pdPASS);
    configASSERT(task_sensor_game_create() == pdPASS);
    configASSERT(task_game_create() == pdPASS);
    configASSERT(task_button_create() == pdPASS);
    g_game_heap_before_scheduler = xPortGetFreeHeapSize();
    /* Idle+timer stacks/TCBs, timer command queue, BMI mutex, and a margin.
     * The true runtime heap and stack watermarks still require board testing. */
    const size_t reserve = (configMINIMAL_STACK_SIZE + configTIMER_TASK_STACK_DEPTH) *
        sizeof(StackType_t) + 2U * sizeof(StaticTask_t) + 2U * sizeof(StaticQueue_t) +
        configTIMER_QUEUE_LENGTH * 16U + 256U;
    configASSERT(g_game_heap_before_scheduler >= reserve);
    vTaskStartScheduler();
    configASSERT(pdFALSE);
    for (;;) { }
}
