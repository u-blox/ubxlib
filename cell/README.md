# Introduction
This directory contains the cellular APIs, designed to provide a simple control interface to a u-blox cellular module.

The cellular APIs are split into the following groups:

- `<no group>`: init/deinit of the cellular API and adding a cellular instance.
- `cfg`: configuration of the cellular module.
- `pwr`: powering up and down the cellular module.
- `net`: attaching to the cellular network.
- `info`: obtaining information about the cellular module.
- `sec`: u-blox security features.
- `sec_tls`: TLS security features.
- `sock`: sockets, for exchanging data (but see the [common/sock](/common/sock) component for the best way to do this).
- `mqtt`: MQTT client (but see the [common/mqtt_client](/common/mqtt_client) component for the best way to do this).
- `loc`: getting a location fix using the Cell Locate service (but see the [common/location](/common/location) component for the best way to do this); you will need an authentication token from the [Location Services section](https://portal.thingstream.io/app/location-services) of your [Thingstream portal](https://portal.thingstream.io/app/dashboard). If you have a GNSS chip attached via a cellular module and want to control it directly from your MCU see the [gnss](/gnss) API but note that the `loc` API here will make use of a such a GNSS chip where that in any case.
- `gpio`: configure and set the state of GPIO lines that are on the cellular module.

The module types supported by this implementation are listed in [u_cell_module_type.h](api/u_cell_module_type.h).

HOWEVER, this is the detailed API; if all you would like to do is bring up a bearer as simply as possible and then get on with exchanging data or establishing location, please consider using the [common/network](/common/network) API, along with the [common/sock](/common/sock) API, the [common/security](/common/security) API and the [common/location](/common/location) API.  You may still dip down into this API from the network level as the handles used at the network level are the ones generated here.

This API relies upon the [common/at_client](/common/at_client) component to send commands to and parse responses received from a cellular module.

# Usage
The [api](api) directory contains the files that define the cellular APIs, each API function documented in its header file.  In the [src](src) directory you will find the implementation of the APIs and in the [test](test) directory the tests for the APIs that can be run on any platform.

A simple usage example is given below.  Note that, before calling `app_start()` the platform must be initialised (clocks started, heap available, RTOS running), in other words `app_task()` can be thought of as a task entry point.  If you open the `u_main.c` file in the `app` directory of your platform you will see how we do this, with `main()` calling a porting API `uPortPlatformStart()` to sort that all out; you could paste the example code into `app_start()` there (and add the inclusions) as a quick and dirty test (`runner` will build it).

Throughout the `cell` API, in functions which can take more than a few seconds to complete, you will find a `keepGoingCallback()` parameter.  This parameter is intended for situations where the application needs control of the timeout of the API call or needs to feed a watchdog timer.  The callback will be called approximately once a second while the API function is operating and, if it returns `false`, the API function will be terminated.  Set the parameter to `NULL` if no specific timeout is required, or no watchdog needs to be fed.

```
#include "ubxlb.h"
#include "u_cfg_app_platform_specific.h"

// The entry point: before this is called the system
// clocks must have been started and the RTOS must be running;
// we are in task space.
int app_start() {
    int32_t uartHandle;
    uAtClientHandle_t atHandle;
    uDeviceHandle_t cellHandle = NULL;
    char buffer[U_CELL_NET_IP_ADDRESS_SIZE];
    int32_t mcc;
    int32_t mnc;

    // Initialise the APIs we will need
    uPortInit();
    uAtClientInit();
    uCellInit();

    // Open a UART with the recommended buffer length
    // on your chosen UART HW block and on the pins
    // where the cellular module's UART interface is
    // connected to your MCU: you need to know these
    // for your hardware, either set the #defines
    // appropriately or replace them with the right
    // numbers, using -1 for a pin that is not connected.
    uartHandle = uPortUartOpen(U_CFG_APP_CELL_UART,
                               115200, NULL,
                               U_CELL_UART_BUFFER_LENGTH_BYTES,
                               U_CFG_APP_PIN_CELL_TXD,
                               U_CFG_APP_PIN_CELL_RXD,
                               U_CFG_APP_PIN_CELL_CTS,
                               U_CFG_APP_PIN_CELL_RTS);

    // Add an AT client on the UART with the recommended
    // default buffer size.
    atHandle = uAtClientAdd(uartHandle,
                            U_AT_CLIENT_STREAM_TYPE_UART,
                            NULL,
                            U_CELL_AT_BUFFER_LENGTH_BYTES);

    // Set printing of AT commands by the cellular driver,
    // which can be useful while debugging.
    uAtClientPrintAtSet(atHandle, true);

    // Add a cell instance, in this case a SARA-R5 module,
    // giving it the AT client handle and the pins where
    // the cellular module's control interface is 
    // connected to your MCU: you need to know these for
    // your hardware; again use -1 for "not connected".
    uCellAdd(U_CELL_MODULE_TYPE_SARA_R5,
             atHandle,
             U_CFG_APP_PIN_CELL_ENABLE_POWER,
             U_CFG_APP_PIN_CELL_PWR_ON,
             U_CFG_APP_PIN_CELL_VINT, false,
             &cellHandle);

    // Power up the cellular module
    if (uCellPwrOn(cellHandle, NULL, NULL) == 0) {
        // Connect to the cellular network with all default parameters
        if (uCellNetConnect(cellHandle, NULL, NULL, NULL, NULL, NULL) == 0) {

            // Do things, for example
            if (uCellNetGetOperatorStr(cellHandle, buffer, sizeof(buffer)) >= 0) {
                printf("Registered on \"%s\".\n", buffer);
            }
            if (uCellNetGetMccMnc(cellHandle, &mcc, &mnc) == 0) {
                printf("The MCC/MNC of the network is %d%d.\n", mcc, mnc);
            }
            if (uCellNetGetIpAddressStr(cellHandle, buffer) >= 0) {
                printf("Our IP address is \"%s\".\n", buffer);
            }
            if (uCellNetGetApnStr(cellHandle, buffer, sizeof(buffer)) >= 0) {
                printf("The APN used was \"%s\".\n", buffer);
            }

            // When finished with the connection
            uCellNetDisconnect(cellHandle, NULL);
        }
        // When finished using the module
        uCellPwrOff(cellHandle, NULL);
    }

    // Calling these will also deallocate all the handles that
    // were allocated above.
    uCellDeinit();
    uAtClientDeinit();
    uPortDeinit();

    while(1);
}
```