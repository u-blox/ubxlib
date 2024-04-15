TARGET = runner.elf

OUTPUT_DIRECTORY ?= output

CMSIS_DEVICE_HEADER ?= stm32u575xx.h

ifeq ($(OS),Windows_NT)
# Force Windows as otherise, if sh.exe is in the path, make
# will use it and mess up on slashes/brackets in file paths 
SHELL = cmd
endif

# Commands
RM = rm
ifdef ARM_GCC_TOOLCHAIN_PATH
CC = $(ARM_GCC_TOOLCHAIN_PATH)/arm-none-eabi-gcc
AR = $(ARM_GCC_TOOLCHAIN_PATH)/arm-none-eabi-ar
else
CC = arm-none-eabi-gcc
AR = arm-none-eabi-ar
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

# Include ubxlib stm32u5 port
UBXLIB_BASE ?= $(realpath $(MAKEFILE_PATH)/../../../../../..)
# stm32u5.mk will define the following for us:
# UBXLIB_INC
# UBXLIB_SRC
# UBXLIB_TEST_SRC
# UBXLIB_TEST_INC
include ../stm32u5.mk

define USAGE_MESSAGE
You must set the following variables in order to build:
	STM32_CMSIS_FREERTOS_PATH: Set this to the path of ST's CMSIS FreeRTOS (https://github.com/STMicroelectronics/x-cube-freertos.git)
	UNITY_PATH:                Set this to the path of Unity (https://github.com/ThrowTheSwitch/Unity)
You may also need to set this to point to your ARM GCC bin directory if arm-none-eabi-gcc is not on the path
	ARM_GCC_TOOLCHAIN_PATH
endef

ifndef STM32_CMSIS_FREERTOS_PATH
$(error $(USAGE_MESSAGE))
endif

ifndef UNITY_PATH
$(error $(USAGE_MESSAGE))
endif

$(info STM32_CMSIS_FREERTOS_PATH: $(STM32_CMSIS_FREERTOS_PATH))
$(info UNITY_PATH:                $(UNITY_PATH))
$(info CMSIS_DEVICE_HEADER:       $(CMSIS_DEVICE_HEADER))

# The user may specify STM32CUBE_FW_PATH in order to pick up the STM32
# HAL files from a different repo (e.g. the main STM32CubeU5 one);
# if they do not then pick the files up from the CMSIS FreeRTOS path.
ifdef STM32CUBE_FW_PATH
$(info STM32CUBE_FW_PATH          $(STM32CUBE_FW_PATH))
else
STM32CUBE_FW_PATH = $(STM32_CMSIS_FREERTOS_PATH)
$(info STM32CUBE_FW_PATH          The same as STM32_CMSIS_FREERTOS_PATH)
endif

OBJ_DIR = $(OUTPUT_DIRECTORY)/obj
PLATFORM_PATH = $(realpath ../../..)
FREERTOS_PATH = $(STM32_CMSIS_FREERTOS_PATH)/Middlewares/Third_Party/FreeRTOS
STM32CUBE_HAL_PATH = $(STM32CUBE_FW_PATH)/Drivers/STM32U5xx_HAL_Driver

# Compiler flags
DEFINES += \
	-DUSE_HAL_DRIVER \
	-DUSE_FULL_LL_DRIVER \
	-DSTM32U575xx \
	-DDEBUG \
	-DU_PORT_STM32_PURE_CMSIS \
	-DU_PORT_STM32_CMSIS_ON_FREERTOS \
	-DCMSIS_V2 \
	-DCMSIS_device_header=\"${CMSIS_DEVICE_HEADER}\"

override CFLAGS += \
	$(DEFINES) -mcpu=cortex-m33 -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -std=gnu11 -g3 -O0 -Wall \
	-ffunction-sections -fdata-sections -fstack-usage --specs=nano.specs -MMD -MP $(U_FLAGS)

# Note: the last line is to let the linker know that we are using FreeRTOS and
# so don't need the ThreadX memory section
override LDFLAGS += \
	-debug -mcpu=cortex-m33 -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -static -T"$(PLATFORM_PATH)/app/STM32U575ZITX_FLASH.ld" \
	-Wl,-Map="$(OUTPUT_DIRECTORY)/runner.map" -Wl,--gc-sections -Wl,--wrap=malloc -Wl,--wrap=_malloc_r -Wl,--wrap=calloc \
	-Wl,--wrap=_calloc_r -Wl,--wrap=realloc -Wl,--wrap=_realloc_r --specs=nano.specs -Wl,--start-group -lc -lm -Wl,--end-group

# Include STM32 common files; this requires STM32CUBE_HAL_PATH and will give us the
# common parts of STM32CUBE_FW_SRC and STM32CUBE_FW_INC (i.e. the HAL etc.)
include ../stm32cube_common.mk

# Add the LP UART, which is only on STM32U5
STM32CUBE_FW_SRC += \
	$(STM32CUBE_HAL_PATH)/Src/stm32u5xx_ll_lpuart.c

# CMSIS FreeRTOS
FREERTOS_SRC += \
	$(FREERTOS_PATH)/Source/croutine.c \
	$(FREERTOS_PATH)/Source/list.c \
	$(FREERTOS_PATH)/Source/queue.c \
	$(FREERTOS_PATH)/Source/tasks.c \
	$(FREERTOS_PATH)/Source/timers.c \
	$(FREERTOS_PATH)/Source/portable/GCC/ARM_CM33_NTZ/non_secure/port.c \
	$(FREERTOS_PATH)/Source/portable/GCC/ARM_CM33_NTZ/non_secure/portasm.c \
	$(FREERTOS_PATH)/Source/CMSIS_RTOS_V2/cmsis_os2.c

FREERTOS_INC += \
	$(FREERTOS_PATH)/Source/include \
	$(FREERTOS_PATH)/Source/portable/GCC/ARM_CM33_NTZ/non_secure \
	$(STM32CUBE_FW_PATH)/CMSIS/RTOS2/FreeRTOS/Include \
	$(STM32CUBE_FW_PATH)/Drivers/CMSIS/Include \
	$(STM32CUBE_FW_PATH)/Drivers/CMSIS/Device/ST/STM32U5xx/Include \
	$(STM32CUBE_FW_PATH)/Drivers/CMSIS/RTOS/Template \
	$(STM32CUBE_FW_PATH)/Drivers/CMSIS/RTOS2/Include

# Unity
UNITY_SRC = \
	$(UNITY_PATH)/src/unity.c

UNITY_INC = \
	$(UNITY_PATH)/src

# Ubxlib port
UBXLIB_INC += \
	$(STM32CUBE_FW_INC) \
	$(FREERTOS_INC) \
	$(UBXLIB_PRIVATE_INC) \
	$(UBXLIB_BASE)/port/clib \
	$(PLATFORM_PATH)/src \
	$(PLATFORM_PATH)/inc

# App
APP_SRC += \
	$(PLATFORM_PATH)/src/stm32u5xx_hal_msp.c \
	$(PLATFORM_PATH)/src/u_exception_handler.c \
	$(PLATFORM_PATH)/src/syscalls.c \
	$(PLATFORM_PATH)/src/heap_useNewlib.c \
	$(PLATFORM_PATH)/src/system_stm32u5xx.c \
	$(UBXLIB_TEST_SRC) \
	$(UBXLIB_BASE)/port/platform/common/heap_check/u_heap_check.c \
	$(PLATFORM_PATH)/app/u_main.c

APP_ASM += \
	$(PLATFORM_PATH)/src/startup_stm32u575zitxq.S

UBXLIB_INC += \
	$(UBXLIB_TEST_INC) \
	$(UNITY_INC) \
	../cfg \
	$(UBXLIB_BASE)/port/platform/common/heap_check \
	$(UBXLIB_BASE)/port/platform/common/runner

INC := $(UBXLIB_INC:%=-I%)

UBXLIB_OBJ += $(UBXLIB_SRC:$(UBXLIB_BASE)/%.c=$(OBJ_DIR)/ubxlib/%.o)
STM32_OBJ += $(STM32CUBE_FW_SRC:$(STM32CUBE_FW_PATH)/%.c=$(OBJ_DIR)/stm32cube_fw/%.o)
FREERTOS_OBJ += $(FREERTOS_SRC:$(FREERTOS_PATH)/%.c=$(OBJ_DIR)/freertos/%.o)
UNITY_OBJ += $(UNITY_SRC:$(UNITY_PATH)/%.c=$(OBJ_DIR)/unity/%.o)
APP_OBJ += $(APP_SRC:$(UBXLIB_BASE)/%.c=$(OBJ_DIR)/app/%.o)
APP_ASM_OBJ += $(APP_ASM:$(UBXLIB_BASE)/%.S=$(OBJ_DIR)/app/%.o)

.PHONY: clean

all: $(OUTPUT_DIRECTORY)/$(TARGET)

clean:
	$(RM) -rf $(OUTPUT_DIRECTORY)

# ubxlib C files
$(OBJ_DIR)/ubxlib/%.o: $(UBXLIB_BASE)/%.c
	$(SILENT)$(call mkdir,$(@D))
	@echo CC $<
	$(SILENT)$(CC) -c -o $@ $< $(CFLAGS) $(INC)

# Library of the above (this helps reduce the command-line length for the linker to below 32k characters)
$(OBJ_DIR)/libubxlib.a: $(UBXLIB_OBJ)
	$(SILENT)$(call mkdir,$(@D))
	@echo Library $@
	$(SILENT)$(AR) r $@ $^

# STM32CubeFW ASM files
$(OBJ_DIR)/stm32cube_fw/%.o: $(STM32CUBE_FW_PATH)/%.S
	$(SILENT)$(call mkdir,$(@D))
	@echo CC $<
	$(SILENT)$(CC) -c -o $@ $< $(CFLAGS) $(INC)

# STM32CubeFW C files
$(OBJ_DIR)/stm32cube_fw/%.o: $(STM32CUBE_FW_PATH)/%.c
	$(SILENT)$(call mkdir,$(@D))
	@echo CC $<
	$(SILENT)$(CC) -c -o $@ $< $(CFLAGS) $(INC)

# Library of the above (this helps reduce the command-line length for the linker to below 32k characters)
$(OBJ_DIR)/libstm32cube_fw.a: $(STM32_OBJ)
	$(SILENT)$(call mkdir,$(@D))
	@echo Library $@
	$(SILENT)$(AR) r $@ $^

# CMSIS FreeRTOS files
$(OBJ_DIR)/freertos/%.o: $(FREERTOS_PATH)/%.c
	$(SILENT)$(call mkdir,$(@D))
	@echo CC $<
	$(SILENT)$(CC) -c -o $@ $< $(CFLAGS) $(INC)

# Library of the above
$(OBJ_DIR)/libfreertos.a: $(FREERTOS_OBJ)
	$(SILENT)$(call mkdir,$(@D))
	@echo Library $@
	$(SILENT)$(AR) r $@ $^

# Unity C files
$(OBJ_DIR)/unity/%.o: $(UNITY_PATH)/%.c
	$(SILENT)$(call mkdir,$(@D))
	@echo CC $<
	$(SILENT)$(CC) -c -o $@ $< $(CFLAGS) $(INC)

# Library of the above
$(OBJ_DIR)/libunity.a: $(UNITY_OBJ)
	$(SILENT)$(call mkdir,$(@D))
	@echo Library $@
	$(SILENT)$(AR) r $@ $^

# App ASM files
$(OBJ_DIR)/app/%.o: $(UBXLIB_BASE)/%.S
	$(SILENT)$(call mkdir,$(@D))
	@echo CC $<
	$(SILENT)$(CC) -c -o $@ $< $(CFLAGS) $(INC)

# App C files
$(OBJ_DIR)/app/%.o: $(UBXLIB_BASE)/%.c
	$(SILENT)$(call mkdir,$(@D))
	@echo CC $<
	$(SILENT)$(CC) -c -o $@ $< $(CFLAGS) $(INC)

# Linker
$(OUTPUT_DIRECTORY)/$(TARGET): $(APP_OBJ) $(APP_ASM_OBJ) $(OBJ_DIR)/libubxlib.a $(OBJ_DIR)/libunity.a $(OBJ_DIR)/libstm32cube_fw.a $(OBJ_DIR)/libfreertos.a
	$(SILENT)$(call mkdir,$(@D))
	@echo Linking $@
	$(SILENT)$(CC) -o $@ $^ $(LDFLAGS)
