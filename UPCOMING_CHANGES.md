** **IMPORTANT PLATFORM CHANGES COMING UP IN MARCH 2022** **

At the end of February 2022 we will be introducing some changes to the build metadata under the platform directory in order to improve maintainability and take the first step towards an architecture where it is possible to compile your choice of `ble`, `cell`, `gnss` or `wifi` network types, dropping unnecessary source files.

The changes will be as follows:
- we will introduce two new files under the [port](/port) directory: `ubxlib.cmake` and `ubxlib.mk`,
- these files will be used to list all of the source and header files that comprise `ubxlib`,
- the files will be constructed so as to clearly separate files required by `ble`, `cell`, `gnss` and `wifi` and also to separate test files from core functionality,
- the platform build metadata will be modified as follows:
  - [zephyr](/port/platform/zephyr) will include the new `ubxlib.cmake` file rather than listing the files it needs by itself, 
  - [esp-idf](/port/platform/esp-idf) will include the new `ubxlib.cmake` file rather than listing the files it needs by itself, 
  - [nrf5sdk gcc](/port/platform/nrf5sdk/mcu/nrf52/gcc) will include the new `ubxlib.mk` file rather than listing the files it needs by itself, 
  - [nrf5sdk ses](/port/platform/nrf5sdk/mcu/nrf52/ses) **WILL NO LONGER BE SUPPORTED**, 
  - [stm32cube](/port/platform/stm32cube) **WILL MOVE TO USING A MAKEFILE RATHER THAN THE NATIVE ECLIPSE .PROJECT/.CPROJECT FILES**; in other words, Eclipse will still be the IDE used for the STM32F4 platform but it will use the "import existing makefile" mechanism to do so, 
  - the [static_size](/port/platform/static_size) platform will move to using CMake and the new `ubxlib.cmake` file,
- the [arduino](/port/platform/arduino) and [lint](/port/platform/lint) platforms will not be affected by this change, though note that we will likely modify the [arduino](/port/platform/arduino) plaform in a similar way at a later date,
- we will introduce support for building and running `ubxlib` on **WINDOWS** on an experimental basis, currently focused on using this for development/test purposes only.

** **IF YOU DO NOT LIKE THE ABOVE, OR WOULD LIKE FURTHER CLARIFICATION ON THE ABOVE, PLEASE RAISE AN ISSUE ON THIS REPO ASAP** **