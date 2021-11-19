# Introduction
This directory contains the build infrastructure for STM32F4 MCU using the STM32Cube FW SDK.  The STM32F4 device configuration matches that of the STM32F437VG device that is mounted on the u-blox C030 board and configures it sufficiently well to run the ubxlib tests and examples: no attempt is made to optimise the MCU RAM/flash etc. sizes, you need to know how to do that yourself.

# SDK Installation
Download a version (this code was tested with version 1.25.0) of the STM32F4 MCU package ZIP file (containing their HAL etc.) from here:

https://www.st.com/en/embedded-software/stm32cubef4.html

You will need to specify the directory where you exctracted this .zip file when you compile the the ubxlib runner (see [runner/README.md](runner/README.md))

# Chip Resource Requirements
The SysTick of the STM32F4 is assumed to provide a 1 ms RTOS tick which is used as a source of time for `uPortGetTickTimeMs()`.  Note that this means that if you want to use FreeRTOS in tickless mode you will need to either find another source of tick for `uPortGetTickTimeMs()` or put in a call that updates `gTickTimerRtosCount` when FreeRTOS resumes after a tickless period.

# Trace Output
In order to conserve HW resources the trace output from this platform is sent over SWD using an ITM channel. There are many ways to read out the ITM trace output:

## STM32Cube IDE
If you have a STM32Cube IDE project setup you can view the SWO trace output in this way: setup up the debugger as normal by pulling down the arrow beside the little button on the toolbar with the "bug" image on it, selecting "STM-Cortex-M C/C++ Application", create a new configuration and then, on the "Debugger" tab, tick the box that enables SWD, set the core clock to 168 MHz, the SWO Clock to 125 kHz (to match `U_CFG_HW_SWO_CLOCK_HZ` defined in `u_cfg_hw_platform_specific.h`), untick "Wait for sync packet" and click "Apply".

You should then be able to download the Debug build from the IDE and the IDE should launch you into the debugger.  To see the SWD trace output, click on "Window" -> "Show View" -> "SWV" -> "SWV ITM Data Console".  The docked window that appears should have a little "spanner" icon on the far right: click on that icon and, on the set of "ITM Stimulus Ports", tick channel 0 and then press "OK".  Beside the "spanner" icon is a small red button: press that to allow trace output to appear; unfortunately it seems that this latter step has to be performed every debug session, ST have chosen not to automate it for some reason.

## ST-Link utility
Alternatively, if you just want to run the target without the debugger and simply view the SWO output, the (ST-Link utility)[https://www.st.com/en/development-tools/stsw-link004.html] utility includes a "Printf via SWO Viewer" option under its "ST-LINK" menu.  Set the core clock to 168 MHz, press "Start" and your debug `printf()`s will appear in that window.  HOWEVER, in this tool ST have fixed the expected SWO clock at 2 MHz whereas in normal operation we run it at 125 kHz to improve reliability; to use the ST-Link viewer you must unfortunately set the conditional complation flag `U_CFG_HW_SWO_CLOCK_HZ` to 2000000 before you build the code and then hope that trace output is sufficiently reliable (for adhoc use it is usually fine, it is under constant automated test that the cracks start to appear).

# stm32f4.mk
This Makefile can be used to include ubxlib in your STM32F4 application. Before including `stm32f4.mk` you must set `UBXLIB_BASE` variable to the root directory of `ubxlib`.
The Makefile will then create a number of variables that you can use for retrieving `ubxlib` source files and include directories:
* `UBXLIB_SRC`: A list of all the .c files
* `UBXLIB_INC`: A list of the include directories
* `UBXLIB_TEST_SRC`: A list of all the test .c files
* `UBXLIB_TEST_INC`: A list of test include directories

# Maintenance
- When updating this build to a new version of `STM32Cube_FW_F4` change the release version stated in the introduction above.
