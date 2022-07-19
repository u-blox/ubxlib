*** IMPORTANT: ONLY THE ESP32 CHIPSET IS SUPPORTED ON ARDUINO ***

# Preface
This platform allows `ubxlib` to be called from within Arduino code.  It does not provide a C++ API into `ubxlib`, does not "Arduino-ise" the `ubxlib` architecture, i.e. `ubxlib` still requires the RTOS world of tasks, mutexes, queues etc., which is provided in this case by the underlying ESP-IDF SDK.

# Introduction
Arduino is a software environment that offers simple C++ device drivers for a range of MCUs but does not including any operating-system-like features (tasks, mutexes, queues, etc.).  However ESP-IDF, which includes a port of FreeRTOS, formally supports Arduino and ESP-IDF is delivered as a pre-compiled library inside the Arduino environment, allowing calls to the native C Espressif world (and hence `ubxlib`) to be combined with that of Arduino.  The scripts here will allow you to build a `ubxlib` Arduino library so that you can call the `ubxlib` C API, along with the standard Arduino APIs, from your Arduino code running on an ESP32 chip (e.g. [NINA-W10](https://www.u-blox.com/en/product/nina-w10-series-open-cpu), Arduino board name `u-blox NINA-W10 series (ESP32)`, Fully Qualified Board Name `esp32:esp32:nina_w10`, or a [Sparkfun ESP32 MicroMod board](https://www.sparkfun.com/products/16781), FQBN `esp32:esp32:esp32micromod`, maybe mounted on a [SparkFun Asset Tracker](https://www.sparkfun.com/products/17272) board where you can use `ubxlib` to drive the [SARA-R510M8S](https://www.u-blox.com/en/product/sara-r5-series) cellular/GNSS module).

IMPORTANT: there are NO configuration files in this directory; the configuration files (`u_cfg_app_platform_specific.h`, `u_cfg_os_platform_specific.h`, `u_cfg_test_platform_specific.h` and `u_cfg_hw_platform_specific`) for this platform are the ones over in the [ESP-IDF platform](../esp-idf), so if you need to change the defaults for which pin of your MCU is connected to which pin of a u-blox module it is those files, e.g. [u_cfg_app_platform_specific.h](../esp-idf/mcu/esp32/cfg/u_cfg_app_platform_specific.h), you should edit or override.

Note that the [Arduino library format](https://arduino.github.io/arduino-cli/0.19/library-specification/#library-metadata) is relatively limited: it supports no source code paths, instead all of the source files beneath a directory named `src` are compiled (without exception), and hence building the `ubxlib` Arduino library involves first copying the `ubxlib` source code into a directory structure that makes sense to Arduino.  The Python scripts here will do that for you.

Note also that the whole of `ubxlib` is brought in as one big library; it would be difficult to split it up and then have Arduino's library system understand how to order the header files if they were brought in discretely.  You may, of course, manually delete unnecessary source files from the library if you wish.

# Limitations Of ESP-IDF Under Arduino
The ESP-IDF that is built into Arduino as a pre-compiled library is compiled by Espressif with a given [sdkconfig file](https://github.com/espressif/esp32-arduino-lib-builder/blob/master/sdkconfig.esp32).  Unless you re-build this ESP-IDF library from the ESP-IDF source code with [ESP32 Arduino lib builder](https://github.com/espressif/esp32-arduino-lib-builder) this is _fixed_; you cannot override values in it and it applies to all sketches (i.e. it is not possible to provide an `sdkconfig` file on a per-sketch basis, it is global).

This means that, for instance, some stack sizes and task priorities are fixed, the usage of some pins is fixed (e.g. GPIO 36 can only be an RTC input pin) and the RTOS tick duration moves from 10 ms to 1 ms (despite this being less efficient).

# SDK Installation
Follow the Espressif instructions to install Arduino and ESP32 support within it:

https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html

This code was tested with `arduino-cli` version 0.20.2.

IMPORTANT: if you are using a NINA-W102 board, the ESP32-provided stuff at the usual link for the "additional Board Manager URLs" as described at the link above is no longer compatible as it defaults to 4 Mbytes flash (for NINA-W106) whereas NINA-W102 only has 2 Mbytes.  To fix this, if you are using `arduino-cli` then add the following build property to the command-line when you build:

`--build-property build.partitions=minimal`

If you are unable to do this you may use instead use the following URL when configuring `arduino-cli`; this is a link to the commit just before the incompatibility was introduced:

https://raw.githubusercontent.com/espressif/arduino-esp32/7517e4b99283abf3048cfea76e4715f3340fa4d6/package_esp32_index.json

...or alternatively, when you call `arduino-cli` to install the ESP32 platform, specify version 2.0.3:

`arduino-cli core install esp32:esp32@2.0.3`

Python 3 is also required.

# Building The `ubxlib` Arduino Library
First, you need to run the script [u_arduino.py](u_arduino.py).  This will create a copy of the `ubxlib` files in a form that Arduino can understand in a sub-directory that is by default named `ubxlib`.  The [u_arduino.py](u_arduino.py) script takes various optional command-line parameters; use `-h` to see a list.  You can ask it to create the library in the Arduino IDE's global libraries folder; e.g. `CD` to this directory and, on Windows, you would run something like this:

```
python u_arduino.py -o C:\Users\myusername\Documents\Arduino\libraries\ubxlib
```

With that done, start the Arduino IDE and configure it for your board (e.g. for a NINA-W1 module you would select the `u-blox NINA-W10 series (ESP32)` board).  Then select `Sketch` -> `Include Library` and you should find listed the "ubxlib" library that was just created.  Add the library to an empty sketch and you will find that a `ubxlib.h` file is included in the sketch, which in turn brings in all the public header files of `ubxlib`, allowing you to access any and all of the `ubxlib` APIs.  Select `Sketch` -> `Verify/Compile` and the library should compile and link with the empty sketch.

The files in the `Arduino\libraries\ubxlib` folder created above are "expendable", i.e. they are copies of the files from the `ubxlib` directory that you can delete, edit, etc.  If you run [u_arduino.py](u_arduino.py), as above, again, any updated/missing files from the `ubxlib` directories will be copied in, any changed files in `Arduino\libraries\ubxlib` will be left alone.  To recreate a fresh, clean, library simply delete the `Arduino\libraries\ubxlib` directory and run the [u_arduino.py](u_arduino.py) script, as above, again.

# Using The `ubxlib` Arduino Library
You should **NOT** use the [app.ino](app/app.ino) sketch as an example application; it is simply a mechanism to run the `ubxlib` C-code tests/examples (see below), it does not use the Arduino `loop()` function (it kicks off an RTOS task from the `setup()` function and runs the examples/tests from there).

You should instead look at the example sketch [sockets.ino](/example/sockets/sockets.ino) which will have been copied into the `examples` sub-directory of the library created by [u_arduino.py](u_arduino.py) above; this example can be loaded through the Arduino IDE in the usual way.  To use this example with a cellular module you *MUST* define a value for `U_CFG_TEST_CELL_MODULE_TYPE` (since we can't know which module you are using); see the comments in [sockets.ino](/example/sockets/sockets.ino#L31) for how to do this.  You may also need to add a value for the `APN` field in the `config` structure if the network-assigned default does not work for you; again see the comments in [sockets.ino](/example/sockets/sockets.ino#L45) for how to do this.  Note that you do not need to call the Arduino `Serial` or `Digital I/O` APIs on behalf of `ubxlib`; all of the required IO configuration/usage is performed within the `ubxlib` C code using the underlying ESP-IDF SDK.

# Running The `ubxlib` Tests
If you wish to run the `ubxlib` tests, the sketch [app.ino](app/app.ino) is provided.  First you need to create an Arduino library of the `.c` test files by running the script [u_arduino_test.py](u_arduino_test.py) (similar to the above), e.g. `CD` to this directory and, on Windows, run:

```
python u_arduino_test.py -o C:\Users\myusername\Documents\Arduino\libraries\ubxlib_test
```

With that done load the [app.ino](app/app.ino) sketch into the Arduino IDE and select `Sketch` -> `Upload` to build it and load it onto your board.  Once it is loaded select `Tools` -> `Serial Monitor` and you should see the standard ESP-IDF menu system of tests (you may need to send a newline to the target first).  Obviously to run your own application you do not need this test libary, just the files that the [u_arduino.py](u_arduino.py) script brought in.

# Maintenance
- When updating the version of `arduino-cli` we test with, update the version number above.
- When a new source file or header search path is added for the ESP-IDF platform, update [source.txt](source.txt) and, if necessary, [include.txt](include.txt).
- Otherwise, when a new non-test, non-example source file or header search path is added, update [source.txt](source.txt) and, if necessary, [include.txt](include.txt), ignoring any files related only to tests/examples or to platform code.
- Finally, when a new test or example source file is added, update [source_test.txt](source_test.txt) and, if necessary, [include_test.txt](include_test.txt).
