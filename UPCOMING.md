# Upcoming Changes In Release 1.4, End January 2024
In the 1.4 release of `ubxlib`, due end January 2024, we would like to inform you of the deprecation of two platforms and we will also make a number of improvements; these changes are described below.

As always, we welcome your feedback on the road-map of features/improvements to `ubxlib`: simply post something in the issues list of this repo and we will respond.

Note: this repo is constantly updated, hence the features below will likely arrive before the release 1.4 tag is laid down, may already be here in fact.

# Deprecation Of Arduino (non-PlatformIO Version) And nRF5SDK Platforms
The following platforms will be deprecated from release 1.4:

- [port/platform/arduino](/port/platform/arduino), i.e. the version of Arduino where we supply a Python script which copies the `ubxlib` files into a structure that Arduino understands,
- [port/platform/nrf5sdk](/port/platform/nrf5sdk), i.e. the previous generation of Nordic SDK, which Nordic have put into maintenance mode.

We will continue to support Arduino through [port/platform/platformio](/port/platform/platformio); if you wish to continue to use `ubxlib` with Arduino, please consider moving to the PlatformIO IDE.

We will continue to support nRF52/nRF53 through what is now the Nordic standard nRF Connect SDK (i.e. Zephyr 3). If you use `ubxlib` with nRF52/nRF53 please consider moving to nRF Connect.

Support for Arduino (non-PlatformIO Version) and nRF5SDK will be entirely removed at release 1.5, likely before the middle of 2024.

# Geofencing
Support will be provided for flexible geofencing, permitting multiple fences, each containing multiple circles and multiple polygons, implemented within `ubxlib` (so not using the geofencing of a GNSS module) in order that the fences can be applied to cellular (CellLocate position) and Wi-Fi (Google, Skyhook and Here) as well as GNSS.

# Cellular LENA-R8
Support will be provided for the GSM/LTE LENA-R8 cellular module series.

# Zephyr Peripheral Selection
In order to be able to use UART, SPI and I2C devices in the Zephyr device tree that do not follow the usual naming convention (i.e. `uartx`, `i2cx` and `spix`), support for an overlay file which aliases the required device to `ubxlib-yyyy` will be added (so for instance the alias `ubxlib-uart1 = &usart1` would mean that `ubxlib` UART 1 corresponds with the device tree entry `usart1`).

# Short-range NORA-W3 Support (Beta)
An initial release of support for the new version of uConnectExpress, as shipped with the NORA-W3 short-range module, will be provided.

# Cellular Helper Function To Set RF Bands
An easier-to-use interface for the setting of the RF bands employed by a cellular module will be provided.

# Cellular PPP-level Integration Of Transport Into ESP-IDF
Integration of cellular with the bottom of the IP stack of the given platform, at PPP level, will permit all of the client entities supplied with the platform (the IP stack, MQTT, HTTP, etc.) to be used with a cellular transport.  This integration will work at least with the ESP-IDF platform; Zephyr, Linux and Windows will come later.

# Module Type "Any"
Support will be provided for setting module type "ANY" within cellular, GNSS or short-range and the `ubxlib` code will do its best to figure out what module type that is attached; note that this will be "best effort": we do not test `ubxlib` with all module types so it is always possible for `ubxlib` to get this wrong, in which case the user can always specify the correct module type. 

# GNSS Auto-baud
Support will be provided for setting the desired GNSS baud rate to "auto", in which case `ubxlib` will try different baud rates until it finds one that the GNSS device responds correctly to.  Note that this will result in a start-up delay: it will always be quicker for the application to chose the correct baud rate at the outset. 