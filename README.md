# STM32F411CE — FreeRTOS + BMI160

Bản chuyển từ `raw.zip` sang **STM32F411CE**, Flash 512 KiB, RAM 128 KiB.
Giữ nguyên từng byte toàn bộ `app/`, `hal/` và `main.c` (15 file).
`PRESERVED-SHA256.json` ghi SHA-256 các file được giữ nguyên.
Các chú thích F103/Blue Pill còn trong những file này là chú thích gốc.

## Build và nạp

Cần GNU Arm Embedded Toolchain (`arm-none-eabi-gcc`, newlib-nano), GNU Make và Python 3.8+.
CMSIS và FreeRTOS đã có sẵn, không cần tải thêm thư viện khi build.

```sh
make -j4
make flash
```

`make flash` cần ST-Link và `st-flash`; nạp vào `0x08000000`.
File đầu ra: `build/firmware.elf`, `.hex`, `.bin`, `.map`.
Debug: chạy `make debug-server` rồi `make debug` ở terminal khác (cần `st-util` và GDB ARM).

## Đấu nối giữ nguyên

| Chức năng | Chân STM32F411CE | Ghi chú |
|---|---|---|
| LED | PC13 | Active-low; cần board có LED tại PC13 hoặc mắc LED ngoài phù hợp |
| UART1 TX | PA9, AF7 | Nối RX của USB–UART mức 3.3 V, chung GND; UART chỉ phát |
| I2C1 SCL | PB6, AF4 | BMI160; cần điện trở kéo lên 3.3 V |
| I2C1 SDA | PB7, AF4 | BMI160; cần điện trở kéo lên 3.3 V |
| SWDIO / SWCLK | PA13 / PA14 | ST-Link, chung GND và tham chiếu nguồn 3.3 V |

Clock dùng HSI 16 MHz, PLL M=16/N=192/P=2/Q=4: CPU/AHB 96 MHz,
APB1 48 MHz, APB2 96 MHz. Không cần biết thạch anh HSE trên board.
Thiết lập Flash 3 wait states, voltage scale 1, dành cho nguồn board 3.3 V.
Nếu PLL không sẵn sàng, CPU tiếp tục chạy HSI; `main.c` cập nhật lại clock thực tế.
HSI có sai số; baud và tốc độ I2C phụ thuộc sai số đó.

## Những phần đã chuyển

- `project.mk`: Cortex-M4, FPU FPv4-SP-D16, hard-float, FreeRTOS GCC/ARM_CM4F.
- `platform/stm32f411ce/`: clock, runtime hooks, linker 512 KiB/128 KiB.
- `middlewares/CMSIS-F411/`: device header và startup chính thức của ST; CMSIS Core của ARM.
  `SOURCE.json` ghi nguồn, revision và SHA-256; kèm giấy phép gốc.
- `bsp/stm32f10x.h`, `bsp/stm32f411_compat.c`, `bsp/stm32f10x_conf.h`:
  lớp tương thích các lời gọi SPL F1 đang được app/HAL sử dụng trên phần cứng F411.
  Tên include cũ được giữ để không sửa app/HAL/main.
- `FreeRTOSConfig.h`: 4 bit ưu tiên NVIC; BSP chuyển yêu cầu nhóm ưu tiên cũ của
  `main.c` sang PRIGROUP=3 (4 bit preemption) cho CM4F.
- Cập nhật trợ giúp build, cấu hình VS Code và tài liệu cho target mới.

Lớp tương thích chỉ hỗ trợ tập API và chân mà project hiện dùng; không phải toàn bộ SPL F1.
SDK F103 và `platform/stm32f103c8/` vẫn được giữ làm nguồn tham khảo, không tham gia build.
Tài liệu F103 gốc nằm ở `README_F103_ORIGINAL.md` và `VALIDATION_F103_ORIGINAL.md`.
Các firmware F103 cũ trong ZIP đầu vào không được đưa vào bản chuyển.

## Phạm vi kiểm chứng

Build và kiểm tra ELF đã đạt; xem `VALIDATION.md`.
Chưa nạp/chạy thử trên STM32F411CE thực tế. Logic HAL I2C gốc, kể cả trình tự
ACK/STOP và timeout, được giữ nguyên. Cần thử đọc CHIP_ID và luồng dữ liệu BMI160
trên board, đặc biệt các lần đọc 1/2 byte và xử lý bus bị kẹt; build thành công
không xác nhận timing điện của bus.

Tài liệu phần cứng: [ST RM0383](https://www.st.com/resource/en/reference_manual/dm00119316.pdf).
