# Introduction
This directory contains the GNSS APIs, designed to configure and read position from a u-blox GNSS chip.  This API is technology (i.e. GNSS) specific; if instead all you want to know is your location and you don't care how it is done (e.g. as well as or instead of using GNSS it could use Cell Locate or in the future BLE beacons), please use the more generic [common/location](/common/location) API instead.

The GNSS APIs are split into the following groups:

- `<no group>`: init/deinit of the GNSS API and adding a GNSS instance.
- `pwr`: control the power state of a GNSS module.
- `cfg`: configuration of a GNSS module.
- `pos`: reading position from a GNSS module.
- `info`: read other information from a GNSS module.
- `util`: utility functions for use with a GNSS module.

The module types supported by this implementation are listed in [u_gnss_module_type.h](api/u_gnss_module_type.h).

This API relies upon the [common/ubx_protocol](/common/ubx_protocol) component to encode commands for and decode responses from a u-blox GNSS module and the [common/at_client](/common/at_client) component when an intermediate AT (e.g. cellular) module is employed between this MCU and the GNSS module.

# Usage
The [api](api) directory contains the files that define the GNSS APIs, each API function documented in its header file.  In the [src](src) directory you will find the implementation of the APIs and in the [test](test) directory the tests for the APIs that can be run on any platform.

A simple usage example is given below.  Note that, before calling `app_start()` the platform must be initialised (clocks started, heap available, RTOS running), in other words `app_task()` can be thought of as a task entry point.  If you open the `u_main.c` file in the `app` directory of your platform you will see how we do this, with `main()` calling a porting API `uPortPlatformStart()` to sort that all out; you could paste the example code into `app_start()` there (and add the inclusion of `ubxlib.h`) as a quick and dirty test (`runner` will build it).

```
#include "ubxlib.h"
#include "u_cfg_app_platform_specific.h"

// The entry point: before this is called the system
// clocks must have been started and the RTOS must be running;
// we are in task space.
int app_start() {
    uGnssTransportHandle_t transportHandle;
    uDeviceHandle_t gnssHandle = NULL;
    int32_t latitudeX1e7;
    int32_t longitudeX1e7;

    // Initialise the APIs we will need
    uPortInit();
    uGnssInit();

    // Open a UART with the recommended buffer length
    // on your chosen UART HW block and on the pins
    // where the GNSS module's UART interface is
    // connected to your MCU: you need to know these
    // for your hardware, either set the #defines
    // appropriately or replace them with the right
    // numbers, using -1 for a pin that is not connected.
    transportHandle.uart = uPortUartOpen(U_CFG_APP_GNSS_UART,
                                         U_GNSS_UART_BAUD_RATE, NULL,
                                         U_GNSS_UART_BUFFER_LENGTH_BYTES,
                                         U_CFG_APP_PIN_GNSS_TXD,
                                         U_CFG_APP_PIN_GNSS_RXD,
                                         U_CFG_APP_PIN_GNSS_CTS,
                                         U_CFG_APP_PIN_GNSS_RTS);

    // Add a GNSS instance, giving it the UART handle and
    // the pin that enables power to the GNSS module; use
    // -1 if there is no such pin.
    uGnssAdd(U_GNSS_MODULE_TYPE_M8,
             U_GNSS_TRANSPORT_UART, transportHandle,
             U_CFG_APP_PIN_GNSS_ENABLE_POWER, false,
             &gnssHandle);

    // To get prints of the message exchange with the GNSS module
    uGnssSetUbxMessagePrint(gnssHandle, true);

    // Power up the GNSS module
    if (uGnssPwrOn(gnssHandle) == 0) {
        // Read position
        if (uGnssPosGet(gnssHandle, &latitudeX1e7, &longitudeX1e7,
                        NULL, NULL, NULL, NULL, NULL, NULL) == 0) {
            printf("I am here: https://maps.google.com/?q=%3.7f,%3.7f\n",
                   ((double) latitudeX1e7) / 10000000,
                   ((double) longitudeX1e7) / 10000000); 
        }
        // When finished using the module
        uGnssPwrOff(gnssHandle);
    }

    // Calling these will also deallocate all the handles that
    // were allocated above.
    uGnssDeinit();
    uPortDeinit();

    while(1);
}
```
