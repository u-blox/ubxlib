# Introduction
These directories provide the implementation of the porting layer on native Linux.

# Building
All software building for this platform is intended to be made using [CMake](https://cmake.org/).

There are typically two scenarios when it comes to building Linux application which includes ubxlib.

The first case is to build the test runner application within this repo. More information about this can be [found here](mcu/posix/runner/README.md).

On the other hand if you want to add ubxlib to an existing or new Linux application of your own you just have to add the following text your *CMakeLists.txt* file

    include(DIRECTORY_WHERE_YOU_HAVE_PUT_UBXLIB/port/platform/linux/linux.cmake)
    target_link_libraries(YOUR_APPLICATION_NAME ubxlib ${UBXLIB_REQUIRED_LINK_LIBS})
    target_include_directories(YOUR_APPLICATION_NAME PUBLIC ${UBXLIB_INC} ${UBXLIB_PUBLIC_INC_PORT})


# Visual Studio Code
Both case listed above can also be made from within Visual Studio Code (on the Linux platform).

In the first case you can just open the predefined Visual Studio project file available in the root directory of this repository, *ubxlib-runner.code-workspace*. You can then select the *Build Linux runner* build target to start a build, and then the *Linux runner* debug target to start debugging.

In the second case you have to install the [Cmake extension for Visual Studio Code](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools).

More information on how to use CMake in Visual Studio Code can be [found here](https://code.visualstudio.com/docs/cpp/CMake-linux).

# Limitations
Some limitations apply on this platform:

- Linux does not provide an implementation of critical sections, something which this code relies upon for cellular power saving (specifically, the process of waking up from cellular power-saving) hence cellular power saving cannot be used from a Linux build.
- On a Raspberry Pi (any flavour), the I2C HW implementation of the Broadcom chip does not correctly support clock stretching, which is required for u-blox GNSS devices, hence it is recommended that, if you are using I2C to talk to the GNSS device, you use the bit-bashing I2C drive to avoid data loss, e.g. by adding the line `dtoverlay=i2c-gpio,i2c_gpio_sda=2,i2c_gpio_scl=3,i2c_gpio_delay_us=2,bus=8` to `/boot/config.txt` and NOT uncommenting the `i2c_arm` line in the same file.
- Use of GPIO chips above 0 are supported but NOT when the pin in question is to be an output that must be set high at initialisation; this is because the `uPortGpioConfig()` call is what tells this code to use an index other than 0 and, to set an output pin high at initialisation, `uPortGpioSet()`, has to be called _before_ `uPortGpioConfig()`.
- All testing has been carried out on a 64-bit Raspberry Pi 4.