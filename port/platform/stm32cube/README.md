# Introduction
These directories provide the implementation of the porting layer on the STM32Cube platform.  Instructions on how to install the necessary tools and perform the build can be found in the MCU directories below.

- [app](app): contains the code that runs the application (both examples and unit tests) on the STM32Cube platform.
- [src](src): contains the implementation of the porting layers for STM32Cube platform.
- [mcu](mcu): contains the configuration and build files for the MCUs supported on the STM32Cube platform.
- [inc](inc): contains [u_cfg_os_platform_specific.h](inc/u_cfg_os_platform_specific.h) which defines task priorities and stack sizes for the platform, built into this code. This directory deviates from the pattern used in all other platforms.  The reason for it is that Eclipse (which underlies the STM32Cube IDE) does not permit the parent directory to be included in the build, it causes a circular reference in the Eclipse project file.  Hence, in this one case only, the OS configuration has to be in its own sub-directory.  It is called `inc` to avoid confusion with the main `cfg` directory under the MCU sub-directory.