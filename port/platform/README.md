# Introduction
These directories provide the implementation of the porting layer on supported MCUs from various vendors.  The `common` directory contains anything that is common across platforms e.g. the `runner` code which allows any or all of the examples/tests to be run.

Also provided is a `lint` "platform".  This includes stubs for the porting layer and dummy configuration files in order that all of the `ubxlib` platform independent code can be passed through Lint.

# Structure
The MCU sub-directories under each vendor will include the following sub-directories:

- `sdk` - build files for the native SDKs for that platform.
- `src` - the `.c` files that implement the porting layer for that platform.
- `app` - the entry point, `main()`: configure the platform, start the RTOS and then run the chosen examples/tests in an RTOS task.
- `cfg` - configuration for that platform defining which MCU pin is connected to which module pin both for normal operation and for testing, stack sizes, task priorities, etc.  At least the following header files will be found for each platform:
  - `u_cfg_app_platform_specific.h`: information that is passed into the APIs from an application, e.g. defining what pin is connected to what.
  - `u_cfg_test_platform_specific.h`: as `u_cfg_app_platform_specific.h` but this time values to be used by the test application.
  - `u_cfg_os_platform_specific.h`: task priorities and stack sizes for the platform, built into this code.
  - `u_cfg_hw_platform_specific.`: HW definitions required by the platform and built into this code, e.g. which MCU HW blocks are used internally by the API implementation for timers etc.