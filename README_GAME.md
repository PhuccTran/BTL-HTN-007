# STAR MAZE — bản nộp C và GNU Make

## Trạng thái nhánh feature/game_task

Nhánh này được tạo từ develop tại b7131f59fd8bbda6f70293db1cc9532cbfdd65b9 và chỉ bổ sung 22 file của phần game/nút/build/test. Không sửa main.c, FreeRTOSConfig.h hoặc file đang có của các thành viên khác. File kiểm kê cục bộ không được commit.

Tại thời điểm chuẩn bị nhánh, develop chưa có app/hal của màn hình và BMI. API màn hình đang nằm ở feature/task-display-phuc (8fa6fd927ddfa82e3a66ea2a377fbc53d60a6a3d); service BMI ở feature/bmi_160_task (2639ecb204cf956b04789bb73d8b6189b8da01a0). Vì vậy nhánh game riêng chưa phải firmware tích hợp có thể build ngay từ develop. Cần review/merge các phần phụ thuộc theo quy trình của nhóm.

GNUmakefile hiện mô tả layout của bản standalone đã chạy trên kit: driver màn hình ở app/hal/bsp; tám file BMI kế thừa ở vendor/bmi160. Nhánh BMI trên Git đặt service và HAL ở app/hal/bsp, nên khi tích hợp phải thống nhất ánh xạ đường dẫn build, không tự chép đè driver/main chung. Các lệnh build và kiểm chứng bên dưới áp dụng khi có đủ phụ thuộc đúng layout standalone. ORIGINALS.sha256 đối chiếu gói ZIP nguồn, không phải checksum của toàn bộ develop.

Kiểm thử trước push dùng 20 file code/test/build của nhánh này với các phụ thuộc standalone đã được kiểm tra trên kit. Không đưa phụ thuộc của các bạn, output build hoặc firmware vào commit game. integration/main_game.c là chương trình ghép tham chiếu; người phụ trách tích hợp quyết định cách gọi task trong main chung trên integration/system-v1.

Đọc file này cho phần game. README.md, Makefile, project.mk và các file gốc của nhóm vẫn giữ nguyên. GNU Make ưu tiên GNUmakefile nên gõ `make` sẽ build game bằng C, không chạy script Python. Startup assembly, CMSIS, FreeRTOS và driver kế thừa vẫn cần để STM32 khởi động và chạy đa nhiệm.

## 1. Build và nạp trên Ubuntu

Cần có `make`, `gcc` cho test máy tính, `arm-none-eabi-gcc` và newlib cho STM32, `st-info`/`st-flash` cho ST-Link. Kiểm tra công cụ đã có:

```sh
command -v make gcc arm-none-eabi-gcc arm-none-eabi-objcopy st-info st-flash sha256sum
```

Trong thư mục có GNUmakefile:

```sh
make verify       # so SHA-256 toàn bộ 108 file kế thừa
make -j4          # build firmware game C
make test         # chạy test logic/nút/vật lý bằng C trên máy tính
st-info --probe   # kiểm tra ST-Link đã nối với Ubuntu
make flash       # nạp build/game/star-maze.bin
```

Đặt thiết bị ngang, đứng yên lúc cấp nguồn/reset để lấy mốc cảm biến. Khi dùng VMware, ST-Link phải được nối vào Ubuntu guest trước `make flash`.

Test độc lập bằng gia tốc giả, không cần BMI hoạt động:

```sh
make mock
make flash-mock
make flash       # nạp lại bản thật dùng BMI sau khi thử mock
```

Mock nằm trong nhánh `GAME_SENSOR_MOCK` của task_sensor_game.c, không cần thêm file producer hoặc main riêng. Hai bản build ở build/game và build/mock, không ghi đè nhau. `make clean` chỉ xóa các thư mục build do phần game/test tạo.

Kết quả build: build/game/star-maze.elf, .bin, .hex, .map. Không commit những file này. Không dùng `make -f Makefile` cho game: đó là Makefile màn hình gốc của bạn khác, được giữ lại để bảo toàn source.

## 2. Cấu trúc và phần của bạn

Có 20 file code/test/build bổ sung: 9 C chạy trên STM32, 9 header, 1 C kiểm thử trên host và 1 GNUmakefile. Commit gồm 22 file tính cả README_GAME.md và ORIGINALS.sha256; file danh sách kiểm kê chỉ dùng cục bộ và không có trong nhánh. ORIGINALS.sha256 nhận diện các file kế thừa trong gói standalone phải giữ nguyên.

```text
app/inc/
  game_core.h          Game_t, GameState_t, API luật game
  game_config.h        thông số game + priority/stack/chu kỳ task
  game_display.h       kiểu/API chuẩn bị message màn hình
  task_game.h          API task game và thống kê
  task_button.h        API task nút + dữ liệu/phép thử chống dội
  task_sensor_game.h   API task đọc BMI và thống kê
  app_game_ipc.h       kiểu sample/command và API queue
app/src/
  game_core.c          luật + vật lý, không điều khiển GPIO/LCD
  game_display.c       chuẩn bị message và phần vẽ UI bổ sung
  task_game.c          task 20 ms: command → core → message display
  task_button.c        chống dội, nhấn/giữ, task KEY 5 ms
  task_sensor_game.c   đọc BMI/hiệu chuẩn/mailbox; có nhánh mock
  app_game_ipc.c       mailbox cảm biến và queue lệnh
bsp/bsp_button.h       KEY tại PA0
hal/inc/hal_button.h   API đọc GPIO nút
hal/src/hal_button.c   cấu hình pull-up, nhấn mức 0
hal/src/hal_i2c_game_read.c  backend I2C đọc/ghi bổ sung
integration/main_game.c     main ghép cục bộ
tests/game/test_game.c      kiểm thử C, không cần phần cứng
GNUmakefile                 chọn source, compile/link, test/nạp
```

Các file trên là phần bổ sung của bạn. Những file app_cfg.h, app_ipc.h, task_display.*, display_render.*, star_maze_levels.*, driver ST7735/SPI/GPIO, vendor/bmi160, platform, middlewares, main.c gốc, FreeRTOSConfig.h, Makefile/project.mk/README.md gốc thuộc phần kế thừa. Không cần nhận là code của mình hoặc học thuộc mã nguồn kernel/driver của các bạn.

Một số file Python gốc còn trong cây source vì không được xóa/sửa file của người khác; GNUmakefile của game không gọi chúng. Toàn bộ runtime bổ sung và test mới là C. Không có Python mới trong bản nộp.

## 3. Những phần đã rút gọn

- Gộp button_debounce.c/.h và button_gesture.c/.h vào task_button.c/.h. Giữ nguyên thuật toán và quy tắc nhấn ngắn/giữ lâu.
- Gộp game_present.c/.h và game_ui_render.c thành game_display.c/.h. Phần chuẩn bị message vẫn chạy ở GAME; phần vẽ chỉ chạy ở DISPLAY. Gộp file không bỏ ranh giới queue/task.
- Gộp app_game_cfg.h vào game_config.h, không sửa app_cfg.h/FreeRTOSConfig.h của nhóm.
- Thay build_game.py và Makefile.game bằng GNUmakefile. Bỏ các script Python, main/producer mock riêng và báo cáo lịch sử của phần bạn khỏi thư mục nộp.
- Mock được chọn bằng macro khi `make mock`; vẫn có cách thử game độc lập theo quy trình.
- Giữ một C test dễ chạy bằng `make test`; các phép kiểm tra hồi quy UI đầy đủ được lưu ngoài thư mục nộp.
- Bỏ biến debug giai đoạn cấu hình I2C; giữ thống kê heap/stack/lỗi vì quy trình yêu cầu kiểm tra tài nguyên RTOS.

## 4. Luồng hoạt động cần giải thích

```text
KEY PA0 → task_button → queue command ─────────┐
                                              ↓
BMI → service/hal I2C → task_sensor_game → task_game + game_core
                             mailbox          ↓
                                  game_display chuẩn bị message
                                              ↓
                             queue display → task_display
                                              ↓
                            UI mới / renderer gốc → ST7735
```

task_game là nơi duy nhất sở hữu Game_t. Sensor chỉ gửi gia tốc, display chỉ vẽ. Lệnh KEY được xử lý trước luật sao/cổng. Display không được tự trừ tim hay chọn level.

| Task | Priority | Chu kỳ | Stack |
| --- | --- | --- | --- |
| BUTTON | 4 | 5 ms | 128 words |
| SENSOR_GAME | 3 | 20 ms | 256 words |
| GAME | 2 | 20 ms | 320 words |
| DISPLAY, kế thừa | 1 | chờ queue, timeout 50 ms | 512 words |

Queue command có 4 phần tử. Mailbox sensor có 1 phần tử, overwrite mẫu cũ và peek mẫu mới nhất. Mẫu chưa hiệu chuẩn hoặc quá 200 ms làm bi dừng nhưng game vẫn nhận KEY. Queue display đầy thì giữ giao dịch chưa gửi, dừng chuyển động trong lúc chờ, không bỏ SCENE/NOTICE.

## 5. Bạn học những gì, theo thứ tự?

1. **game_core.c + game_core.h:** Game_t chứa level, lives, vị trí/vận tốc, mask sao và state. Giải thích load level, respawn, restart khác nhau thế nào. Hiểu mỗi sao là một bit; bẫy chỉ giảm tim/đặt lại vị trí-vận tốc, giữ mask sao. Đủ sao mới mở cổng; tiếp xúc cổng mở mới tự chuyển màn.
2. **task_button.c:** ba phần đã đánh dấu trong file: chống dội 30 ms → phân biệt ngắn/giữ 1000 ms → task gửi command. Ngắn phát khi thả; dài phát một lần khi đủ ngưỡng, thả không phát thêm next. Nút giữ sẵn lúc boot phải thả trước khi nhận lượt nhấn mới.
3. **task_game.c + app_game_ipc.c:** cách tạo/nhận queue và thứ tự xử lý command trước physics. Hiểu xTaskDelayUntil, pdMS_TO_TICKS, timeout, kiểm tra lỗi tạo task/queue. Hai tầng ưu tiên: priority BUTTON cao hơn GAME và thứ tự logic trong GAME.
4. **game_display.c:** phần đầu tạo SCENE/UPDATE/NOTICE/RESUME; phần sau vẽ logo/tên LEVEL/thắng/thua. Hai phần không dùng chung state mutable ngoài context truyền vào. Font là bitmap 5x7 phóng to; không cần học thuộc từng byte font.
5. **task_sensor_game.c:** service BMI raw → mg → lấy trung bình 32 mẫu ổn định → trừ offset → mailbox. Đổi trục/dấu nằm trong game_config/game_core. Giữ KEY chỉ reset game, RESET mới khởi động lại cả cảm biến và hiệu chuẩn.
6. **hal_button.c + bsp_button.h:** PA0 input pull-up, IDR=0 là đang nhấn; không dùng EXTI.
7. **hal_i2c_game_read.c:** START, địa chỉ, thanh ghi, repeated START, ACK/STOP, đọc dữ liệu; mọi vòng chờ có giới hạn. Đây là lớp ghép sửa đường đọc/ghi I2C cho kit, không phải bạn viết lại toàn bộ BMI service.
8. **integration/main_game.c + GNUmakefile:** thứ tự tạo IPC/task/start scheduler, chỉ chọn một main, cách compiler/linker tạo ELF/BIN, cách cờ build chọn renderer/backend mới. Root main.c của nhóm vẫn giữ nguyên.

Các thông số quan trọng: Q8=256 đơn vị cho một pixel; tick game 20 ms; deadzone 40 mg; lọc bằng 1/4 sai lệch mỗi bước; ma sát giữ 92% vận tốc; tăng lực ở độ nghiêng mạnh; tổng |vx|+|vy| không vượt 60 pixel/giây. Chia bước va chạm tối đa nửa pixel/trục để tránh xuyên tường. Không cần học thuộc toàn bộ C test; cần giải thích test đang kiểm chứng luật nào.

## 6. Luật/UI đang giữ nguyên

- 5 màn cũ, 3 tim, không vật phẩm hồi tim; sao giữ sau khi đụng X.
- Cổng trắng đặc khi khóa, đen viền trắng khi mở.
- KEY ngắn qua màn dù thiếu sao, 5→1. Giữ KEY: chỉ LEVEL 1 khoảng 1 giây → chơi lại 3 tim.
- RESET/cấp nguồn: sao + STAR MAZE khoảng 2 giây → LEVEL 1 khoảng 1 giây → chơi.
- Mất 1 tim khi còn mạng: chỉ hiện trái tim vỡ trắng 54x42 pixel, căn giữa nền đen khoảng 0,7 giây; không có -1 HP, PLEASE WAIT hay chữ phụ. Sau đó về vị trí xuất phát, giữ sao. Hết tim vẫn GAME OVER.
- Trước các level có tên LEVEL khoảng 1 giây; GAME OVER/YOU WIN giữ nguyên cho đến lệnh nút/reset.
- Trục swap=1, X=+1, Y=-1; độ nhạy và tốc độ như bản đã được người dùng phản hồi tốt.

## 7. Đối chiếu đề cương và quy trình

HTNN04_N07.pdf yêu cầu đọc BMI, hiệu chỉnh trục, giới hạn vận tốc, va chạm theo trục, ST7735, chống dội/restart, thông báo không bị ghi đè và chia module để kiểm thử: các phần này được giữ. PDF gốc ghi 3 màn/quay vòng; người dùng đã chọn giữ bản mở rộng 5 màn, sao/tim/bẫy và YOU WIN. Cần nêu rõ đây là phần mở rộng đã chốt, không nói PDF vốn ghi 5 màn.

Theo QuyTrinh-Git-2.pdf: vẫn có bsp/hal/app/middlewares; task riêng và API create; regular RTOS API ở task, không ở ISR; root main.c và FreeRTOSConfig.h không đổi; dùng mock và test. Bản này không thêm ISR ngoại vi, SPI dùng polling, kernel vẫn dùng SysTick/PendSV/SVC.

Nhóm vẫn cần review cấu hình tài nguyên, IPC, UI và backend I2C trước khi merge. Backend hiện có một task sở hữu I2C, service BMI có mutex khi đọc nhưng chưa có mutex bus tổng quát cho nhiều task; recovery gốc còn delay NOP ngắn. Không tự chạy thêm BMI demo/task I2C khác song song. Những điểm này không được che đi để nói đã tuân thủ 100% quy trình chia sẻ HAL.

main_game.c là main tham chiếu/tích hợp cục bộ, không sửa main.c chung ở feature. GNUmakefile thay đổi entry build mặc định khi có trong repo; người tích hợp phải review cùng nhóm. Nhánh feature/game_task cần PR về develop và peer review trước khi ghép trên integration/system-v1 rồi phát hành.

## 8. Kiểm chứng và giới hạn

Bản thật và mock đã build bằng GNU Make + ARM GCC trên Windows. Makefile có đường recipe Ubuntu dùng /bin/sh, mkdir, rm và sha256sum; chưa điều khiển trực tiếp Ubuntu VMware trong lượt này. `make test` đạt 11418 kiểm tra/13946 frame và giải đủ 5 màn với 3 tim. Kiểm thử hồi quy riêng chạy lại C renderer/driver/core với mock, đạt 11638 kiểm tra game, 1025196 kiểm tra renderer; 13 frame UI khác giống từng pixel bản trước, riêng frame mất mạng đã đổi sang trái tim vỡ. Kiểm tra frame mới xác nhận hai mảnh tim nằm trong vùng 54x42, mọi pixel bên ngoài đều đen, không còn chữ.

Đã nạp bản UI trái tim vỡ, BIN thật 29236 byte, đọc lại flash khớp rồi reset. Snapshot debugger của bản gọn trước thay đổi icon, sau khoảng 35 giây: 1647 mẫu BMI, 0 lỗi sensor, 0 lỗi vẽ, heap tối thiểu 624 byte; stack còn BUTTON 95, SENSOR 180, GAME 273, DISPLAY 332 words. Đây là kiểm tra ngắn, chưa phải chạy bền nhiều giờ hoặc xác nhận người dùng đã chơi tay đủ 5 màn trên bản gọn.

Đối chiếu SHA-256: toàn bộ 108 file kế thừa không đổi. Thư mục nộp và ZIP source không chứa output build/firmware hay các script Python mới của phần game. Bằng chứng kiểm thử/ảnh tham chiếu giữ ở ngoài thư mục nộp để giảm số file cần trình bày.
