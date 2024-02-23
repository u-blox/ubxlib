# Introduction
This directory contains a build configuration for building the test runner application for STM32F7 with native Zephyr.  

# Build And Flash With `west`
While in the `port/platform/zephyr/runner_stm32` directory, for example:

  ```
  west build -p auto -b nucleo_f767zi . --build-dir build_nucleo_f767zi
  west flash --build-dir nucleo_f767zi
  ```

# Usage
Make sure you have followed the instructions in the [zephyr port README.md](../README.md) to install Zephyr.

By default all of the examples and tests supported by this platform will be executed.  To execute just a subset set the conditional compilation flag `U_CFG_APP_FILTER` to the example and/or test you wish to run.  For instance, to run all of the examples you would set `U_CFG_APP_FILTER=example`, or to run all of the porting tests `U_CFG_APP_FILTER=port`, or to run a particular example `U_CFG_APP_FILTER=examplexxx`, where `xxx` is the start of the rest of the example name.  In other words, the filter is a simple partial string compare with the start of the example/test name.  Note that quotation marks must NOT be used around the value part.

You may set this compilation flag directly in `CMakeLists.txt` using e.g. `target_compile_definitions(app PRIVATE U_CFG_APP_FILTER=example)`, or you may set the compilation flag `U_CFG_OVERRIDE` and provide it in the header file `u_cfg_override.h` (which you must create) or you may use the mechanism described in the directory above to pass the compilation flag as an environment variable without modifying any files.

## Devicetree Overlay
If you need to change the pin assignment of a peripheral you can do this using an `.overlay` file which will be picked up automatically by Zephyr when they are placed in `boards` sub-folder.
Please see the existing overlay files in the [boards](boards) directory. You will find more details on how to use `.overlay` files in [Zephyr device tree documention](https://docs.zephyrproject.org/latest/guides/dts/howtos.html#set-devicetree-overlays).

# Hardware Requirements
Since STM32 MCUs have many UARTs and `west` doesn't currently support reading traces over SWDIO, logging is through the Zephyr console UART.