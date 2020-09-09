# Introduction
This directory contains a build which compiles and runs any or all of the examples and tests for the NRF5340 platform under Segger Embedded Studio.

# Usage
Make sure you have followed the instructions in the directory above this to install nRFConnectSDK and toolchain.

By default all of the examples and tests supported by this platform will be executed.  To execute just a subset set the conditional compilation flag `U_CFG_APP_FILTER` to the example and/or test you wish to run.  For instance, to run all of the examples you would set `U_CFG_APP_FILTER=example`, or to run all of the porting tests `U_CFG_APP_FILTER=port`, or to run a particular example `U_CFG_APP_FILTER=examplexxx`, where `xxx` is the start of the rest of the example name.  In other words, the filter is a simple partial string compare with the start of the example/test name.  Note that quotation marks must NOT be used around the value part.

You may set this compilation flag directly in `Cmake file` using e.g. `target_compile_definitions(app PRIVATE U_CFG_APP_FILTER=portOs)`, or you may set the compilation flag `U_CFG_OVERRIDE` and provide it in the header file `u_cfg_override.h` (which you must create).

With that done follow the instructions in the directory above this to start Segger Embedded Studio and build/run the examples and/or unit tests on NRF5340.