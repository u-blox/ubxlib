# Introduction
This directory and its sub-directories contain the build infrastructure for Nordic NRF52 building under Segger Embedded Studio IDE.

# SDK Installation
Follow the instructions to install the development tools:

https://infocenter.nordicsemi.com/topic/ug_nrf52840_dk/UG/common/nordic_tools.html

Make sure you install Segger Embedded Studio, the Nordic nRF5 SDK (the builds here have been tested with version 17) and the Nordic command-line tools.  Ensure NO SPACES in the install location of the nRF5 SDK.

When you install the nRF5 SDK, if you install it to the same directory as you cloned this repo with the name `nrf5`, i.e.:

```
..
.
nrf5
ubxlib
```

...then the builds here will find it, otherwise you will need to tell the builds where you have installed it (see below).

# SDK Usage
You may override or provide conditional compilation flags to Segger Embedded Studio without modifying the build file.  Do this by passing in a flag called `U_FLAGx`, where `x` is a number from 0 to 19, e.g. `U_FLAG0` into the command line to Segger Embedded Studio when you start it up, e.g.:

```
"C:\Program Files\Segger\SEGGER Embedded Studio for ARM 4.50\bin\emstudio" -D U_FLAG0=MY_FLAG
```

```
"C:\Program Files\Segger\SEGGER Embedded Studio for ARM 4.50\bin\emstudio" -D U_FLAG0=MY_FLAG -D U_FLAG1="U_CFG_APP_PIN_CELL_ENABLE_POWER=-1"
```

...noting the way quotation marks are used.

If you have installed the nRF5 SDK to somewhere other than the location mentioned above, you must also set the variable `NRF5_PATH` in the command line to Segger Embedded Studio, e.g. something like:

```
"C:\Program Files\Segger\SEGGER Embedded Studio for ARM 4.50\bin\emstudio" -D NRF5_PATH=c:/nrf5
```

....noting the use of `/` instead of `\`.

So, start Segger Embedded Studio as indicated above and follow their instructions to open/build/download/run one of the projects in the directories below on a connected NINA-B3 EVK, NRF52840 DK development board or a bare chip connected via a Segger JLink box.

# Maintenance
- When updating this build to a new version of the nRF5 SDK change the release version stated in the introduction above.
- When adding a new API, find any `.emPoject` files in folders under this directory and, in those files, update the `c_user_include_directories` variable and add new `<file />` item(s) within the `ubxlib` section for any new `.c` files.