# Build the complete layered C project with GNU Make.
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
.SUFFIXES:

CROSS_COMPILE ?= arm-none-eabi-
CC := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy
SIZE := $(CROSS_COMPILE)size
STFLASH ?= st-flash
override BUILD_DIR := build/game
ELF := $(BUILD_DIR)/star-maze.elf
BIN := $(BUILD_DIR)/star-maze.bin

KERNEL := third_party/FreeRTOS-Kernel
PORT := $(KERNEL)/portable/GCC/ARM_CM4F
INCLUDES := . bsp/inc hal/inc middle/inc app/inc \
            third_party/CMSIS-F411/Core third_party/CMSIS-F411/Device \
            $(KERNEL)/include $(PORT)
ARCH := -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
CPPFLAGS := -DSTM32F411xE -DBSP_DISPLAY_USE_DMA=0 $(addprefix -I,$(INCLUDES))
CFLAGS := $(ARCH) -Og -g3 -std=c11 -Wall -Wextra -Werror \
          -ffunction-sections -fdata-sections -fno-common -fstack-usage
LDFLAGS := $(ARCH) -Tplatform/stm32f411ce/stm32f411ce.ld -nostartfiles \
           --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections,-Map=$(BUILD_DIR)/star-maze.map

C_SOURCES := middle/src/sensor_processing.c middle/src/button_logic.c middle/src/game_core.c middle/src/game_display.c app/src/task_game.c \
 app/src/task_button.c app/src/task_sensor_game.c middle/src/app_game_ipc.c \
 app/src/task_telemetry.c middle/src/sys_monitor.c \
 hal/src/hal_button.c main.c \
 app/src/task_display.c middle/src/display_render.c middle/src/star_maze_levels.c \
 hal/src/hal_gpio.c hal/src/hal_spi.c hal/src/hal_st7735.c \
 hal/src/hal_sensor.c hal/src/hal_uart.c \
 platform/stm32f411ce/system.c platform/stm32f411ce/rtos_hooks.c \
 $(addprefix $(KERNEL)/,tasks.c queue.c list.c timers.c event_groups.c stream_buffer.c) \
 $(PORT)/port.c $(KERNEL)/portable/MemMang/heap_4.c
ASM_SOURCES := third_party/CMSIS-F411/startup_stm32f411xe.s
OBJECTS := $(addprefix $(BUILD_DIR)/,$(addsuffix .o,$(C_SOURCES) $(ASM_SOURCES)))

# Adapt supplied code at compile time; do not edit the teammates' files.
$(BUILD_DIR)/middle/src/display_render.c.o: CPPFLAGS += \
 -Dtask_display_render_apply=game_ui_base_apply \
 -Dtask_display_render_refresh=game_ui_base_refresh
ifeq ($(OS),Windows_NT)
SHELL := cmd.exe
.SHELLFLAGS := /c
mkdir = if not exist "$(subst /,\,$(1))" mkdir "$(subst /,\,$(1))"
else
SHELL := /bin/sh
mkdir = mkdir -p "$(1)"
endif

.PHONY: all flash reset clean help
all: $(ELF) $(BIN)

$(BUILD_DIR)/%.c.o: %.c Makefile
	@$(call mkdir,$(@D))
	"$(CC)" $(CPPFLAGS) $(CFLAGS) -MMD -MP -c "$<" -o "$@"

$(BUILD_DIR)/%.s.o: %.s Makefile
	@$(call mkdir,$(@D))
	"$(CC)" $(CPPFLAGS) $(ARCH) -g3 -x assembler-with-cpp -MMD -MP -c "$<" -o "$@"

$(ELF): $(OBJECTS) platform/stm32f411ce/stm32f411ce.ld
	"$(CC)" $(OBJECTS) $(LDFLAGS) -Wl,--start-group -lc -lm -lgcc -Wl,--end-group -o "$@"
	"$(SIZE)" "$@"

$(BIN): $(ELF)
	"$(OBJCOPY)" -O binary "$<" "$@"

flash: $(BIN)
	"$(STFLASH)" --reset write "$(BIN)" 0x08000000

reset:
	"$(STFLASH)" reset

# Only generated output in the fixed project build/game directory.
clean:
ifeq ($(OS),Windows_NT)
	@if exist "build\game" rmdir /s /q "build\game"
else
	rm -rf -- build/game
endif

help:
	@echo make - build the real BMI160 firmware
	@echo make flash - build and flash using ST-Link
	@echo make reset - reset the connected board
	@echo make clean - remove generated firmware build output

-include $(OBJECTS:.o=.d)
