# Introduction
This directory contains the Wi-Fi APIs for control and data.

The Wi-Fi APIs are split into the following groups:

- `<no group>`: init/deinit of the wifi API and adding a wifi instance.
- `cfg`: configuration of the wifi module.
- `net`: connection to a wifi network.
- `sock`: sockets, for exchanging data (but see the [common/sock](/common/sock) component for the best way to do this).

The module types supported by this implementation are listed in [u_wifi_module_type.h](api/u_wifi_module_type.h).


HOWEVER this is the detailed API; if all you would like to do is bring up a Wi-Fi bearer as simply as possible and then get on with exchanging data, please consider using the [common/network](/common/network) API, along with the [common/sock](/common/sock) API.  You may still dip down into this API from the network level as the handles used at the network level are the ones generated here.

NOTES:
* Secure and server sockets not yet supported for Wi-Fi
* UDP sockets has some limitations that are documented in [u_wifi_sock.h](api/u_wifi_sock.h)

# Usage
The [api](api) directory contains the files that define the Wi-Fi APIs, each API function documented in its header file.  In the [src](src) directory you will find the implementation of the APIs and in the [test](test) directory the tests for the APIs that can be run on any platform.
