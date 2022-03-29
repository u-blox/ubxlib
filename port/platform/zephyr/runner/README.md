# Introduction
This directory contains build configurations for the test runner application based on nRFConnect SDK.  The methods described here are those for Segger Embedded Studio (SES) and for `west`, the Zephyr command-line build tool.  The configuration here is sufficient to run the `ubxlib` tests and examples, no attempt is made to optimise the MCU RAM/flash etc. sizes, you need to know how to do that yourself.

# Segger Embedded Studio
Please see the [Segger Embedded Studio section in zephyr port README.md](../README.md#Segger_Embedded_Studio).

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
Make sure you have followed the instructions in the [zephyr port README.md](../README.md) to install nRFConnectSDK and toolchain.

By default all of the examples and tests supported by this platform will be executed.  To execute just a subset set the conditional compilation flag `U_CFG_APP_FILTER` to the example and/or test you wish to run.  For instance, to run all of the examples you would set `U_CFG_APP_FILTER=example`, or to run all of the porting tests `U_CFG_APP_FILTER=port`, or to run a particular example `U_CFG_APP_FILTER=examplexxx`, where `xxx` is the start of the rest of the example name.  In other words, the filter is a simple partial string compare with the start of the example/test name.  Note that quotation marks must NOT be used around the value part.

You may set this compilation flag directly in `CMakeLists.txt` using e.g. `target_compile_definitions(app PRIVATE U_CFG_APP_FILTER=example)`, or you may set the compilation flag `U_CFG_OVERRIDE` and provide it in the header file `u_cfg_override.h` (which you must create) or you may use the mechanism described in the directory above to pass the compilation flag as an environment variable without modifying any files.

With that done follow the instructions in [zephyr port README.md](../README.md) to start Segger Embedded Studio and build/run the examples and/or unit tests.

## Devicetree Overlay
If you need to change the pin assignment of a peripheral you can do this using an `.overlay` file which will be picked up automatically by Zephyr when they are placed in `boards` sub-folder.
Please see the existing overlay files in the [boards](boards) directory. You will find more details on how to use `.overlay` files in [Zephyr device tree documention](https://docs.zephyrproject.org/latest/guides/dts/howtos.html#set-devicetree-overlays).

# Hardware Requirements
In order to preserve valuable HW resources this code is configured to send trace output over the SWDIO (AKA RTT) port which a Segger J-Link debugger can interpret.

If you are using a Nordic based u-blox EVK such a debugger is already included on the board.  If you're working to a bare chip or on a bare u-blox module you should equip yourself with a Segger [J-Link Base](https://www.segger.com/products/debug-probes/j-link/models/j-link-base/) debugger and the right cable to connect it to your board.

For debugging you will need the Segger J-Link tools, of which the Windows ones can be found here:

https://www.segger.com/downloads/jlink/JLink_Windows.exe

To redirect the logging to the UART instead, remove the 'SEGGER RTT' section of `prj.conf` and the `uart0` section of the `.overlay` file for your board (in the `boards` sub-directory).

# Maintenance
When updating this build to a new version of the NRFConnect SDK change the release version stated in the introduction above.
