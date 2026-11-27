#ifndef APP_IPC_H
#define APP_IPC_H
#include <stdint.h>
#include "app_cfg.h"


#define DISPLAY_IPC_VERSION 2U
typedef enum {
    DISPLAY_TILE_FLOOR=0, DISPLAY_TILE_WALL=1, DISPLAY_TILE_START=2,
    DISPLAY_TILE_GATE=3, DISPLAY_TILE_STAR=4, DISPLAY_TILE_TRAP=5
} DisplayTile_t;
typedef enum { DISPLAY_MSG_SCENE=0, DISPLAY_MSG_UPDATE, DISPLAY_MSG_NOTICE, DISPLAY_MSG_RESUME } DisplayMsgType_t;
typedef enum {
    DISPLAY_NOTICE_COMPLETE=0, DISPLAY_NOTICE_RESTART, DISPLAY_NOTICE_TRAP,
    DISPLAY_NOTICE_YOU_WIN, DISPLAY_NOTICE_GAME_OVER
} DisplayNotice_t;
typedef struct {
    uint16_t collected_star_mask;
    uint8_t ball_x, ball_y, lives, collected_stars, gate_open;
} DisplayUpdate_t;
typedef struct {
    DisplayUpdate_t hud;
    uint8_t level;
    uint8_t total_stars;
    uint8_t tiles[APP_DISPLAY_ROWS][APP_DISPLAY_COLS];
} DisplayScene_t;
typedef struct {
    DisplayMsgType_t type;
    uint32_t scene_id;
    union {
        DisplayScene_t scene;
        DisplayUpdate_t update;
        DisplayNotice_t notice;
    } data;
} DisplayMsg_t;

#endif
