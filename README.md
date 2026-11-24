# BTL HTN 007 Phần màn hình STM32F411CE

Nhánh `feature/task-display` chứa driver ST7735, renderer, task Display và bộ test độc lập của Trần Thiện Phúc. Đây là phần màn hình trước tích hợp hệ thống; chưa nối task BMI160 hoặc logic game thật.

## Phạm vi và cấu trúc

- `bsp/bsp_display.h`: chân SPI/GPIO/DMA và cấu hình panel.
- `hal/inc`, `hal/src`: driver GPIO, SPI1 DMA/polling và ST7735.
- `app/inc`, `app/src`: cấu hình task, hợp đồng IPC v2, renderer và 5 bản đồ STAR MAZE.
- `tests/display`: entry point test riêng, mock và kiểm thử mô hình ARM.
- `middlewares`, `platform/stm32f411ce`: CMSIS, FreeRTOS, port Cortex M4F, heap_4, startup và linker để build độc lập; giữ license của thư viện.

Màn dọc 128×160, đen trắng; lưới 12×12 ô, mỗi ô 10×10 pixel tại (4,25). Header LEVEL/STAR cao 25 pixel; footer HP/tim cao 15 pixel. Display nhận SCENE, UPDATE, NOTICE, RESUME; game chịu trách nhiệm chuyển động, va chạm, sao, tim, mở cổng và thắng/thua.

## Build và nạp trên Linux

Cần `arm-none-eabi-gcc`, Newlib, Make và Python 3. Nạp kit cần `stlink-tools`; debug dùng `st-util` và `gdb-multiarch`.

Chạy tại gốc repository:

```bash
python3 tests/display/build_kit.py basic
st-flash --reset write build/display-basic/display-basic.bin 0x08000000
```

Mock 5 màn:

```bash
python3 tests/display/build_kit.py mock
st-flash --reset write build/display-mock/display-mock.bin 0x08000000
```

`make` mặc định build mock. `python3 tests/display/build_kit.py basic --polling` build bài Basic dùng polling. Build không tự nạp kit. Dừng GDB và st-util trước khi nạp lại.

Dây TFT: GND→GND, VCC→3V3 nếu module phù hợp, SCK→PA5, MOSI→PA7, CS→PA4, DC→PB0, RST→PB1. BL cấp qua mạch phù hợp module; mặc định không điều khiển bằng GPIO. ST-Link nối SWDIO→PA13, SWCLK→PA14 và GND chung; SWCLK khác chân SPI SCK PA5.

## Kiểm thử và giới hạn

Bài Basic đã được xác nhận hiển thị trên kit. Mock 5 màn cần xác nhận lại trên kit; các test mô hình không chứng minh timing điện hoặc stack runtime.

Sau khi build mock, có thể chạy các kiểm thử ARM với Python đã cài package `unicorn`:

```bash
python3 tests/display/run_unit.py
python3 tests/display/run_spi_unit.py
python3 tests/display/run_mock_unit.py
python3 tests/display/check_startup.py build/display-mock/display-mock.elf
```

Các script sinh kết quả vào `build/`. `generate_star_mock.py` đối chiếu bản đồ C với TXT/JSON gốc và sinh đường test. Khi đổi bản đồ cần cập nhật fixture và dữ liệu test tương ứng. Không đưa mock hoặc `tests/display/kit_main.c` vào firmware nhóm.

## Tích hợp và quy trình nhánh

Nhánh tính năng được tạo từ `develop`; `develop` khởi tạo từ commit main `61e6127`. Chưa ghép với nhánh `kien`, chưa thay main nhóm. Khi cần review, mở Pull Request từ `feature/task-display` vào `develop`; người tích hợp duyệt HAL/IPC trước khi merge. Sau đó nhóm mới ghép hệ thống và nghiệm thu phần cứng theo quy trình.

API công khai ở `app/inc/task_display.h`; kiểu message ở `app/inc/app_ipc.h`; stack/priority ở `app/inc/app_cfg.h`. Queue hiện được task sở hữu và gửi qua `task_display_post()`. Nhóm cần thống nhất việc tạo tài nguyên trong `app_ipc.c` chung và lời gọi `task_display_create()` trong main nhóm trước scheduler.

`FreeRTOSConfig.h` giữ nguyên bản nền, trùng với nhánh `kien` ở thời điểm chuẩn bị nhánh. Heap 8 KiB đủ cho test độc lập; người quản lý cấu hình phải tính lại khi thêm Sensor/Game. Không sinh code CubeMX đè cấu hình chung. ISR chỉ dùng API FromISR; task dùng delay RTOS và chờ hữu hạn.

Báo cáo Word được bàn giao riêng. Không đưa backup, ZIP nguồn, binary build hoặc cấu hình IDE cá nhân vào commit.
