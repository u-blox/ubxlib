# Introduction

These directories provides the implementation of the necessary files for enabling ubxlib as a library which can be used by [PlatformIO](https://platformio.org/).

In order to use ubxlib in the context of PlatformIO all you need to do is adding the following line to your *platformio.ini* :

    lib_deps = https://github.com/u-blox/ubxlib

This will give you the latest version but it is also possible to specify a certain version of ubxlib, more info about that can be found [here](https://docs.platformio.org/en/latest/projectconf/section_env_library.html#lib-deps).

The actual inclusion and subsequent operations are controlled by the file [library.json](../../../library.json), located in the top directory of this repo.

Ubxlib should work on all boards supported by the **Zephyr**, **espidf** and **Arduino** frameworks in PlatformIO.

**Please note** that the Arduino framework is only supported for boards on the espressif32 platform.

