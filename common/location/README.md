# Introduction
These directories provide a very simple location API, providing a means to establish position using any u-blox module, potentially in conjunction with a cloud service.  It relies on the [common/network](/common/network) API to bring up and down the network type that it uses and the underlying APIs ([gnss](/gnss), [cell](/cell), [wifi](/wifi), etc.) to do the heavy lifting.

# Usage
The directories include the API and the C source files necessary to call into the underlying [gnss](/gnss), [cell](/cell) and [wifi](/wifi) APIs.  The [test](test) directory contains a small number of generic tests for the `location` API; for comprehensive tests of networking please refer to the test directory of the underlying APIs.

A simple usage example, obtaining position via a GNSS chip, is shown below.  Note that, before calling `app_start()` the platform must be initialised (clocks started, heap available, RTOS running), in other words `app_task()` can be thought of as a task entry point.  If you open the `u_main.c` file in the `app` directory of your platform you will see how we do this, with `main()` calling a porting API `uPortPlatformStart()` to sort that all out; you could paste the example code into `app_start()` there (and add the inclusions) as a quick and dirty test (`runner` will build it).

```
#include "ubxlib.h"
#include "u_cfg_app_platform_specific.h"

// The entry point: before this is called the system
// clocks must have been started and the RTOS must be running;
// we are in task space.
int app_start() {
    uDeviceHandle_t devHandle = NULL;
    int32_t x;
    uLocation_t location;
    uDeviceCfg_t deviceCfg = {
        .deviceType = U_DEVICE_TYPE_GNSS,
        .deviceCfg = {
            .cfgGnss = {
                .moduleType = U_CFG_TEST_GNSS_MODULE_TYPE,
                .transportType = U_GNSS_TRANSPORT_UART,
                .pinGnssEnablePower = U_CFG_APP_PIN_GNSS_ENABLE_POWER,
                .devHandleAt = NULL, // Only relevant for transport U_GNSS_TRANSPORT_AT
                .gnssAtPinPwr = -1, // Only relevant for transport U_GNSS_TRANSPORT_AT
                .gnssAtPinDataReady = -1 // Only relevant for transport U_GNSS_TRANSPORT_AT
            },
        },
        .transportType = U_DEVICE_TRANSPORT_TYPE_UART,
        .transportCfg = {
            .cfgUart = {
                .uart = U_CFG_APP_GNSS_UART,
                .baudRate = U_GNSS_UART_BAUD_RATE,
                .pinTxd = U_CFG_APP_PIN_GNSS_TXD,
                .pinRxd = U_CFG_APP_PIN_GNSS_RXD,
                .pinCts = U_CFG_APP_PIN_GNSS_CTS,
                .pinRts = U_CFG_APP_PIN_GNSS_RTS
            },
        },
    };
    uNetworkCfgGnss_t networkCfg = {
        .type = U_NETWORK_TYPE_GNSS
    };

    // Initialise the APIs we will need
    uPortInit();
    uDeviceInit();

    // Open the device
    x = uDeviceOpen(&deviceCfg, &devHandle);
    uPortLog("## Opened device with return code %d.\n", x);

    // Bring up the network layer
    if (uNetworkInterfaceUp(devHandle, U_NETWORK_TYPE_GNSS,
                            &networkCfg) == 0) {
        // Get location
        if (uLocationGet(devHandle, U_LOCATION_TYPE_GNSS,
                         NULL, NULL, &location, NULL) == 0) {

            printf("I am here: https://maps.google.com/?q=%3.7f,%3.7f\n",
                    ((double) location.latitudeX1e7) / 10000000,
                    ((double) location.longitudeX1e7) / 10000000);

        }
        // When finished with the GNSS network layer
        uNetworkInterfaceDown(devHandle, U_NETWORK_TYPE_GNSS);
    }

    // Close the device
    uDeviceClose(devHandle, true);

    // Tidy up
    uDeviceDeinit();
    uPortDeinit();

    while(1);
}
```