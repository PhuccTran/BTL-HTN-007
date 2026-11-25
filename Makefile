# GNU Make >= 4.0. Recipe commands work with native Windows or POSIX shells.
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
.SUFFIXES:

include project.mk
-include project.local.mk

ifeq ($(OS),Windows_NT)
PYTHON ?= python
else
PYTHON ?= python3
endif

CC := $(TOOLCHAIN_PREFIX)gcc
OBJCOPY := $(TOOLCHAIN_PREFIX)objcopy
OBJDUMP := $(TOOLCHAIN_PREFIX)objdump
SIZE := $(TOOLCHAIN_PREFIX)size
GDB := $(TOOLCHAIN_PREFIX)gdb
BUILD_DIR := build
ELF := $(BUILD_DIR)/$(TARGET).elf
HEX := $(BUILD_DIR)/$(TARGET).hex
BIN := $(BUILD_DIR)/$(TARGET).bin
MAP := $(BUILD_DIR)/$(TARGET).map
LISTING := $(BUILD_DIR)/$(TARGET).lst
HELPER := scripts/build_support.py

KERNEL_DIR := middlewares/FreeRTOS-Kernel
PORT_DIR := $(KERNEL_DIR)/portable/$(FREERTOS_PORT)
rwildcard = $(foreach p,$(wildcard $(1)/*),$(call rwildcard,$(p),$(2)) $(filter $(subst *,%,$(2)),$(p)))
KERNEL_SOURCES := $(addprefix $(KERNEL_DIR)/,tasks.c queue.c list.c timers.c event_groups.c stream_buffer.c)
C_SOURCES := $(filter-out $(EXCLUDE_SOURCES),$(sort main.c $(foreach d,$(SOURCE_DIRS),$(call rwildcard,$(d),*.c)) $(KERNEL_SOURCES) $(wildcard $(PORT_DIR)/*.c) $(KERNEL_DIR)/portable/MemMang/heap_$(FREERTOS_HEAP).c $(EXTRA_C_SOURCES)))
ASM_SOURCES := $(filter-out $(EXCLUDE_SOURCES),$(sort $(STARTUP_SOURCES) $(wildcard $(PORT_DIR)/*.s $(PORT_DIR)/*.S) $(EXTRA_ASM_SOURCES)))
ALL_INCLUDES := $(sort $(INCLUDE_DIRS) $(EXTRA_INCLUDE_DIRS) $(KERNEL_DIR)/include $(PORT_DIR))
OBJECTS := $(addprefix $(BUILD_DIR)/,$(addsuffix .o,$(C_SOURCES) $(ASM_SOURCES)))
DEPS := $(OBJECTS:.o=.d)
ARCH_FLAGS := -mcpu=$(CPU) -mthumb -mfloat-abi=$(FLOAT_ABI) $(if $(strip $(FPU)),-mfpu=$(FPU))
CPPFLAGS := $(addprefix -D,$(DEFINES)) $(addprefix -I,$(ALL_INCLUDES))
CFLAGS := $(ARCH_FLAGS) $(OPT) -g3 -std=gnu11 -Wall -Wextra -ffunction-sections -fdata-sections -fno-common $(EXTRA_CFLAGS)
ASFLAGS := $(ARCH_FLAGS) -g3 -x assembler-with-cpp
LDFLAGS := $(ARCH_FLAGS) -T$(LINKER_SCRIPT) -nostartfiles --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections,-Map=$(MAP),--cref $(EXTRA_LDFLAGS)
LDLIBS := -Wl,--start-group -lc -lm -lgcc -Wl,--end-group
CONFIG_FILES := Makefile project.mk $(wildcard project.local.mk) FreeRTOSConfig.h $(HELPER)
CONFIG_STAMP := $(BUILD_DIR)/.build-config
.PHONY: all build help check check-flash sources print-config clean rebuild flash reset debug-server debug size hex bin disasm FORCE
all: $(ELF) $(HEX) $(BIN)
build: all
hex: $(HEX)
bin: $(BIN)

help:
	@$(PYTHON) $(HELPER) help

check:
	@$(PYTHON) $(HELPER) check --tool "$(CC)" --tool "$(OBJCOPY)" --tool "$(SIZE)" --required "$(LINKER_SCRIPT)" --required "FreeRTOSConfig.h" --required "$(PORT_DIR)/portmacro.h" $(foreach f,$(C_SOURCES) $(ASM_SOURCES),--required "$(f)")

# Keep the stamp's mtime unchanged when the source list and flags are unchanged.
# This also forces relinking when a source file is removed from the project.
$(CONFIG_STAMP): FORCE $(CONFIG_FILES) | check
	@$(PYTHON) $(HELPER) stamp --output "$@" --entry "CC=$(CC)" --entry "CPPFLAGS=$(CPPFLAGS)" --entry "CFLAGS=$(CFLAGS)" --entry "ASFLAGS=$(ASFLAGS)" --entry "LDFLAGS=$(LDFLAGS) $(LDLIBS)" --entry "SOURCES=$(C_SOURCES) $(ASM_SOURCES)"

$(BUILD_DIR)/%.c.o: %.c $(CONFIG_FILES) $(CONFIG_STAMP) | check
	@$(PYTHON) $(HELPER) mkdir --output "$(@D)"
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -MF "$(@:.o=.d)" -c "$<" -o "$@"

$(BUILD_DIR)/%.s.o: %.s $(CONFIG_FILES) $(CONFIG_STAMP) | check
	@$(PYTHON) $(HELPER) mkdir --output "$(@D)"
	$(CC) $(CPPFLAGS) $(ASFLAGS) -MMD -MP -MF "$(@:.o=.d)" -c "$<" -o "$@"

$(BUILD_DIR)/%.S.o: %.S $(CONFIG_FILES) $(CONFIG_STAMP) | check
	@$(PYTHON) $(HELPER) mkdir --output "$(@D)"
	$(CC) $(CPPFLAGS) $(ASFLAGS) -MMD -MP -MF "$(@:.o=.d)" -c "$<" -o "$@"

$(ELF): $(OBJECTS) $(LINKER_SCRIPT) $(CONFIG_FILES) $(CONFIG_STAMP) | check
	$(CC) $(OBJECTS) $(LDFLAGS) $(LDLIBS) -o "$@"
	$(SIZE) "$@"

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex "$<" "$@"

$(BIN): $(ELF)
	$(OBJCOPY) -O binary "$<" "$@"

size: $(ELF)
	$(SIZE) "$<"

disasm: $(ELF)
	$(OBJDUMP) -h -S "$<" > "$(LISTING)"

check-flash:
	@$(PYTHON) $(HELPER) check --tool "$(STFLASH)"

flash: $(BIN) check-flash
	$(STFLASH) --reset write "$<" $(FLASH_ADDR)

reset: check-flash
	$(STFLASH) reset

debug-server:
	@$(PYTHON) $(HELPER) check --tool "$(STUTIL)"
	$(STUTIL) -p $(GDB_PORT)

debug: $(ELF)
	@$(PYTHON) $(HELPER) check --tool "$(GDB)"
	$(GDB) "$(ELF)" -ex "target extended-remote $(GDB_HOST):$(GDB_PORT)" -ex "monitor reset" -ex "tbreak main"

sources:
	@$(PYTHON) $(HELPER) show $(foreach f,$(C_SOURCES) $(ASM_SOURCES),--entry "$(f)")

print-config:
	@$(PYTHON) $(HELPER) show --entry "TARGET=$(TARGET)" --entry "CPU=$(CPU)" --entry "CC=$(CC)" --entry "FREERTOS_PORT=$(FREERTOS_PORT)" --entry "FREERTOS_HEAP=$(FREERTOS_HEAP)" --entry "LINKER_SCRIPT=$(LINKER_SCRIPT)" --entry "INCLUDES=$(ALL_INCLUDES)" --entry "STFLASH=$(STFLASH)" --entry "STUTIL=$(STUTIL)" --entry "FLASH_ADDR=$(FLASH_ADDR)"

clean:
	@$(PYTHON) $(HELPER) clean

rebuild:
	"$(MAKE)" clean
	"$(MAKE)" all

FORCE:
-include $(DEPS)
