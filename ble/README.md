# Introduction
This directory contains the BLE APIs for control and connectivity.

The BLE APIs are split into the following groups:

- `<no group>`: init/deinit of the BLE api and adding a BLE instance.
- `cfg`: configuration of the short range module or internal setup.
- `data`: for exchanging data.

If all you would like to do is bring up a connection as simply as possible and then get on with exchanging data, please consider using the [common/network](/common/network) API to set everything up instead, then use the [u_ble_sps.h](api/u_ble_sps.h) API for transport.

This API is designed to work in either of two ways:
1.  with an external short range module connected over UART, this is if:
  - it is desired to have the connectivity and radio on a separate hardware to avoid having it interfere with the product's main tasks,
  - the main MCU lacks BLE connectivity,
2. using internal BLE connectivity.

NOTE: internal BLE is a work in progress, only supported on Zephyr so far.

The module types supported by this implementation are listed in [u_ble_module_type.h](api/u_ble_module_type.h). The actual connection will be using the u-Blox SPS protocol, so the remote device needs to support this. All u-Blox NINA, ANNA and ODIN family products support this (if BLE is supported) and there are open source apps for Android  (https://github.com/u-blox/Android-u-blox-BLE) and iOS (https://github.com/u-blox/iOS-u-blox-BLE), or you can implement it yourself following the [documentation](https://www.u-blox.com/sites/default/files/LowEnergySerialPortService_ProtocolSpec_%28UBX-16011192%29.pdf).

This API can safely be used simultaneously with the [wifi](/wifi) API using the same short range module, both will be calling into [common/short_range](/common/short_range) but all possible conflicts will be handled by the lower layers and all calls are protected by mutexes.

This API relies on the [common/at_client](/common/at_client) component to send commands to and parse responses received from a short range module.

Communication with an external module uses the binary EDM protocol. The app can ignore this, it will be handled by lower layers.

It is not in the scope of this API to support the full range short range module (AT) API. However, using the BLE `cfg` API as a prototype it is easy to add additional commands to your application. Make sure your code locks the short range mutex. 

# Usage
The [api](api) directory contains the files that define the BLE APIs, each API function documented in its header file.  In the [src](src) directory you will find the implementation of the APIs and in the [test](test) directory the tests for the APIs that can be run on any platform.