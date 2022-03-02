# Introduction
This directory contains a build which compiles and runs any or all of the examples and tests for Windows with MSVC and Make.

# Usage
Make sure you have followed the instructions in the directory above this to install the MSVC toolchain/Make and establish a command prompt.

You will also need a copy of Unity, the unit test framework, which can be Git cloned from here:

https://github.com/ThrowTheSwitch/Unity

Clone it to the same directory level as `ubxlib`, i.e.:

```
..
.
Unity
ubxlib
```

Note: you may put this repo in a different location but if you do so you will need to tell the build where it is by setting an environment variable named `UNITY_PATH` , e.g. `UNITY_PATH=c:/Unity`, before you build.


Before building you must tell the tests which module(s) you are using and the UARTs they are connected on.  For instance, to do so using the `U_FLAGS` mechanism, if you were using a SARA-R5 cellular module on COM3, you would set:

`U_FLAGS=-DU_CFG_APP_CELL_UART=3 -DU_CFG_TEST_CELL_MODULE_TYPE=U_CELL_MODULE_TYPE_SARA_R5`

By default all of the examples and tests supported by this platform will be executed.  To execute just a subset set the conditional compilation flag `U_CFG_APP_FILTER` to the example and/or test you wish to run.  For instance, to run all of the examples you would set `U_CFG_APP_FILTER=example`, or to run all of the porting tests `U_CFG_APP_FILTER=port`, or to run a particular example `U_CFG_APP_FILTER=examplexxx`, where `xxx` is the start of the rest of the example name.  In other words, the filter is a simple partial string compare with the start of the example/test name.  Note that quotation marks must NOT be used around the value part.

You may set this compilation flag using the environment variable mechanism as described in the [README.md in the directory above](../README.md), or you may set the compilation flag `U_CFG_OVERRIDE` and provide it in the header file `u_cfg_override.h` (which you must create).