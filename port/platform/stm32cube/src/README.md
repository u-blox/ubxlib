When we supported only STM32F4 the contents of this directory were relatively clear.  However, adding support for STM32U5 under STM32Cube has muddied the waters somewhat, especially since the default RTOS integration provided by ST with the STM32U5Cube FW is now ThreadX rather than FreeRTOS.  You will find details on how to choose between the two platforms and, in the STM32U5 case, between the two RTOSes, in the directories under [mcu](../mcu).

# Relevance
These files are relevant to both STM32F4 and STM32U5:
- [heap_useNewlib.c](heap_useNewlib.c), note: only relevant to STM32U5 if `U_PORT_STM32_CMSIS_ON_FREERTOS` is defined, may be ignored for STM32U5 otherwise,
- [stm32_assert.h](stm32_assert.h),
- [syscalls.c](syscalls.c),
- [u_exception_handler.c](u_exception_handler.c),
- [u_port.c](u_port.c),
- [u_port_clib_platform_specific.h](u_port_clib_platform_specific.h),
- [u_port_debug.c](u_port_debug.c),
- [u_port_gpio.c](u_port_gpio.c),
- [u_port_i2c.c](u_port_i2c.c),
- [u_port_private.c](u_port_private.c),
- [u_port_private.h](u_port_private.h),
- [u_port_spi.c](u_port_spi.c),
- [u_port_uart.c](u_port_uart.c).

These files are relevant only to STM32F4:
- [system_stm32f4xx.c](system_stm32f4xx.c),
- [startup_stm32f437vgtx.s](startup_stm32f437vgtx.s),
- [stm32f4xx_hal_conf.h](stm32f4xx_hal_conf.h),
- [stm32f4xx_hal_msp.c](stm32f4xx_hal_msp.c),
- [u_port_os.c](u_port_os.c), note: this is the implemenation of the OS porting layer for CMSIS versions 1 or 2 on top of FreeRTOS.

These files are relevant only to STM32U5 (for which `U_PORT_STM32_PURE_CMSIS` must be defined):
- [system_stm32u5xx.c](system_stm32u5xx.c),
- [startup_stm32u575zitxq.S](startup_stm32u575zitxq.S),
- [stm32u5xx_hal_conf.h](stm32u5xx_hal_conf.h),
- [stm32u5xx_hal_msp.c](stm32u5xx_hal_msp.c),
- [tx_initialize_low_level.S](tx_initialize_low_level.S), note: provides ThreadX initialisation, may be ignored if `U_PORT_STM32_CMSIS_ON_FREERTOS` is defined,
- [tx_user.h](tx_user.h), note: provides ThreadX compile-time configuration, may be ignored if `U_PORT_STM32_CMSIS_ON_FREERTOS` is defined,
- [u_port_clib.c](u_port_clib.c), note: maps `malloc()` and `free()` to ThreadX memory pools, may be ignored if `U_PORT_STM32_CMSIS_ON_FREERTOS` is defined,
- [sysmem.c](sysmem.c), note: provides an implementation of `_sbrk()` for ThreadX memory pools, may be ignored if `U_PORT_STM32_CMSIS_ON_FREERTOS` is defined,
- [u_port_os_pure_cmsis.c](u_port_os_pure_cmsis.c), note: a version of [u_port_os.c](u_port_os.c) that adapts only to the CMSIS version 2 API, enabling either ThreadX or FreeRTOS to be used.
- [i2c_timing_utility.c](i2c_timing_utility.c), note: this file is provided by ST with the I2C examples for STM32U5 as an implementation of the relatively complex calculation required to set the timing register correctly for a given I2C clock rate in Hertz (the I2C HW in the STM32U5 series MCUs is significantly different to that of the STM32F4 series MCUs); it effectively forms part of [u_port_i2c.c](u_port_i2c.c) for STM32U5.
