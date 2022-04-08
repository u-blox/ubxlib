IMPORTANT: the directory structure under platform remains subject to change as we add new platforms.  We aim to settle this structure as `ubxlib` approaches release 1.0.0, likely towards the end of 2021.

# Introduction
These directories provide the implementation of the porting layer on supported SDKs/MCUs from various vendors.  The [platform/common](common) directory contains anything that is common across platforms e.g. the [platform/common/runner](common/runner) source code which allows any or all of the examples/tests to be run.

Also provided are [lint](lint) and [static_size](static_size) "platforms": both include stubs for the porting layer and dummy configuration files in order that all of the `ubxlib` platform independent code can be passed through Lint and measured for \[static\] flash/RAM size.

# Supported MCUs
The MCUs supported by the platforms are as follows:

- Espressif [ESP-IDF](esp-idf) and [Arduino-ESP32](arduino): ESP32.
- ST Microelectronics' [STM32Cube IDE](stm32cube): STM32F4.
- Nordic [nRF5 SDK](nrf5sdk): NRF52.
- [zephyr](zephyr): NRF52/NRF53, and also Linux/Posix for development/test purposes.
- not really an MCU but [windows](windows) is supported for development/test purposes.

# Structure
Each platform sub-directory includes the following items:

- `src`: the `.c` files that implement the porting layer for the platform.
- `app`: the entry point, `main()`: configure the platform, start the RTOS and then run the chosen examples/tests in an RTOS task.
- `u_cfg_os_platform_specific.h`: task priorities and stack sizes for the platform, built into this code.
- `mcu/<mcu>`: build files and configuration files for the MCUs supported on the platform.
  - `cfg`: configuration defining which MCU pin is connected to which module pin both for normal operation and for testing, stack sizes, task priorities, etc.  At least the following header files will be found for each platform:
    - `u_cfg_app_platform_specific.h`: information that is passed into the APIs from an application, e.g. defining what pin is connected to what.
    - `u_cfg_test_platform_specific.h`: as `u_cfg_app_platform_specific.h` but this time values to be used by the test application.
    - `u_cfg_hw_platform_specific.h`: HW definitions required by the platform and built into this code, e.g. which MCU HW blocks are used internally by the API 
  - `runner`: build metadata that will build all of the unit tests and examples for the given MCU.