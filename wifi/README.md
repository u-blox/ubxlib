# Introduction
This directory contains the Wi-Fi APIs for control and data.

The Wi-Fi APIs are split into the following groups:

- `<no group>`: init/deinit of the wifi API and adding a wifi instance.
- `cfg`: configuration of the wifi module.
- `net`: connection to a wifi network.
- `sock`: sockets, for exchanging data (but see the [common/sock](/common/sock) component for the best way to do this).

The module types supported by this implementation are listed in [u_wifi_module_type.h](api/u_wifi_module_type.h).


HOWEVER for Wi-Fi connection and data transfer the recommendation is to use the [common/network](/common/network) API, along with the [common/sock](/common/sock) API.
The handle returned by `uNetworkAdd()` can still be used with the `wifi` API for configuration etc. Please see the [socket example](/example/sockets) for details.

NOTES:
* Secure and server sockets not yet supported for Wi-Fi
* UDP sockets has some limitations that are documented in [u_wifi_sock.h](api/u_wifi_sock.h)

# Usage
The [api](api) directory contains the files that define the Wi-Fi APIs, each API function documented in its header file.  In the [src](src) directory you will find the implementation of the APIs and in the [test](test) directory the tests for the APIs that can be run on any platform.
