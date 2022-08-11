# Introduction
This directory contains the build/configuration information for the ESP32 MCU under the Espressif [ESP-IDF framework](https://github.com/espressif/esp-idf).  The configuration here is sufficient to run the `ubxlib` tests and examples, no attempt is made to optimise the MCU RAM/flash etc. sizes, you need to know how to do that yourself.

# SDK Installation
Follow the instructions to build for the ESP-IDF platform:

https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html#get-started-step-by-step

The builds here are tested with the v4.3.2 release of ESP-IDF from [Github](https://github.com/espressif/esp-idf/releases/tag/v4.3.2).

# SDK Usage
You may override or provide conditional compilation flags to ESP-IDF without modifying the build file.  Do this by setting an environment variable `U_FLAGS`, e.g.:

```
set U_FLAGS=-DMY_FLAG
```

...or:

```
set U_FLAGS=-DMY_FLAG -DU_CFG_APP_PIN_CELL_ENABLE_POWER=-1
```

With this done, `cd` to your chosen build directory beneath this one to build and download your code.

# Integration With Your Application
To use this port in your ESP32 application you need to include the [port/platform/esp-idf/mcu/esp32/components](components) directory in your `EXTRA_COMPONENT_DIRS` and add `ubxlib` to `COMPONENTS`. As an example you can have a look at [runner/CMakeLists.txt](runner/CMakeLists.txt).  An `sdkconfig` configuration that allows the complete `ubxlib` test suite to be run can be found in [runner/sdkconfig.defaults](runner/sdkconfig.defaults) but the `ubxlib` core code requires no particular configuration beyond the default (just UART console output defaulting to HW block 0 for debugging); stack/heap should simply be configured as you require for your application.

If you aren't already familiar with the ESP-IDF build environment, here's a step-by-step example, based on the approach ESP-IDF suggests and assuming you want to use, for instance, a sockets connection with a u-blox cellular module:

- Copy the ESP32 `hello_world` example to somewhere convenient.
- Copy [port/platform/esp-idf/mcu/esp32/runner/CMakeLists.txt](/port/platform/esp-idf/mcu/esp32/runner/CMakeLists.txt) into the above.
- In this `CMakeLists.txt` file:
  - Remove `ubxlib_runner` from the `EXTRA_COMPONENT_DIRS` and `COMPONENTS` set-lines ('cos we aint building `runner` anymore).
  - Remove the `TEST_COMPONENTS` set-line ('cos we aint building tests anymore).
  - Change the `../components` path in the `EXTRA_COMPONENT_DIRS` set-line to where you have put the `ubxlib` components, i.e. `<path to the ubxlib root directory>/port/platform/esp-idf/mcu/esp32/components`.
  - Modify the `project()` line to have your project name in it :-).
- At a command prompt, `CD` to the above directory and run 
  - `<path to the esp-idf installation directory>/install.bat`
  - `<path to the esp-idf installation directory>/export.bat`
  - `idf.py build`
- This should build to completion, including all of the `ubxlib` core code and none of the `ubxlib` test code; in your build directory there will be a `build/esp-idf/ubxlib/libubxlib.a` file and the build output will end with something like:
```
Project build complete. To flash, run this command:
python.exe <path to the esp-idf installation directory>\components\esptool_py\esptool\esptool.py -p (PORT) -b 460800 --before default_reset --after hard_reset --chip esp32  write_flash --flash_mode dio --flash_size detect --flash_freq 40m 0x1000 build\bootloader\bootloader.bin 0x8000 build\partition_table\partition-table.bin 0x10000 build\<my project name>.bin
or run 'idf.py -p (PORT) flash'
```

Of course, it is not going to do anything useful because your `app_main()` still only has the ESP32 `hello_world` program in it.

- Go find [example/sockets/main.c](/example/sockets/main.c) and copy the contents of it into `main/hello_world_main.c`, replacing what is already there.
- Following the pattern described in the main [README.md](https://github.com/u-blox/ubxlib_priv#quick-start-guide), remove all the `ubxlib` test-related stuff in `hello_world_main.c`, i.e.:
  - Delete the inclusion of `u_short_range_test_selector.h`, we're not running tests anymore.
  - Delete the lines within and including `#ifndef U_CFG_DISABLE_TEST_AUTOMATION` (same reason).
  - Delete the lines within and including `#ifdef U_PORT_TEST_ASSERT`.
  - Delete the lines within and including `#ifndef U_PORT_TEST_FUNCTION`.
  - Delete the bit under `#if U_SHORT_RANGE_TEST_WIFI()` if you are only using cellular, making sure to leave the cellular bit there.
  - Replace the `U_PORT_TEST_FUNCTION()` line with just `void app_main()`.
  - Delete the lines within and including `#ifdef U_CFG_TEST_CELL_MODULE_TYPE` at the end of the file.
  - You should probably also search and replace `uPortLog` with `printf`, it is the same thing, and `#include "stdio.h"`.
- Open `main/CMakeLists.txt` and:
  - Assuming you want to use a SARA-R5 cellular module, and as an example of how to pass definitions into a project, let's pass the value of `U_CFG_TEST_CELL_MODULE_TYPE` into the project by adding the following line to `main/CMakeLists.txt` (the `-Wno-missing-field-initializers` is also added as we _deliberately_ leave structure fields uninitialised in the example code):
```
target_compile_options(${COMPONENT_TARGET} PUBLIC -DU_CFG_TEST_CELL_MODULE_TYPE=U_CELL_MODULE_TYPE_SARA_R5 -Wno-missing-field-initializers)
```
- Run `idf.py build` again and it should build to completion again; assuming you have an ESP32 board of some form to hand, plug it in and download to it, e.g.:
```
idf.py -p COM1 flash
```
- To see what it is up to, use the ESP-IDF `monitor` program, e.g.:
```
idf.py -p COM1 monitor
```
You should see it try to talk to a SARA-R5 cellular module on whatever pins happened to already be defined, e.g.:
```
U_CELL: initialising with enable power pin 2 (0x02) (where 1 is on), PWR_ON pin 25 (0x19) (and is toggled from 1 to 0) and VInt pin 36 (0x24) (and is 1 when module is on).
I (440) gpio: GPIO[25]| InputEn: 0| OutputEn: 1| OpenDrain: 1| Pullup: 1| Pulldown: 0| Intr:0
I (450) gpio: GPIO[2]| InputEn: 1| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (460) gpio: GPIO[36]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
U_CELL_PWR: powering on.
AT
AT
AT
AT
```
Sort your pins out, either by editing the configuration values in `hello_world_main.c` directly, or by editing the values of the macros in [port/platform/esp-idf/mcu/esp32/cfg/u_cfg_app_platform_specific.h](port/platform/esp-idf/mcu/esp32/cfg/u_cfg_app_platform_specific.h).

etc.

# Hardware Requirements
None aside from the standard ESP-IDF trace log point over UART.

# Chip Resource Requirements
None over and above those used for your chosen example/test, e.g. a UART to talk to a u-blox module.

# Maintenance
- When updating this build to a new version of ESP-IDF change the release version stated in the introduction above.
