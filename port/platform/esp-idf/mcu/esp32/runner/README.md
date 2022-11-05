# Introduction
This directory contains a build which compiles and runs any or all of the examples and tests for the native Espressif build system, ESP-IDF.  It does so using the native ESP-IDF unit test mechanism.

# Usage
Make sure you have followed the instructions in the directory above this to install the ESP-IDF tools.

Since we usually run under automation, we have the task watchdog switched on in `sdkconfig.defaults`:

```
CONFIG_ESP_TASK_WDT=y
```

When running manually you probably don't want this, or the target will panic before you press a key to select a test, so you might want to change it to:

```
CONFIG_ESP_TASK_WDT=n
```

...either in the generated `sdkconfig` after you have configured/built, or in our `sdkconfig.defaults` before you do your first build.  

Then, to build and download this code, execute the following:

```
idf.py -p COMx -D TEST_COMPONENTS="ubxlib_runner" flash monitor
```

...where `COMx` is replaced by the COM port to which your ESP32 board is attached. The command adds this directory to ESP-IDF as an ESP-IDF component and requests that the examples and tests for this component are built, downloaded to the board and run.

If you have specified build flags using the `U_FLAGS` environment variable then, during the build, check that you see a line something like the following somewhere near the start of the build:

```
ubxlib: added -DYOUR_FLAGS_HERE due to environment variable U_FLAGS.
runner: added -DYOUR_FLAGS_HERE due to environment variable U_FLAGS.
```

e.g.:

```
-- Adding linker script C:/projects/esp32/cellular/port/platform/espressif/esp32/sdk/esp-idf/unit_test/build/esp-idf/esp32/esp32_out.ld
-- Adding linker script C:/projects/esp32/esp-idf/components/esp32/ld/esp32.rom.ld
-- Adding linker script C:/projects/esp32/esp-idf/components/esp32/ld/esp32.peripherals.ld
-- Adding linker script C:/projects/esp32/esp-idf/components/esp32/ld/esp32.rom.libgcc.ld
-- Adding linker script C:/projects/esp32/esp-idf/components/esp32/ld/esp32.rom.spiram_incompatible_fns.ld
runner: added -DMY_FLAG;-DU_CFG_APP_PIN_CELL_ENABLE_POWER=-1; due to environment variable U_FLAGS.
ubxlib: added -DMY_FLAG;-DU_CFG_APP_PIN_CELL_ENABLE_POWER=-1; due to environment variable U_FLAGS.
```

If you do not then the build has not picked up the `U_FLAGS` environment variable for some reason.  This can happen if you are somehow not running the Python version that comes with `esp-idf`, in which case you might try running:

```
python idf.py -p COMx -D TEST_COMPONENTS="ubxlib_runner" flash monitor
```

...instead to make sure that Windows picks up the first version of Python on the path rather than the one associated with the `.py` extension in the Windows registry.  If that doesn't work you can also try adding `reconfigure` to the `idf.py` command-line, i.e.:

```
idf.py  -p COMx -D TEST_COMPONENTS="ubxlib_runner" flash monitor reconfigure
```

When the code has built and downloaded, the Espressif monitor terminal will be launced on the same `COMx` port at 115200 baud and the board will be reset.  If you prefer to use your own serial terminal program then omit `monitor` from the command line above and launch your own serial terminal program instead.  You should see a prompt:

```
Press ENTER to see the list of tests.
```

Press ENTER and all of the examples and the tests will be listed, something like:

```
Here's the test menu, pick your combo:
(1)     "examplexxx" [example]
(2)     "portInit" [port]
(3)     "portGpio" [port]

Enter test for running.
```

Press `1` followed by ENTER to run example or test number 1, `\*` to run all, `[example]` to run just the items in the example category, etc.
