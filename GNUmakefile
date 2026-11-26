# Build the C game with GNU Make. The inherited Makefile remains untouched.
# GNU Make reads GNUmakefile before Makefile, so plain `make` builds this game.
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
.SUFFIXES:

CROSS_COMPILE ?= arm-none-eabi-
CC := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy
SIZE := $(CROSS_COMPILE)size
HOSTCC ?= gcc
STFLASH ?= st-flash
STUTIL ?= st-util
MODE ?= game
ifneq ($(filter $(MODE),game mock),$(MODE))
$(error MODE must be game or mock)
endif
override BUILD_DIR := build/$(MODE)
ELF := $(BUILD_DIR)/star-maze.elf
BIN := $(BUILD_DIR)/star-maze.bin
HEX := $(BUILD_DIR)/star-maze.hex

KERNEL := middlewares/FreeRTOS-Kernel
PORT := $(KERNEL)/portable/GCC/ARM_CM4F
INCLUDES := . bsp hal/inc app/inc vendor/bmi160/bsp vendor/bmi160/hal/inc \
            vendor/bmi160/app/inc middlewares/CMSIS-F411/Core middlewares/CMSIS-F411/Device \
            $(KERNEL)/include $(PORT)
ARCH := -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
CPPFLAGS := -DSTM32F411xE -DBSP_DISPLAY_USE_DMA=0 $(addprefix -I,$(INCLUDES))
ifeq ($(MODE),mock)
CPPFLAGS += -DGAME_SENSOR_MOCK=1
endif
CFLAGS := $(ARCH) -Og -g3 -std=c11 -Wall -Wextra -Werror \
          -ffunction-sections -fdata-sections -fno-common -fstack-usage
LDFLAGS := $(ARCH) -Tplatform/stm32f411ce/stm32f411ce.ld -nostartfiles \
           --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections,-Map=$(BUILD_DIR)/star-maze.map

C_SOURCES := app/src/game_core.c app/src/game_display.c app/src/task_game.c \
 app/src/task_button.c app/src/task_sensor_game.c app/src/app_game_ipc.c \
 hal/src/hal_button.c hal/src/hal_i2c_game_read.c integration/main_game.c \
 app/src/task_display.c app/src/display_render.c app/src/star_maze_levels.c \
 hal/src/hal_gpio.c hal/src/hal_spi.c hal/src/hal_st7735.c \
 vendor/bmi160/bsp/stm32f411_compat.c vendor/bmi160/hal/src/hal_i2c.c \
 vendor/bmi160/app/src/bmi160_service.c \
 platform/stm32f411ce/system.c platform/stm32f411ce/rtos_hooks.c \
 $(addprefix $(KERNEL)/,tasks.c queue.c list.c timers.c event_groups.c stream_buffer.c) \
 $(PORT)/port.c $(KERNEL)/portable/MemMang/heap_4.c
ASM_SOURCES := middlewares/CMSIS-F411/startup_stm32f411xe.s
OBJECTS := $(addprefix $(BUILD_DIR)/,$(addsuffix .o,$(C_SOURCES) $(ASM_SOURCES)))

# Adapt supplied code at compile time; do not edit the teammates' files.
$(BUILD_DIR)/app/src/display_render.c.o: CPPFLAGS += \
 -Dtask_display_render_apply=game_ui_base_apply \
 -Dtask_display_render_refresh=game_ui_base_refresh
$(BUILD_DIR)/vendor/bmi160/hal/src/hal_i2c.c.o: CPPFLAGS += \
 -Dhal_i2c_read=hal_i2c_read_legacy -Dhal_i2c_read_reg=hal_i2c_read_reg_legacy \
 -Dhal_i2c_write=hal_i2c_write_legacy -Dhal_i2c_write_reg=hal_i2c_write_reg_legacy

ifeq ($(OS),Windows_NT)
SHELL := cmd.exe
.SHELLFLAGS := /c
TEST_EXE := build/tests/test_game.exe
mkdir = if not exist "$(subst /,\,$(1))" mkdir "$(subst /,\,$(1))"
else
SHELL := /bin/sh
TEST_EXE := build/tests/test_game
mkdir = mkdir -p "$(1)"
endif

.PHONY: all test mock flash flash-mock reset debug-server verify clean help
all: $(ELF) $(BIN) $(HEX)

mock:
	"$(MAKE)" MODE=mock all

flash-mock:
	"$(MAKE)" MODE=mock flash

$(BUILD_DIR)/%.c.o: %.c GNUmakefile
	@$(call mkdir,$(@D))
	"$(CC)" $(CPPFLAGS) $(CFLAGS) -MMD -MP -c "$<" -o "$@"

$(BUILD_DIR)/%.s.o: %.s GNUmakefile
	@$(call mkdir,$(@D))
	"$(CC)" $(CPPFLAGS) $(ARCH) -g3 -x assembler-with-cpp -MMD -MP -c "$<" -o "$@"

$(ELF): $(OBJECTS) platform/stm32f411ce/stm32f411ce.ld
	"$(CC)" $(OBJECTS) $(LDFLAGS) -Wl,--start-group -lc -lm -lgcc -Wl,--end-group -o "$@"
	"$(SIZE)" "$@"

$(BIN): $(ELF)
	"$(OBJCOPY)" -O binary "$<" "$@"

$(HEX): $(ELF)
	"$(OBJCOPY)" -O ihex "$<" "$@"

TEST_SOURCES := tests/game/test_game.c app/src/game_core.c app/src/task_button.c app/src/star_maze_levels.c
$(TEST_EXE): $(TEST_SOURCES) app/inc/game_core.h app/inc/game_config.h app/inc/task_button.h GNUmakefile
	@$(call mkdir,$(@D))
	"$(HOSTCC)" -std=c11 -O1 -Wall -Wextra -Werror -DGAME_HOST_TEST -Iapp/inc $(TEST_SOURCES) -o "$@"

test: $(TEST_EXE)
	"$(TEST_EXE)"

flash: $(BIN)
	"$(STFLASH)" --reset write "$(BIN)" 0x08000000

reset:
	"$(STFLASH)" reset

debug-server:
	"$(STUTIL)" -p 4242

# Ubuntu coreutils. This verifies all 108 inherited files without Python.
verify:
	sha256sum --quiet -c ORIGINALS.sha256
	@echo Verified all inherited files.

# Both paths are fixed subdirectories of this project, never the source tree.
clean:
ifeq ($(OS),Windows_NT)
	@if exist "build\game" rmdir /s /q "build\game"
	@if exist "build\mock" rmdir /s /q "build\mock"
	@if exist "build\tests" rmdir /s /q "build\tests"
else
	rm -rf -- build/game build/mock build/tests
endif

help:
	@echo make - build C firmware
	@echo make test - run C logic tests on the computer
	@echo make mock - build with dummy acceleration instead of BMI
	@echo make flash - program the STM32 using ST-Link
	@echo make flash-mock - program the independent mock-input demo
	@echo make verify - verify inherited files on Ubuntu
	@echo make clean - remove generated game/test output

-include $(OBJECTS:.o=.d)
