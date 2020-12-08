# Introduction
These directories provide the implementation of the porting layer on the STM32F4 platform plus the associated build and board configuration information:

- `cfg`: contains the configuration files for the MCU, the OS, for the application and for testing (mostly which MCU pins are connected to which module pins).
- `sdk/cube`: contains the files to build/test for the STM32F4 platform using the STM32Cube IDE.
- `src`: contains the implementation of the porting layer for STM32F4.
- `app`: contains the code that runs the application (both examples and unit tests) on the STM32F4 platform.

# Hardware Requirements
In order to run on the u-blox C030 board out of the box the MCU crystal is configured to be an external 12 MHz crystal, as reflected by the value of `HSE_VALUE` in `stm32f4xx_hal_conf.h` of `((uint32_t) 12000000U)`; if your board is different (e.g. the STM32F4 Discovery board which uses an 8 MHz crystal) you should override `HSE_VALUE` as required (e.g. to `((uint32_t)8000000U)`).  See the `sdk` directory for instructions on how to do this without modifying `stm32f4xx_hal_conf.h`.

# Chip Resource Requirements
The SysTick of the STM32F4 is assumed to provide a 1 ms RTOS tick which is used as a source of time for `uPortGetTickTimeMs()`.  Note that this means that if you want to use FreeRTOS in tickless mode you will need to either find another source of tick for `uPortGetTickTimeMs()` or put in a call that updates `gTickTimerRtosCount` when FreeRTOS resumes after a tickless period.

# Trace Output
In order to conserve HW resources the trace output from this platform is sent over SWD.  Instructions for how to view the trace output in the STM32Cube IDE via an ST-Link probe can be found in the `cube` sub-directory below.

Alternatively, if you just want to run the target without the debugger and simply view the SWO output, the (ST-Link utility)[https://www.st.com/en/development-tools/stsw-link004.html] utility includes a "Printf via SWO Viewer" option under its "ST-LINK" menu.  Set the core clock to 168 MHz, press "Start" and your debug `printf()`s will appear in that window.  HOWEVER, in this tool ST have fixed the exepcted SWO clock at 2 MHz whereas in normal operation we run it at 125 kHz to improve reliability; to use the ST-Link viewer you must unfortunately set the conditional complation flag `U_CFG_HW_SWO_CLOCK_HZ` to 2000000 before you build the code and then hope that trace output is sufficiently reliable (for adhoc use it is usually fine, it is under constant automated test that the cracks start to appear).