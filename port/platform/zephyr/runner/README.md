# Introduction
This directory and its sub-directories contain the build infrastructure for the Zephyr platform in association with the nRFConnect tools.  The methods described here are those for Segger Embedded Studio (SES) and for `west`, the Zephyr command-line build tool.  The configuration here is sufficient to run the `ubxlib` tests and examples, no attempt is made to optimise the MCU RAM/flash etc. sizes, you need to know how to do that yourself.

This is tested on `nRFConnect SDK version 1.4.2` which is the recommended version.

IMPORTANT: the pin usage defined in the `../cfg` directory for GPIO testing and in the `.overlay` files here for UART testing is subject to change as we try to settle on a single configuration that will work for all of the various board types.

# SDK Installation
Follow the instructions to install the development tools:

- Install nRF connect. https://www.nordicsemi.com/Software-and-Tools/Development-Tools/nRF-Connect-for-desktop
- Start nRFConnect and use the tool chain manager to install the recommended SDK version (see above).
- IMPORTANT!: Update SDK and toolchain using the dropdown menu for your SDK version.

From tool chain manager start Segger embedded studio (SES) using Open IDE button.

# SDK Usage
- IMPORTANT: Zephyr pin choices for any HW peripheral managed by Zephyr (e.g. UART, I2C, SPI, etc.) are made at compile-time in the Zephyr device tree, they cannot be passed into the `ubxlib` functions as run-time variables.  Look in the `.overlay` file of your build to find/set the pin allocations for these peripherals.  As a reminder, any associated pin assignments in the `cfg` header files for this platform are set to -1 and functions such as `uPortUartOpen()` which take pin assignments as parameters will return an error if passed anything other than -1 for a pin assignment.
- Zephyr is assumed to use its internal minimal C library, not newlib; this is because the Zephyr integration of newlib is not threadsafe.  If, at some point in the future, the integration is made thread-safe, or if you make it thread-safe yourself, and you wish to use newlib then you should configure Zephyr appropriately and then add `U_CFG_ZEPHYR_USE_NEWLIB` to the conditional compilation flags passed into the build (see below for how to do this without modifying `CMakeLists.txt`).
- Always load project from SES using file->Open nRF connect SDK project
- Select the `CMakeLists.txt` of the application you want to build.
- Board file should be `{your_sdk_path}/zephyr/boards/arm/nrf5340dk_nrf5340` for EVK-NORA-B1.
  For a custom board e.g. `port/platform/zephyr/boards/short_range/zephyr/boards/arm/ubx_evkninab4_nrf52833`
- Board name should be `nrf5340dk_nrf5340_cpuapp` for EVK-NORA-B1.  For a custom board e.g. `ubx_evkninab4_nrf52833`.
- Where a board-specific configuration file is available (e.g. `ubx_evkninab4_nrf52833.conf`) this will be picked up automatically.
- Always clean the build directory when upgrading to a new ubxlib version.
- You may override or provide conditional compilation flags to CMake without modifying `CMakeLists.txt`.  Do this by setting an environment variable `U_FLAGS`, e.g.:
  
  ```
  set U_FLAGS=-DMY_FLAG
  ```
  
  ...or:
  
  ```
  set U_FLAGS=-DMY_FLAG -DU_CFG_APP_PIN_CELL_ENABLE_POWER=-1
  ```

# Build And Flash With `west`
While in the `port/platform/zephyr/runner` directory:

  ```
  west build -p auto -b nrf5340dk_nrf5340_cpuapp . --build-dir build_nrf5340dk_nrf5340_cpuapp
  west flash --build-dir build_nrf5340dk_nrf5340_cpuapp
  ```
  
  ```
  west build -p auto -b ubx_evkninab4_nrf52833 . -DBOARD_ROOT=../boards/short_range/zephyr --build-dir build_ubx_evkninab4_nrf52833
  west flash --build-dir build_ubx_evkninab4_nrf52833
  ```

  ```
  west build -p auto -b ubx_evkninab3_nrf52840 . -DBOARD_ROOT=../boards/short_range/zephyr --build-dir build_ubx_evkninab3_nrf52840
  west flash --build-dir build_ubx_evkninab3_nrf52840
  ```

# Usage
Make sure you have followed the instructions in the directory above this to install nRFConnectSDK and toolchain.

By default all of the examples and tests supported by this platform will be executed.  To execute just a subset set the conditional compilation flag `U_CFG_APP_FILTER` to the example and/or test you wish to run.  For instance, to run all of the examples you would set `U_CFG_APP_FILTER=example`, or to run all of the porting tests `U_CFG_APP_FILTER=port`, or to run a particular example `U_CFG_APP_FILTER=examplexxx`, where `xxx` is the start of the rest of the example name.  In other words, the filter is a simple partial string compare with the start of the example/test name.  Note that quotation marks must NOT be used around the value part.

You may set this compilation flag directly in `CMakeLists.txt` using e.g. `target_compile_definitions(app PRIVATE U_CFG_APP_FILTER=example)`, or you may set the compilation flag `U_CFG_OVERRIDE` and provide it in the header file `u_cfg_override.h` (which you must create) or you may use the mechanism described in the directory above to pass the compilation flag as an environment variable without modifying any files.

With that done follow the instructions in the directory above this to start Segger Embedded Studio and build/run the examples and/or unit tests.

# Hardware Requirements
In order to preserve valuable HW resources this code is configured to send trace output over the SWDIO (AKA RTT) port which a Segger J-Link debugger can interpret.

If you are using a Nordic based u-blox EVK such a debugger is already included on the board.  If you're working to a bare chip or on a bare u-blox module you should equip yourself with a Segger [J-Link Base](https://www.segger.com/products/debug-probes/j-link/models/j-link-base/) debugger and the right cable to connect it to your board.

For debugging you will need the Segger J-Link tools, of which the Windows ones can be found here:

https://www.segger.com/downloads/jlink/JLink_Windows.exe

To redirect the logging to the UART instead, remove the 'SEGGER RTT' section of `prj.conf` and the `uart0` section of the `.overlay` file for your board (in the `boards` sub-directory).

# Maintenance
When updating this build to a new version of the NRFConnect SDK change the release version stated in the introduction above.
