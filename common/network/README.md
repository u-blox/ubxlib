# Introduction
These directories provide a very simple network API, to be used in conjunction with the [common/device](/common/device) API.  Its purpose is to allow an application which purely performs data-plane operations, e.g. using the [common/sock](/common/sock) API or using the [common/location](/common/location) API for positioning, to bring up a network connection and tear it down again without knowing in detail about the processes required to do so.  The connection may be over cellular, Wi-Fi, BLE or GNSS (which is obviously downlink only).

For any other operations the same handle as returned by the network API may be used to call the underlying network layer so that the [cell](/cell), [wifi](/wifi), [ble](/ble) or [gnss](/gnss) APIs can also be called in the normal way.

This is not a "manager", it is not smart, it solely exists to allow applications to obtain a connection without caring about the details of the connection other than making a choice between cellular, Wi-Fi etc.  Note that the configuration structure provided to this API by the underlying APIs may be limited to keep things simple, e.g. it may not be possible to chose the interface speed or to provide static data buffers etc.

# Leaving Things Out
You will notice that there are `_stub.c` files in the [src](src) directory; if you are only interested in, say, cellular, and want to leave out Wi-Fi/BLE/GNSS functionality, you can simply replace, for instance, [u_network_private_wifi.c](src/u_network_private_wifi.c) with [u_network_private_wifi_stub.c](src/u_network_private_wifi_stub.c), etc. in your build metadata and your linker should then drop the unwanted things from your build.  You will need to do the same for the GNSS and short-range (i.e. Wi-Fi and BLE) components in [common/device/src](/common/device/src).

# Usage
The directories include the API and the C source files necessary to call into the underlying [cell](/cell), [wifi](/wifi), [ble](/ble) and [gnss](/gnss) APIs.  The [test](test) directory contains a small number of generic tests for the [common/network](/common/network) API; for comprehensive tests of networking please refer to the test directory of the underlying APIs.

A simple usage example, sending data over a TCP socket, is shown below.  Note that, before calling `app_start()` the platform must be initialised (clocks started, heap available, RTOS running), in other words `app_task()` can be thought of as a task entry point.  If you open the `u_main.c` file in the `app` directory of your platform you will see how we do this, with `main()` calling a porting API `uPortPlatformStart()` to sort that all out; you could paste the example code into `app_start()` there (and add the inclusions) as a quick and dirty test (`runner` will build it).

```c
#include "ubxlib.h"
#include "string.h"
#include "u_cfg_app_platform_specific.h"

// Cellular configuration.
// Set U_CFG_TEST_CELL_MODULE_TYPE to your module type,
// chosen from the values in cell/api/u_cell_module_type.h
//
// Note that the pin numbers are those of the MCU: if you
// are using an MCU inside a u-blox module the IO pin numbering
// for the module is likely different that from the MCU: check
// the data sheet for the module to determine the mapping.

// DEVICE i.e. module/chip configuration: in this case a cellular
// module connected via UART
static const uDeviceCfg_t gDeviceCfg = {
    .deviceType = U_DEVICE_TYPE_CELL,
    .deviceCfg = {
        .cfgCell = {
            .moduleType = U_CFG_TEST_CELL_MODULE_TYPE,
            .pSimPinCode = NULL, /* SIM pin */
            .pinEnablePower = U_CFG_APP_PIN_CELL_ENABLE_POWER,
            .pinPwrOn = U_CFG_APP_PIN_CELL_PWR_ON,
            .pinVInt = U_CFG_APP_PIN_CELL_VINT,
            .pinDtrPowerSaving = U_CFG_APP_PIN_CELL_DTR
        },
    },
    .transportType = U_DEVICE_TRANSPORT_TYPE_UART,
    .transportCfg = {
        .cfgUart = {
            .uart = U_CFG_APP_CELL_UART,
            .baudRate = U_CELL_UART_BAUD_RATE,
            .pinTxd = U_CFG_APP_PIN_CELL_TXD,
            .pinRxd = U_CFG_APP_PIN_CELL_RXD,
            .pinCts = U_CFG_APP_PIN_CELL_CTS,
            .pinRts = U_CFG_APP_PIN_CELL_RTS
        },
    },
};
// NETWORK configuration for cellular
static const uNetworkCfgCell_t gNetworkCfg = {
    .type = U_NETWORK_TYPE_CELL,
    .pApn = NULL, /* APN: NULL to accept default.  If using a Thingstream SIM enter "tsiot" here */
    .timeoutSeconds = 240 /* Connection timeout in seconds */
};

#define MY_SERVER_NAME "something.com"
#define MY_SERVER_PORT 42

// The entry point: before this is called the system
// clocks must have been started and the RTOS must be running;
// we are in task space.
int app_start() {
    uDeviceHandle_t devHandle;
    int32_t sock;
    uSockAddress_t remoteAddress;
    int32_t x;
    size_t size;
    size_t sentSize = 0;
    const char *pMessage = "The quick brown fox jumps over the lazy dog.";

    // Initialise the APIs we will need
    uPortInit();
    uDeviceInit();

    // Open the device
    x = uDeviceOpen(&gDeviceCfg, &devHandle);
    uPortLog("## Opened device with return code %d.\n", x);

    // Bring up the network layer
    if (uNetworkInterfaceUp(devHandle, U_NETWORK_TYPE_CELL,
                            &gNetworkCfg) == 0) {

        // Do things using the network, for
        // example send data over a TCP socket
        // to a server as follows

        // Get the server's IP address using
        // the network's DNS resolution facility
        uSockGetHostByName(devHandle, MY_SERVER_NAME,
                           &(remoteAddress.ipAddress));
        remoteAddress.port = MY_SERVER_PORT;

        // Create the socket using the network
        sock = uSockCreate(devHandle,
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
        uNetworkInterfaceDown(devHandle, U_NETWORK_TYPE_CELL);
    }

    // Close the device
    uDeviceClose(devHandle, true);

    // Tidy up
    uDeviceDeinit();
    uPortDeinit();

    while(1);
}
```