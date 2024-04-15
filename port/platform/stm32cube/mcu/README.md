# SM32F4 v STM32U5
`ubxlib` supports both STM32F4 series and STM32U5 series MCUs directly with the STM32Cube FW.  `ubxlib` _also_ supports both of these processors under [zephyr](/port/platform/zephyr), which you may find a richer and more flexible environment, however if you come out in hives at the mention of the Device Tree, this is the way.

How to build and run `ubxlib` for STM32Cube is detailed in the directories below; the key differences are summarised here.

# STM32F4
On STM32F4 `ubxlib` expects to find the underlying FreeRTOS, as pre-integrated by ST in their STM32Cube FW.  `ubxlib` supports both the CMSIS V1 and the CMSIS V2 flavours of RTOS API, noting that in the CMSIS V1 case `ubxlib` has to reach through to underyling FreeRTOS functions due to some deficiencies in the version 1 CMSIS API.  It should be noted that, out of the box, the ST integration of `malloc()`/`free()` is NOT thread-safe; thread-safety is added in the `ubxlib` integration courtesy of Dave Nadler's [heap_useNewlib.c](../src/heap_useNewlib.c).

Note that the system tick is set to 1 ms.

The UART HW blocks within the STM32F4 series MCUs have no HW FIFO to speak of and _will_ lose characters in normal operation unless DMA is employed; `ubxlib` implements the required DMA handling in the [uPortUart](/port/api/u_port_uart.h) API.

On the ST32F407 Discovery boards used in the `ubxlib` test system we had reliability issues with SWO, our default trace mechanism, and hence there is code that runs at porting layer initialisation (see [u_port_private.c](../src/u_port_private.c)) which will set the SWO rate to 125 kHz (as opposed to 2 MHz), see `U_CFG_HW_SWO_CLOCK_HZ` in the STM32F4 [u_cfg_hw_platform_specific.h](stm32f4/cfg/u_cfg_hw_platform_specific.h); this is quite sufficient for our needs and setting the rate up in the embedded code, rather than letting the debugger do it, means that SWO communications at this reduced rate will be maintained across an MCU-driven reset, when no debugger would know it needed to reconfigure the rate. 

# STM32U5
With the STM32Cube FW [provided as default](https://github.com/STMicroelectronics/STM32CubeU5) by ST for STM32U5, FreeRTOS has been replaced by ThreadX; `ubxlib` will use this ThreadX integration by default.  Since FreeRTOS remains popular, and since there is no requirement to support CMSIS V1 on this new chipset, the `ubxlib` integration is at the CMSIS V2 API and in this way `ubxlib` is also able to support FreeRTOS via the ST-provided [CMSIS V2 implementation on FreeRTOS](https://github.com/STMicroelectronics/x-cube-freertos).  In the [stm32u5](stm32u5) directory below this one you will find builds for each RTOS.  When using ThreadX, `ubxlib` maps `pUPortMalloc()`/`uPortFree()` to ThreadX memory management, which is assumed to be thread-safe.

For STM32U5 ST have set the ThreadX system tick to 10 ms; we have aligned the tick rate for the FreeRTOS version to the same value and hence it is _different_ to the STM32F4 case.

It is a limitation of ThreadX that an item on an RTOS queue has a maximum size of 64 bytes; `ubxlib` is able to work within this constraint.

In the STM32U5 series MCUs, ST have seen the light and implemented 8-byte FIFOs on all UARTs, hence the relatively complex DMA-based UART implementation is not employed for STM32U5.

STM32U5 supports a single low-power UART; this may be used in `ubxlib` by requesting UART HW block 0 at the [uPortUart](/port/api/u_port_uart.h) API.  It should be noted that the low-power UART is only really low power if the MCU is running from a 32 kHz source clock in which case the maximum UART baud rate is limited to 9600.

The `ubxlib` code makes no use of the Trust Zone features of the STM32U5; your application may, of course, do so.

For reasons we don't understand, employing code in the target FW to set the SWO rate at boot prevents the NUCLEO-U575ZI-Q board from connecting to a debugger ever again after the board has been power-cycled; only manual intervention (connecting `BOOT0` to `VDD` to boot from ROM, connecting under reset with the [STM32Cube Programmer](https://www.st.com/en/development-tools/stm32cubeprog.html) and performing a full flash erase) recovers the situation.  Since running SWO at the default rate of 2 MHz on the NUCLEO-U575ZI-Q boards doesn't seem to be a problem, use of the SWO-rate-setting code is disabled by default (see `U_CFG_HW_SWO_CLOCK_HZ` in the STM32U5 version of [u_cfg_hw_platform_specific.h](stm32u5/cfg/u_cfg_hw_platform_specific.h)).