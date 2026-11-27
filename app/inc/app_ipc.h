#ifndef APP_IPC_H
#define APP_IPC_H
#include <stdint.h>
#include "app_cfg.h"

#define DISPLAY_IPC_VERSION 2U

typedef enum {
    DISPLAY_TILE_FLOOR=0, DISPLAY_TILE_WALL=1, DISPLAY_TILE_START=2,
    DISPLAY_TILE_GATE=3, DISPLAY_TILE_STAR=4, DISPLAY_TILE_TRAP=5
} DisplayTile_t;
/*Định danh 6 loại ô địa hình trên lưới bản đồ 12×12:
*floor=0: đường đi trống
*wall=1: tường chắn k thể đi qua
*start=2: ô vị trí xuất phát ban đầu
*gate=3: cổng đích qua màn
*star=4: số sao cần thu thập
*trap=5: bẫy*/

typedef enum { DISPLAY_MSG_SCENE=0, DISPLAY_MSG_UPDATE, DISPLAY_MSG_NOTICE, DISPLAY_MSG_RESUME } DisplayMsgType_t;
/*phân loại 4 bản tin được gửi tới task hiển thị:
*scence: gui ban do va trang thai ban dau
*update: cap nhat vi tri bong,thanh hp, so sao da an
*notice: yeu cau hien thi thong bao: qua man, thang thua, trung bay
*resume: tat thong bao dang hien thi va quay lai man choi*/

typedef enum {
    DISPLAY_NOTICE_COMPLETE=0, DISPLAY_NOTICE_RESTART, DISPLAY_NOTICE_TRAP,
    DISPLAY_NOTICE_YOU_WIN, DISPLAY_NOTICE_GAME_OVER
} DisplayNotice_t;
/*Định danh 5 loại nội dung thông báo đi kèm bản tin DISPLAY_MSG_NOTICE:
*DISPLAY_NOTICE_COMPLETE = 0: Hoàn thành màn chơi hiện tại.
*DISPLAY_NOTICE_RESTART: Khởi động lại màn chơi
*DISPLAY_NOTICE_TRAP: Người chơi vừa chạm bẫy
*DISPLAY_NOTICE_YOU_WIN: Chiến thắng toàn bộ trò chơi (qua hết 5 màn)
*DISPLAY_NOTICE_GAME_OVER: Hết mạng (lives == 0), thua cuộc*/

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
