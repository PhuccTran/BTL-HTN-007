#ifndef GAME_DISPLAY_H
#define GAME_DISPLAY_H
#include "app_ipc.h"
#include "game_core.h"

/* Fixed-size transaction: a failed send keeps the unsent message for retry. */
typedef struct {
    DisplayMsg_t pending[3];
    uint32_t scene_id;
    GameState_t state;
    uint8_t count, next;
    bool valid, has_notice;
} GamePresent_t;
void game_present_init(GamePresent_t *present);
void game_present_prepare(GamePresent_t *present, const Game_t *game);
const DisplayMsg_t *game_present_peek(const GamePresent_t *present);
void game_present_accept(GamePresent_t *present);
#endif
