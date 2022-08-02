# Include ubxlib src and inc
UBXLIB_BASE ?= $(realpath $(MAKEFILE_PATH)/../../../../../..)
UBXLIB_FEATURES = cell gnss short_range

# ubxlib.mk will define the following for us:
# UBXLIB_INC
# UBXLIB_PRIVATE_INC
# UBXLIB_SRC
# UBXLIB_TEST_SRC
# UBXLIB_TEST_INC
include $(UBXLIB_BASE)/port/ubxlib.mk

PLATFORM_PATH = $(UBXLIB_BASE)/platform/stm32cube/mcu/stm32f4

# Ubxlib port
UBXLIB_SRC += \
	$(UBXLIB_BASE)/port/clib/u_port_clib_mktime64.c \
	$(UBXLIB_BASE)/port/platform/common/mbedtls/u_port_crypto.c \
	$(PLATFORM_PATH)/src/u_port_debug.c \
	$(PLATFORM_PATH)/src/u_port_gpio.c \
	$(PLATFORM_PATH)/src/u_port_os.c \
	$(PLATFORM_PATH)/src/u_port_private.c \
	$(PLATFORM_PATH)/src/u_port_uart.c \
	$(PLATFORM_PATH)/src/u_port_i2c.c \
	$(PLATFORM_PATH)/src/u_port.c \
	$(PLATFORM_PATH)/src/heap_useNewlib.c

UBXLIB_ASM += \
	$(PLATFORM_PATH)/src/startup_stm32f437vgtx.s

UBXLIB_INC += \
	$(UBXLIB_PRIVATE_INC) \
	$(UBXLIB_BASE)/port/clib \
	$(PLATFORM_PATH)/src \
	$(PLATFORM_PATH)

