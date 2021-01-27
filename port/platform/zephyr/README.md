# Introduction
These directories provide the implementation of the porting layer on the Zephyr platform.  Instructions on how to install the necessary tools and perform the build can be found in the MCU directories below.  Though this is intended to become a generic Zephyr platform, at the moment it supports only Nordic MCUs as they require a **specific** version/configuration of Zephyr.

Note: the directory structure here differs from that in the other platform directories in order to follow more closely the approach adopter by Zephyr, which is hopefully familiar to Zephyr users.

- `app`: contains the code that runs the application (both examples and unit tests) on the Zephyr platform.
- `cfg`: contains the configuration files for the MCU, the OS, for the application and for testing (mostly which MCU pins are connected to which module pins).
- `src`: contains the implementation of the porting layers for Zephyr platform.
- `runner`: contains the configuration and build files for the MCUs supported on the Zephyr platform.
- `custom_boards`: contains custom u-blox boards that have not yet been added to the Zephyr repo.
