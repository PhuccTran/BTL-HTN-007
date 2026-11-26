#ifndef GAME_CORE_H
#define GAME_CORE_H
#include <stdbool.h>
#include <stdint.h>
#include "game_config.h"
#include "star_maze_levels.h"

typedef enum {
    GAME_PLAYING = 0, GAME_TRAP_NOTICE, GAME_LEVEL_NOTICE,
    GAME_RESTART_NOTICE, GAME_OVER, GAME_WON, GAME_SPLASH
} GameState_t;

/* One owner: task_game. Position and velocity use Q8 pixel units. */
typedef struct {
    GameState_t state;
    uint32_t scene_id, state_since_ms;
    int32_t x_q, y_q, vx_q, vy_q, filtered_ax, filtered_ay;
    uint16_t collected_mask;
    uint8_t level, lives, collected, total_stars;
    uint8_t start_col, start_row;
} Game_t;

void game_core_init(Game_t *game, uint32_t now_ms);
/* New run: LEVEL 1 title -> play, with three hearts. Splash is boot/reset only. */
void game_core_restart(Game_t *game, uint32_t now_ms);
/* Manual selection bypasses the gate/current notice, then shows the next level
 * title for 1 s. Keeps lives; after GAME OVER starts a fresh three-life run. */
void game_core_next_level(Game_t *game, uint32_t now_ms);
void game_core_stop(Game_t *game);
void game_core_step(Game_t *game, int16_t ax_mg, int16_t ay_mg, uint32_t now_ms);
bool game_core_gate_open(const Game_t *game);
uint8_t game_core_ball_x(const Game_t *game);
uint8_t game_core_ball_y(const Game_t *game);
const uint8_t (*game_core_map(const Game_t *game))[STAR_MAZE_COLS];
#endif
