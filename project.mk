# Board: STM32F411CE, 512 KiB Flash, 128 KiB RAM, ST-Link.
# Paths are project-relative, with forward slashes and no spaces.
TARGET ?= firmware
CPU ?= cortex-m4
FLOAT_ABI ?= hard
FPU ?= fpv4-sp-d16
OPT ?= -Og

PLATFORM_DIR ?= platform/stm32f411ce
LINKER_SCRIPT ?= $(PLATFORM_DIR)/stm32f411ce.ld
STARTUP_SOURCES ?= middlewares/CMSIS-F411/startup_stm32f411xe.s
FREERTOS_PORT ?= GCC/ARM_CM4F
FREERTOS_HEAP ?= 4

# Sources below these directories are found recursively.
SOURCE_DIRS = bsp hal/src app/src $(PLATFORM_DIR)
INCLUDE_DIRS = . bsp hal/inc app/inc $(PLATFORM_DIR)

# Official F411 CMSIS headers and startup; no F1 vendor drivers are built.
CMSIS_DIR = middlewares/CMSIS-F411
EXTRA_C_SOURCES =
EXTRA_ASM_SOURCES =
EXTRA_INCLUDE_DIRS = $(CMSIS_DIR)/Core $(CMSIS_DIR)/Device
EXCLUDE_SOURCES =
DEFINES = STM32F411xE
EXTRA_CFLAGS =
EXTRA_LDFLAGS =

TOOLCHAIN_PREFIX ?= arm-none-eabi-
STFLASH ?= st-flash
STUTIL ?= st-util
FLASH_ADDR ?= 0x08000000
GDB_HOST ?= localhost
GDB_PORT ?= 3333
