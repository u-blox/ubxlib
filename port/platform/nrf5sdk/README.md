*** IMPORTANT: this platform is now DEPRECATED, it is no longer supported and will be REMOVED in release 1.5, mid 2024: please use [zephyr](/port/platform/zephyr) instead. ***

# Introduction
These directories provide the implementation of the porting layer on the Nordic nRF5 platform.  Instructions on how to install the necessary tools and perform the build can be found in the MCU directories below.

- [app](app): contains the code that runs the application (both examples and unit tests) on the nRF5 platform.
- [src](src): contains the implementation of the porting layers for nRF5.
- [mcu](mcu): contains the configuration and build files for the MCUs supported on the nRF5 platform.
- [u_cfg_os_platform_specific.h](u_cfg_os_platform_specific.h): task priorities and stack sizes for the platform, built into this code.