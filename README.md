# Module màn hình ST7735

STM32F411CE, ST7735 dọc 128×160, bản đồ 12×12 ô. Mã trong `app/`, `bsp/`, `hal/`; test mô hình trong `tests/display/`.

Cần GNU Arm Embedded Toolchain/Newlib và Python:

```bash
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install -r tests/display/requirements.txt
python3 tests/display/generate_star_mock.py
python3 tests/display/run_unit.py
python3 tests/display/run_spi_unit.py
python3 tests/display/run_mock_unit.py
```

Test SPI tự tải CMSIS ST/ARM theo manifest và SHA256 cố định vào `build/test-deps/`. Có thể dùng `DISPLAY_CMSIS_DIR` với hai thư mục Core và Device để chạy offline.

Project đích cung cấp CMSIS, FreeRTOS/heap_4 và cấu hình board. Gọi `task_display_create()` trước scheduler; gửi dữ liệu bằng `task_display_post()`. Đây là module, không có firmware nạp kit.
