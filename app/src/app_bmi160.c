/* Layer 3: Ứng dụng BMI160 - Xử lý gia tốc, vận tốc và hướng chuyển động cho Game Matrix Ball */
#include "app_bmi160.h"
#include "bmi160_service.h"
#include "app_uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include <math.h>

#define RAD_TO_DEG          57.2957795f
#define FILTER_ALPHA        0.35f       /* Hệ số lọc thông thấp Low-pass Filter (0.0 -> 1.0) */
#define BALL_FRICTION       0.92f       /* Hệ số ma sát bề mặt (làm chậm bóng khi để phẳng) */
#define BALL_ACC_SENSITIVITY 0.60f      /* Hệ số khuếch đại gia tốc thành vận tốc */

static TaskHandle_t s_bmi160_task = NULL;

/* Trạng thái vật lý tích lũy của quả bóng */
static float s_filtered_ax = 0.0f;
static float s_filtered_ay = 0.0f;
static float s_ball_vx = 0.0f;
static float s_ball_vy = 0.0f;

static int16_t s_cached_acc_x = 0;
static int16_t s_cached_acc_y = 0;
static int8_t  s_cached_dir_x = 0;
static int8_t  s_cached_dir_y = 0;

/* Xử lý vùng chết (Deadzone) chống trôi bóng do rung tay hoặc sai số cảm biến */
static float apply_deadzone(float val, float deadzone)
{
    if (val > deadzone)
    {
        return val - deadzone;
    }
    else if (val < -deadzone)
    {
        return val + deadzone;
    }
    return 0.0f;
}

/* Hàm cập nhật mô hình vật lý quả bóng */
static void update_ball_physics(float raw_ax_mg, float raw_ay_mg, bmi160_data_msg_t *out)
{
    const float dt = (float)APP_BMI160_PERIOD_MS / 1000.0f; /* 0.05s */

    /* 1. Lọc nhiễu cảm biến bằng Low-Pass Filter để chuyển động đầm, không giật */
    s_filtered_ax += FILTER_ALPHA * (raw_ax_mg - s_filtered_ax);
    s_filtered_ay += FILTER_ALPHA * (raw_ay_mg - s_filtered_ay);

    /* 2. Áp dụng Deadzone: khi board đặt gần phẳng, gia tốc bóng = 0 */
    float eff_ax = apply_deadzone(s_filtered_ax, (float)BALL_DEADZONE_MG);
    float eff_ay = apply_deadzone(s_filtered_ay, (float)BALL_DEADZONE_MG);

    out->ball_acc_x = (int16_t)eff_ax;
    out->ball_acc_y = (int16_t)eff_ay;
    s_cached_acc_x  = out->ball_acc_x;
    s_cached_acc_y  = out->ball_acc_y;

    /* 3. Tích phân gia tốc theo thời gian để tính vận tốc bóng: v = v * ma_sat + a * dt */
    s_ball_vx = (s_ball_vx * BALL_FRICTION) + (eff_ax * BALL_ACC_SENSITIVITY * dt);
    s_ball_vy = (s_ball_vy * BALL_FRICTION) + (eff_ay * BALL_ACC_SENSITIVITY * dt);

    /* Dừng hẳn bóng nếu vận tốc quá nhỏ (chống trôi vô hạn) */
    if (eff_ax == 0.0f && fabsf(s_ball_vx) < 1.0f)
    {
        s_ball_vx = 0.0f;
    }
    if (eff_ay == 0.0f && fabsf(s_ball_vy) < 1.0f)
    {
        s_ball_vy = 0.0f;
    }

    /* Giới hạn vận tốc cực đại (Clamp max velocity) */
    if (s_ball_vx > (float)BALL_MAX_VELOCITY)  s_ball_vx = (float)BALL_MAX_VELOCITY;
    if (s_ball_vx < -(float)BALL_MAX_VELOCITY) s_ball_vx = -(float)BALL_MAX_VELOCITY;
    if (s_ball_vy > (float)BALL_MAX_VELOCITY)  s_ball_vy = (float)BALL_MAX_VELOCITY;
    if (s_ball_vy < -(float)BALL_MAX_VELOCITY) s_ball_vy = -(float)BALL_MAX_VELOCITY;

    out->ball_vel_x = (int16_t)s_ball_vx;
    out->ball_vel_y = (int16_t)s_ball_vy;

    /* 4. Xác định hướng rời rạc (-1, 0, 1) cho game ma trận dạng ô lưới (LED Matrix) */
    if (s_filtered_ax > (float)BALL_TILT_THRESH_MG)
    {
        out->dir_x = 1;   /* Nghiêng phải */
    }
    else if (s_filtered_ax < -(float)BALL_TILT_THRESH_MG)
    {
        out->dir_x = -1;  /* Nghiêng trái */
    }
    else
    {
        out->dir_x = 0;   /* Cân bằng */
    }

    if (s_filtered_ay > (float)BALL_TILT_THRESH_MG)
    {
        out->dir_y = 1;   /* Nghiêng xuống / lùi */
    }
    else if (s_filtered_ay < -(float)BALL_TILT_THRESH_MG)
    {
        out->dir_y = -1;  /* Nghiêng lên / tiến */
    }
    else
    {
        out->dir_y = 0;   /* Cân bằng */
    }

    s_cached_dir_x = out->dir_x;
    s_cached_dir_y = out->dir_y;
}

/* Chuyển đổi dữ liệu thô sang đơn vị chuẩn và tính góc nghiêng */
static void process_bmi160_data(const bmi160_raw_data_t *raw, bmi160_data_msg_t *out)
{
    /* 1. Đổi gia tốc sang milli-g (mg) với dải đo ±2g (độ nhạy 16384 LSB/g) */
    out->acc_x_mg = ((int32_t)raw->acc_x * 1000) / 16384;
    out->acc_y_mg = ((int32_t)raw->acc_y * 1000) / 16384;
    out->acc_z_mg = ((int32_t)raw->acc_z * 1000) / 16384;

    /* 2. Đổi vận tốc góc sang deg/s (dps) với dải đo ±2000 dps (độ nhạy 16.4 LSB/dps) */
    out->gyr_x_dps = ((int32_t)raw->gyr_x * 10) / 164;
    out->gyr_y_dps = ((int32_t)raw->gyr_y * 10) / 164;
    out->gyr_z_dps = ((int32_t)raw->gyr_z * 10) / 164;

    /* 3. Tính độ lớn gia tốc tổng hợp */
    float ax = (float)out->acc_x_mg;
    float ay = (float)out->acc_y_mg;
    float az = (float)out->acc_z_mg;
    out->total_acc_mg = (uint32_t)sqrtf(ax * ax + ay * ay + az * az);

    /* 4. Góc nghiêng Pitch & Roll */
    float roll_rad = atan2f(ay, az);
    float pitch_rad = atan2f(-ax, sqrtf(ay * ay + az * az));
    out->roll_deg = (int16_t)(roll_rad * RAD_TO_DEG);
    out->pitch_deg = (int16_t)(pitch_rad * RAD_TO_DEG);

    out->timestamp_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

    /* 5. Cập nhật vật lý cho game Matrix Ball (Gia tốc hiệu dụng & Vận tốc bóng) */
    update_ball_physics(ax, ay, out);
}

static void app_bmi160_task(void *argument)
{
    (void)argument;
    bmi160_raw_data_t raw_data;
    bmi160_data_msg_t processed_data;

    /* Khởi tạo cảm biến trong Task (tận dụng vTaskDelay giải phóng CPU) */
    while (bmi160_service_init() != BMI160_OK)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(APP_BMI160_PERIOD_MS);

    for (;;)
    {
        if (bmi160_service_read_raw(&raw_data) == BMI160_OK)
        {
            /* Xử lý dữ liệu & mô phỏng vật lý bóng */
            process_bmi160_data(&raw_data, &processed_data);

            /* Đẩy vào Queue để UART truyền lên PC (hoặc các task khác lấy) */
            (void)app_uart_post_sensor_data(&processed_data);
        }

        (void)xTaskDelayUntil(&last_wake, period);
    }
}

BaseType_t app_bmi160_start(void)
{
    if (s_bmi160_task != NULL)
    {
        return pdPASS;
    }

    return xTaskCreate(app_bmi160_task, "BMI160_APP", 256U, NULL,
                       tskIDLE_PRIORITY + 2U, &s_bmi160_task);
}

/* --- API TRUY XUẤT CHO TASK GAME --- */

void app_bmi160_get_ball_physics(int16_t *acc_x, int16_t *acc_y, int16_t *vel_x, int16_t *vel_y)
{
    if (acc_x) *acc_x = s_cached_acc_x;
    if (acc_y) *acc_y = s_cached_acc_y;
    if (vel_x) *vel_x = (int16_t)s_ball_vx;
    if (vel_y) *vel_y = (int16_t)s_ball_vy;
}

void app_bmi160_get_ball_direction(int8_t *dir_x, int8_t *dir_y)
{
    if (dir_x) *dir_x = s_cached_dir_x;
    if (dir_y) *dir_y = s_cached_dir_y;
}

void app_bmi160_reset_ball_physics(void)
{
    s_ball_vx = 0.0f;
    s_ball_vy = 0.0f;
    s_filtered_ax = 0.0f;
    s_filtered_ay = 0.0f;
    s_cached_acc_x = 0;
    s_cached_acc_y = 0;
    s_cached_dir_x = 0;
    s_cached_dir_y = 0;
}
