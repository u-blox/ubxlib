# Introduction
This directory contains a Makefile for the `ubxlib` test runner, which runs any or all of the examples and tests for the STM32F4 MCU.

# Dependencies
The test runner uses `ThrowTheSwitch Unity` test framework. You will need to clone a copy of this framework in order to compile the `ubxlib` test runner:

```sh
git clone https://github.com/ThrowTheSwitch/Unity
```

# Usage

## Compiling
The test runner application is Makefile based so you need to have `make` installed. In order to build you must specify the path of STM32Cube_FW_F4 (see [SDK_Installation](../README.md#SDK_Installation)) and Unity (see [Dependencies](./README.md#Dependencies])):

```sh
make STM32CUBE_FW_PATH=<PATH_TO_STM32Cube_FW_F4> UNITY_PATH=<PATH_TO_UNITY>
```

If you are building the code as an imported Makefile project from within the STM32Cube IDE then you should define the values for `STM32CUBE_FW_PATH` and `UNITY_PATH` in the `C/C++ Build`/`Environment` section of the project properties.

## Compilation flags
You may override or provide conditional compilation flags to this build without modifying the build file.  Do this by adding them to a `CFLAGS` variable passed in via the `make` command-line, e.g.:

`make CFLAGS=-DU_CFG_APP_PIN_CELL_ENABLE_POWER=-1`

...or:

`make CFLAGS="-DU_CFG_APP_PIN_CELL_ENABLE_POWER=-1 -DMY_FLAG -DHSE_VALUE=8000000U"`

Note: if you are building the code as an imported Makefile project from within the STM32Cube IDE then you should NOT put quotes around the `CFLAGS` value you set in `C/C++ Build`/`Environment`.

## Test filter
By default all of the examples and tests supported by this platform will be executed.  To execute just a subset set the conditional compilation flag `U_CFG_APP_FILTER` to the example and/or test you wish to run.  For instance, to run all of the examples you would set `U_CFG_APP_FILTER=example`, or to run all of the porting tests `U_CFG_APP_FILTER=port`, or to run a particular example `U_CFG_APP_FILTER=examplexxx`, where `xxx` is the start of the rest of the example name.  In other words, the filter is a simple partial string compare with the start of the example/test name.  Note that quotation marks must NOT be used around the value part.

