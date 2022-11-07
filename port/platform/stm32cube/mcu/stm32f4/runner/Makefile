TARGET = runner.elf

OUTPUT_DIRECTORY ?= output

ifeq ($(OS),Windows_NT)
# Force Windows as otherise, if sh.exe is in the path, make
# will use it and mess up on slashes/brackets in file paths 
SHELL = cmd
endif

# Commands
RM = rm
ifdef ARM_GCC_TOOLCHAIN_PATH
CC = $(ARM_GCC_TOOLCHAIN_PATH)/arm-none-eabi-gcc
else
CC = arm-none-eabi-gcc
endif

ifeq ($(OS),Windows_NT)
mkdir = mkdir $(subst /,\,$(1)) > nul 2>&1 || (exit 0)
else
mkdir = mkdir -p $(1)
endif

# Comment out line below to show all command output
SILENT = @

# Get absolute path of this Makefile
MAKEFILE_PATH := $(realpath $(subst \,/,$(dir $(lastword $(MAKEFILE_LIST)))))

# Include ubxlib stm32f4 port
UBXLIB_BASE ?= $(realpath $(MAKEFILE_PATH)/../../../../../..)
# stm32f4.mk will define the following for us:
# UBXLIB_INC
# UBXLIB_SRC
# UBXLIB_TEST_SRC
# UBXLIB_TEST_INC
include ../stm32f4.mk

define USAGE_MESSAGE
You must set the following variables in order to build:
	STM32CUBE_FW_PATH: Set this to the path of STM32CubeF4 (https://github.com/STMicroelectronics/STM32CubeF4.git)
	UNITY_PATH:        Set this to the path of Unity (https://github.com/ThrowTheSwitch/Unity)
You my also need to set this to point to your ARM GCC bin directory if arm-none-eabi-gcc is not on the path
	ARM_GCC_TOOLCHAIN_PATH
endef

ifndef STM32CUBE_FW_PATH
$(error $(USAGE_MESSAGE))
endif

ifndef UNITY_PATH
$(error $(USAGE_MESSAGE))
endif

$(info STM32CUBE_FW_PATH: $(STM32CUBE_FW_PATH))
$(info UNITY_PATH:        $(UNITY_PATH))

OBJ_DIR = $(OUTPUT_DIRECTORY)/obj
PLATFORM_PATH = $(realpath ../../..)
STM32_HAL_PATH = $(STM32CUBE_FW_PATH)/Drivers/STM32F4xx_HAL_Driver
FREERTOS_PATH = $(STM32CUBE_FW_PATH)/Middlewares/Third_Party/FreeRTOS
MBEDTLS_PATH = $(STM32CUBE_FW_PATH)/Middlewares/Third_Party/mbedTLS

# Compiler flags
DEFINES += \
	-DUSE_HAL_DRIVER \
	-DUSE_FULL_LL_DRIVER \
	-DSTM32F437xx \
	-DDEBUG \
	-DMBEDTLS_USER_CONFIG_FILE=\"user_mbedtls_config.h\"

override CFLAGS += \
	$(DEFINES) -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -std=gnu11 -g3 -O0 -Wall \
	-ffunction-sections -fdata-sections -fstack-usage --specs=nano.specs -MMD -MP $(U_FLAGS)

override LDFLAGS += \
	-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -static -T"$(PLATFORM_PATH)/app/STM32F437VGTX_FLASH.ld" \
	-Wl,-Map="$(OUTPUT_DIRECTORY)/runner.map" -Wl,--gc-sections -Wl,--wrap=malloc -Wl,--wrap=_malloc_r -Wl,--wrap=calloc \
	-Wl,--wrap=_calloc_r -Wl,--wrap=realloc -Wl,--wrap=_realloc_r --specs=nano.specs -Wl,--start-group -lc -lm -Wl,--end-group \
	-Wl,--undefined=uxTopUsedPriority


# STM32 HAL
STM32CUBE_FW_SRC += \
	$(STM32_HAL_PATH)/Src/stm32f4xx_hal.c \
	$(STM32_HAL_PATH)/Src/stm32f4xx_hal_cortex.c \
	$(STM32_HAL_PATH)/Src/stm32f4xx_hal_dma.c \
	$(STM32_HAL_PATH)/Src/stm32f4xx_hal_dma_ex.c \
	$(STM32_HAL_PATH)/Src/stm32f4xx_hal_exti.c \
	$(STM32_HAL_PATH)/Src/stm32f4xx_hal_flash.c \
	$(STM32_HAL_PATH)/Src/stm32f4xx_hal_flash_ex.c \
	$(STM32_HAL_PATH)/Src/stm32f4xx_hal_flash_ramfunc.c \
	$(STM32_HAL_PATH)/Src/stm32f4xx_hal_gpio.c \
	$(STM32_HAL_PATH)/Src/stm32f4xx_hal_pwr.c \
	$(STM32_HAL_PATH)/Src/stm32f4xx_hal_pwr_ex.c \
	$(STM32_HAL_PATH)/Src/stm32f4xx_hal_rcc.c \
	$(STM32_HAL_PATH)/Src/stm32f4xx_hal_rcc_ex.c \
	$(STM32_HAL_PATH)/Src/stm32f4xx_hal_tim.c \
	$(STM32_HAL_PATH)/Src/stm32f4xx_hal_tim_ex.c \
	$(STM32_HAL_PATH)/Src/stm32f4xx_ll_rcc.c \
	$(STM32_HAL_PATH)/Src/stm32f4xx_ll_gpio.c \
	$(STM32_HAL_PATH)/Src/stm32f4xx_ll_dma.c \
	$(STM32_HAL_PATH)/Src/stm32f4xx_ll_usart.c

STM32CUBE_FW_INC += \
	$(STM32_HAL_PATH)/Inc/Legacy \
	$(STM32_HAL_PATH)/Inc \
	$(STM32CUBE_FW_PATH)/Drivers/CMSIS/Include \
	$(STM32CUBE_FW_PATH)/Drivers/CMSIS/Device/ST/STM32F4xx/Include

# FreeRTOS
STM32CUBE_FW_SRC += \
	$(FREERTOS_PATH)/Source/croutine.c \
	$(FREERTOS_PATH)/Source/list.c \
	$(FREERTOS_PATH)/Source/queue.c \
	$(FREERTOS_PATH)/Source/tasks.c \
	$(FREERTOS_PATH)/Source/timers.c \
	$(FREERTOS_PATH)/Source/CMSIS_RTOS/cmsis_os.c \
	$(FREERTOS_PATH)/Source/portable/GCC/ARM_CM4F/port.c

STM32CUBE_FW_INC += \
	$(FREERTOS_PATH)/Source/include \
	$(FREERTOS_PATH)/Source/portable/GCC/ARM_CM4F \
	$(FREERTOS_PATH)/Source/CMSIS_RTOS

# mbedTLS
STM32CUBE_FW_SRC += \
	$(MBEDTLS_PATH)/library/sha256.c \
	$(MBEDTLS_PATH)/library/md.c \
	$(MBEDTLS_PATH)/library/md_wrap.c \
	$(MBEDTLS_PATH)/library/md5.c \
	$(MBEDTLS_PATH)/library/aes.c \
	$(MBEDTLS_PATH)/library/platform_util.c

STM32CUBE_FW_INC += \
	$(MBEDTLS_PATH)/include

# Unity
UNITY_SRC = \
	$(UNITY_PATH)/src/unity.c

UNITY_INC = \
	$(UNITY_PATH)/src

# Ubxlib port

UBXLIB_ASM += \
	$(PLATFORM_PATH)/src/startup_stm32f437vgtx.s

UBXLIB_INC += \
	$(STM32CUBE_FW_INC) \
	$(UBXLIB_PRIVATE_INC) \
	$(UBXLIB_BASE)/port/clib \
	$(PLATFORM_PATH)/src \
	$(PLATFORM_PATH)/inc

# App
UBXLIB_SRC += \
	$(PLATFORM_PATH)/src/stm32f4xx_hal_msp.c \
	$(PLATFORM_PATH)/src/u_exception_handler.c \
	$(PLATFORM_PATH)/src/syscalls.c \
	$(PLATFORM_PATH)/src/system_stm32f4xx.c \
	$(UBXLIB_TEST_SRC) \
	$(UBXLIB_BASE)/port/platform/common/heap_check/u_heap_check.c \
	$(PLATFORM_PATH)/app/u_main.c

UBXLIB_INC += \
	$(UBXLIB_TEST_INC) \
	$(UNITY_INC) \
	../cfg \
	$(UBXLIB_BASE)/port/platform/common/heap_check \
	$(UBXLIB_BASE)/port/platform/common/runner

UBXLIB_ASM += \
	$(PLATFORM_PATH)/src/startup_stm32f437vgtx.s


INC := $(UBXLIB_INC:%=-I%)

OBJ += $(UBXLIB_SRC:$(UBXLIB_BASE)/%.c=$(OBJ_DIR)/ubxlib/%.o)
OBJ += $(STM32CUBE_FW_SRC:$(STM32CUBE_FW_PATH)/%.c=$(OBJ_DIR)/stm32cube_fw/%.o)
OBJ += $(UNITY_SRC:$(UNITY_PATH)/%.c=$(OBJ_DIR)/unity/%.o)

ASM_OBJ += $(UBXLIB_ASM:$(UBXLIB_BASE)/%.s=$(OBJ_DIR)/ubxlib/%.o)


.PHONY: clean

all: $(OUTPUT_DIRECTORY)/$(TARGET)

clean:
	$(RM) -rf $(OUTPUT_DIRECTORY)

# ubxlib ASM files
$(OBJ_DIR)/ubxlib/%.o: $(UBXLIB_BASE)/%.s
	$(SILENT)$(call mkdir,$(@D))
	@echo CC $<
	$(SILENT)$(CC) -c -o $@ $< $(CFLAGS) $(INC)

# ubxlib C files
$(OBJ_DIR)/ubxlib/%.o: $(UBXLIB_BASE)/%.c
	$(SILENT)$(call mkdir,$(@D))
	@echo CC $<
	$(SILENT)$(CC) -c -o $@ $< $(CFLAGS) $(INC)

# STM32CubeFW C files
$(OBJ_DIR)/stm32cube_fw/%.o: $(STM32CUBE_FW_PATH)/%.c
	$(SILENT)$(call mkdir,$(@D))
	@echo CC $<
	$(SILENT)$(CC) -c -o $@ $< $(CFLAGS) $(INC)

# Unity C files
$(OBJ_DIR)/unity/%.o: $(UNITY_PATH)/%.c
	$(SILENT)$(call mkdir,$(@D))
	@echo CC $<
	$(SILENT)$(CC) -c -o $@ $< $(CFLAGS) $(INC)

# Linker
$(OUTPUT_DIRECTORY)/$(TARGET): $(OBJ) $(ASM_OBJ)
	$(SILENT)$(call mkdir,$(@D))
	@echo Linking $@
	$(SILENT)$(CC) -o $@ $^ $(LDFLAGS)

