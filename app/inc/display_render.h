#ifndef DISPLAY_RENDER_H
#define DISPLAY_RENDER_H
#include <stdbool.h>
#include "app_ipc.h"
#include "hal_st7735.h"


typedef struct {
    DisplayScene_t scene; /*Bản đồ 12×12 cùng thông số hiện tại (vị trí bi, tim, sao, cổng).*/
    DisplayUpdate_t painted;/*Thông số đã vẽ thật lên LCD lần gần nhất, dùng để biết cái gì đã đổi.*/
    uint32_t scene_id;/*Số hiệu của màn đang chơi. Message mang số hiệu khác sẽ bị bỏ qua.*/
    DisplayNotice_t notice; /*Loại thông báo đang hiện (qua màn, chạm bẫy, thắng, thua...).*/
    uint8_t star_rows[APP_DISPLAY_MAX_STARS], star_cols[APP_DISPLAY_MAX_STARS]; /*Danh sách vị trí các ngôi sao. Sao thứ i ứng với bit thứ i của collected_star_mask, nên biết bit nào đổi là biết ô nào cần vẽ lại.*/
    bool have_scene, overlay, dirty, painted_valid;
/*(cờ true/false)
have_scene: đã nhận được màn chơi nào chưa. Chưa có thì chưa có gì để vẽ.
overlay: đang hiện bảng thông báo đè lên bản đồ.
dirty: màn hình đang lệch so với scene, cần vẽ lại.
painted_valid: painted còn đáng tin. Nếu false thì không được vẽ từng phần mà phải vẽ lại toàn bộ.*/
} DisplayRender_t;


void task_display_render_reset(DisplayRender_t *r);
/*Khởi tạo hoặc đưa toàn bộ cấu trúc DisplayRender_t về trạng thái trống ban đầu*/
bool task_display_render_validate(const DisplayMsg_t *msg);
/*Kiểm tra tính hợp lệ của bản tin IPC đầu vào trước khi xử lý (ví dụ: kiểm tra phạm vi tọa độ bóng [3, 116], level từ 1..5, lives từ 0..3, số sao total_stars khớp với số ô DISPLAY_TILE_STAR trên bản đồ, và collected_stars khớp với số bit 1 trong collected_star_mask).*/
hal_st7735_status_t task_display_render_apply(DisplayRender_t *r,const DisplayMsg_t *msg);
/*: Tiếp nhận bản tin msg đã hợp lệ, cập nhật trạng thái vào r theo loại bản tin (SCENE, UPDATE, NOTICE, RESUME) và gọi các hàm driver ST7735 để vẽ lên màn hình; trả về mã lỗi hoặc thành công của giao tiếp phần cứng.*/
hal_st7735_status_t task_display_render_refresh(DisplayRender_t *r);
/*nếu có màn và đang dirty==true là đang đúng thì vẽ lại toàn bộ, ngược lại không làm gì.*/
void task_display_render_invalidate(DisplayRender_t *r);
/*Dùng khi đánh dấu "màn hình không còn đáng tin" bằng dirty = true và painted_valid = false có lỗi vẽ, khi đổi màn, hoặc khi đóng bảng thông báo*/
#endif
