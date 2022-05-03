![u-blox logo](/readme_images/ublox-logo.png)

[![important message](/readme_images/important_msg.svg)](/UPCOMING_CHANGES.md)

# Introduction to `ubxlib`

This repository contains an add-on to microcontroller and RTOS SDKs for building embedded applications with u-blox products and services. It provides portable C libraries which expose APIs with examples. ubxlib supports [u-blox](https://www.u-blox.com) modules with [cellular](https://www.u-blox.com/en/cellular-modules) (2G/3G/4G), [short-range](https://www.u-blox.com/en/short-range-radio-chips-and-modules) (Bluetooth and Wi-Fi) and [positioning](https://www.u-blox.com/en/positioning-chips-and-modules) (GNSS) funcionality. The `ubxlib` libraries present high level C APIs for use in customer applications (e.g. connect to a network, open a TCP socket, establish location, etc.) and implements these APIs on selected popular MCUs, also available inside u-blox modules.

The goal of `ubxlib` is to deliver a single tested solution with examples which provides uniform easy-to-use APIs across several u-blox products. Releases of `ubxlib` are tested automatically for all configurations on multiple boards in a [test farm](/port/platform/common/automation/DATABASE.md).

![ubxlib high level overview](/readme_images/ubxlib_high_level.png)

The easiest way to quickly explore `ubxlib` is to start with a board listed in the [test farm](/port/platform/common/automation/DATABASE.md). u-blox EVKs (evaluation kits) or application boards can be found [here](https://www.u-blox.com/en/evk-search) or at major electronics distributors.

`ubxlib` runs on a host microcontroller and has a peripheral attached. This setup is very common in embedded applications. An example of such a host-peripheral configuration with EVK-NINA-B301 (Bluetooth 5.0) and EVK-R4 (SARA-R4 with 2G/3G/4G) in which the `ubxlib` host sets up a TCP connection is shown in the following figure. Many other combinations can be achieved, with the supported hosts and peripherals in the tables in the next section.


![EVK setup](/readme_images/EVK_NINA_R4.png)

# APIs

The key APIs provided by this repo, and their relationships with each other, are shown in the picture below.

![APIs](/readme_images/apis.jpg)

- If you wish to bring up a network and don't care about the details, use the common [network](/common/network) API, which can bring up cellular, BLE or Wi-Fi network(s) at your choosing.
- If you wish to use a socket over that network, use the common [sock](/common/sock) API.
- If you wish to use security, use the common [security](/common/security) API.
- If you wish to contact an MQTT broker over that network, use the common [mqtt_client](/common/mqtt_client) API.
- If you wish to get a location fix use the common [location](/common/location) API.
- If you wish to take finer control of [cellular](/cell), [ble](/ble), [wifi](/wifi) or [gnss](/gnss), use the respective control API directly.
- GNSS is used via the [gnss](/gnss) API.
- The BLE and Wi-Fi APIs are internally common within u-blox and so they both use the common [short_range](/common/short_range) API.
- The [at_client](/common/at_client) API is used by the cellular and short range APIs to talk to AT-based u-blox modules.
- The [ubx_protocol](/common/ubx_protocol) API implements the necessary encoding/decoding to talk to u-blox GNSS modules.
- The [port](/port) API permits all of the above to run on different hosts; this API is not really intended for customer use - you can use it if you wish but it is quite restricted and is intended only to provide what `ubxlib` needs in the form that `ubxlib` needs it.



# Supported `ubxlib` host platforms and APIs

Hosts run `ubxlib` and interact with an attached periperal. A host platform contains an MCU, toolchain and RTOS/SDK as listed in the table below. Hosts are typically u-blox open CPU (standalone) modules or other MCUs. To use a host you need a development board or an EVK. Currently `ubxlib` supports the following purchasable boards out-of-the box.

- [u-blox C030-U201 board](https://www.u-blox.com/en/product/c030-application-board)
- [Nordic nRF52840 DK board](https://www.nordicsemi.com/Products/Development-hardware/nrf52840-dk)
- [Nordic nRF5340 DK board](https://www.nordicsemi.com/Products/Development-hardware/nRF5340-DK)
- [STM32F4 Discovery board](https://www.st.com/en/evaluation-tools/stm32f4discovery.html)
- [ESP32-DevKitC](https://www.espressif.com/en/products/devkits/esp32-devkitc/overview)
- [u-blox NINA-W1 EVK](https://www.u-blox.com/en/product/evk-nina-w10)



If your MCU is on the list but your board is not:
- Just set the HW pins in the source file of the example to match how your MCU is wired to the u-blox peripheral.

If your MCU is not on the list:
- To port `ubxlib` to a new host platform follow the [DYI instructions](https://github.com/u-blox/ubxlib_priv/tree/master/port) for the port API.

|`ubxlib` hosts |NINA-W10|NINA-B40 series<br />NINA-B30 series<br />NINA-B1 series<br />ANNA-B1 series<br />|NORA-B1 series|C030 board|PC|PC|
|-----------|-----------|--------------|-----|-----|------|------|
|**MCU**|Espressif ESP32|Nordic nRF52|Nordic nRF53|ST-Micro STM32F4|x86 (win32)|x86 (32-bit Linux)|
|**Toolchain**|ESP-IDF<br />Arduino-ESP32|GCC<br />nRF Connect|nRF Connect|Cube|MSVC|Zephyr|
|**RTOS/SDK**|FreeRTOS|FreeRTOS<br />Zephyr|Zephyr|FreeRTOS|Windows|Zephyr|
|**APIs provided by host**|[wifi](/wifi)<br />[ble](/ble "ble API")<br />[network](/common/network "network API")<br />[sock](/common/sock "sock API")|[ble](/ble "ble API")<br />[network](/common/network "network API")|[ble](/ble "ble API")<br />[network](/common/network "network API")| [cell](/cell "cell API")<br />[network](/common/network "network API")<br />[sock](/common/sock "sock API")<br />[location*](/common/location "location API")<br />[tls&nbsp;security](/common/security "security API")<br>| N/A | N/A |


# Supported modules as `ubxlib` peripherals and APIs

Peripherals are u-blox modules which accept commands (e.g. AT-commands) over a serial interface and have no open MCU environment. To run the APIs they need to be attached to a host which runs `ubxlib`. For example in the [test farm](/port/platform/common/automation/DATABASE.md) combinations of hosts and peripherals are listed.

|`ubxlib` peripherals |NINA-B41 series<br />NINA-B31 series<br />NINA-B1 series<br />ANNA-B1|NINA-W13|NINA-W15|SARA-U2 series|SARA-R4 series<br />SARA-R5 series<br />|SARA-R510M8S<br />SARA-R422M8S|M8 series|
|-----------|-----------|--------------|-----|-----|------|------|------|
|**APIs provided by host with peripheral attached**|[ble](/ble "ble API")<br />[network](/common/network "network API")|[wifi](/wifi)<br />[network](/common/network "network API")<br />[sock](/common/sock "sock API")|[wifi](/wifi)<br />[ble](/ble "ble API")<br />[network](/common/network "network API")<br />[sock](/common/sock "sock API")| [cell](/cell "cell API")<br />[network](/common/network "network API")<br />[sock](/common/sock "sock API")<br />[location*](/common/location "location API")<br />[tls&nbsp;security](/common/security "security API")<br>|[cell](/cell "cell API")<br />[network](/common/network "network API")<br />[sock](/common/sock "sock API")<br />[location*](/common/location "location API")<br />[security](/common/security "security API")<br>[mqtt_client](/common/mqtt_client "MQTT client API")|All APIs of<br />SARA-R4,<br />SARA-R5 series&nbsp;+<br />[gnss](/gnss "GNSS API")<br />[location](/common/location "location API")|[gnss](/gnss "GNSS API")<br />[location](/common/location "location API")|



\* Through the u-blox [CellLocate](https://www.u-blox.com/en/product/celllocate) mobile network-based location service.

# Structure of `ubxlib`
The APIs for each type of u-blox module can be found in the relevant directory (e.g. [cell](/cell) for cellular modules and [ble](/ble)/[wifi](/wifi) for BLE/Wi-Fi modules).  The [common](/common) directory contains APIs and 'helper' modules that are shared by u-blox modules, most importantly the [network](/common/network) API and the [sockets](/common/sockets) API.  All APIs are documented in the API header files.

Examples demonstrating the use of the APIs can be found in the [example](/example) directory.

Each API includes a `test` sub-directory containing the tests for that API which you may compile and run if you wish.

Build information for each platform can be found in the [platform](/port/platform) sub-directory of [port](/port); more on this below.

In order for u-blox to support multiple platforms with this code there is also a [port API](/port/api).  This is not intended to be a generic porting API, it is simply sufficient to support the APIs we require.  If you have not chosen a supported platform you may still be able to use the high level APIs here unchanged by [implementing the port API](/port#diy) for your platform.

```
+---example                    <-- examples that introduce the main features 
+---cfg                        <-- global configuration header files
+---common                     <-- APIs that are common across u-blox modules
¦   +---network                <-- the simple network API for BLE, cell, Wi-Fi and GNSS
¦   ¦   +---api                <-- all folders, in general, have an API directory
¦   ¦   +---src                    containing public headers, a source directory with
¦   ¦   +---test                   the implementation and a test directory with the tests
¦   +---sock                   <-- the sockets API for cell, Wi-Fi (and in the future BLE)
¦   +---security               <-- common API for u-blox security and TLS security/credential storage
¦   +---mqtt_client            <-- common MQTT client API
¦   +---location               <-- common location API, can use GNSS, Cell Locate, Cloud Locate and in the future Wi-Fi/BLE stations, etc.
¦   +---short_range            <-- internal API used by the BLE and Wi-Fi APIs (see below)
¦   +---at_client              <-- internal API used by the BLE, cell and Wi-Fi APIs
¦   +---ubx_protocol           <-- internal API used by the GNSS API
¦   +---error                  <-- u_error_common.h: error codes common across APIs
¦   +---assert                 <-- assert hook
¦   +---utils                  <-- contains common utilities
¦   ...
+---cell                       <-- API for cellular (if you need more than network provides)
+---wifi                       <-- API for Wi-Fi (if you need more than network provides)
+---ble                        <-- API for BLE
+---gnss                       <-- API for GNSS
+---port                       <-- port API: maps to SDKs and MCU platforms, includes build metadata
    +---api
    +---test
    +---clib
    +---platform               <-- look here for the supported SDKs and MCU platforms
        +---<platform>         <-- e.g. esp-idf
        ¦   +---app            <-- main() for this platform: runs all examples and tests
        ¦   +---src            <-- implementation of the port API for this platform
        ¦   +---mcu            <-- configuration and build metadata for the MCUs supported on this platform
        ¦       +---<mcu>      <-- e.g. esp32
        ¦           +---cfg    <-- platform specific config (pins, OS things, MCU HW blocks)
        ¦           +---runner <-- a build which compiles and links all examples and tests
        +---lint               <-- Lint checking, used by the test automation
        +---static_size        <-- a build that measures RAM/flash usage
        +---common             <-- things common to all platforms, most notably...
            +---automation     <-- the internal Python automation scripts that test everything
            ...
```

# How To Use This Repo
This repo uses Git submodules: make sure that once it has been cloned you do something like:

`git submodule update --init --recursive`

...to obtain the submodules.

The native SDKs for each supported platform are used directly, unchanged, by this code.  To use this repo you must first choose your MCU and associated platform.  For instance, you might choose an STM32F4 MCU, which is supported via ST's STM32Cube IDE.  Instructions for how to install and use each platform can be found in your chosen MCU sub-directory; for an STM32F4 MCU this would be [port/platform/stm32cube/mcu/stm32f4](/port/platform/stm32cube/mcu/stm32f4).

Having chosen your MCU and installed the platform tools, navigate to the directories below your chosen MCU directory to find the required build information.  For instance, you may find a `runner` directory, which is a generic build that compiles any or all of the examples and tests that can run on a given platform.  In that directory you will find detailed information on how to perform the build.

Configuration information for the examples and the tests can be found in the `cfg` directory of your chosen MCU.  Depending on how you have connected your MCU to a u-blox module you may need to override this configuration, e.g. to change which MCU pin is connected to which pin of the u-blox module.  The `README.md` in the `runner` directory of you chosen MCU will tell you how to override conditional compilation flags in order to do this.

# Examples: How To Use `ubxlib`

|  Technology  | Example |
|--------------|----------|
| Cellular     | The [sockets](/example/sockets "socket example") example brings up a TCP/UDP socket by using the [network](/common/network "network API") and [sock](/common/sock "sock API") APIs.  |
| Cellular     | The [end-to-end security](/example/security/e2e "E2E example") example using the [security](/common/security "security API") API. |
| Cellular     | The [PSK generation](/example/security/psk "PSK example") example using the [security](/common/security "security API") API. |
| Cellular     | The [chip-to-chip security](/example/security/c2c "C2C example") example using the [security](/common/security "security API") API. |
| Cellular     | A [TLS-secured version](/example/sockets "TLS sockets example") of the sockets example. |
| Cellular     | An [MQTT client](/example/mqtt_client "MQTT example") using the [MQTT client](/common/mqtt_client "MQTT client API") API.|
| Cellular     | [CellLocate](/example/location "CellLocate example") example. | Q2 2021|
| Bluetooth    | SPS (serial port service). |
| Wi-Fi         | The [sockets](/example/sockets "sockets example") example brings up a TCP/UDP socket by using the [network](/common/network "network API") and [sock](/common/sock "sock API") APIs.  |
| GNSS         | [location](/example/location "location example") example using a GNSS chip connected directly or via a cellular module.|

# Feature Request And Roadmap
New features can be requested and up-voted [here](https://github.com/u-blox/ubxlib/issues/12). The comments of this issue also contains an outlook about features of upcoming releases. Also it is the right place to discuss features and their priority.

# License
The software in this repository is Apache 2.0 licensed and copyright u-blox with the following exceptions:

- The heap management code (`heap_useNewlib.c`), required because the [nRF5 SDK](/port/platform/nrf5sdk) and [STM32Cube](/port/platform/stm32cube) platforms don't provide the necessary memory management for [newlib](https://sourceware.org/newlib/libc.html) and [FreeRTOS](https://www.freertos.org) to play together, is copyright Dave Nadler.
- The `mbedtls_platform_zeroize()` function in [mbedtls_platform_zeroize.c](/port/platform/nrf5sdk/src/mbedtls_platform_zeroize.c) is copied from the Apache licensed [mbedTLS](https://www.trustedfirmware.org/projects/mbed-tls/) and is copyright Arm Limited.
- The SEGGER RTT logging files in [port/platform/nrf5sdk/src/segger_rtt](/port/platform/nrf5sdk/src/segger_rtt) are copyright SEGGER MICROCONTROLLER GmbH & Co. KG.
- The AT client code in [common/at_client](/common/at_client) is derived from the Apache 2.0 licensed AT parser of [mbed-os](https://github.com/ARMmbed/mbed-os).
- The [stm32cube platform directory](/port/platform/stm32cube/src) necessarily includes porting files from the STM32F4 SDK that are copyright ST Microelectronics.
- The `go` echo servers in [common/sock/test/echo_server](/common/sock/test/echo_server) are based on those used in testing of [AWS FreeRTOS](https://github.com/aws/amazon-freertos).
- The `setjmp()/longjmp()` implementation in [port/clib/u_port_setjmp.S](/port/clib/u_port_setjmp.S), used when testing the Zephyr platform, is copyright Nick Clifton, Cygnus Solutions and part of [newlib](https://sourceware.org/newlib/libc.html).
- The `base64` implementation in [common/utils/src/base64.h](/common/utils/src/base64.h) is copyright [William Sherif](https://github.com/superwills/NibbleAndAHalf).
- The ARM callstack iterator in [port/platform/common/debug_utils/src/arch/arm/u_stack_frame_cortex.c](/port/platform/common/debug_utils/src/arch/arm/u_print_callstack_cortex.c) is copyright Armink part of [CmBacktrace](https://github.com/armink/CmBacktrace).
- The FreeRTOS additions [port/platform/common/debug_utils/src/freertos/additions](port/platform/common/debug_utils/src/freertos/additions) is copied from the Apache licensed [ESP-IDF](https://github.com/espressif/esp-idf).

In all cases copyright, and our thanks, remain with the original authors.
