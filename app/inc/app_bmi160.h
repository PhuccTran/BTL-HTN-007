#ifndef APP_BMI160_H
#define APP_BMI160_H

#include <stdint.h>
#include "FreeRTOS.h"

/* Layer 3: App Logic - Đọc cảm biến BMI160, tính toán vật lý bóng (Gia tốc & Vận tốc) cho Game Matrix Ball */

/* Chu kỳ cập nhật vật lý bóng: 50 ms (20 FPS / 20 Hz) giúp bóng lăn mượt mà */
#define APP_BMI160_PERIOD_MS    50UL

/* Cấu hình vật lý cho quả bóng trong Matrix Ball */
#define BALL_DEADZONE_MG        40      /* Vùng chết chống trôi khi board đặt phẳng (±40 mg ≈ 2.3°) */
#define BALL_TILT_THRESH_MG     120     /* Ngưỡng nghiêng để kích hoạt bước nhảy ô lưới (Discrete direction) */
#define BALL_MAX_VELOCITY       600     /* Giới hạn vận tốc tối đa của bóng (đơn vị: đơn vị game/giây) */

/* Cấu trúc dữ liệu cảm biến & trạng thái vật lý bóng */
typedef struct
{
    /* --- DỮ LIỆU CẢM BIẾN TIÊU CHUẨN --- */
    int32_t acc_x_mg;           /* Gia tốc thô đo được (milli-g, ±2g) */
    int32_t acc_y_mg;
    int32_t acc_z_mg;

    int32_t gyr_x_dps;          /* Vận tốc góc (deg/s, ±2000 dps) */
    int32_t gyr_y_dps;
    int32_t gyr_z_dps;

    uint32_t total_acc_mg;      /* Độ lớn gia tốc tổng hợp: sqrt(ax² + ay² + az²) */
    int16_t pitch_deg;          /* Góc nghiêng dọc (độ °) */
    int16_t roll_deg;           /* Góc nghiêng ngang (độ °) */

    /* --- DỮ LIỆU DÀNH RIÊNG CHO GAME MATRIX BALL --- */
    /* 1. Gia tốc bóng trên mặt phẳng 2D sau khi lọc mượt (LPF) và loại bỏ vùng chết (Deadzone) */
    int16_t ball_acc_x;
    int16_t ball_acc_y;

    /* 2. Vận tốc bóng được tích phân theo thời gian và chịu lực ma sát bề mặt (Damping/Friction) */
    int16_t ball_vel_x;
    int16_t ball_vel_y;

    /* 3. Hướng dịch chuyển rời rạc (-1, 0, 1) cho game ma trận dạng ô lưới (LED Matrix 8x8, 16x16) */
    int8_t dir_x;               /* -1: Nghiêng trái, 0: Thăng bằng, 1: Nghiêng phải */
    int8_t dir_y;               /* -1: Nghiêng lên (tiến), 0: Thăng bằng, 1: Nghiêng xuống (lùi) */

    uint32_t timestamp_ms;      /* Uptime ghi nhận mẫu */
} bmi160_data_msg_t;

/* Khởi tạo task đọc BMI160 và tính toán vật lý game */
BaseType_t app_bmi160_start(void);

/* --- CÁC HÀM TIỆN ÍCH DÀNH CHO TASK GAME TRUY XUẤT TRỰC TIẾP --- */

/* Lấy trực tiếp gia tốc và vận tốc bóng hiện tại */
void app_bmi160_get_ball_physics(int16_t *acc_x, int16_t *acc_y, int16_t *vel_x, int16_t *vel_y);

/* Lấy hướng nghiêng rời rạc cho game dạng nhảy ô */
void app_bmi160_get_ball_direction(int8_t *dir_x, int8_t *dir_y);

/* Reset vận tốc bóng về 0 (ví dụ: bóng va chạm tường, qua màn, reset game) */
void app_bmi160_reset_ball_physics(void);

#endif /* APP_BMI160_H */
