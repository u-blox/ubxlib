# Include the main STM32 make file, the default is F4
include $(realpath $(MAKEFILE_PATH)/../..)/stm32.mk

# Add files specific to the STM32F4 way of working
UBXLIB_SRC += \
	$(PLATFORM_PATH)/src/u_port_os.c \
	$(PLATFORM_PATH)/src/heap_useNewlib.c
