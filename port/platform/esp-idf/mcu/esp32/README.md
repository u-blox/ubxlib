# Introduction
This directory contains the build/configuration information for the ESP32 MCU under the Espressif ESP-IDF platform.  The configuration here is sufficient to run the ubxlib tests and examples, no attempt is made to optimise the MCU RAM/flash etc. sizes, you need to know how to do that yourself.

# SDK Installation
Follow the instructions to build for the ESP-IDF platform:

https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html#get-started-step-by-step

The builds here have been tested with release 4.1 of ESP-IDF.

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

# Hardware Requirements
None aside from the standard ESP-IDF trace log point over UART.

# Chip Resource Requirements
None over and above those used for your chosen example/test, e.g. a UART to talk to a u-blox module.

# Maintenance
- When updating this build to a new version of ESP-IDF change the release version stated in the introduction above.
- When adding a new API, update the `COMPONENT_ADD_INCLUDEDIRS`, `COMPONENT_SRCS` and if necessary the `COMPONENT_PRIV_INCLUDEDIRS` variables in the `CMakeLists.txt` file in this directory.