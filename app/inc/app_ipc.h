#ifndef APP_IPC_H
#define APP_IPC_H
#include <stdint.h>
#include "app_cfg.h"

/* ĐỀ XUẤT IPC của phần màn hình - CẦN NHÓM THỐNG NHẤT trước tích hợp game.
 * Queue copy TOÀN BỘ message, không gửi con trỏ bản đồ hoặc biến dùng chung.
 * ball_x/y: tâm bi, đơn vị pixel tương đối với góc bản đồ (không phải chỉ số ô).
 * Giá trị hợp lệ [3,116]. Game chịu trách nhiệm va chạm và chuyển level.
 * scene_id tăng mỗi lần tải lại/đổi màn; UPDATE/NOTICE/RESUME phải cùng ID.
 */
/* IPC v2 cho bộ STAR MAZE; không tương thích tile=2 của mock 3 màn cũ.
 * Giữ đúng ID của ZIP: START chỉ là nền, bi được vẽ theo tọa độ ball_x/y.
 */
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
    uint8_t level; /* 1..5. */
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

/* Quy ước: SCENE rồi UPDATE; NOTICE giữ màn thông báo vô thời hạn về mặt
 * trạng thái, task vẫn chờ Queue hữu hạn. Gửi SCENE/UPDATE khi đang thông báo
 * chỉ lưu dữ liệu mới. Game gửi RESUME sau thời gian muốn hiển thị thông báo.
 * collected_star_mask: bit 0 là sao đầu tiên theo thứ tự hàng rồi cột trong
 * tiles SCENE; bit=1 nghĩa là game báo đã nhặt. Tối đa 16 sao/màn.
 * Giữ nguyên tiles gốc trong SCENE, KHÔNG đổi tile STAR thành FLOOR trong
 * UPDATE. Renderer ẩn sao bằng mask. collected_stars phải bằng popcount(mask).
 * total_stars của SCENE phải khớp số tile STAR. gate_open do GAME gửi (0/1),
 * chỉ cho phép mở khi collected_stars==total_stars. Display không tự mở cổng,
 * trừ tim, nhặt sao hay chuyển màn. lives=0..3, dùng chung 5 màn do game giữ.
 * Trúng bẫy: UPDATE tọa độ/lives, NOTICE TRAP, UPDATE về đầu màn với cùng ID
 * và cùng mask sao, RESUME. Restart/đổi màn: SCENE với ID mới.
 * Queue đầy -> producer nhận pdFAIL và tự retry hữu hạn; không bỏ sự kiện.
 */
#endif
