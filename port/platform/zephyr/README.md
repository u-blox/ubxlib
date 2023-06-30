# Introduction
These directories provide the implementation of the porting layer on the Zephyr platform.  Instructions on how to install the necessary tools and perform the build can be found in the [runner](runner) directory for Nordic platforms and the [runner_linux](runner_linux) directory for Linux/Posix.  While this should, generically, work with any Zephyr board, you should note that we only test with the following HW:

- Nordic MCUs, which require a **specific** version/configuration of Zephyr,
- Linux/posix, for debugging/development only, just like [windows](../windows).

Note: the directory structure here differs from that in the other platform directories in order to follow more closely the approach adopter by Zephyr, which is hopefully familiar to Zephyr users.

- [app](app): contains the code that runs the test application (both examples and unit tests) on the Zephyr platform.
- [cfg](cfg): contains the configuration files for the MCU, the OS, for the application and for testing (mostly which MCU pins are connected to which module pins).
- [src](src): contains the implementation of the porting layers for Zephyr platform on Nordic chips and on Linux/posix.
- [runner](runner): contains the test application configuration and build files for the Nordic MCUs supported on the Zephyr platform.
- [runner_linux](runner_linux): contains the test application configuration and build files for Linux/Posix on the Zephyr platform.
- [boards](boards): contains custom u-blox boards that are not \[yet\] in the Zephyr repo.

# SDK Installation (NRF Connect)
`ubxlib` is tested with the version of Zephyr that comes with `nRFConnect SDK version 2.2.0` which is the recommended version; it is intended to build with all versions nRFConnect SDK from 1.6.1 up til 2.2.0.

Follow the instructions to install the development tools:

- Install nRF connect. https://www.nordicsemi.com/Software-and-Tools/Development-Tools/nRF-Connect-for-desktop
- Start nRFConnect and use the tool chain manager to install the recommended SDK version (see above).
- IMPORTANT: update SDK and toolchain using the dropdown menu for your SDK version.

If you intend to use Zephyr on Linux/posix then you must also follow the instructions here:

https://docs.zephyrproject.org/latest/boards/posix/native_posix/doc/index.html

# Integration
`ubxlib` is a [Zephyr module](https://docs.zephyrproject.org/latest/guides/modules.html).

To add `ubxlib` to your Zephyr application you can either add it to your west.yml, or make use of [ZEPHYR_EXTRA_MODULES](https://docs.zephyrproject.org/latest/guides/modules.html#integrate-modules-in-zephyr-build-system).

To add it to your west.yml, add the u-blox remote to your remotes section and the ubxlib module to the projects section:
```yml
  remotes:
    - name: u-blox
      url-base: git@github.com:u-blox

  projects:
    - name: ubxlib
      remote: u-blox
      path: ubxlib
      revision: v1.2.0
      import: true
      clone-depth: 1
```

You must then also enable `UBXLIB` either via [menuconfig](https://docs.zephyrproject.org/latest/guides/build/kconfig/menuconfig.html#menuconfig) or by adding the following line to your `prj.conf`:

```
CONFIG_UBXLIB=y
```

`ubxlib` also requires some Zephyr config to be enabled: check [default.conf](default.conf) for the basics or, for details to do with BLE configuration or including I2C/SPI etc., check [runner/prj.conf](runner/prj.conf)/[runner_linux/prj.conf](runner_linux/prj.conf).

If you wish to build the default `runner` build and associated tests _without_ nRF Connect SDK then you will need to bring in a copy of Unity from somewhere, identifying the Unity directory by defining the environment variable `UNITY_PATH`, e.g. `UNITY_PATH=/home/ubxlib/unity`.

# SDK Usage

## Nordic MCUs: Segger Embedded Studio
When you install nRFConnect SDK for Windows you will get a copy of Segger Embedded Studio (SES).

You can either start SES from the nRF Toolchain Manager by clicking "Open Segger Embedded Studio" or by running `toolchain/SEGGER Embedded Studio.cmd` found in your installation of nRFconnect SDK.

- Always load project from SES using file->Open nRF connect SDK project
- Select the project folder containing the `CMakeLists.txt` of the application you want to build.
- Board file should be `{your_sdk_path}/zephyr/boards/arm/nrf5340dk_nrf5340` for EVK-NORA-B1.
- Board name should be `nrf5340dk_nrf5340_cpuapp` for EVK-NORA-B1.  For a custom board e.g. `ubx_evkninab4_nrf52833`.
- Where a board-specific configuration file is available (e.g. `ubx_evkninab4_nrf52833.conf`) this will be picked up automatically.

## Linux/Posix
With this code fetched to your Linux machine all you have to do is set the correct UART number for any u-blox module which is attached to that machine.  When the executable starts running it will print something like:

```
UART_1 connected to pseudotty: /dev/pts/5
UART_0 connected to pseudotty: /dev/pts/3
```

This indicates that two UARTs, 0 and 1 (the maximum for Linux/Posix builds) are available and that they emerge on the corresponding pseudo-terminals; these pseudo-terminals are assigned by the operating system and may change on each run.  You can use the Linux utility `socat` to redirect the pseudo-terminals to real devices.  For instance:

```
socat /dev/pts/5,echo=0,raw /dev/tty/0,echo=0,raw
```

...would redirect `/dev/pts/5` to `/dev/tty/0` so, using the example above, that means UART 1 will be effectively on `/dev/tty/0`, or:

```
socat /dev/pts/3,echo=0,raw /dev/pts/3,echo=0,raw
```

...would loop `/dev/pts/3` (in the example above UART 0) back on itself.

## Device Tree
Zephyr pin choices for any HW peripheral managed by Zephyr (e.g. UART, I2C, SPI, etc.) are made at compile-time in the Zephyr device tree, they cannot be passed into the functions as run-time variables.  Look in the `zephyr/zephyr.dts` file located in your build directory to find the resulting pin allocations for these peripherals.

If you want to find out more about device tree please see Zephyr [Introduction to devicetree](https://docs.zephyrproject.org/latest/guides/dts/intro.html)

You will, though, still need to pass into `ubxlib` the HW block that is used: e.g. UART 0, UART 1, etc.  The UARTs, for instance, will be named `uart0`, `uart1`... in the device tree; the ending number is the value you should use to tell `ubxlib` what device to open.

## Additional Notes
- Always clean the build directory when upgrading to a new `ubxlib` version.
- You may override or provide conditional compilation flags to CMake without modifying `CMakeLists.txt`.  Do this by setting an environment variable `U_FLAGS`, e.g.:

  ```
  set U_FLAGS=-DMY_FLAG
  ```

  ...or:

  ```
  set U_FLAGS=-DMY_FLAG -DU_CFG_APP_PIN_CELL_ENABLE_POWER=-1
  ```
