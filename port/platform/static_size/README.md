# Introduction
This directory provides a means to measure the static flash and RAM consumption of the `ubxlib` code using a 32-bit GCC ARM compiler.  It is kept here, nestled amongst the other platform builds, so that the files are in an obvious place for maintenance, not forgotten, but of course it is not really a platform, sort of a non-platform in fact (like Lint) since it measures the size of the non-platform related aspects of `ubxlib`.

The following data is required:

- `source.txt` paths to all of the files built as part of `ubxlib` that are NOT related to a platform or to testing or to examples, i.e. the core functionality.
- `include.txt` paths to the include directories required to build those source files.

# A Note On Dynamic RAM
Of course this build only measures the statically allocated RAM consumption of the core `ubxlib` implementation, it does not measure the RAM or flash consumption of the porting layer and does not measure the heap/stack RAM usage when `ubxlib` is in use.  The porting layer is generally a pretty thin wrapper over the platform-specific OS/SDK, so maybe a few kbytes of flash and very little statically-allocated RAM.  For a measure of the dynamic RAM usage, which will obviously also have a platform-dependent aspect, use `runner` to compile all of the unit tests for your platform, run them capturing the trace output and look in the trace output at the end of every test suite where you will find a trace print from a function named `xxxCleanUp()` (where `xxx` is the test suite name, e.g. `sock` for the sockets tests):

```
U_XXX_TEST: main task stack had a minimum of 5472 byte(s) free at the end of these tests.
U_XXX_TEST: heap had a minimum of 10036 byte(s) free at the end of these tests.
```

These are measures of the minimum free stack, ever, since `runner` began execution and, likewise, the minimum free heap, ever, since `runner` began execution, in other words the "high water marks" for stack and heap respectively; look at the last one of these to see the measurement after all of the tests have been run.  The tests execute and stress all parts of `ubxlib` to obtain worst-case numbers and, of course, the values will depend entirely on your platform and how much heap, in particular, you have made available (we configure and test the main task stack to guarantee at least 5 kbytes of stack free for the user); the above example is from a run of all tests on the nRF52 MCU with GCC/FreeRTOS where the main task stack was 8 kbytes and the RAM allocated to heap was 40 kbytes.  Note that the STM32F4 SDK does not recover memory when library calls are made from dynamically created/destroyed tasks so beware of the heap numbers for that case (you will see the minimum heap free gradually decline throughout the test run).

# Installation
You will need a version of GCC for ARM (the builds here have been tested with version `10 2020-q4-major`):

https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads

You will also need Python 3.4 to run the script.

# Usage
Run the `static_size.py` script with parameter `--h` for command-line help.

An example might be:

```
python static_size.py -p "C:/Program Files (x86)/GNU Arm Embedded Toolchain/10 2020-q4-major/bin" -u c:\projects\ubxlib_priv -c "-Os -g0 -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16" -l "-Os -g0 -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16 --specs=nano.specs -lc -lnosys -lm -lc"
```

# Maintenance
- When adding a new platform-independent, non-test, non-example source file update `source.txt` and, if necessary, `include.txt` with the relevant source files and headers, ignoring any related only to tests or examples or to  platform code.
- If new stuff is added to the `port` API or to the `cfg` files for all platforms, you may need to add new stubs for those things also.
