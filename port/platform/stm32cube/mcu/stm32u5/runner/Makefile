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
	STM32CUBE_FW_PATH:         Set this to the path of STM32CubeU5 (https://github.com/STMicroelectronics/STM32CubeU5.git)
	UNITY_PATH:                Set this to the path of Unity (https://github.com/ThrowTheSwitch/Unity)
You may also need to set this to point to your ARM GCC bin directory if arm-none-eabi-gcc is not on the path
	ARM_GCC_TOOLCHAIN_PATH
endef

ifndef STM32CUBE_FW_PATH
$(error $(USAGE_MESSAGE))
endif

ifndef UNITY_PATH
$(error $(USAGE_MESSAGE))
endif

$(info STM32CUBE_FW_PATH:         $(STM32CUBE_FW_PATH))
$(info UNITY_PATH:                $(UNITY_PATH))
$(info CMSIS_DEVICE_HEADER:       $(CMSIS_DEVICE_HEADER))

OBJ_DIR = $(OUTPUT_DIRECTORY)/obj
PLATFORM_PATH = $(realpath ../../..)
THREADX_PATH = $(STM32CUBE_FW_PATH)/Middlewares/ST/threadx
STM32CUBE_HAL_PATH = $(STM32CUBE_FW_PATH)/Drivers/STM32U5xx_HAL_Driver

# Compiler flags
# Note: USE_DYNAMIC_MEMORY_ALLOCATION (or USE_MEMORY_POOL_ALLOCATION)
# is required by the ST CMSIS adapter; we use USE_DYNAMIC_MEMORY_ALLOCATION.
# Note: RTOS2_BYTE_POOL_HEAP_SIZE + RTOS2_BYTE_POOL_STACK_SIZE should
# equal the amount of space you have reserved in the _threadx_heap
# section of the .ld file.
DEFINES += \
	-DUSE_HAL_DRIVER \
	-DUSE_FULL_LL_DRIVER \
	-DSTM32U575xx \
	-DDEBUG \
	-DU_PORT_STM32_PURE_CMSIS \
	-DCMSIS_V2 \
	-DCMSIS_device_header=\"${CMSIS_DEVICE_HEADER}\" \
	-DTX_INCLUDE_USER_DEFINE_FILE \
	-DTX_SINGLE_MODE_NON_SECURE=1 \
	-DUSE_DYNAMIC_MEMORY_ALLOCATION \
	-DRTOS2_BYTE_POOL_HEAP_SIZE=688128 \
	-DRTOS2_BYTE_POOL_STACK_SIZE=65536

override CFLAGS += \
	$(DEFINES) -mcpu=cortex-m33 -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -std=gnu11 -g3 -O0 -Wall \
	-ffunction-sections -fdata-sections -fstack-usage --specs=nano.specs -MMD -MP $(U_FLAGS)

override LDFLAGS += \
	-debug -mcpu=cortex-m33 -mfpu=fpv5-sp-d16 -mfloat-abi=hard -mthumb -static -T"$(PLATFORM_PATH)/app/STM32U575ZITX_FLASH.ld" \
	-Wl,-Map="$(OUTPUT_DIRECTORY)/runner.map" -Wl,--gc-sections --specs=nano.specs -Wl,--start-group -lc -lm -Wl,--end-group \
	-Wl,--defsym=RTOS2_BYTE_POOL_SIZE=753664

# Include STM32 common files; this requires STM32CUBE_HAL_PATH and will give us the
# common parts of STM32CUBE_FW_SRC and STM32CUBE_FW_INC (i.e. the HAL etc.)
include ../stm32cube_common.mk

# Add the LP UART, which is only on STM32U5
STM32CUBE_FW_SRC += \
	$(STM32CUBE_HAL_PATH)/Src/stm32u5xx_ll_lpuart.c

# Threadx
STM32CUBE_FW_SRC += \
	$(THREADX_PATH)/common/src/tx_block_allocate.c \
	$(THREADX_PATH)/common/src/tx_block_pool_cleanup.c \
	$(THREADX_PATH)/common/src/tx_block_pool_create.c \
	$(THREADX_PATH)/common/src/tx_block_pool_delete.c \
	$(THREADX_PATH)/common/src/tx_block_pool_info_get.c \
	$(THREADX_PATH)/common/src/tx_block_pool_initialize.c \
	$(THREADX_PATH)/common/src/tx_block_pool_performance_info_get.c \
	$(THREADX_PATH)/common/src/tx_block_pool_performance_system_info_get.c \
	$(THREADX_PATH)/common/src/tx_block_pool_prioritize.c \
	$(THREADX_PATH)/common/src/tx_block_release.c \
	$(THREADX_PATH)/common/src/tx_byte_allocate.c \
	$(THREADX_PATH)/common/src/tx_byte_pool_cleanup.c \
	$(THREADX_PATH)/common/src/tx_byte_pool_create.c \
	$(THREADX_PATH)/common/src/tx_byte_pool_delete.c \
	$(THREADX_PATH)/common/src/tx_byte_pool_info_get.c \
	$(THREADX_PATH)/common/src/tx_byte_pool_initialize.c \
	$(THREADX_PATH)/common/src/tx_byte_pool_performance_info_get.c \
	$(THREADX_PATH)/common/src/tx_byte_pool_performance_system_info_get.c \
	$(THREADX_PATH)/common/src/tx_byte_pool_prioritize.c \
	$(THREADX_PATH)/common/src/tx_byte_pool_search.c \
	$(THREADX_PATH)/common/src/tx_byte_release.c \
	$(THREADX_PATH)/common/src/tx_event_flags_cleanup.c \
	$(THREADX_PATH)/common/src/tx_event_flags_create.c \
	$(THREADX_PATH)/common/src/tx_event_flags_delete.c \
	$(THREADX_PATH)/common/src/tx_event_flags_get.c \
	$(THREADX_PATH)/common/src/tx_event_flags_info_get.c \
	$(THREADX_PATH)/common/src/tx_event_flags_initialize.c \
	$(THREADX_PATH)/common/src/tx_event_flags_performance_info_get.c \
	$(THREADX_PATH)/common/src/tx_event_flags_performance_system_info_get.c \
	$(THREADX_PATH)/common/src/tx_event_flags_set.c \
	$(THREADX_PATH)/common/src/tx_event_flags_set_notify.c \
	$(THREADX_PATH)/common/src/tx_initialize_high_level.c \
	$(THREADX_PATH)/common/src/tx_initialize_kernel_enter.c \
	$(THREADX_PATH)/common/src/tx_initialize_kernel_setup.c \
	$(THREADX_PATH)/common/src/tx_misra.c \
	$(THREADX_PATH)/common/src/tx_mutex_cleanup.c \
	$(THREADX_PATH)/common/src/tx_mutex_create.c \
	$(THREADX_PATH)/common/src/tx_mutex_delete.c \
	$(THREADX_PATH)/common/src/tx_mutex_get.c \
	$(THREADX_PATH)/common/src/tx_mutex_info_get.c \
	$(THREADX_PATH)/common/src/tx_mutex_initialize.c \
	$(THREADX_PATH)/common/src/tx_mutex_performance_info_get.c \
	$(THREADX_PATH)/common/src/tx_mutex_performance_system_info_get.c \
	$(THREADX_PATH)/common/src/tx_mutex_prioritize.c \
	$(THREADX_PATH)/common/src/tx_mutex_priority_change.c \
	$(THREADX_PATH)/common/src/tx_mutex_put.c \
	$(THREADX_PATH)/common/src/tx_queue_cleanup.c \
	$(THREADX_PATH)/common/src/tx_queue_create.c \
	$(THREADX_PATH)/common/src/tx_queue_delete.c \
	$(THREADX_PATH)/common/src/tx_queue_flush.c \
	$(THREADX_PATH)/common/src/tx_queue_front_send.c \
	$(THREADX_PATH)/common/src/tx_queue_info_get.c \
	$(THREADX_PATH)/common/src/tx_queue_initialize.c \
	$(THREADX_PATH)/common/src/tx_queue_performance_info_get.c \
	$(THREADX_PATH)/common/src/tx_queue_performance_system_info_get.c \
	$(THREADX_PATH)/common/src/tx_queue_prioritize.c \
	$(THREADX_PATH)/common/src/tx_queue_receive.c \
	$(THREADX_PATH)/common/src/tx_queue_send.c \
	$(THREADX_PATH)/common/src/tx_queue_send_notify.c \
	$(THREADX_PATH)/common/src/tx_semaphore_ceiling_put.c \
	$(THREADX_PATH)/common/src/tx_semaphore_cleanup.c \
	$(THREADX_PATH)/common/src/tx_semaphore_create.c \
	$(THREADX_PATH)/common/src/tx_semaphore_delete.c \
	$(THREADX_PATH)/common/src/tx_semaphore_get.c \
	$(THREADX_PATH)/common/src/tx_semaphore_info_get.c \
	$(THREADX_PATH)/common/src/tx_semaphore_initialize.c \
	$(THREADX_PATH)/common/src/tx_semaphore_performance_info_get.c \
	$(THREADX_PATH)/common/src/tx_semaphore_performance_system_info_get.c \
	$(THREADX_PATH)/common/src/tx_semaphore_prioritize.c \
	$(THREADX_PATH)/common/src/tx_semaphore_put.c \
	$(THREADX_PATH)/common/src/tx_semaphore_put_notify.c \
	$(THREADX_PATH)/common/src/tx_thread_create.c \
	$(THREADX_PATH)/common/src/tx_thread_entry_exit_notify.c \
	$(THREADX_PATH)/common/src/tx_thread_identify.c \
	$(THREADX_PATH)/common/src/tx_thread_info_get.c \
	$(THREADX_PATH)/common/src/tx_thread_initialize.c \
	$(THREADX_PATH)/common/src/tx_thread_performance_info_get.c \
	$(THREADX_PATH)/common/src/tx_thread_performance_system_info_get.c \
	$(THREADX_PATH)/common/src/tx_thread_preemption_change.c \
	$(THREADX_PATH)/common/src/tx_thread_priority_change.c \
	$(THREADX_PATH)/common/src/tx_thread_relinquish.c \
	$(THREADX_PATH)/common/src/tx_thread_resume.c \
	$(THREADX_PATH)/common/src/tx_thread_shell_entry.c \
	$(THREADX_PATH)/common/src/tx_thread_sleep.c \
	$(THREADX_PATH)/common/src/tx_thread_stack_analyze.c \
	$(THREADX_PATH)/common/src/tx_thread_stack_error_handler.c \
	$(THREADX_PATH)/common/src/tx_thread_stack_error_notify.c \
	$(THREADX_PATH)/common/src/tx_thread_suspend.c \
	$(THREADX_PATH)/common/src/tx_thread_system_preempt_check.c \
	$(THREADX_PATH)/common/src/tx_thread_system_resume.c \
	$(THREADX_PATH)/common/src/tx_thread_system_suspend.c \
	$(THREADX_PATH)/common/src/tx_thread_terminate.c \
	$(THREADX_PATH)/common/src/tx_thread_time_slice.c \
	$(THREADX_PATH)/common/src/tx_thread_time_slice_change.c \
	$(THREADX_PATH)/common/src/tx_thread_timeout.c \
	$(THREADX_PATH)/common/src/tx_thread_wait_abort.c \
	$(THREADX_PATH)/common/src/tx_time_get.c \
	$(THREADX_PATH)/common/src/tx_time_set.c \
	$(THREADX_PATH)/common/src/tx_timer_activate.c \
	$(THREADX_PATH)/common/src/tx_timer_change.c \
	$(THREADX_PATH)/common/src/tx_timer_create.c \
	$(THREADX_PATH)/common/src/tx_timer_deactivate.c \
	$(THREADX_PATH)/common/src/tx_timer_delete.c \
	$(THREADX_PATH)/common/src/tx_timer_expiration_process.c \
	$(THREADX_PATH)/common/src/tx_timer_info_get.c \
	$(THREADX_PATH)/common/src/tx_timer_initialize.c \
	$(THREADX_PATH)/common/src/tx_timer_performance_info_get.c \
	$(THREADX_PATH)/common/src/tx_timer_performance_system_info_get.c \
	$(THREADX_PATH)/common/src/tx_timer_system_activate.c \
	$(THREADX_PATH)/common/src/tx_timer_system_deactivate.c \
	$(THREADX_PATH)/common/src/tx_timer_thread_entry.c \
	$(THREADX_PATH)/common/src/tx_trace_buffer_full_notify.c \
	$(THREADX_PATH)/common/src/tx_trace_disable.c \
	$(THREADX_PATH)/common/src/tx_trace_enable.c \
	$(THREADX_PATH)/common/src/tx_trace_event_filter.c \
	$(THREADX_PATH)/common/src/tx_trace_event_unfilter.c \
	$(THREADX_PATH)/common/src/tx_trace_initialize.c \
	$(THREADX_PATH)/common/src/tx_trace_interrupt_control.c \
	$(THREADX_PATH)/common/src/tx_trace_isr_enter_insert.c \
	$(THREADX_PATH)/common/src/tx_trace_isr_exit_insert.c \
	$(THREADX_PATH)/common/src/tx_trace_object_register.c \
	$(THREADX_PATH)/common/src/tx_trace_object_unregister.c \
	$(THREADX_PATH)/common/src/tx_trace_user_event_insert.c \
	$(THREADX_PATH)/common/src/txe_block_allocate.c \
	$(THREADX_PATH)/common/src/txe_block_pool_create.c \
	$(THREADX_PATH)/common/src/txe_block_pool_delete.c \
	$(THREADX_PATH)/common/src/txe_block_pool_info_get.c \
	$(THREADX_PATH)/common/src/txe_block_pool_prioritize.c \
	$(THREADX_PATH)/common/src/txe_block_release.c \
	$(THREADX_PATH)/common/src/txe_byte_allocate.c \
	$(THREADX_PATH)/common/src/txe_byte_pool_create.c \
	$(THREADX_PATH)/common/src/txe_byte_pool_delete.c \
	$(THREADX_PATH)/common/src/txe_byte_pool_info_get.c \
	$(THREADX_PATH)/common/src/txe_byte_pool_prioritize.c \
	$(THREADX_PATH)/common/src/txe_byte_release.c \
	$(THREADX_PATH)/common/src/txe_event_flags_create.c \
	$(THREADX_PATH)/common/src/txe_event_flags_delete.c \
	$(THREADX_PATH)/common/src/txe_event_flags_get.c \
	$(THREADX_PATH)/common/src/txe_event_flags_info_get.c \
	$(THREADX_PATH)/common/src/txe_event_flags_set.c \
	$(THREADX_PATH)/common/src/txe_event_flags_set_notify.c \
	$(THREADX_PATH)/common/src/txe_mutex_create.c \
	$(THREADX_PATH)/common/src/txe_mutex_delete.c \
	$(THREADX_PATH)/common/src/txe_mutex_get.c \
	$(THREADX_PATH)/common/src/txe_mutex_info_get.c \
	$(THREADX_PATH)/common/src/txe_mutex_prioritize.c \
	$(THREADX_PATH)/common/src/txe_mutex_put.c \
	$(THREADX_PATH)/common/src/txe_queue_create.c \
	$(THREADX_PATH)/common/src/txe_queue_delete.c \
	$(THREADX_PATH)/common/src/txe_queue_flush.c \
	$(THREADX_PATH)/common/src/txe_queue_front_send.c \
	$(THREADX_PATH)/common/src/txe_queue_info_get.c \
	$(THREADX_PATH)/common/src/txe_queue_prioritize.c \
	$(THREADX_PATH)/common/src/txe_queue_receive.c \
	$(THREADX_PATH)/common/src/txe_queue_send.c \
	$(THREADX_PATH)/common/src/txe_queue_send_notify.c \
	$(THREADX_PATH)/common/src/txe_semaphore_ceiling_put.c \
	$(THREADX_PATH)/common/src/txe_semaphore_create.c \
	$(THREADX_PATH)/common/src/txe_semaphore_delete.c \
	$(THREADX_PATH)/common/src/txe_semaphore_get.c \
	$(THREADX_PATH)/common/src/txe_semaphore_info_get.c \
	$(THREADX_PATH)/common/src/txe_semaphore_prioritize.c \
	$(THREADX_PATH)/common/src/txe_semaphore_put.c \
	$(THREADX_PATH)/common/src/txe_semaphore_put_notify.c \
	$(THREADX_PATH)/common/src/txe_thread_create.c \
	$(THREADX_PATH)/common/src/txe_thread_delete.c \
	$(THREADX_PATH)/common/src/txe_thread_entry_exit_notify.c \
	$(THREADX_PATH)/common/src/txe_thread_info_get.c \
	$(THREADX_PATH)/common/src/txe_thread_preemption_change.c \
	$(THREADX_PATH)/common/src/txe_thread_priority_change.c \
	$(THREADX_PATH)/common/src/txe_thread_relinquish.c \
	$(THREADX_PATH)/common/src/txe_thread_reset.c \
	$(THREADX_PATH)/common/src/txe_thread_resume.c \
	$(THREADX_PATH)/common/src/txe_thread_suspend.c \
	$(THREADX_PATH)/common/src/txe_thread_terminate.c \
	$(THREADX_PATH)/common/src/txe_thread_time_slice_change.c \
	$(THREADX_PATH)/common/src/txe_thread_wait_abort.c \
	$(THREADX_PATH)/common/src/txe_timer_activate.c \
	$(THREADX_PATH)/common/src/txe_timer_change.c \
	$(THREADX_PATH)/common/src/txe_timer_create.c \
	$(THREADX_PATH)/common/src/txe_timer_deactivate.c \
	$(THREADX_PATH)/common/src/txe_timer_delete.c \
	$(THREADX_PATH)/common/src/txe_timer_info_get.c \
	$(THREADX_PATH)/common/src/tx_thread_reset.c \
	$(THREADX_PATH)/common/src/tx_thread_delete.c \
	$(THREADX_PATH)/ports/cortex_m33/gnu/src/txe_thread_secure_stack_allocate.c \
	$(THREADX_PATH)/ports/cortex_m33/gnu/src/txe_thread_secure_stack_free.c \
	$(THREADX_PATH)/ports/cortex_m33/gnu/src/tx_thread_secure_stack.c

STM32CUBE_FW_SRC_ASM += \
	$(THREADX_PATH)/ports/cortex_m33/gnu/src/tx_thread_context_restore.S \
	$(THREADX_PATH)/ports/cortex_m33/gnu/src/tx_thread_context_save.S \
	$(THREADX_PATH)/ports/cortex_m33/gnu/src/tx_thread_interrupt_control.S \
	$(THREADX_PATH)/ports/cortex_m33/gnu/src/tx_thread_interrupt_disable.S \
	$(THREADX_PATH)/ports/cortex_m33/gnu/src/tx_thread_interrupt_restore.S \
	$(THREADX_PATH)/ports/cortex_m33/gnu/src/tx_thread_schedule.S \
	$(THREADX_PATH)/ports/cortex_m33/gnu/src/tx_thread_secure_stack_allocate.S \
	$(THREADX_PATH)/ports/cortex_m33/gnu/src/tx_thread_secure_stack_free.S \
	$(THREADX_PATH)/ports/cortex_m33/gnu/src/tx_thread_stack_build.S \
	$(THREADX_PATH)/ports/cortex_m33/gnu/src/tx_thread_system_return.S \
	$(THREADX_PATH)/ports/cortex_m33/gnu/src/tx_timer_interrupt.S

STM32CUBE_FW_INC += \
	$(THREADX_PATH)/ports/cortex_m33/gnu/inc \
	$(THREADX_PATH)/common/inc

# CMSIS wrapper to ThreadX
STM32CUBE_FW_SRC += \
	$(STM32CUBE_FW_PATH)/Middlewares/ST/cmsis_rtos_threadx/cmsis_os2.c

STM32CUBE_FW_INC += \
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
	$(UBXLIB_PRIVATE_INC) \
	$(UBXLIB_BASE)/port/clib \
	$(PLATFORM_PATH)/src \
	$(PLATFORM_PATH)/inc

# App
APP_SRC += \
	$(PLATFORM_PATH)/src/stm32u5xx_hal_msp.c \
	$(PLATFORM_PATH)/src/u_exception_handler.c \
	$(PLATFORM_PATH)/src/syscalls.c \
	$(PLATFORM_PATH)/src/sysmem.c \
	$(PLATFORM_PATH)/src/system_stm32u5xx.c \
	$(UBXLIB_TEST_SRC) \
	$(PLATFORM_PATH)/app/u_main.c

APP_ASM += \
	$(PLATFORM_PATH)/src/startup_stm32u575zitxq.S \
	$(PLATFORM_PATH)/src/tx_initialize_low_level.S

UBXLIB_INC += \
	$(UBXLIB_TEST_INC) \
	$(UNITY_INC) \
	../cfg \
	$(UBXLIB_BASE)/port/platform/common/heap_check \
	$(UBXLIB_BASE)/port/platform/common/runner

INC := $(UBXLIB_INC:%=-I%)

UBXLIB_OBJ += $(UBXLIB_SRC:$(UBXLIB_BASE)/%.c=$(OBJ_DIR)/ubxlib/%.o)
STM32_OBJ += $(STM32CUBE_FW_SRC:$(STM32CUBE_FW_PATH)/%.c=$(OBJ_DIR)/stm32cube_fw/%.o)
# Note: there _is_ a difference between an assembler file with a lower-case
# s and one with an upper-case S: the upper-case one is preprocessed, the
# lower-case one is not, and ThreadX, bless-em, do include conditional
# compilation flags in their assembler files.
STM32_ASM_PREPROCESSED_OBJ += $(STM32CUBE_FW_SRC_ASM:$(STM32CUBE_FW_PATH)/%.S=$(OBJ_DIR)/stm32cube_fw/%.o)
UNITY_OBJ += $(UNITY_SRC:$(UNITY_PATH)/%.c=$(OBJ_DIR)/unity/%.o)
APP_OBJ += $(APP_SRC:$(UBXLIB_BASE)/%.c=$(OBJ_DIR)/app/%.o)
APP_ASM_PREPROCESSED_OBJ += $(APP_ASM:$(UBXLIB_BASE)/%.S=$(OBJ_DIR)/app/%.o)

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
$(OBJ_DIR)/libstm32cube_fw.a: $(STM32_OBJ) $(STM32_ASM_PREPROCESSED_OBJ)
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
$(OUTPUT_DIRECTORY)/$(TARGET): $(APP_OBJ) $(APP_ASM_PREPROCESSED_OBJ) $(OBJ_DIR)/libubxlib.a $(OBJ_DIR)/libunity.a $(OBJ_DIR)/libstm32cube_fw.a
	$(SILENT)$(call mkdir,$(@D))
	@echo Linking $@
	$(SILENT)$(CC) -o $@ $^ $(LDFLAGS)

