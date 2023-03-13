# Introduction
These directories provides the implementation of the necessary files for enabling `ubxlib` as a library which can be used by [PlatformIO](https://platformio.org/).

In order to use `ubxlib` in the context of PlatformIO all you need to do is add the following line to your `platformio.ini`:

    lib_deps = https://github.com/u-blox/ubxlib

This will give you the latest version but it is also possible to specify a certain version of `ubxlib`, more info about that can be found [here](https://docs.platformio.org/en/latest/projectconf/section_env_library.html#lib-deps).

The actual inclusion and subsequent operations are controlled by the file [library.json](/library.json), located in the root directory of this repo.

`ubxlib` should work on all boards supported by the `zephyr`, `espidf` and `arduino` frameworks in PlatformIO.

**Please note** that the Arduino framework is only supported for boards on the `espressif32` platform.

# `UBXLIB_FEATURES`
If you wish to build only cellular and/or GNSS and/or short-range (WiFi and BLE), rather than the lot, you may do this by defining an environment variable `UBXLIB_FEATURES` before you build.  For instance, to include just GNSS you would set `UBXLIB_FEATURES=gnss`, or to include GNSS, WiFi and BLE you would set `UBXLIB_FEATURES=gnss short_range`, or to include GNSS and cellular `UBXLIB_FEATURES=cell gnss` etc.