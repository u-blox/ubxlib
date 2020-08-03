# Introduction
This directory contains a build which compiles and runs any or all of the examples and tests for the NRF52 platform under Segger Embedded Studio.

# Usage
Make sure you have followed the instructions in the directory above this to install Segger Embedded Studio and the Nordic command-line tools.

You will also need a copy of Unity, the unit test framework, which can be Git cloned from here:

https://github.com/ThrowTheSwitch/Unity

Clone it to the same directory level as `ubxlib`, i.e.:

```
..
.
Unity
ubxlib
```

Note: you may put this repo in a different location but if you do so you will need to specify the path of your `Unity` directory on the command-line to Segger Embedded Studio (using `/` instead of `\`), e.g.:

```
"C:\Program Files\Segger\SEGGER Embedded Studio for ARM 4.50\bin\emstudio" -D UNITY_PATH=c:/Unity
```

By default all of the examples and tests supported by this platform will be executed.  To execute just a subset set the conditional compilation flag `U_CFG_APP_FILTER` to the example and/or test you wish to run.  For instance, to run all of the examples you would set `U_CFG_APP_FILTER=example`, or to run all of the porting tests `U_CFG_APP_FILTER=port`, or to run a particular example `U_CFG_APP_FILTER=examplexxx`, where `xxx` is the start of the rest of the example name.  In other words, the filter is a simple partial string compare with the start of the example/test name.  Note that quotation marks must NOT be used around the value part.

You may set this compilation flag directly in `Makefile`, or you may set the compilation flag `U_CFG_OVERRIDE` and provide it in the header file `u_cfg_override.h` (which you must create) or you may use the mechanism described in the directory above to pass the compilation flag via the Segger Embedded Studio command-line without modifying the build files at all.

With that done follow the instructions in the directory above this to start Segger Embedded Studio and build/run the examples and/or unit tests on NRF52.