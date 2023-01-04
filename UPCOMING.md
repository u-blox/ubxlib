# Upcoming Changes In Release 1.2, End March 2023
In the 1.2 release of `ubxlib`, due end March 2023, we plan to make a number of improvements; these are described below.

As always, we welcome your feedback on the road-map of features/improvements to `ubxlib`: simply post something in the issues list of this repo and we will respond.

## nRFConnect 2.1/Zephyr 3
The current `ubxlib` code _already_ supports nRFConnect 2.1/Zephyr 3, as well as nRFConnect 1.6/Zephyr 2, but the `.overlay` and `.dts` files we provide are _only_ for nRFConnect 1.6/Zephyr 2, which means that we only _test_ nRFConnect 1.6/Zephyr 2.  From `ubxlib` release 1.2 the `overlay` and `.dts` will be converted to nRFConnect 2.1/Zephyr 3 format and hence we will test nRFConnect 2.1/Zephyr 3; support for nRFConnect 1.6/Zephyr 2 will be retained in the code but will no longer be tested.

## PlatformIO
We will add support for building `ubxlib` through PlatformIO.  This will initially be for the Zephyr, ESP-IDF and Arduino\[ESP-IDF only\] platforms.  Since Zephyr provides support for a wide variety of chipsets this means that `ubxlib` can also be used on a wider variety of MCUs (e.g. TODO LIST).  Note that we will initially only regression test Zephyr on Nordic nRF52/53 and Linux but we intend to add testing (of the platform [port](/port/platform/zephyr/src) layer code only) on other MCUs and will make the list of tested MCUs available at that time.

## Bluetooth Extended Support
Generic GAP/GATT support will be added where it will be possible to control the module to assume the Central/Client and Peripheral/Server roles. Multiple simultanenous connections will be supported. The number of connections is dependent on the module. This will not affect the current SPS (cable-replacement) feature already supported in `ubxlib`.

## Cellular CMUX Support
We will add support for running multiple UARTs to a cellular module using the 3GPP 27.010 defined CMUX protocol.  In particular, this will allow support of a UART connection to a GNSS chip that is inside (e.g. SARA-R510M8S) or connected via the cellular module, as opposed to the current AT-command based mechanism.

## GNSS Streamed Position
The current GNSS implementation is "polled", i.e. the MCU always requests position from the GNSS chip.  With the addition of CMUX support, a "streamable" connection is now available to GNSS in all cases and hence we will provide a location API which makes position available to the MCU at a periodicity of your choice.

## HTTP Client
We will add support for HTTP client operations (PUT, POST, GET, HEAD, DELETE).

## Leaving Stuff Out
In the current code you must build all of cellular, short-range and GNSS, irrespective of whether you are only interested in one or two of these.  From release 1.2 the [`UBXLIB_FEATURES`](https://github.com/u-blox/ubxlib_priv/tree/doc_upcoming_rmea/port#ubxlib_features) switch, already defined in [ubxlib.cmake](/port/ubxlib.cmake) and [ubxlib.mk](/port/ubxlib.mk), will operate such that you may chose one or more of `cell`, `short_range` and `gnss` in order to leave out code from the `cell`, `short_range`, `wifi`, `ble` and `gnss` directories that you do not need at compile time.

## Removal Of Deprecated GNSS Types
The deprecated types in `uGnssTransportType_t` (see [u_gnss_type.h](/gnss/api/u_gnss_type.h)) will be removed; see that file for the replacement types, which were introduced in commit [f107a](https://github.com/u-blox/ubxlib/commit/f107a1c63b5219414220eef1d02bc95458ade36d), 27 August 2022.

## Internal Change Only: Test System Re-Write
The test system that underlies `ubxlib` will be re-written to improve scaleability; if you believe there are areas that deserve better test coverage please let us know as it may now be possible to address the gaps.