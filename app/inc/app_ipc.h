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
/*Gói dữ liệu cập nhật trạng thái tức thời (kích thước gọn nhẹ 8 bytes sau căn lề):
*uint16_t collected_star_mask: Bitmask cho biết những ngôi sao nào đã được thu thập
*ball_x, ball_y: tọa độ của bóng
*lives: số mạng còn lại
*collected_stars:Tổng số sao đã ăn được trong màn (phải khớp với tổng số bit 1 trong collected_star_mask).
*gate_open;: Trạng thái cổng đích (0 = đóng, 1 = mở).*/

typedef struct {
    DisplayUpdate_t hud;
    uint8_t level; /* 1..5. */
    uint8_t total_stars;
    uint8_t tiles[APP_DISPLAY_ROWS][APP_DISPLAY_COLS];
} DisplayScene_t;
/*Cấu trúc này chứa toàn bộ thông tin của một màn chơi:
*HUD: vị trí nhân vật, HP, số sao, trạng thái cổng
*Level hiện tại.
*Tổng số sao.
*bản đồ 12x12*/

typedef struct {
    DisplayMsgType_t type;
    uint32_t scene_id;
    union {
        DisplayScene_t scene;
        DisplayUpdate_t update;
        DisplayNotice_t notice;
    } data;
} DisplayMsg_t;
/*Đây là cấu trúc message hoàn chỉnh được đưa vào Queue.
*DisplayMsgType_t type;: Trường nhãn xác định loại bản tin đang gửi
*uint32_t scene_id;: Mã định danh phiên của màn chơi
*Vùng nhớ dùng chung (union) giúp tiết kiệm RAM bằng cách chỉ cấp phát kích thước bằng thành phần lớn nhất (DisplayScene_t)
chứa một trong ba dữ liệu tùy theo type: scene (khi nạp màn), update (khi cập nhật khung hình),
hoặc notice (khi hiện thông báo; riêng DISPLAY_MSG_RESUME không cần dữ liệu kèm theo).*/

#endif
