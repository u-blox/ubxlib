# Introduction
These directories provide the implementation of the porting layer on the STM32Cube platform.  Instructions on how to install the necessary tools and perform the build can be found in the MCU directories below.

- [app](app): contains the code that runs the application (both examples and unit tests) on the STM32Cube platform.
- [src](src): contains the implementation of the porting layers for STM32Cube platform.
- [mcu](mcu): contains the configuration and build files for the MCUs supported on the STM32Cube platform.
- [u_cfg_os_platform_specific.h](u_cfg_os_platform_specific.h): task priorities and stack sizes for the platform, built into this code.