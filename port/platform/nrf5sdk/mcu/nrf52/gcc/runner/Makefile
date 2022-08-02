PROJECT_NAME     := u_pca10056
TARGETS          := nrf52840_xxaa

# Note "realpath" in the line below to remove the last trailing slash for neatness
MAKEFILE_DIR := $(realpath $(subst \,/,$(dir $(lastword $(MAKEFILE_LIST)))))
UBXLIB_PATH ?= $(MAKEFILE_DIR)/../../../../../../..
NRF5_PORT_PATH ?= $(MAKEFILE_DIR)/../../../..
OUTPUT_DIRECTORY ?= _build
UNITY_PATH ?= $(UBXLIB_PATH)/../Unity
NRF5_PATH ?= $(UBXLIB_PATH)/../nrf5
JLINK_SERIAL_NUMBER := 

# This is picked up in gcc_startup_nrf52840.S to
# set the heap size.  If you modify this value then
# make sure that the value of configTOTAL_HEAP_SIZE
# in FreeRTOSConfig.h is modified to match
# (and in the SES SDK you'll find a variable
# "arm_linker_heap_size" nestling in the .emProject
# file, if you want to align the two SDKs).
HEAP_SIZE        := 51200

$(info    MAKEFILE_DIR is "$(MAKEFILE_DIR)")
$(info    OUTPUT_DIRECTORY will be "$(OUTPUT_DIRECTORY)")
$(info    UNITY_PATH will be "$(UNITY_PATH)")
$(info    NRF5_PATH will be "$(NRF5_PATH)")
$(info    UBXLIB_PATH will be $(UBXLIB_PATH))
$(info    JLINK_SERIAL_NUMBER will be "$(JLINK_SERIAL_NUMBER)")
$(info    HEAP_SIZE will be $(HEAP_SIZE))
ifneq ($(strip $(CFLAGS)),)
$(info    CFLAGS will start with $(CFLAGS))
endif
ifneq ($(strip $(JLINK_SERIAL_NUMBER)),)
JLINK_SERIAL_NUMBER_SWITCH := -s $(JLINK_SERIAL_NUMBER)
endif
$(info    GNU_INSTALL_ROOT will be "$(GNU_INSTALL_ROOT)")
$(info    GNU_VERSION will be $(GNU_VERSION))
$(info    GNU_PREFIX will be $(GNU_PREFIX))

$(OUTPUT_DIRECTORY)/nrf52840_xxaa.out: \
  LINKER_SCRIPT  := $(MAKEFILE_DIR)/uart_gcc_nrf52.ld

# Include ubxlib src and inc
UBXLIB_BASE = $(UBXLIB_PATH)
UBXLIB_FEATURES = cell gnss short_range
# ubxlib.mk will define UBXLIB_INC, UBXLIB_PRIVATE_INC and UBXLIB_SRC for us
include $(UBXLIB_PATH)/port/ubxlib.mk

# Source files common to all targets
SRC_FILES += \
  $(NRF5_PATH)/modules/nrfx/mdk/gcc_startup_nrf52840.S \
  $(NRF5_PATH)/modules/nrfx/mdk/system_nrf52840.c \
  $(NRF5_PATH)/modules/nrfx/soc/nrfx_atomic.c \
  $(NRF5_PATH)/components/boards/boards.c \
  $(NRF5_PATH)/components/libraries/util/app_error.c \
  $(NRF5_PATH)/components/libraries/util/app_error_handler_gcc.c \
  $(NRF5_PATH)/components/libraries/util/app_error_weak.c \
  $(NRF5_PATH)/components/libraries/util/app_util_platform.c \
  $(NRF5_PATH)/components/libraries/util/nrf_assert.c \
  $(NRF5_PATH)/components/libraries/atomic/nrf_atomic.c \
  $(NRF5_PATH)/components/libraries/balloc/nrf_balloc.c \
  $(NRF5_PATH)/components/libraries/memobj/nrf_memobj.c \
  $(NRF5_PATH)/components/libraries/ringbuf/nrf_ringbuf.c \
  $(NRF5_PATH)/components/libraries/strerror/nrf_strerror.c \
  $(NRF5_PATH)/components/libraries/uart/retarget.c \
  $(NRF5_PATH)/modules/nrfx/drivers/src/prs/nrfx_prs.c \
  $(NRF5_PATH)/modules/nrfx/drivers/src/nrfx_ppi.c \
  $(NRF5_PATH)/modules/nrfx/drivers/src/nrfx_timer.c \
  $(NRF5_PATH)/modules/nrfx/drivers/src/nrfx_clock.c \
  $(NRF5_PATH)/modules/nrfx/drivers/src/nrfx_twim.c \
  $(NRF5_PATH)/external/fprintf/nrf_fprintf.c \
  $(NRF5_PATH)/external/fprintf/nrf_fprintf_format.c \
  $(NRF5_PATH)/integration/nrfx/legacy/nrf_drv_clock.c \
  $(NRF5_PATH)/components/libraries/log/src/nrf_log_str_formatter.c \
  $(NRF5_PATH)/components/libraries/hardfault/nrf52/handler/hardfault_handler_gcc.c \
  $(NRF5_PATH)/components/libraries/hardfault/hardfault_implementation.c \
  $(NRF5_PATH)/external/freertos/source/croutine.c \
  $(NRF5_PATH)/external/freertos/source/event_groups.c \
  $(NRF5_PATH)/external/freertos/source/list.c \
  $(NRF5_PATH)/external/freertos/portable/GCC/nrf52/port.c \
  $(NRF5_PATH)/external/freertos/portable/CMSIS/nrf52/port_cmsis.c \
  $(NRF5_PATH)/external/freertos/portable/CMSIS/nrf52/port_cmsis_systick.c \
  $(NRF5_PATH)/external/freertos/source/queue.c \
  $(NRF5_PATH)/external/freertos/source/stream_buffer.c \
  $(NRF5_PATH)/external/freertos/source/tasks.c \
  $(NRF5_PATH)/external/freertos/source/timers.c \
  $(NRF5_PATH)/external/mbedtls/library/sha256.c \
  $(NRF5_PATH)/external/mbedtls/library/md.c \
  $(NRF5_PATH)/external/mbedtls/library/md_wrap.c \
  $(NRF5_PATH)/external/mbedtls/library/md5.c \
  $(NRF5_PATH)/external/mbedtls/library/aes.c \
  $(UBXLIB_SRC) \
  $(UBXLIB_TEST_SRC) \
  $(UBXLIB_PATH)/port/clib/u_port_clib_mktime64.c \
  $(UBXLIB_PATH)/port/platform/common/heap_check/u_heap_check.c \
  ${UBXLIB_BASE}/port/platform/common/mbedtls/u_port_crypto.c \
  $(NRF5_PORT_PATH)/src/u_port.c \
  $(NRF5_PORT_PATH)/src/u_port_debug.c \
  $(NRF5_PORT_PATH)/src/u_port_gpio.c \
  $(NRF5_PORT_PATH)/src/u_port_os.c \
  $(NRF5_PORT_PATH)/src/u_port_uart.c \
  $(NRF5_PORT_PATH)/src/u_port_i2c.c \
  $(NRF5_PORT_PATH)/src/u_port_private.c \
  $(NRF5_PORT_PATH)/src/u_exception_handler.c \
  $(NRF5_PORT_PATH)/src/heap_useNewlib.c \
  $(NRF5_PORT_PATH)/src/mbedtls_platform_zeroize.c \
  $(NRF5_PORT_PATH)/src/segger_rtt/RTT/SEGGER_RTT.c \
  $(NRF5_PORT_PATH)/src/segger_rtt/RTT/SEGGER_RTT_printf.c \
  $(NRF5_PORT_PATH)/src/segger_rtt/Syscalls/SEGGER_RTT_Syscalls_GCC.c \
  $(NRF5_PORT_PATH)/app/u_main.c \
  $(UNITY_PATH)/src/unity.c \

# Include folders common to all targets
INC_FOLDERS += \
  $(MAKEFILE_DIR)/../../cfg \
  $(MAKEFILE_DIR)/. \
  $(NRF5_PATH)/components \
  $(NRF5_PATH)/modules/nrfx/mdk \
  $(NRF5_PATH)/components/libraries/fifo \
  $(NRF5_PATH)/components/libraries/strerror \
  $(NRF5_PATH)/components/toolchain/cmsis/include \
  $(NRF5_PATH)/external/freertos/source/include \
  $(NRF5_PATH)/external/freertos/config \
  $(NRF5_PATH)/components/libraries/util \
  $(NRF5_PATH)/components/libraries/balloc \
  $(NRF5_PATH)/components/libraries/ringbuf \
  $(NRF5_PATH)/modules/nrfx/hal \
  $(NRF5_PATH)/components/libraries/bsp \
  $(NRF5_PATH)/components/libraries/uart \
  $(NRF5_PATH)/components/libraries/log \
  $(NRF5_PATH)/modules/nrfx \
  $(NRF5_PATH)/components/libraries/experimental_section_vars \
  $(NRF5_PATH)/integration/nrfx/legacy \
  $(NRF5_PATH)/external/freertos/portable/CMSIS/nrf52 \
  $(NRF5_PATH)/components/libraries/delay \
  $(NRF5_PATH)/integration/nrfx \
  $(NRF5_PATH)/components/drivers_nrf/nrf_soc_nosd \
  $(NRF5_PATH)/components/libraries/atomic \
  $(NRF5_PATH)/components/boards \
  $(NRF5_PATH)/components/libraries/memobj \
  $(NRF5_PATH)/external/freertos/portable/GCC/nrf52 \
  $(NRF5_PATH)/modules/nrfx/drivers/include \
  $(NRF5_PATH)/external/fprintf \
  $(NRF5_PATH)/components/libraries/log/src \
  $(NRF5_PATH)/components/libraries/hardfault \
  $(NRF5_PATH)/external/mbedtls/include \
  $(UBXLIB_INC) \
  $(UBXLIB_PRIVATE_INC) \
  $(UBXLIB_TEST_INC) \
  $(UBXLIB_PATH)/port/clib \
  $(UBXLIB_PATH)/port/platform/common/heap_check \
  $(NRF5_PORT_PATH) \
  $(NRF5_PORT_PATH)/src \
  $(NRF5_PORT_PATH)/app \
  $(NRF5_PORT_PATH)/src/segger_rtt/RTT \
  $(UNITY_PATH)/src \

# Libraries common to all targets
LIB_FILES += \

# Optimization flags
OPT = -O3 -g3
# Uncomment the line below to enable link time optimization
#OPT += -flto

# C flags common to all targets
override CFLAGS += $(OPT)
override CFLAGS += -DBOARD_PCA10056
override CFLAGS += -DBSP_DEFINES_ONLY
override CFLAGS += -DCONFIG_GPIO_AS_PINRESET
override CFLAGS += -DFLOAT_ABI_HARD
override CFLAGS += -DNRF52840_XXAA
override CFLAGS += -DFREERTOS
override CFLAGS += -mcpu=cortex-m4
override CFLAGS += -mthumb -mabi=aapcs
override CFLAGS += -Wall -Werror
override CFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
# keep every function in a separate section, this allows linker to discard unused ones
override CFLAGS += -ffunction-sections -fdata-sections -fno-strict-aliasing
override CFLAGS += -fno-builtin -fshort-enums
override CFLAGS += -DUNITY_INCLUDE_CONFIG_H
override CFLAGS += -DDEBUG_NRF
override CFLAGS += -DMBEDTLS_USER_CONFIG_FILE=\"user_mbedtls_config.h\"

# C++ flags common to all targets
CXXFLAGS += $(OPT)

# Assembler flags common to all targets
ASMFLAGS += -g3
ASMFLAGS += -mcpu=cortex-m4
ASMFLAGS += -mthumb -mabi=aapcs
ASMFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
ASMFLAGS += -DBOARD_PCA10056
ASMFLAGS += -DBSP_DEFINES_ONLY
ASMFLAGS += -DCONFIG_GPIO_AS_PINRESET
ASMFLAGS += -DFLOAT_ABI_HARD
ASMFLAGS += -DFREERTOS
ASMFLAGS += -DNRF52840_XXAA
ASMFLAGS += -DDEBUG_NRF

# Linker flags
LDFLAGS += $(OPT)
LDFLAGS += -mthumb -mabi=aapcs -L$(NRF5_PATH)/modules/nrfx/mdk -T$(LINKER_SCRIPT)
LDFLAGS += -mcpu=cortex-m4
LDFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
# let linker dump unused sections
LDFLAGS += -Wl,--gc-sections
# use newlib in nano version
LDFLAGS += --specs=nano.specs
# wrap malloc for max heap usage tracking
LDFLAGS += -Wl,--wrap=malloc
LDFLAGS += -Wl,--wrap=_malloc_r
LDFLAGS += -Wl,--wrap=calloc
LDFLAGS += -Wl,--wrap=_calloc_r
LDFLAGS += -Wl,--wrap=realloc
LDFLAGS += -Wl,--wrap=_realloc_r

nrf52840_xxaa: CFLAGS += -D__HEAP_SIZE=$(HEAP_SIZE)
nrf52840_xxaa: CFLAGS += -D__STACK_SIZE=8192
nrf52840_xxaa: ASMFLAGS += -D__HEAP_SIZE=$(HEAP_SIZE)
nrf52840_xxaa: ASMFLAGS += -D__STACK_SIZE=8192

# Add standard libraries at the very end of the linker input, after all objects
# that may need symbols provided by these libraries.
LIB_FILES += -lc -lnosys -lm

.PHONY: default help

# Default target - first one defined
default: nrf52840_xxaa

# Print all targets that can be built
help:
	@echo following targets are available:
	@echo		nrf52840_xxaa
	@echo		flash      - flashing binary

TEMPLATE_PATH := $(NRF5_PATH)/components/toolchain/gcc


include $(TEMPLATE_PATH)/Makefile.common


$(foreach target, $(TARGETS), $(call define_target, $(target)))

.PHONY: flash erase

# Flash the program
flash: default
	nrfjprog -f nrf52 --program $(OUTPUT_DIRECTORY)/nrf52840_xxaa.hex --chiperase --verify -r $(JLINK_SERIAL_NUMBER_SWITCH)

erase:
	nrfjprog -f nrf52 --chiperase

SDK_CONFIG_FILE := $(MAKEFILE_DIR)/../../cfg/sdk_config.h
CMSIS_CONFIG_TOOL := $(NRF5_PATH)/external_tools/cmsisconfig/CMSIS_Configuration_Wizard.jar
sdk_config:
	java -jar $(CMSIS_CONFIG_TOOL) $(SDK_CONFIG_FILE)
