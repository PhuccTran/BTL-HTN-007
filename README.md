# BTL HTN 007 Module màn hình của Trần Thiện Phúc

Nhánh `feature/task-display-phuc` được tạo trực tiếp từ `develop`, không kế thừa commit chứa bộ nền của nhánh màn hình trước và không merge nhánh `kien`. Bộ này chỉ chứa phần màn hình và test mô hình. Không kèm code BMI160/UART/I2C/LED, main hệ thống, FreeRTOSConfig.h, Makefile/project.mk, clock, startup, linker hoặc runtime của project thành viên khác.

## Chức năng

Hiển thị STAR MAZE trên ST7735 SPI dọc 128×160, đen trắng. Bản đồ 12×12 ô, mỗi ô 10×10 pixel, bắt đầu tại (4,25). Header LEVEL/STAR cao 25 pixel; footer HP/tim cao 15 pixel.

Game gửi SCENE/UPDATE/NOTICE/RESUME qua `task_display_post()`. Display vẽ tường, đường đi, bi, sao, bẫy và cổng; cập nhật vùng thay đổi và giữ thông báo tới RESUME. Di chuyển, va chạm, nhặt sao, trừ tim, mở cổng và thắng/thua do game quyết định.

## Cấu trúc

- `bsp/bsp_display.h`: pin mapping, SPI1/DMA2 và profile panel dành riêng màn hình.
- `hal/inc`, `hal/src`: GPIO màn hình, SPI1 polling/DMA và driver ST7735.
- `app/inc`, `app/src`: task Display, renderer, hợp đồng IPC v2, cấu hình riêng Display và dữ liệu 5 bản đồ do người dùng cung cấp.
- `tests/display`: mock RTOS/ngoại vi, kịch bản Display, fixture bản đồ và test ARM.

`app_cfg.h` chỉ chứa thông số của module, không phải FreeRTOSConfig.h của hệ thống. Không có task, HAL hoặc cấu hình lấy từ code của bạn Kiên trong bộ này. RTOS giả trong tests chỉ dùng kiểm thử, không phải kernel FreeRTOS chạy trên kit.

## Chạy test mô hình

Cần GNU Arm Embedded Toolchain với Newlib và Python có package `unicorn`. Ví dụ cài Python dependency vào virtual environment riêng:

```bash
python3 -m venv .venv
.venv/bin/python -m pip install -r tests/display/requirements.txt
.venv/bin/python tests/display/generate_star_mock.py
.venv/bin/python tests/display/run_unit.py
.venv/bin/python tests/display/run_spi_unit.py
.venv/bin/python tests/display/run_mock_unit.py
```

Renderer/Queue và mock dùng các stub do bộ test định nghĩa. Test SPI chạy backend CMSIS thật trên mô hình thanh ghi/RTOS; lần đầu tự tải header CMSIS từ repository chính thức của ST/ARM theo commit/version và SHA256 cố định ở `tests/display/fixtures/cmsis_sources.json`. Header và license chỉ lưu trong `build/test-deps`, được bỏ qua khi commit. Không lấy các file này từ nhánh của thành viên khác. Các lần sau dùng cache đã kiểm tra checksum; cũng có thể đặt `DISPLAY_CMSIS_DIR` trỏ tới bộ header chính thức đã có, có hai thư mục Core và Device.

Kết quả JSON/PPM/ELF chỉ sinh trong `build/`. `generate_star_mock.py` đối chiếu ma trận C với TXT/JSON và sinh đường test. Khi sửa bản đồ phải cập nhật fixture và kịch bản kiểm thử cho khớp. Test mô hình không đo timing điện, scheduler thật hoặc stack runtime.

Kết quả chạy lại trên nhánh module: renderer/Queue PASS với 637026 assertion, backend SPI/DMA PASS với 91 assertion, mock PASS với 18586 assertion và 914 message. Các số assertion gồm kiểm tra từng pixel, không phải số tình huống độc lập.

## Ghép vào một project STM32F411CE

Đây là module để ghép, không phải firmware có thể build/nạp trực tiếp. Project đích tự cung cấp CMSIS STM32F411, kernel/port FreeRTOS, heap_4 và cấu hình board/clock/startup/linker của mình. Không dùng các header mock ở tests để build firmware thật.

Người tích hợp thêm BSP/HAL/App vào đường build, thống nhất `app_cfg.h` và `app_ipc.h`, rồi gọi `task_display_create()` tuần tự trước scheduler và kiểm tra pdPASS/pdFAIL. Init/vẽ chạy trong task sau scheduler. Producer dùng `task_display_post()` với thời hạn 0–100 ms và xử lý Queue đầy; không lấy chung Queue với consumer khác.

Kernel cần hỗ trợ cấp phát động, Queue, Mutex/Semaphore, thống kê heap_4, xTaskGetSchedulerState và high water mark. Display mặc định stack 512 words, priority +1 và Queue hai message. Người quản lý FreeRTOSConfig.h tính heap và ưu tiên ngắt cho project đích; không chép cấu hình của thành viên khác vào module.

SPI1 dùng PA5 SCK, PA7 MOSI, PA4 CS, PB0 DC, PB1 RESET. Backend DMA dùng DMA2 Stream3 Channel3, IRQ priority số 3; cấu hình FreeRTOS của project đích phải cho phép IRQ này gọi FromISR. Clock SPI là APB2/16; tốc độ thực phụ thuộc clock do project đích thiết lập. BL mặc định không điều khiển bằng GPIO.

## Nguồn tham chiếu

Bộ bản đồ `star_maze_levels.c/.h` và fixture được lấy từ gói STAR MAZE người dùng cung cấp. HAL, renderer, task và test là phần màn hình được chuẩn bị cho Trần Thiện Phúc; không chứa code chức năng của thành viên cảm biến/truyền thông.

CMSIS là mã thư viện của ST/ARM, không phải code của bạn Kiên. Manifest ghi rõ URL, version/commit, checksum và license. Driver tham chiếu lệnh/profile từ [datasheet ST7735](https://www.displayfuture.com/Display/datasheet/controller/ST7735.pdf) và [component STMicroelectronics](https://github.com/STMicroelectronics/stm32-st7735/blob/main/st7735.c).

Nhánh này độc lập để nhóm review qua PR vào develop; chưa tích hợp hệ thống. Bản test kit và báo cáo Word đã bàn giao riêng vẫn được giữ ngoài nhánh module này.
