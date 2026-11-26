# Phần game — firmware chạy trên mạch thật

Nhánh feature/game_task được đồng bộ với develop tại 82488a6. Mục đích lần dọn này là bỏ scaffolding kiểm thử và các bản sao đường dẫn cũ của phần game, dùng cấu trúc app/hal/middle hiện tại của nhóm. Các file telemetry, task SENSOR_GAME, HAL sensor/UART, sensor_processing và sys_monitor giữ nguyên nội dung từ develop.

## Cấu trúc đang dùng

- app/src/task_game.c: nhận gia tốc/lệnh KEY, điều phối luật và gửi trạng thái màn hình.
- app/src/task_button.c: đọc nút theo chu kỳ, gửi lệnh NEXT/RESTART.
- middle/src/game_core.c: luật, vật lý, va chạm, sao/tim/bẫy/cổng và chuyển màn.
- middle/src/button_logic.c: chống dội và phân biệt nhấn ngắn/giữ.
- middle/src/game_display.c: chuẩn bị message và UI bổ sung cho display.
- middle/src/app_game_ipc.c: mailbox mẫu cảm biến và queue lệnh KEY.
- bsp/inc/bsp_button.h và hal/src/hal_button.c: ánh xạ/cấu hình/đọc KEY PA0.
- main.c, Makefile: điểm vào và lựa chọn source tích hợp sẵn của develop; giữ nguyên.

Các header tương ứng nằm ở inc của từng tầng. Game không tạo thêm vòng lặp đọc sensor, không thay UART/telemetry và không thay cơ chế theo dõi hệ thống.

## Các phần giữ nguyên từ develop

```text
app/inc/task_telemetry.h          app/src/task_telemetry.c
app/inc/task_sensor_game.h        app/src/task_sensor_game.c
hal/inc/hal_sensor.h              hal/src/hal_sensor.c
hal/inc/hal_uart.h                hal/src/hal_uart.c
middle/inc/sensor_processing.h    middle/src/sensor_processing.c
middle/inc/sys_monitor.h          middle/src/sys_monitor.c
```

Không lấy các file tương ứng từ bản STAR_MAZE standalone để ghi đè những phần này. Bản tham khảo chỉ được dùng để đối chiếu cách bỏ test/mock và các đường dẫn dư.

## Build và nạp

Trong thư mục gốc repository, với GNU Make, ARM GCC và ST-Link đã có trong PATH:

```sh
make -j4
make flash
```

Windows có thể dùng lệnh make đã cấu hình hoặc mingw32-make. Makefile là file build duy nhất; không còn GNUmakefile cũ ưu tiên nhầm entry standalone. Firmware thật nằm ở build/game/star-maze.bin; ELF và các file trung gian được sinh trong build/game.

Không cần compiler host hoặc script Python để build firmware này. Không giữ target test/mock trong phần game. Không đưa build/, BIN/ELF, ảnh mô phỏng hoặc file IDE cá nhân lên Git; .gitignore của nhóm giữ nguyên.

## Nội dung đã dọn

- Bỏ tests/game/test_game.c và Makefile/checksum phục vụ gói standalone cũ.
- Bỏ integration/main_game.c vì main.c hiện tại đã tạo đủ các task của nhóm.
- Bỏ các bản sao game_core/game_display/IPC/config dưới app; dùng các bản đang có ở middle.
- Bỏ bsp/bsp_button.h cũ; dùng bsp/inc/bsp_button.h.
- Bỏ hal_i2c_game_read.c cũ không còn được Makefile tích hợp sử dụng; không sửa hal_sensor.c.

Luật game hiện tại được giữ: năm màn, ba tim, giữ sao khi trúng bẫy, đủ sao mở cổng, KEY ngắn chuyển màn dù thiếu sao, giữ KEY chơi lại; giữ UI tối giản và tim vỡ. Việc dọn source không thay mã xử lý runtime đã tích hợp trên develop.

Đã build lại bằng ARM GCC. Firmware được đối chiếu SHA-256 với bản develop xuất riêng; thông tin kiểm chứng lưu ngoài repository. Lượt này không tự nạp board hoặc push lên GitHub.
