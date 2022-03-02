# Introduction
These directories provide the implementation of the porting layer on the Zephyr platform.  Instructions on how to install the necessary tools and perform the build can be found in the [runner](runner) directory.  Though this is intended to become a generic Zephyr platform, at the moment it supports only Nordic MCUs as they require a **specific** version/configuration of Zephyr.

Note: the directory structure here differs from that in the other platform directories in order to follow more closely the approach adopter by Zephyr, which is hopefully familiar to Zephyr users.

- [app](app): contains the code that runs the test application (both examples and unit tests) on the Zephyr platform.
- [cfg](cfg): contains the configuration files for the MCU, the OS, for the application and for testing (mostly which MCU pins are connected to which module pins).
- [src](src): contains the implementation of the porting layers for Zephyr platform.
- [runner](runner): contains the test application configuration and build files for the MCUs supported on the Zephyr platform.
- [boards](boards): contains custom u-blox boards that are not \[yet\] in the Zephyr repo.

# SDK Installation
`ubxlib` is tested with `nRFConnect SDK version 1.6.1` which is the recommended version.

Follow the instructions to install the development tools:

- Install nRF connect. https://www.nordicsemi.com/Software-and-Tools/Development-Tools/nRF-Connect-for-desktop
- Start nRFConnect and use the tool chain manager to install the recommended SDK version (see above).
- IMPORTANT!: Update SDK and toolchain using the dropdown menu for your SDK version.

# Integration
`ubxlib` is a [Zephyr module](https://docs.zephyrproject.org/latest/guides/modules.html).
To add `ubxlib` to your Zephyr application you can make use of [ZEPHYR_EXTRA_MODULES](https://docs.zephyrproject.org/latest/guides/modules.html#integrate-modules-in-zephyr-build-system). By adding the following line **to the top** of your existing CMakeLists.txt, Zephyr should pickup `ubxlib`:
```cmake
list(APPEND ZEPHYR_EXTRA_MODULES <PATH_TO_UBXLIB_DIRECTORY>)
```
You must then also enable `UBXLIB` either via [menuconfig](https://docs.zephyrproject.org/latest/guides/build/kconfig/menuconfig.html#menuconfig) or by adding the following line to your `prj.conf`:
```
CONFIG_UBXLIB=y
```
`ubxlib` also requires some Zephyr config to be enabled, but you currently need to check [runner/prj.conf](runner/prj.conf) to get these correct.

# SDK Usage

## Segger Embedded Studio
When you install nRFConnect SDK for Windows you will get a copy of Segger Embedded Studio (SES).
You can either start SES from the nRF Toolchain Manager by clicking "Open Segger Embedded Studio" or by running `toolchain/SEGGER Embedded Studio.cmd` found in your installation of nRFconnect SDK.
- Always load project from SES using file->Open nRF connect SDK project
- Select the project folder containing the `CMakeLists.txt` of the application you want to build.
- Board file should be `{your_sdk_path}/zephyr/boards/arm/nrf5340dk_nrf5340` for EVK-NORA-B1.
  For a custom board e.g. `port/platform/zephyr/boards/short_range/zephyr/boards/arm/ubx_evkninab4_nrf52833`
- Board name should be `nrf5340dk_nrf5340_cpuapp` for EVK-NORA-B1.  For a custom board e.g. `ubx_evkninab4_nrf52833`.
- Where a board-specific configuration file is available (e.g. `ubx_evkninab4_nrf52833.conf`) this will be picked up automatically.

## Device Tree
Zephyr pin choices for any HW peripheral managed by Zephyr (e.g. UART, I2C, SPI, etc.) are made at compile-time in the Zephyr device tree, they cannot be passed into the `ubxlib` functions as run-time variables.  Look in the `zephyr/zephyr.dts` file located in your build directory to find the resulting pin allocations for these peripherals.
If you want to find out more about device tree please see Zephyr [Introduction to devicetree](https://docs.zephyrproject.org/latest/guides/dts/intro.html)

## Important UART Note
Since pin assignment for UARTs are made in the device tree, functions such as `uPortUartOpen()` which take pin assignments as parameters, should have all the pins set to -1.
You can look through the resulting `zephyr/zephyr.dts` located in your build directory to find the UART you want to use.
The UARTs will be named `uart0`, `uart1`, ... in the device tree - the ending number is the value you should use to tell `ubxlib` what UART to open.

## Additional Notes
- Zephyr is assumed to use its internal minimal C library, not [newlib](https://sourceware.org/newlib/libc.html); this is because the Zephyr integration of [newlib](https://sourceware.org/newlib/libc.html) is not threadsafe.  If, at some point in the future, the integration is made thread-safe, or if you make it thread-safe yourself, and you wish to use [newlib](https://sourceware.org/newlib/libc.html) then you should configure Zephyr appropriately and then add `U_CFG_ZEPHYR_USE_NEWLIB` to the conditional compilation flags passed into the build (see below for how to do this without modifying `CMakeLists.txt`).
- Always clean the build directory when upgrading to a new ubxlib version.
- You may override or provide conditional compilation flags to CMake without modifying `CMakeLists.txt`.  Do this by setting an environment variable `U_FLAGS`, e.g.:
  
  ```
  set U_FLAGS=-DMY_FLAG
  ```
  
  ...or:
  
  ```
  set U_FLAGS=-DMY_FLAG -DU_CFG_APP_PIN_CELL_ENABLE_POWER=-1
  ```
