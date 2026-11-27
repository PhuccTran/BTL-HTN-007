#ifndef APP_CFG_H
#define APP_CFG_H
#include "FreeRTOS.h"
/* Khai báo mới thuộc phần DISPLAY; các mục cảm biến/game do nhóm thống nhất. */
#define APP_DISPLAY_STACK_WORDS       512U                    //Cấp phát vùng nhớ ngăn xếp (stack) cho task hiển thị là 512 words.
#define APP_DISPLAY_PRIORITY          (tskIDLE_PRIORITY + 1U)
#define APP_DISPLAY_QUEUE_LENGTH      2U
#define APP_DISPLAY_QUEUE_WAIT_MS     50U
#define APP_DISPLAY_POST_MAX_MS       100U
#define APP_DISPLAY_RETRY_MS          500U
#define APP_DISPLAY_STARTUP_RESERVE   1536U
#define APP_DISPLAY_MOCK_STACK_WORDS  256U
#define APP_DISPLAY_MOCK_PRIORITY     (tskIDLE_PRIORITY + 1U)
#define APP_DISPLAY_MOCK_PERIOD_MS    50U
#define APP_DISPLAY_LEVEL_COUNT       5U
#define APP_DISPLAY_MAX_STARS         16U
#define APP_DISPLAY_MAX_LIVES         3U

#define APP_DISPLAY_COLS              12U
#define APP_DISPLAY_ROWS              12U
#define APP_DISPLAY_TILE_PX           10U
#define APP_DISPLAY_MAP_X             4
#define APP_DISPLAY_MAP_Y             25
#define APP_DISPLAY_MAP_SIZE          120
#define APP_DISPLAY_FOOTER_Y          145
#define APP_DISPLAY_BALL_RADIUS       3U
#endif
