# Introduction
These directories provide a very simple location API, providing a means to establish position using any u-blox module, potentially in conjunction with a cloud service.  It relies on underlying APIs (`gnss`, `cell`, `short_range`, etc.) to do the heavy lifting.

# Usage
The directories include the API and the C source files necessary to call into the underlying `cell`, `short_range` and `gnss` APIs.  The `test` directory contains a small number of generic tests for the `location` API; for comprehensive tests of networking please refer to the test directory of the underlying APIs.

A simple usage example, obtaining position via a GNSS chip, is shown below.  Note that, before calling `app_start()` the platform must be initialised (clocks started, heap available, RTOS running), in other words `app_task()` can be thought of as a task entry point.  If you open the `u_main.c` file in the `app` directory of your platform you will see how we do this, with `main()` calling a porting API `uPortPlatformStart()` to sort that all out; you could paste the example code into `app_start()` there (and add the inclusions) as a quick and dirty test (`runner` will build it).

```
#include "stdio.h"
#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"

#include "u_port.h"

#include "u_cfg_app_platform_specific.h"

#include "u_gnss_types.h"

#include "u_network.h"
#include "u_network_config_gnss.h"

#include "u_location.h"

// Configuration information specific to GNSS.
// Default values for all of U_CFG_APP_xxx are
// defined in the u_cfg_app_platform_specific.h
// file for your chosen platform.
const uNetworkConfigurationGnss_t gConfiguration = {U_NETWORK_TYPE_GNSS,
                                                    U_GNSS_MODULE_TYPE_M8,
                                                    U_CFG_APP_PIN_GNSS_EN,
                                                    U_GNSS_TRANSPORT_UBX_UART, // Connecton is UART
                                                    U_CFG_APP_GNSS_UART,
                                                    U_CFG_APP_PIN_GNSS_TXD,
                                                    U_CFG_APP_PIN_GNSS_RXD,
                                                    U_CFG_APP_PIN_GNSS_CTS,
                                                    U_CFG_APP_PIN_GNSS_RTS};

// The entry point: before this is called the system
// clocks must have been started and the RTOS must be running;
// we are in task space.
int app_start() {
    int32_t networkHandle;
    uLocation_t location;

    // Initialise the APIs we will need
    uPortInit();
    uNetworkInit();

    // Add a network instance of type GNSS
    networkHandle = uNetworkAdd(U_NETWORK_TYPE_GNSS,
                                (void *) &gConfiguration);

    // Bring up the network layer
    if (uNetworkUp(networkHandle) == 0) {

        // Get location
        if (uLocationGet(networkHandle, U_LOCATION_TYPE_GNSS,
                         NULL, NULL, &location, NULL) == 0) {
            printf("I am here: https://maps.google.com/?q=%.5f,%.5f\n",
                   ((double) location.latitudeX1e6) / 1000000,
                   ((double) location.longitudeX1e6) / 1000000); 
        }

        // When finished with the network layer
        uNetworkDown(networkHandle);
    }

    // Calling these will also deallocate the network handle
    uNetworkDeinit();
    uPortDeinit();

    while(1);
}
```