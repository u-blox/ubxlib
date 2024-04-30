# Introduction
These directories provide the porting layer that allows the high level C APIs to be built and run on a supported MCU.

No attempt is made to create a full MCU porting API; only the APIs necessary to run the high level C APIs are implemented here.

Note: aside from calling `uPortInit()` at start of day, `uPortDeinit()` at end of day, and `uPortFree()` if you are freeing some memory that `ubxlib` has allocated, this API is NOT INTENDED FOR CUSTOMER USE.  You may use it if you wish but it is quite restricted and is intended _only_ to provide what `ubxlib` needs in the form that `ubxlib` needs it, internally to the `ubxlib` code.  It is used in the `ubxlib` examples but that is only because we need those examples to work on all of our supported platforms.  When writing your application you are better off using the fully-featured native APIs of your platform.

# Usage
The [api](api) directory defines the port API, each API function documented in the header file.  The [clib](clib) directory contains implementations of C library APIs that may be missing on some platforms (e.g. `strtok_r`, the re-entrant version of the `strtok` library function).  In the [platform](platform) directory you will find the implementation of the porting API on various target SDKs and MCUs and the necessary instructions to create a working binary and tests on each of those target MCUs.  Please refer to the `README.md` files in your chosen target platform directory for more information.

The [test](test) directory contains tests for this API that can be run on any platform.

# DIY
If your platform is not currently supported you may be able to port it yourself, something like this:
- provide implementations of the functions in the port [api](api); use the existing platform implementations for guidance (e.g. [platform/nrf5sdk/src](platform/nrf5sdk/src)):
  - the [initialisation](api/u_port.h) and [OS](api/u_port_os.h) interfaces are probably the simplest: you will need task creation/deletion and an entry point into task-land, plain-old non-recursive mutexes, a way to queue things, a way to block a task for x milliseconds, a way to obtain a count of \[32-bit\] milliseconds since boot and also semaphores,
  - the common [platform/common/event_queue](platform/common/event_queue) code will likely form most of your implementation of the [u_port_event_queue.h](api/u_port_event_queue.h) API,
  - you will need a way to get [debug](api/u_port_debug.h) strings off the platform, i.e. \[non-floating point\] `printf()` to somewhere,
  - the [GPIO API](api/u_port_gpio.h) will require some plumbing into the specifics of your MCU,
  - the [UART API](api/u_port_uart.h) will likely be the most complex thing to implement; you will probably need to know details of the interrupt and DMA behaviours of your MCU to complete this,
  - if you intend to use a GNSS chip connected directly to your MCU via I2C then you will need to port the [I2C API](api/u_port_i2c.h),
  - if you intend to use a GNSS chip connected directly to your MCU through SPI then you will need to port the [SPI API](api/u_port_spi.h),
  - if you intend to use the BLE that is on-chip, inside the same host MCU as `ubxlib` is running, you will require an implementation of the [GATT](api/u_port_gatt.h) access functions,
  - if your platform does not use [newlib](https://sourceware.org/newlib/) (if you are using GCC it will bring [newlib](https://sourceware.org/newlib/) with it) then you may find you are missing some C library functions; implementations of C library functions we have already found to be missing on some platforms can be found in [port/clib](/port/clib) and can just be hooked-in from there but you may need to add more if your code doesn't compile,
  - if your platform does not offer `malloc()` and `free()`, or you wish to do your own thing with heap memory, you should override the default, weakly-linked, implementations of `pUPortMalloc()` and `uPortFree()` by defining your own implementations of [these functions](/port/api/u_port_heap.h) in a file inside the `src` directory of your port,
  - if your platform supports setting a time-zone offset you will need to implement `uPortGetTimezoneOffsetSeconds()`; if not then you may simply include the file [port/u_port_timezone.c](/port/u_port_timezone.c) in your build (already included through weak linkage via [ubxlib.cmake](ubxlib.cmake) and [ubxlib.mk](ubxlib.mk)) to get a default timezone offset of zero,
  - if your platform has some form of compile-time device configuration mechanism of its own (like the Zephyr Device Tree) then you may wish to implement `uPortBoardCfgDevice()` and `uPortBoardCfgNetwork()` (see [port/api/u_port_board_cfg.h](/port/api/u_port_board_cfg.h)) to accommodate that,
  - if you wish to test BLE bonding then you should implement the [named pipe API](api/u_port_named_pipe.h); if not then you may simply include the file [port/u_port_named_pipe.c](/port/u_port_named_pipe.c) in your build (already included through weak linkage via [ubxlib.cmake](ubxlib.cmake) and [ubxlib.mk](ubxlib.mk)) to get a default implementation that returns `U_ERROR_COMMON_NOT_SUPPORTED`.
- provide your own versions of the header files `u_cfg_app_platform_specific.h`, `u_cfg_hw_platform_specific.h`, `u_cfg_test_platform_specific.h` and `u_cfg_os_platform_specific.h` (see examples in the existing platform directories); take particular note of translating the task priority values into those of your OS,
- provide your own build metadata files (for CMake, Make, a home-grown Python lash-up, whatever): usually your chosen platform will dictate the shape of these and you just need to add to your existing structure the paths to the `ubxlib` source files and the `ubxlib` include files; otherwise take a look at the existing [nrf5 GCC platform](platform/nrf5sdk/mcu/nrf52/gcc/runner) or [static_size](platform/static_size) platforms as a starting point (though note that the latter does not bring in any `platform` or `test` files),
- add [Unity](https://github.com/ThrowTheSwitch/Unity) to your build and then compile and run the tests in [u_port_test.c](test/u_port_test.c): if these pass then you have likely completed the necessary porting.  If you are using a UART it is a good idea to run all of the `ubxlib` tests with a real target device, if you can, since the `uPortUartRequiresSpecificWiring()` test does not stress asynchronicity and stop/start behaviour.

# Shared CMake file
For ports that use CMake [ubxlib.cmake](ubxlib.cmake) can be used to collect the `ubxlib` non-platform-specific source code files, include directories etc.

`ubxlib.cmake` is typically included in a port specific CMake file and will then define a couple of variables that the calling CMake file can make use of (see the [esp32 component CMakeLists.txt](platform/esp-idf/mcu/esp32/components/ubxlib/CMakeLists.txt) as example).

Before including `ubxlib.cmake` you must set the variable `UBXLIB_BASE` to root directory of `ubxlib`.

You must also specify what `ubxlib` features to enable using the `UBXLIB_FEATURES` variable described below.

To bring the platform-specific files into your CMake file, take a look at, for instance, the Windows-platform [CMakeLists.txt](platform/windows/mcu/win32/runner/CMakeLists.txt) file.

## `UBXLIB_FEATURES`
Available features are:
- `u_lib`: include the `lib_common` API,
- `short_range`: include `wifi`, `ble` and `short_range` API,
- `short_range_gen2`: should be used IN ADDITION TO `short_range` and ONLY IF you are using the short-range NORA-W36 module; this is required since NORA-W36 comes with a second generation u-connectExpress,
- `cell`: include `cell` API,
- `gnss`: include `gnss` API,
- `geodesic`: include [GeographicLib](https://github.com/geographiclib/geographiclib), required only if `U_CFG_GEOFENCE` is defined and shapes larger than 1 km in size are being used in the fence, see [u_geofence_geodesic.h](/common/geofence/api/u_geofence_geodesic.h) for details.

## Example
```cmake
set(UBXLIB_BASE <path_to_ubxlib>)
# Set the ubxlib features to compile (all needs to be enabled at the moment)
set(UBXLIB_FEATURES short_range cell gnss)

# From this line we will get back:
# - UBXLIB_SRC
# - UBXLIB_INC
# - UBXLIB_PRIVATE_INC
# - UBXLIB_TEST_SRC
# - UBXLIB_TEST_INC
# and optionally:
# - UBXLIB_EXTRA_LIBS
# - UBXLIB_COMPILE_OPTIONS
include(${UBXLIB_BASE}/port/ubxlib.cmake)
```

## Output Variables
After `ubxlib.cmake` has been included the following variables will be available:
- `UBXLIB_SRC`: A list of all the  non-platform-specific `.c` files,
- `UBXLIB_INC`: A list of the public include directories,
- `UBXLIB_PRIVATE_INC`: A list of the private include directories required to build `ubxlib`,
- `UBXLIB_TEST_SRC`: A list of all the test `.c` files,
- `UBXLIB_TEST_INC`: A list of test include directories,

# Shared Makefile
For ports that use `make` [ubxlib.mk](ubxlib.mk) can be used to collect the non-platform-specific `ubxlib` source code files, include directories etc. in the same way as for CMake above.

Just like the shared CMake file you must first set the variable `UBXLIB_BASE` to root directory of `ubxlib` and configure the `ubxlib` features using `UBXLIB_FEATURES` (see above). `ubxlib.mk` will then define the same output variables as the shared CMake above (see [Output Variables](#Output-Variables))

To bring the platform-specific files into your `Makefile`, take a look at, for instance, the [STM32F4 Makefile](platform/stm32cube/mcu/stm32f4/runner/Makefile).

## Example
```makefile
# Include ubxlib src and inc
UBXLIB_BASE = <path_to_ubxlib>
UBXLIB_FEATURES = cell gnss short_range
# From this line we will get back:
# - UBXLIB_SRC
# - UBXLIB_INC
# - UBXLIB_PRIVATE_INC
# - UBXLIB_TEST_SRC
# - UBXLIB_TEST_INC
# and optionally:
# - UBXLIB_EXTRA_LIBS
# - UBXLIB_COMPILE_OPTIONS
include $(UBXLIB_BASE)/port/ubxlib.mk
```
