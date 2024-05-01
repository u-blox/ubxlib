# Switch to STM32U5 family
STM32_MCU_FAMILY = stm32u5

# Switch to startup file for STM32U575
STARTUP_ASSEMBLER_FILE = startup_stm32u575zitxq.s

# Include the main STM32 make file
include $(realpath $(MAKEFILE_PATH)/../..)/stm32.mk

# Add files specific to the STM32U5 way of working
UBXLIB_SRC += \
	$(PLATFORM_PATH)/src/u_port_os_pure_cmsis.c \
	$(PLATFORM_PATH)/src/u_port_clib.c \
	$(PLATFORM_PATH)/src/i2c_timing_utility.c
