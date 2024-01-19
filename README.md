<img src="/readme_images/ubxlib-logo.svg" width="400">

[![important message](/readme_images/important_msg.svg)](/UPCOMING.md)

<i>Note: if you are reading this from a package-file created by a third-party \[e.g. PlatformIO\] and the links do not work, please try reading it at the original [ubxlib Github site](https://github.com/u-blox/ubxlib) instead.</i>

# Introduction to `ubxlib`
This repository contains an add-on to microcontroller and RTOS SDKs for building embedded applications with u-blox products and services. It provides portable C libraries which expose APIs with examples. `ubxlib` supports [u-blox](https://www.u-blox.com) modules with [cellular](https://www.u-blox.com/en/cellular-modules) (2G/3G/4G), [short-range](https://www.u-blox.com/en/short-range-radio-chips-and-modules) (Bluetooth and Wi-Fi) and [positioning](https://www.u-blox.com/en/positioning-chips-and-modules) (GNSS) functionality. The `ubxlib` libraries present high level C APIs for use in customer applications (e.g. connect to a network, open a TCP socket, establish location, etc.) and implements these APIs on selected popular MCUs, also available inside u-blox modules.

The goal of `ubxlib` is to deliver a single tested solution, with examples, which provides uniform easy-to-use APIs across several u-blox products. Releases of `ubxlib` are tested automatically for all configurations on multiple boards in a [test farm](/port/platform/common/automation/DATABASE.md).

![ubxlib high level overview](/readme_images/ubxlib_high_level.png)

The easiest way to quickly explore `ubxlib` is to start with a board listed in the [test farm](/port/platform/common/automation/DATABASE.md). u-blox EVKs (evaluation kits) or application boards can be found [here](https://www.u-blox.com/en/evk-search) or at major electronics distributors and code examples which run on the u-blox [XPLR-IOT-1 platform](https://www.u-blox.com/en/product/xplr-iot-1) can be found [here](https://github.com/u-blox/ubxlib_examples_xplr_iot).  If you've heard enough and want to get started with the [XPLR-IOT-1 platform](https://www.u-blox.com/en/product/xplr-iot-1), or maybe with [PlatformIO](https://platformio.org/), jump straight to **[How To Use This Repo](#how-to-use-this-repo)** below.

`ubxlib` runs on a host microcontroller and has a peripheral attached. This setup is very common in embedded applications. An example of such a host-peripheral configuration with EVK-NINA-B301 (Bluetooth 5.0) and EVK-R4 (SARA-R4 with 2G/3G/4G) in which the `ubxlib` host sets up a TCP connection is shown in the following figure. Many other combinations can be achieved, with the supported hosts and peripherals in the tables in the next section.

![EVK setup](/readme_images/EVK_NINA_R4.png)

# APIs
The key APIs provided by this repo, and their relationships with each other, are shown in the picture below.

![APIs](/readme_images/apis.jpg)

- If you wish to bring up a device/network and don't care about the details, use the common [device](/common/device) and [network](/common/network) APIs, which can bring up cellular, BLE/Wi-Fi or GNSS network(s) at your choosing.
- If you wish to use a socket over that network, use the common [sock](/common/sock) API.
- If you wish to use security, use the common [security](/common/security) API.
- If you wish to contact an MQTT broker over that network, use the common [mqtt_client](/common/mqtt_client) API.
- If you wish to use HTTP, use the common [http_client](/common/http_client) API.
- If you wish to get a location fix use the common [location](/common/location) API.
- If you wish to build a geofence that can be used with [gnss](/gnss), [wifi](/wifi) and [cellular](/cell), see the common [geofence](/common/geofence) API.
- If you wish to take finer control of [cellular](/cell), [ble](/ble), [wifi](/wifi) or [gnss](/gnss), use the respective control API directly.
- GNSS may be used via the [gnss](/gnss) API.
- The BLE and Wi-Fi APIs are internally common within u-blox and so they both use the common [short_range](/common/short_range) API.
- The [at_client](/common/at_client) API is used by the cellular and short range APIs to talk to AT-based u-blox modules.
- The [ubx_protocol](/common/ubx_protocol) API implements the necessary encoding/decoding to talk to u-blox GNSS modules.
- The [port](/port) API permits all of the above to run on different hosts; this API is not really intended for customer use - you can use it if you wish but it is quite restricted and is intended only to provide what `ubxlib` needs in the form that `ubxlib` needs it.

All APIs are documented with Doxygen compatible comments: simply download the latest [Doxygen](https://doxygen.nl/) and either run it from the `ubxlib` directory at a command prompt or open [Doxyfile](/Doxyfile) in the Doxygen GUI and run it to obtain the output.

# Supported `ubxlib` host platforms and APIs
Hosts run `ubxlib` and interact with an attached periperal. A host platform contains an MCU, toolchain and RTOS/SDK as listed in the table below. Hosts are typically u-blox open CPU (standalone) modules or other MCUs. To use a host you need a development board or an EVK. Currently `ubxlib` supports and tests the following purchasable boards out-of-the box.

- [u-blox XPLR-IOT-1](https://www.u-blox.com/en/product/xplr-iot-1)
- [u-blox XPLR-HPG-1](https://www.u-blox.com/en/product/xplr-hpg-1)
- [u-blox XPLR-HPG-2](https://www.u-blox.com/en/product/xplr-hpg-2)
- [u-blox C030-U201 board](https://www.u-blox.com/en/product/c030-application-board)
- [u-blox NINA-W1 EVK](https://www.u-blox.com/en/product/evk-nina-w10)
- [Nordic nRF52840 DK board](https://www.nordicsemi.com/Products/Development-hardware/nrf52840-dk)
- [Nordic nRF5340 DK board](https://www.nordicsemi.com/Products/Development-hardware/nRF5340-DK)
- [STM32F4 Discovery board](https://www.st.com/en/evaluation-tools/stm32f4discovery.html)
- [ESP32-DevKitC](https://www.espressif.com/en/products/devkits/esp32-devkitc/overview)



If your MCU is on the list but your board is not:
- just set the HW pins in the source file of the example to match how your MCU is wired to the u-blox peripheral.

If your MCU is not on the list:
- if you are using [PlatformIO](https://platformio.org/) and [Zephyr](https://www.zephyrproject.org/) then any MCU that Zephyr supports should work with `ubxlib`, just follow the instructions [here](/port/platform/platformio) to bring `ubxlib` into your existing [PlatformIO](https://platformio.org/) environment,
- otherwise, to port `ubxlib` to a new host platform follow the [DIY instructions](/port#diy) for the port API.

|`ubxlib` hosts |NINA-W10|NINA-B40 series<br />NINA-B30 series<br />NINA-B1 series<br />ANNA-B1 series<br />|NORA-B1 series|C030 board|PC|PC|
|-----------|-----------|--------------|-----|-----|------|------|
|**MCU**|Espressif ESP32|Nordic nRF52|Nordic nRF53|ST-Micro STM32F4|x86/64 (Linux)|x86 (Win32)<sup>*</sup>|
|**Toolchain**|ESP-IDF<br />Arduino-ESP32|GCC<br />nRF Connect|nRF Connect|Cube|GCC|MSVC|
|**RTOS/SDK**|FreeRTOS|FreeRTOS<br />Zephyr|Zephyr|FreeRTOS|Posix|Windows|
|**APIs provided by host with peripheral attached<sup>**</sup>**|[wifi](/wifi)<br />[ble](/ble "ble API")<br />[device](/common/device "device API")<br />[network](/common/network "network API")<br />[sock](/common/sock "sock API")|[ble](/ble "ble API")<br />[device](/common/device "device API")<br />[network](/common/network "network API")|[ble](/ble "ble API")<br />[device](/common/device "device API")<br />[network](/common/network "network API")<br />| [cell](/cell "cell API")<br />[device](/common/device "device API")<br />[network](/common/network "network API")<br />[sock](/common/sock "sock API")<br />[location<sup>***</sup>](/common/location "location API")<br />[TLS&nbsp;security](/common/security "security API")<br>| N/A | N/A |

<sup>* For development/test purposes only.</sup></br>
<sup>** Only SPS API provided for native (on-chip) BLE interface, other APIs for native (on-chip) access, e.g. WiFi support, are a work in progress.</sup>

# Supported modules as `ubxlib` peripherals and APIs
Peripherals are u-blox modules which accept commands (e.g. AT-commands) over a serial interface and have no open MCU environment. To run the APIs they need to be attached to a host which runs `ubxlib`. For example in the [test farm](/port/platform/common/automation/DATABASE.md) combinations of hosts and peripherals are listed.

|`ubxlib` peripherals |NINA-B41 series<br />NINA-B31 series<br />NINA-B1 series<br />ANNA-B1|NINA-W13|NINA-W15|SARA-U2 series|LENA-R8 series|SARA-R4 series<br />SARA-R5 series<br />LARA-R6 series<br />|SARA-R510M8S<br />SARA-R422M8S|M8/M9/M10 series|
|-----------|-----------|--------------|-----|-----|-----|------|------|------|
|**APIs provided by host with peripheral attached**|[ble](/ble "ble API")<br />[device](/common/device "device API")<br />[network](/common/network "network API")|[wifi](/wifi)<br />[device](/common/device "device API")<br />[network](/common/network "network API")<br />[sock](/common/sock "sock API")<br />[TLS&nbsp;security](/common/security "security API")<br />[mqtt_client](/common/mqtt_client "MQTT client API")|[wifi](/wifi)<br />[ble](/ble "ble API")<br />[device](/common/device "device API")<br />[network](/common/network "network API")<br />[sock](/common/sock "sock API")<br />[TLS&nbsp;security](/common/security "security API")<br />[mqtt_client](/common/mqtt_client "MQTT client API")|[cell](/cell "cell API")<br />[device](/common/device "device API")<br />[network](/common/network "network API")<br />[sock](/common/sock "sock API")<br />[location<sup>***</sup>](/common/location "location API")<br />[TLS&nbsp;security](/common/security "security API")<br />[http_client](/common/http_client "HTTP client API")|[cell](/cell "cell API")<br />[device](/common/device "device API")<br />[network](/common/network "network API")<br />[sock](/common/sock "sock API")<br />[location<sup>***</sup>](/common/location "location API")<br />[TLS&nbsp;security](/common/security "security API")<br />[mqtt_client](/common/mqtt_client "MQTT client API")|[cell](/cell "cell API")<br />[device](/common/device "device API")<br />[network](/common/network "network API")<br />[sock](/common/sock "sock API")<br />[location<sup>***</sup>](/common/location "location API")<br />[security](/common/security "security API")<br />[mqtt_client](/common/mqtt_client "MQTT client API")<br />[http_client](/common/httpt_client "HTTP client API")|All APIs of<br />SARA-R4,<br />SARA-R5 series&nbsp;+<br />[gnss](/gnss "GNSS API")<br />[location](/common/location "location API")|[gnss](/gnss "GNSS API")<br />[location](/common/location "location API")|


<sup>*** Through the u-blox [CellLocate](https://www.u-blox.com/en/product/celllocate) mobile network-based location service.</sup>

# Structure of `ubxlib`
The APIs for each type of u-blox module can be found in the relevant directory (e.g. [cell](/cell) for cellular modules and [ble](/ble)/[wifi](/wifi) for BLE/Wi-Fi modules).  The [common](/common) directory contains APIs and 'helper' modules that are shared by u-blox modules, most importantly the [device](/common/device) API, the [network](/common/network) API and the [sockets](/common/sockets) API.  All APIs are documented in the API header files.

Examples demonstrating the use of the APIs can be found in the [example](/example) directory.  If you are using Zephyr or the u-blox [XPLR-IOT-1 platform](https://www.u-blox.com/en/product/xplr-iot-1) you will find examples that are very simple to install and use in https://github.com/u-blox/ubxlib_examples_xplr_iot.

Each API includes a `test` sub-directory containing the tests for that API which you may compile and run if you wish.

Build information for each platform can be found in the [platform](/port/platform) sub-directory of [port](/port); more on this below.

In order for u-blox to support multiple platforms with this code there is also a [port API](/port/api).  This is not intended to be a generic porting API, it is simply sufficient to support the APIs we require.  If you have not chosen a supported platform you may still be able to use the high level APIs here unchanged by [implementing the port API](/port#diy) for your platform.

```
+---example                    <-- examples that introduce the main features 
+---cfg                        <-- global configuration header files
+---common                     <-- APIs that are common across u-blox modules
¦   +---device                 <-- the simple device API for opening cell, short-range (i.e. BLE or Wi-Fi) and GNSS modules
¦   ¦   +---api                <-- all folders, in general, have an API directory
¦   ¦   +---src                    containing public headers, a source directory with
¦   ¦   +---test                   the implementation and a test directory with the tests
¦   +---network                <-- the simple network API for BLE, cell, Wi-Fi and GNSS
¦   +---sock                   <-- the sockets API for cell, Wi-Fi (and in the future BLE)
¦   +---security               <-- common API for u-blox security and TLS security/credential storage
¦   +---mqtt_client            <-- common MQTT client API
¦   +---http_client            <-- common HTTP client API
¦   +---location               <-- common location API, can use GNSS, Cell Locate, Cloud Locate and in the future Wi-Fi/BLE stations, etc.
¦   +---short_range            <-- internal API used by the BLE and Wi-Fi APIs (see below)
¦   +---at_client              <-- internal API used by the BLE, cell and Wi-Fi APIs
¦   +---ubx_protocol           <-- internal API used by the GNSS API
¦   +---geofence               <-- common geofence API that can be used with GNSS, cellular and Wi-Fi-based positioning
¦   +---spartn                 <-- message validation utilities for SPARTN
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
        +---static_size        <-- a build that measures RAM/flash usage
        +---common             <-- things common to all platforms, most notably...
            +---automation     <-- the internal Python automation scripts that test everything
            ...
```

# How to use this Repo
There are a few possible approaches to adopting `ubxlib`.  If you do not have a fixed environment (MCU, toolchain, RTOS, etc.) then the first two options offer an easy start; if you are constrained in how you must work (i.e. you must use a particular MCU or toolchain or RTOS), or you are happy to dive into the detail from the outset, then the third way is for you.

## The Easy Way 1: XPLR IoT
If you would like to explore cellular, positioning, Wi-Fi and BLE, along with a suite of sensors, all at once, you might consider purchasing a u-blox [XPLR-IOT-1 platform](https://www.u-blox.com/en/product/xplr-iot-1) and using the associated [ubxlib examples repo](https://github.com/u-blox/ubxlib_examples_xplr_iot), which allows you to install, build and run [Zephyr](https://www.zephyrproject.org/)-based applications.

## The Easy Way 2: PlatformIO
`ubxlib` is supported as a [PlatformIO](https://platformio.org/) library; if you have a board that either (a) runs [Zephyr](https://www.zephyrproject.org/) or (b) contains an ESP32 chip (running [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) or [Arduino](https://www.arduino.cc/)), then just follow the instructions [here](/port/platform/platformio) to load `ubxlib` directly into the [PlatformIO](https://platformio.org/) Visual Studio Code-based IDE.

## The Hard Way: Down in the Detail
This repo uses Git submodules: make sure that once it has been cloned you do something like:

`git submodule update --init --recursive`

...to obtain the submodules.

The native SDKs for each supported platform are used directly, unchanged, by this code.  To use this repo you must first choose your MCU and associated platform.  For instance, you might choose an STM32F4 MCU, which is supported via ST's STM32Cube IDE.  Instructions for how to install and use each platform can be found in your chosen MCU sub-directory; for an STM32F4 MCU this would be [port/platform/stm32cube/mcu/stm32f4](/port/platform/stm32cube/mcu/stm32f4).

Having chosen your MCU and installed the platform tools, navigate to the directories below your chosen MCU directory to find the required build information.  For instance, you may find a `runner` directory, which is a generic build that compiles any or all of the examples and tests that can run on a given platform.  In that directory you will find detailed information on how to perform the build.

Configuration information for the examples and the tests can be found in the `cfg` directory of your chosen MCU.  Depending on how you have connected your MCU to a u-blox module you may need to override this configuration, e.g. to change which MCU pin is connected to which pin of the u-blox module.  The `README.md` in the `runner` directory of your chosen MCU will tell you how to override conditional compilation flags in order to do this.

# Examples
A number of examples are provided with this repo:

|  Technology  | Example |
|--------------|---------|
| Cellular     | The [sockets](/example/sockets "socket example") example brings up a TCP/UDP socket by using the [device](/common/device "device API"), [network](/common/network "network API") and [sock](/common/sock "sock API") APIs.  |
| Cellular     | A [TLS-secured version](/example/sockets "TLS sockets example") of the sockets example. |
| Cellular     | A [DTLS-secured version](/example/sockets "DTLS sockets example") of the sockets example. |
| Cellular     | A [PPP version](/example/sockets "PPP sockets example") of the sockets example that shows how the platform IP stack/applications can use a cellular connection. |
| Cellular     | An [MQTT/MQTT-SN client](/example/mqtt_client "MQTT/MQTT-SN example") using the [MQTT/MQTT-SN client](/common/mqtt_client "MQTT/MQTT-SN client API") API.|
| Cellular     | An [HTTP client](/example/http_client "HTTP example") using the [HTTP client](/common/http_client "HTTP client API") API.|
| Cellular     | [CellLocate](/example/location "CellLocate example") example.|
| Cellular     | The [PSK generation](/example/security/psk "PSK example") example using the [security](/common/security "security API") API. |
| Bluetooth    | See the BLE examples in the [XPLR-IOT-1 ubxlib examples repo](https://github.com/u-blox/ubxlib_examples_xplr_iot/tree/master/examples). |
| Wi-Fi        | The [sockets](/example/sockets "sockets example") example brings up a TCP/UDP socket by using the [device](/common/device "device API"), [network](/common/network "network API") and [sock](/common/sock "sock API") APIs.  |
| GNSS         | [location](/example/location "location example") example using a GNSS chip connected directly or via a cellular module.|
| GNSS         | [cfg_val](/example/gnss "CFGVALXXX example") example configuring an M9 or later GNSS chip with CFGVALXXX messages.|
| GNSS         | [message](/example/gnss "message example") example communicating directly with a GNSS chip, messages of your choice.|
| GNSS         | [decode](/example/gnss "decode example") example decoding messages of your choice, not otherwise provided by `ubxlib`, from a GNSS chip.|
| GNSS         | [position](/example/gnss "position example") example obtaining streamed position directly from a GNSS chip.|
| GNSS         | [AssistNow](/example/gnss "AssistNow example") example of how to use the u-blox AssistNow service to improve the time to first fix of GNSS.|
| GNSS         | [geofence](/example/gnss "geofence example") example of how to use the comon [geofence](/common/geofence "geofence API") API with GNSS; note that it can equally be used with cellular (CellLocate) and Wi-Fi (Google, SkyHook and Here).|

You may use the code from any of these examples as a basis for your work as follows:
- Copy the source files for the [example](/example) that is closest to your intended application to your project directory.
- Remove all definitions and include files that are related purely to the `ubxlib` test system; for example you only need to include the [ubxlib.h](/ubxlib.h) file and you will want the entry point to be something like `int main()` rather than `U_PORT_TEST_FUNCTION(...)`.
- Adapt the definitions needed for your example, see the include file `u_cfg_app_platform_specific.h` for your platform; some examples of definitions that need to be set are:
  - UART/I2C/SPI number and UART/I2C/SPI pins (i.e. for whichever transport is being used) connecting the MCU to the target module,
  - network credentials (e.g. Wi-Fi SSID and password).
- Copy the make or cmake files from the `runner` directory of the port ([port/platform/](/port/platform/)) of your chosen MCU and adapt them to your application:
  - point out the `ubxlib` directory by setting the `UBXLIB_BASE` variable,
  - remove any definitions related to the `ubxlib` test environment as you wish,
  - if needed, add the source file(s) of your application to the make/cmake files.
- Build and flash your adapted example using your IDE of choice or command-line make/cmake.

General information about how to integrate `ubxlib` into a build system is available in the [port directory](/port) and platform specific information is available in the [platform specific port directory](/port/platform) for your chosen MCU.



# Where is `ubxlib` used?

The following table showcases repositories which build applications and use `ubxlib`.

| Description | Code repository | Radio Technology | Modules used | Boards used |
|-------------|--------------------|------------------|------------|-------|
| **BLE examples** such as: scanning, Serial Port Service (SPS), beacon, angle-of-arrival (AoA). | [https://github.com/u-blox/ubxlib_examples_xplr_iot](https://github.com/u-blox/ubxlib_examples_xplr_iot) | BLE | NORA-B1 | [XPLR-IOT-1](https://www.u-blox.com/en/product/XPLR-IOT-1) |
| **Cellular tracker** which publishes cellular signal strength parameters and location to an MQTT broker. | [https://github.com/u-blox/ubxlib_cellular_applications_xplr_iot](https://github.com/u-blox/ubxlib_cellular_applications_xplr_iot/releases/tag/v1.0) | LTE Cat-M1, GNSS | SARA-R5, MAX-M10 | [XPLR-IOT-1](https://www.u-blox.com/en/product/XPLR-IOT-1) |
| **Location example** to get location using both [CellLocate](https://www.u-blox.com/en/product/celllocate) and [CloudLocate](https://www.u-blox.com/en/product/cloudlocate). Calculates location based on cellular, Wi-Fi or GNSS information. | [https://github.com/u-blox/XPLR-IOT-1-location-example](https://github.com/u-blox/XPLR-IOT-1-location-example) | LTE Cat-M1, WiFi, BLE | NINA-W1, SARA-R5, MAX-M10 | [XPLR-IOT-1](https://www.u-blox.com/en/product/XPLR-IOT-1) |
| **High precision GNSS (HPG)** solution for evaluation and prototyping with the [PointPerfect](https://www.u-blox.com/en/product/pointperfect) GNSS augmentation service. | [https://github.com/u-blox/XPLR-HPG-software](https://github.com/u-blox/XPLR-HPG-software) | GNSS, WiFi, LTE Cat-1 |ZED-F9, NEO-D9, NINA-W1, NORA-W1, LARA-R6 | [XPLR-HPG-1](https://www.u-blox.com/en/product/xplr-hpg-1), [XPLR-HPG-2](https://www.u-blox.com/en/product/xplr-hpg-2) |
| **Sensor aggregation**. Collect data from sensors and send it to the Thingstream platform using either Wi-Fi or Cellular Network connections. | [https://github.com/u-blox/XPLR-IOT-1-software](https://github.com/u-blox/XPLR-IOT-1-software) | WiFi, LTE Cat-M1 | NINA-W1, SARA-R5 | [XPLR-IOT-1](https://www.u-blox.com/en/product/XPLR-IOT-1) |
| **Air Quality Monitor** with Sensirion Sensor showing how sensor data be sent to the [Thingstream](https://www.u-blox.com/en/product/thingstream) platform via MQTT, and then visualized in a Node-RED dashboard. | [https://github.com/u-blox/XPLR-IOT-1-Air-Quality-Monitor-Example](https://github.com/u-blox/XPLR-IOT-1-Air-Quality-Monitor-Example) | BLE | NORA-B1 | [XPLR-IOT-1](https://www.u-blox.com/en/product/XPLR-IOT-1), [MikroE HVAC](https://www.mikroe.com/hvac-click) |



# Feature Request and Roadmap
New features can be requested and up-voted [here](https://github.com/u-blox/ubxlib/issues/12). The comments of this issue also contains an outlook about features of upcoming releases. Also it is the right place to discuss features and their priority.

# License
The software in this repository is Apache 2.0 licensed and copyright u-blox with the following exceptions:

- The heap management code (`heap_useNewlib.c`), required because the [nRF5 SDK](/port/platform/nrf5sdk) and [STM32Cube](/port/platform/stm32cube) platforms don't provide the necessary memory management for [newlib](https://sourceware.org/newlib/libc.html) and [FreeRTOS](https://www.freertos.org) to play together, is copyright Dave Nadler.
- The SEGGER RTT logging files in [port/platform/nrf5sdk/src/segger_rtt](/port/platform/nrf5sdk/src/segger_rtt) are copyright SEGGER MICROCONTROLLER GmbH & Co. KG.
- The AT client code in [common/at_client](/common/at_client) is derived from the Apache 2.0 licensed AT parser of [mbed-os](https://github.com/ARMmbed/mbed-os).
- The [stm32cube platform directory](/port/platform/stm32cube/src) necessarily includes porting files from the STM32F4 SDK that are copyright ST Microelectronics.
- The `go` echo servers in [common/sock/test/echo_server](/common/sock/test/echo_server) are based on those used in testing of [AWS FreeRTOS](https://github.com/aws/amazon-freertos).
- The `setjmp()/longjmp()` implementation in [port/clib/u_port_setjmp.S](/port/clib/u_port_setjmp.S), used when testing the Zephyr platform, is copyright Nick Clifton, Cygnus Solutions and part of [newlib](https://sourceware.org/newlib/libc.html).
- The `base64` implementation in [common/utils/src/base64.h](/common/utils/src/base64.h) is copyright [William Sherif](https://github.com/superwills/NibbleAndAHalf).
- The ARM callstack iterator in [port/platform/common/debug_utils/src/arch/arm/u_stack_frame_cortex.c](/port/platform/common/debug_utils/src/arch/arm/u_print_callstack_cortex.c) is copyright Armink, part of [CmBacktrace](https://github.com/armink/CmBacktrace).
- The FreeRTOS additions [port/platform/common/debug_utils/src/freertos/additions](/port/platform/common/debug_utils/src/freertos/additions) are copied from the Apache licensed [ESP-IDF](https://github.com/espressif/esp-idf).
- If you compile-in geofencing by defining the conditional compilation flag `U_CFG_GEOFENCE` for your build:
  - For shapes larger than about 1 km, to employ a true-earth model you may choose to conditionally link the sub-module [common/geofence](geographiclib) from the MIT licensed [GeographicLib](https://github.com/geographiclib) by Charles A. Karney.
  - The default functions, which assume a spherical earth, are derived from the valuable advice (though not the code) of https://www.movable-type.co.uk/scripts/latlong.html, MIT licensed and copyright Chris Veness.

In all cases copyright, and our thanks, remain with the original authors.

# Disclaimer
- The software in this repository assumes the module is in a state equal to a factory reset.
- If you modify the AT command sequences employed by `ubxlib` please take the time to debug/test those changes as we can't easily support you.
