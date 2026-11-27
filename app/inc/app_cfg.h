#ifndef APP_CFG_H
#define APP_CFG_H
#include "FreeRTOS.h"
/* Khai báo mới thuộc phần DISPLAY; các mục cảm biến/game do nhóm thống nhất. */
#define APP_DISPLAY_STACK_WORDS       512U                    /*Cấp phát vùng nhớ ngăn xếp (stack) cho task hiển thị là 512 words. */
#define APP_DISPLAY_PRIORITY          (tskIDLE_PRIORITY + 1U) /*Thiết lập độ ưu tiên của task hiển thị cao hơn task rảnh (tskIDLE_PRIORITY, mặc định bằng 0) 1 bậc (tức mức ưu tiên 1). */
#define APP_DISPLAY_QUEUE_LENGTH      2U                      /*Queue chứa tối đa 2 message*/
#define APP_DISPLAY_QUEUE_WAIT_MS     50U                     /*Thời gian tối đa (50 ms) task hiển thị chờ bản tin mới từ hàng đợi trước khi hết thời gian chờ (timeout) để tiếp tục vòng lặp xử lý.*/
#define APP_DISPLAY_POST_MAX_MS       100U                    /*Thời gian tối đa (100 ms) mà các task khác được phép chờ để đẩy dữ liệu vào hàng đợi hiển thị nếu hàng đợi đang đầy.*/
#define APP_DISPLAY_RETRY_MS          500U                    /*Khoảng thời gian trễ (500 ms) trước khi thử lại nếu quá trình khởi tạo hoặc cấp phát tài nguyên hiển thị gặp lỗi.*/
#define APP_DISPLAY_STARTUP_RESERVE   1536U                   /*Ngưỡng dung lượng bộ nhớ Heap tối thiểu (1536 bytes = 1.5 KB) phải giữ lại dự phòng trong FreeRTOS khi khởi động, tránh việc task hiển thị chiếm hết RAM của hệ thống.*/
#define APP_DISPLAY_MOCK_STACK_WORDS  256U
#define APP_DISPLAY_MOCK_PRIORITY     (tskIDLE_PRIORITY + 1U)
#define APP_DISPLAY_MOCK_PERIOD_MS    50U
#define APP_DISPLAY_LEVEL_COUNT       5U
#define APP_DISPLAY_MAX_STARS         16U
#define APP_DISPLAY_MAX_LIVES         3U

#define APP_DISPLAY_COLS              12U /* ban dau 12 cot*/
#define APP_DISPLAY_ROWS              12U /*12 hang*/
#define APP_DISPLAY_TILE_PX           10U /* moi o vuong co kich thuong la 10x10 pixel*/
#define APP_DISPLAY_MAP_X             4 /*ban dau bat dau o toa do (4,25)*/
#define APP_DISPLAY_MAP_Y             25
#define APP_DISPLAY_MAP_SIZE          120 /*ban dau co kich thuong 120x120pixel*/
#define APP_DISPLAY_FOOTER_Y          145
#define APP_DISPLAY_BALL_RADIUS       3U /*ban kinh qua bong la 3pixel*/
#endif
