#ifndef DISPLAY_RENDER_H
#define DISPLAY_RENDER_H
#include <stdbool.h>
#include "app_ipc.h"
#include "hal_st7735.h"


typedef struct {
    DisplayScene_t scene;
    DisplayUpdate_t painted;
    uint32_t scene_id;
    DisplayNotice_t notice;
    uint8_t star_rows[APP_DISPLAY_MAX_STARS], star_cols[APP_DISPLAY_MAX_STARS];
    bool have_scene, overlay, dirty, painted_valid;
} DisplayRender_t;

/*Đóng gói toàn bộ bộ nhớ đệm (cache) và cờ trạng thái của màn hình nhằm tránh phải vẽ lại toàn bộ 128×160 pixels mỗi khung hình 
(giúp giảm tải bus SPI và chống nháy hình):
*DisplayScene_t scene;: Lưu trữ bản sao đầy đủ của màn chơi hiện tại, bao gồm ma trận bản đồ tiles[12][12],
số màn level, tổng số sao total_stars và trạng thái động mới nhất nằm trong scene.hud.
*DisplayUpdate_t painted;: Lưu trữ trạng thái động của khung hình trước đó đã được vẽ thực tế lên LCD.
*Khi có bản tin UPDATE mới, bộ vẽ so sánh scene.hud với painted để chỉ xóa vị trí bóng cũ, vẽ bóng mới, hoặc ẩn ngôi sao vừa ăn thay vì vẽ lại cả mê cung.
uint32_t scene_id;: Lưu mã định danh phiên của màn chơi đang hiển thị để đối chiếu tính hợp lệ với các gói UPDATE, NOTICE, RESUME gửi đến sau đó.
DisplayNotice_t notice;: Lưu loại thông báo đang hiển thị(ví dụ: DISPLAY_NOTICE_TRAP, DISPLAY_NOTICE_YOU_WIN, DISPLAY_NOTICE_GAME_OVER) khi màn hình ở chế độ thông báo.
*: Hai mảng tra cứu nhanh lưu tọa độ hàng và cột (0..11) của tối đa 16 ngôi sao trong màn theo thứ tự duyệt từ trên xuống dưới, trái sang phải. 
Chỉ số mảng 0..15 tương ứng trực tiếp với từng bit của collected_star_mask,
giúp định vị ngay ô cần xóa sao mà không phải quét lại ma trận 12×12.
*bool have_scene;: Cờ báo hiệu bộ vẽ đã nhận được ít nhất một gói DISPLAY_MSG_SCENE hợp lệ hay chưa 
(nếu false, các gói UPDATE sẽ bị bỏ qua).
*bool overlay;: Cờ báo hiệu màn hình thông báo (NOTICE) đang phủ lên giao diện chơi game (true = đang hiện thông báo, tạm dừng vẽ cập nhật bản đồ).
*bool dirty;: Cờ đánh dấu dữ liệu nội bộ đã thay đổi nhưng chưa được đẩy xuống phần cứng LCD (cần vẽ lại ở chu kỳ refresh tiếp theo).
*bool painted_valid;: Cờ xác nhận dữ liệu trong biến painted có khớp với hình ảnh đang hiển thị trên LCD hay không
(false khi mới nạp màn hoặc sau khi màn hình bị reset, buộc bộ vẽ phải vẽ lại toàn bộ khung hình từ đầu).
*/


void task_display_render_reset(DisplayRender_t *r);
/*Khởi tạo hoặc đưa toàn bộ cấu trúc DisplayRender_t về trạng thái trống ban đầu*/
bool task_display_render_validate(const DisplayMsg_t *msg);
/*Kiểm tra tính hợp lệ của bản tin IPC đầu vào trước khi xử lý (ví dụ: kiểm tra phạm vi tọa độ bóng [3, 116], level từ 1..5, lives từ 0..3, số sao total_stars khớp với số ô DISPLAY_TILE_STAR trên bản đồ, và collected_stars khớp với số bit 1 trong collected_star_mask).*/
hal_st7735_status_t task_display_render_apply(DisplayRender_t *r,const DisplayMsg_t *msg);
/*: Tiếp nhận bản tin msg đã hợp lệ, cập nhật trạng thái vào r theo loại bản tin (SCENE, UPDATE, NOTICE, RESUME) và gọi các hàm driver ST7735 để vẽ lên màn hình; trả về mã lỗi hoặc thành công của giao tiếp phần cứng.*/
hal_st7735_status_t task_display_render_refresh(DisplayRender_t *r);
/*Đồng bộ lại những thay đổi đang chờ (dirty == true) từ bộ nhớ đệm r ra màn hình vật lý (thường dùng khi lần vẽ trước gặp lỗi SPI hoặc khi cần làm mới khung hình định kỳ).*/
void task_display_render_invalidate(DisplayRender_t *r);
/*Dùng khi có lỗi vẽ, khi đổi màn, hoặc khi đóng bảng thông báo*/
#endif
