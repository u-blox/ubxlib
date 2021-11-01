# Introduction
This directory contains the wifi APIs for control and data.

The wifi APIs are split into the following groups:

- `<no group>`: init/deinit of the wifi API and adding a wifi instance.
- `cfg`: configuration of the wifi module.
- `net`: connection to a wifi network.
- `sock`: sockets, for exchanging data (but see the [common/sock](/common/sock) component for the best way to do this).

The module types supported by this implementation are listed in [u_wifi_module_type.h](api/u_wifi_module_type.h).

**NOTES:**
* Secure and server sockets not yet supported for wifi
* Wifi UDP sockets has some limitations (also documented in [u_wifi_sock.h](api/u_wifi_sock.h)):
   - Each UDP socket can only be used for communicating with a *single* remote peer.
   - Before using `uWifiSockReceiveFrom()` either `uWifiSockSendTo()` or `uWifiSockConnect()` must have been called

# Usage
The [api](api) directory contains the files that define the wifi APIs, each API function documented in its header file.  In the [src](src) directory you will find the implementation of the APIs and in the [test](test) directory the tests for the APIs that can be run on any platform.

HOWEVER for wifi connection and data transfer the recommendation is to use the [common/network](/common/network) API, along with the [common/sock](/common/sock) API. The handle returned by `uNetworkAdd()` can still be used with the `wifi` API for configuration etc. Please see the [socket example](/example/sockets) for details.

## Example
Below is a simple example that will setup a wifi connection using the [common/network](/common/network) API:

```c
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"

#include "u_short_range_module_type.h"
#include "u_network.h"
#include "u_network_config_wifi.h"

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
    int32_t netHandle;

    static const uNetworkConfigurationWifi_t wifiConfig = {
        .type = U_NETWORK_TYPE_WIFI,
        .module = U_SHORT_RANGE_MODULE_TYPE_NINA_W15,
        .uart = 1,
        .pinTxd = -1,
        .pinRxd = -1,
        .pinCts = -1,
        .pinRts = -1,
        .pSsid = "MySSID",
        .authentication = 2 /* WPA/WPA2/WPA3 - see wifi/api/u_wifi_net.h */,
        .pPassPhrase = "MyPassphrase"
    };

    VERIFY(uPortInit() == 0, "uPortInit failed\n");
    VERIFY(uNetworkInit() == 0, "uNetworkInit failed\n");

    netHandle = uNetworkAdd(U_NETWORK_TYPE_WIFI, &wifiConfig);
    uPortLog("Bring up wifi\n");
    VERIFY(uNetworkUp(netHandle) == 0, "uNetworkUp failed\n");

    uPortLog("Wifi connected\n");
    // Just sleep for 10 sec
    uPortTaskBlock(10*1000);

    // Cleanup
    uNetworkDown(netHandle);
    uNetworkDeinit();
    uPortDeinit();

    while(true);
}
```