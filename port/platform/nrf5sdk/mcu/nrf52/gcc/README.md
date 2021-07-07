# Introduction
This directory and its sub-directories contain the build infrastructure for Nordic NRF52 building under GCC with Make.

# SDK Installation
The blog post at the link below describes how to install GCC for building the Nordic platform:

https://devzone.nordicsemi.com/nordic/nordic-blog/b/blog/posts/development-with-gcc-and-eclipse

However it expects you to be using Eclipse; the instructions that follow are modified to work wholly from the command-line (and, in this case, on Windows).

First, install a version of GCC for ARM from here (the builds here have been tested with version `10 2020-q4-major`):

https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads

Next obtain a version of Make and add it to your path.  A Windows version can be obtained from here:

http://gnuwin32.sourceforge.net/packages/make.htm

Install a version of the nRF5 SDK from here:

https://www.nordicsemi.com/Software-and-Tools/Software/nRF5-SDK

The builds here have been tested with version 17 of the nRF5 SDK.

If you install it to the same directory as you cloned this repo with the name `nrf5`, i.e.:

```
..
.
nrf5
ubxlib
```

...then the builds here will find it else you will need to tell `make` about its location by adding, for instance, `NRF5_PATH=c:/nrf5` on the command-line to `make`.

In the `components\toolchain\gcc` sub-directory of the nRF5 installation you will find two makefiles: if you are running on Linux or OS X you need to pay attention to the `.posix` one else pay attention to the `.windows` one.  Edit the appropriate `Makefile` to set the `GNU_INSTALL_ROOT` variable to the location of the `bin` directory of your GCC installation, e.g.:

```
GNU_INSTALL_ROOT := C:/Program Files (x86)/GNU Arm Embedded Toolchain/10 2020-q4-major/bin/
GNU_VERSION := 10.2.1
GNU_PREFIX := arm-none-eabi
```

Note the use of `/` and not `\`; no quotation marks are required but a final `/` is required.

You will also need to have installed the Nordic command-line tools from here if you haven't done so already and have these on your path:

https://www.nordicsemi.com/Software-and-Tools/Development-Tools/nRF-Command-Line-Tools

With this done, go to the relevant sub-directory of this directory to actually build something.

# SDK Usage
You may override or provide conditional compilation flags to this build without modifying the build file.  Do this by adding them to a `CFLAGS` variable passed in via the `make` command-line, e.g.:

`make flash CFLAGS=-DU_CFG_APP_PIN_CELL_ENABLE_POWER=-1`

...or:

`make flash CFLAGS="-DU_CFG_APP_PIN_CELL_ENABLE_POWER=-1 -DMY_FLAG"`

Note the use of quotation marks when sending more than one conditional compilation flag, otherwise the second `-D` will appear as another parameter to `make` which will just cause it to pause for 30 seconds.

`make flash` will build the code and download it to a connected NINA-B3 EVK, NRF52840 DK development board or a bare chip connected via a Segger JLink box.

# Trace Output
To obtain trace output using Segger RTT, start JLink Commander from a command prompt with:

```
jlink -Device NRF52840_XXAA -If SWD -Speed 500 -Autoconnect 1
```

With this done, from a separate command prompt start `JLinkRTTClient` and it will find the JLink session and connect up to display trace output.  The first run of the target after programming or power-on for some reason takes about 10 seconds to start.

To reset the target, in `jlink` user `r` to stop it and `g` to kick it off again.

# Maintenance
- When updating this build to a new version of the nRF5 SDK change the release version stated in the introduction above.
- When adding a new API, update the `SRC_FILES` and `INC_FOLDERS` variables in the `Makefile` of each build under this directory.