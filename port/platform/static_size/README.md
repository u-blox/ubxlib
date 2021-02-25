# Introduction
This directory provides a means to measure the static flash and RAM consumption of the `ubxlib` code using a 32-bit GCC ARM compiler.  It is kept here, nestled amongst the other platform builds, so that the files are in an obvious place for maintenance, not forgotten, but of course it is not really a platform, sort of a non-platform in fact (like Lint) since it measures the size of the non-platform related aspects of `ubxlib`.

The following data is required:

- `source.txt` paths to all of the files built as part of `ubxlib` that are NOT related to a platform or to testing or to examples, i.e. the core functionality.
- `include.txt` paths to the include directories required to build those source files.

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
- When adding a new platform-independent, non-test, non-example source file update `source.txt` and, if necessary, `include.txt` with the relevant source files and headers, ignoring any related only to tests or examples or to  platform code.  If new stuff is added to the `port` API or to the `cfg` files for all platforms, you may need to add new stubs for those things also.