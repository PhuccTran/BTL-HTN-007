#ifndef APP_GAME_IPC_H
#define APP_GAME_IPC_H
#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>

typedef struct { int16_t ax_mg, ay_mg; TickType_t sampled_at; uint8_t valid; } GameSensorMsg_t;
typedef enum { GAME_COMMAND_NEXT_LEVEL = 1, GAME_COMMAND_RESTART } GameCommand_t;
/* Startup, before scheduler. A separate sensor mailbox prevents stealing UART data. */
BaseType_t app_game_ipc_init(void);
BaseType_t app_game_sensor_post(const GameSensorMsg_t *sample);
BaseType_t app_game_sensor_latest(GameSensorMsg_t *sample);
BaseType_t app_game_command_post(GameCommand_t command);
BaseType_t app_game_take_command(GameCommand_t *command);
#endif
