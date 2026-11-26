#include "app_game_ipc.h"
#include "queue.h"
#include "stm32f4xx.h"

static QueueHandle_t s_sensor_queue;
static QueueHandle_t s_command_queue;

BaseType_t app_game_ipc_init(void)
{
    if (__get_IPSR() != 0U || xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        return pdFAIL;
    }
    if (s_sensor_queue != NULL && s_command_queue != NULL) { return pdPASS; }
    s_sensor_queue = xQueueCreate(1U, sizeof(GameSensorMsg_t));
    s_command_queue = xQueueCreate(4U, sizeof(GameCommand_t));
    if (s_sensor_queue != NULL && s_command_queue != NULL) { return pdPASS; }
    if (s_sensor_queue != NULL) { vQueueDelete(s_sensor_queue); s_sensor_queue = NULL; }
    if (s_command_queue != NULL) { vQueueDelete(s_command_queue); s_command_queue = NULL; }
    return pdFAIL;
}

BaseType_t app_game_sensor_post(const GameSensorMsg_t *sample)
{
    if (__get_IPSR() != 0U || sample == NULL || s_sensor_queue == NULL) { return pdFAIL; }
    return xQueueOverwrite(s_sensor_queue, sample);
}
BaseType_t app_game_sensor_latest(GameSensorMsg_t *sample)
{
    if (__get_IPSR() != 0U || sample == NULL || s_sensor_queue == NULL) { return pdFAIL; }
    return xQueuePeek(s_sensor_queue, sample, 0U);
}
BaseType_t app_game_command_post(GameCommand_t command)
{
    if (__get_IPSR() != 0U || s_command_queue == NULL ||
        (command != GAME_COMMAND_RESTART && command != GAME_COMMAND_NEXT_LEVEL)) { return pdFAIL; }
    return xQueueSend(s_command_queue, &command, 0U);
}
BaseType_t app_game_take_command(GameCommand_t *command)
{
    if (__get_IPSR() != 0U || s_command_queue == NULL || command == NULL) { return pdFAIL; }
    return xQueueReceive(s_command_queue, command, 0U);
}
