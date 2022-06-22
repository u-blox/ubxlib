# Introduction
This directory contains the Wi-Fi APIs for control and data.

The Wi-Fi APIs are split into the following groups:

- `<no group>`: init/deinit of the Wi-Fi API and adding a Wi-Fi instance.
- `cfg`: configuration of the Wi-Fi module.
- `sock`: sockets, for exchanging data (but see the [common/sock](/common/sock) component for the best way to do this).
- `mqtt`: MQTT client over wifi network. Refer to [common/mqtt_client](/common/mqtt_client) component for generic
mqtt client implementation.

The module types supported by this implementation are listed in [u_wifi_module_type.h](api/u_wifi_module_type.h).

**NOTES:**
* Secure and server sockets not yet supported for Wi-Fi
* Wi-Fi UDP sockets has some limitations (also documented in [u_wifi_sock.h](api/u_wifi_sock.h)):
   - Each UDP socket can only be used for communicating with a *single* remote peer.
   - Before using `uWifiSockReceiveFrom()` either `uWifiSockSendTo()` or `uWifiSockConnect()` must have been called
* For using MQTT client over Wi-Fi connection the recommendation is to use the [common/mqtt_client](/common/mqtt_client) API

# Usage
The [api](api) directory contains the files that define the Wi-Fi APIs, each API function documented in its header file.  In the [src](src) directory you will find the implementation of the APIs and in the [test](test) directory the tests for the APIs that can be run on any platform.

HOWEVER for Wi-Fi connection and data transfer the recommendation is to use the [common/network](/common/network) API, along with the [common/sock](/common/sock) API. The handle returned by `uNetworkAdd()` can still be used with the `wifi` API for configuration etc. Please see the [socket example](/example/sockets) for details.

## Example
Below is a simple example that will setup a Wi-Fi connection using the [common/device](/common/device) and [common/network](/common/network) APIs:

```c
#include <ubxlib.h>

#define VERIFY(cond, fail_msg) \
    if (!(cond)) {\
        failed(fail_msg); \
    }

static void failed(const char *msg)
{
    uPortLog(msg);
    while(1);
}

int main(void)
{
    uDeviceHandle_t devHandle = NULL;

    static const uDeviceCfg_t gDeviceCfg = {
        .deviceType = U_DEVICE_TYPE_SHORT_RANGE,
        .deviceCfg = {
            .cfgSho = {
                .moduleType = U_SHORT_RANGE_MODULE_TYPE_NINA_W15
            }
        },
        .transportType = U_DEVICE_TRANSPORT_TYPE_UART,
        .transportCfg = {
            .cfgUart = {
                .uart = 1,
                .baudRate = 115200,
                .pinTxd = -1,
                .pinRxd = -1,
                .pinCts = -1,
                .pinRts = -1
            }
        }
    };

    static const uNetworkCfgWifi_t gNetworkCfg = {
        .type = U_NETWORK_TYPE_WIFI,
        .pSsid = "MySSID",
        .authentication = 2 /* WPA/WPA2/WPA3 - see wifi/api/u_wifi.h */,
        .pPassPhrase = "MyPassphrase"
    };

    VERIFY(uPortInit() == 0, "uPortInit failed\n");
    VERIFY(uDeviceInit() == 0, "uDeviceInit failed\n");

    VERIFY(uDeviceOpen(&gDeviceCfg, &devHandle) == 0, "uDeviceOpen failed\n");
    uPortLog("Bring up Wi-Fi\n");
    VERIFY(uNetworkInterfaceUp(devHandle, U_NETWORK_TYPE_WIFI, &gNetworkCfg) == 0, "uNetworkInterfaceUp failed\n");

    uPortLog("Wi-Fi connected\n");
    // Just sleep for 10 sec
    uPortTaskBlock(10*1000);

    // Cleanup
    uNetworkInterfaceDown(devHandle, U_NETWORK_TYPE_WIFI);
    uDeviceClose(devHandle, true);
    uDeviceDeinit();
    uPortDeinit();

    while(true);
}
```