# Introduction
These directories provide a very simple network API.  Its purpose is to allow an application which purely performs data-plane operations, e.g. using the `sock` API or using the `location` API for positioning, to bring up a network connection and tear it down again without knowing in detail about the processes required to do so.  The connection may be over cellular, Wifi, BLE or GNSS (which is obviously downlink only).

For any other operations the same handle as returned by the network API may be used to call the underlying network layer so that the `cell`, `wifi`, `ble` or `gnss` APIs can also be called in the normal way.

This is not a "manager", it is not smart, it solely exists to allow applications to obtain a connection without caring about the details of the connection other than making a choice between cellular, Wifi etc.  Note that the configuration structure provided to this API by the underlying APIs may be limited to keep things simple, e.g. it may not be possible to chose the interface speed or to provide static data buffers etc.

# Usage
The directories include the API and the C source files necessary to call into the underlying `cell`, `wifi`, `ble` and `gnss` APIs.  The `test` directory contains a small number of generic tests for the `network` API; for comprehensive tests of networking please refer to the test directory of the underlying APIs.

A simple usage example, sending data over a TCP socket, is shown below.  Note that, before calling `app_start()` the platform must be initialised (clocks started, heap available, RTOS running), in other words `app_task()` can be thought of as a task entry point.  If you open the `u_main.c` file in the `app` directory of your platform you will see how we do this, with `main()` calling a porting API `uPortPlatformStart()` to sort that all out; you could paste the example code into `app_start()` there (and add the inclusions) as a quick and dirty test (`runner` will build it).

```
#include "stdio.h"
#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"
#include "string.h"

#include "u_port.h"

#include "u_cfg_app_platform_specific.h"

#include "u_cell_module_type.h" // For U_CELL_MODULE_TYPE_SARA_R5

#include "u_network.h"
#include "u_network_config_ble.h"
#include "u_network_config_cell.h"
#include "u_network_config_wifi.h"

#include "u_sock.h"

// Configuration information specific to the
// chosen transport, in this case cell,
// could equally be of type uNetworkConfigurationWifi_t
// with the specific configuration for a Wifi
// connection, etc.
// Default values for all of U_CFG_APP_xxx are
// defined in the u_cfg_app_platform_specific.h
// file for your chosen platform.
const uNetworkConfigurationCell_t gConfiguration = {U_NETWORK_TYPE_CELL,
                                                    U_CELL_MODULE_TYPE_SARA_R5,
                                                    NULL, /* SIM pin */
                                                    NULL, /* APN: accept default */
                                                    240, /* Connection timeout in seconds */
                                                    U_CFG_APP_CELL_UART,
                                                    U_CFG_APP_PIN_CELL_TXD,
                                                    U_CFG_APP_PIN_CELL_RXD,
                                                    U_CFG_APP_PIN_CELL_CTS,
                                                    U_CFG_APP_PIN_CELL_RTS,
                                                    U_CFG_APP_PIN_CELL_ENABLE_POWER,
                                                    U_CFG_APP_PIN_CELL_PWR_ON,
                                                    U_CFG_APP_PIN_CELL_VINT};

#define MY_SERVER_NAME "something.com"
#define MY_SERVER_PORT 42

// The entry point: before this is called the system
// clocks must have been started and the RTOS must be running;
// we are in task space.
int app_start() {
    int32_t networkHandle;
    int32_t sock;
    uSockAddress_t remoteAddress;
    int32_t x;
    size_t size;
    size_t sentSize = 0;
    const char *pMessage = "The quick brown fox jumps over the lazy dog."

    // Initialise the APIs we will need
    uPortInit();
    uNetworkInit();

    // Add a network instance, in this case of type cell
    // since that's what we have configuration information for
    networkHandle = uNetworkAdd(U_NETWORK_TYPE_CELL,
                                (void *) &gConfiguration);

    // Bring up the network layer
    if (uNetworkUp(networkHandle) == 0) {

        // Do things using the network, for
        // example send data over a TCP socket
        // to a server as follows

        // Get the server's IP address using
        // the network's DNS resolution facility
        uSockGetHostByName(networkHandle, MY_SERVER_NAME,
                           &(remoteAddress.ipAddress));
        remoteAddress.port = MY_SERVER_PORT;

        // Create the socket using the network
        sock = uSockCreate(networkHandle,
                           U_SOCK_TYPE_STREAM,
                           U_SOCK_PROTOCOL_TCP);

        // Connect the socket to our server
        uSockConnect(sock, &remoteAddress);

        // Send the data over the socket
        size = strlen(pMessage);
        while (sentSize < size) {
            x = uSockWrite(sock, (void *) (pMessage + sentSize),
                           size - sentSize);
            if (x > 0) {
                sentSize += x;
            }
        }

        // Close the socket
        uSockClose(sock);
        uSockCleanUp(sock);

        // When finished with the network layer
        uNetworkDown(networkHandle);
    }

    // Calling these will also deallocate the network handle
    uNetworkDeinit();
    uPortDeinit();

    while(1);
}
```