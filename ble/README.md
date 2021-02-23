# Introduction
This directory contains the BLE APIs for control and connectivity.

The BLE APIs are split into the following groups:

- `<no group>`: init/deinit of the BLE api and adding a BLE instance.
- `cfg`: configuration of the short range module or internal setup
- `data`: for exchanging data

If all you would like to do is bring up connection as simply as possible and then get on with exchanging data, please consider using the `common/network` API to set everything up instead, then just the `data` API for transport.

This API is designed to work in either of two ways:
1.  With an external short range module connected over UART, this is if;
- it is desired to have the connectivity and radio on a separate hardware to avoid having it interfere with the product´s main tasks.
- the main MCU lacks BLE connectivity
2. Using internal BLE connectivity

NOTE! Internal BLE is work in progress, not included in this version. For that reason all of the below is in the 1. external short range module context.

The module types supported by this implementation are listed in `api/u_ble_module_type.h`. The actually connection will be using the u-Blox SPS protocol. So the remote device needs to support this. All u-Blox NINA, ANNA and ODIN family products support this (if ble is supported) and there are open source apps for Android  (https://github.com/u-blox/Android-u-blox-BLE) and iOS (https://github.com/u-blox/iOS-u-blox-BLE). Or you can implement it yourself using (https://www.u-blox.com/sites/default/files/LowEnergySerialPortService_ProtocolSpec_%28UBX-16011192%29.pdf)

This API can safely be using simultaneously with the `wifi` API using the same short range module, both will be calling into `common/short_range` but all possible conflicts will be handled by lower layers and all calls are protected by mutexes.

This API relies upon the `at_client` common component to send commands to and parse responses received from a short range module.

The communication with the module is using the binary EDM protocol. The app can ignore this, it will be handled by lower layers.

It is not in the scoop of this API to support the full range short range module (AT) API. However, using `cfg` as a prototype it is easy to add additional commands to your application. Make sure your code locks the short range mutex. 

# Usage
The `api` directory contains the files that define the BLE APIs, each API function documented in its header file.  In the `src` directory you will find the implementation of the APIs and in the `test` directory the tests for the APIs that can be run on any platform.

A simple usage example is given below.  Note that, before calling `app_start()` the platform must be initialised (clocks started, heap available, RTOS running), in other words `app_task()` can be thought of as a task entry point.  If you open the `u_main.c` file in the `app` directory of your platform you will see how we do this, with `main()` calling a porting API `uPortPlatformStart()` to sort that all out; you could paste the example code into `app_start()` there (and add the inclusions) as a quick and dirty test (`runner` will build it).

```
#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"

#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_uart.h"

#include "u_at_client.h"

#include "u_short_range_module_type.h"
#include "u_short_range.h"
#include "u_short_range_edm_stream.h"

#include "u_ble_module_type.h"
#include "u_ble.h"

// The entry point: before this is called the system
// clocks must have been started and the RTOS must be running;
// we are in task space.
int app_start() {
{
    int32_t uartHandle = -1;
    int32_t edmStreamHandle = -1;
    int32_t atClientHandle = NULL;
    int32_t bleHandle = -1;

    // Initialise the porting layer
    if (uPortInit() == 0) {
        // Open a UART with the standard parameters
        // Note that for some platform overlay files will be used instead of the pins set hear.
        uartHandle = uPortUartOpen(U_CFG_APP_SHORT_RANGE_UART,
                                   115200, NULL,
                                   U_SHORT_RANGE_UART_BUFFER_LENGTH_BYTES,
                                   U_CFG_APP_PIN_SHORT_RANGE_TXD,
                                   U_CFG_APP_PIN_SHORT_RANGE_RXD,
                                   U_CFG_APP_PIN_SHORT_RANGE_CTS,
                                   U_CFG_APP_PIN_SHORT_RANGE_RTS);
    }

    if (uartHandle >= 0) {
        // UART is up, create a EDM stream (handle the short range module binary communication in the UART)
        if (uShortRangeEdmStreamInit() == 0) {
            edmStreamHandle = uShortRangeEdmStreamOpen(uartHandle);
            if (edmStreamHandle >= 0) {
                // Start the AT client and connect i to the EDM stream (data sent to and from the AT client is
                // human readable format)
                if (uAtClientInit() == 0) {
                    atClientHandle = uAtClientAdd(edmStreamHandle,
                                                  U_AT_CLIENT_STREAM_TYPE_EDM,
                                                  NULL,
                                                  U_SHORT_RANGE_AT_BUFFER_LENGTH_BYTES);
                    if (atClientHandle != NULL) {
                        uShortRangeEdmStreamSetAtHandle(edmStreamHandle, atClientHandle);
                    }
                }
            }
        }
    }

    if (atClientHandle != NULL) {
        // So that we can see what we're doing
        uAtClientTimeoutSet(pParameters->atClientHandle, 2000);
        uAtClientPrintAtSet(pParameters->atClientHandle, true);
        uAtClientDebugSet(pParameters->atClientHandle, true);
        // Setup the ble instance, will start a short range instance if needed
        if (uBleInit() == 0) {
            bleHandle = uBleAdd(moduleType, atClientHandle);
        }
    }

    if (bleHandle >= 0) {
        // All needed instances created, detect if a module is connected
        uBleModuleType_t module = uBleDetectModule(bleHandle);

        if (!(module == (U_SHORT_RANGE_MODULE_TYPE_INVALID ||
                         U_SHORT_RANGE_MODULE_TYPE_UNSUPPORTED))) {
            // A valid module responding as expected and supported ble is found, configure it
            // to start ble as a peripheral
            uBleCfg_t cfg;
            cfg.role = U_BLE_CFG_ROLE_PERIPHERAL;
            cfg.spsServer = false; // If you want to use SPS, it is recommended to use the network layer
            errorCode = uBleCfgConfigure(bleHandle, &cfg);
            
            if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
            // Module is up and running with the set configuration and ble radio is on advertising
            }
        }

    }
    
    //Clean up
    uBleDeinit();

    uAtClientRemove(atClientHandle);
    uAtClientDeinit();

    uShortRangeEdmStreamClose(edmStreamHandle);
    uShortRangeEdmStreamDeinit();
    uPortUartClose(uartHandle);

    uPortDeinit();

    while(1);
}
```
