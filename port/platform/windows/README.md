**IMPORTANT**: This platform is currently intended for debugging/development only and will be subject to change if/when we decide to make it more of a product platform.

# Introduction
These directories provide the implementation of the porting layer on Windows.  Instructions on how to install the necessary tools and perform the build can be found in the [win32](win32) directory below.

- [app](app): contains the code that runs the application (both examples and unit tests) on Windows.
- [src](src): contains the implementation of the porting layers for Windows.
- [mcu/win32](mcu/win32): contains the configuration and build files for Windows 32-bit.
- [u_cfg_os_platform_specific.h](u_cfg_os_platform_specific.h): task priorities and stack sizes for the platform, built into this code.

Windows is a great environment for rapid development and debug visibility but note that both **stack checking** and **heap checking** cannot be done under Windows.

Note that if you wish to run stuff such as Valgrind, which is only supported on Linux, then you can do so by running [Zephyr on Linux](..\zephyr).