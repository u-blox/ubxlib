# Upcoming Changes In Release 1.3, End July 2023
In the 1.3 release of `ubxlib`, due end July 2023, we would like to inform you of the future deprecation of two platforms, and we will also make a number of improvements; these changes are described below.

As always, we welcome your feedback on the road-map of features/improvements to `ubxlib`: simply post something in the issues list of this repo and we will respond.

## Arduino (the non-PlatformIO one) and nRF5SDK Future Deprecation Notice
We would like to inform you of the future deprecation of two platforms:

- [port/platform/arduino](/port/platform/arduino), i.e. the version of Arduino where we supply a Python script which copies the `ubxlib` files into a structure that Arduino understands,
- [port/platform/nrf5sdk](/port/platform/nrf5sdk), i.e. the [previous generation of Nordic SDK](https://www.nordicsemi.com/Products/Development-software/nrf5-sdk), which Nordic have put into maintenance mode.

We will continue to support Arduino through [port/platform/platformio](/port/platform/platformio); if you wish to continue to use `ubxlib` with Arduino, please consider moving to the rather excellent [PlatformIO IDE](https://platformio.org/).

We will continue to support nRF52/nRF53 through what is now the Nordic standard [nRF Connect SDK](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/index.html) (i.e. Zephyr 3).  If you use `ubxlib` with nRF52/nRF53 please consider moving to [nRF Connect](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/index.html).

Nothing will actually _happen_ at release 1.3: the deprecation clock will begin ticking.  At release 1.4 we will formally deprecate these two platforms (i.e. they will still be supported but you may get some additional annoying prints) and, at release 1.5, support for the platforms will be removed; you likely have until the end of 2023 before support for these platforms disappears.

## Wifi Access Point
Support will be provided for using Wifi as an access point during provisioning.  When connected-to by, for example, a mobile phone, the access point will scan for other Wifi access points, present a HTML page that allows the correct SSID to be selected and a password entered, before switching back to Wifi station mode in order to operate normally.

As part of this implementation the `uSockBind()`, `uSockListen()` and `uSockAccept()` APIs will be supported for short range (i.e. Wifi) modules.

## GNSS AssistNow
Support will be provided for AssistNow: online, offline and autonomous, enabling faster time to first fix and reducing GNSS power consumption.

## Wifi Positioning
Support will be added for positioning using the Google Maps, Skyhook or Here cloud services, which base their position on knowledge of local Wifi access points, a subscription to one or more of these services will be required.
This feature is supported only by u-blox Wifi/BLE modules NINA-W13 and NINA-W15 (with u-connectXpress version 5.0.0 or later).

## SLIPPED TO 1.4: Cellular LENA-R8 Module Type
Support  for the [LENA-R8](https://www.u-blox.com/en/product/lena-r8-series) cellular module will now be added in release **1.4**, not release 1.3.

## Cellular CloudLocate Compact
Support will be added for version 2 [CloudLocate](https://www.u-blox.com/en/product/cloudlocate), using the new compact RRLP message formats available in u-blox M10 GNSS modules and later, which give a significant improvement in transmission speed/cost.

## ESP-IDF Version 5
Support will be added for using `ubxlib` with ESP-IDF version 5 (5.0.1 to be exact); support for ESP-IDF version 4 will remain.

## Wifi HTTP Maximum Page Size
The maximum HTTP page size that can be retrieved over Wifi HTTP will be increased from 512 bytes to 2 kbytes.

## Cellular Timeouts
The timeouts for certain cellular operations will be increased in order to match the times recommended in the AT command manuals:

- `U_CELL_NET_UPSD_CONTEXT_ACTIVATION_TIME_SECONDS` (only relevant for SARA-U201) is changed from 30 to 180 seconds.
- `U_CELL_NET_SCAN_TIME_SECONDS`, the time to wait for `uCellNetScanGetFirst()` to return, is changed from 180 to 1580 seconds.
- `U_CELL_SOCK_CONNECT_TIMEOUT_SECONDS`, the timeout when making a sockets connection over cellular, is changed from 30 to 332 seconds.
- `U_CELL_SOCK_DNS_LOOKUP_TIME_SECONDS`, the timeout on returning the domain name for a given IP address over cellular, is changed from 60 to 332 seconds.
- `U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS`, the time to wait for a response from the MQTT server over cellular, is changed from 30 to 120 seconds.

## Test Change Only: `ubxlib.redirectme.net` -> `ubxlib.com`
The server used with the `ubxlib` tests and the test farm as a TCP/UDP/TLS/MQTT/MQTTSN/HTTP peer will move from `ubxlib.redirectme.net` to `ubxlib.com`.
