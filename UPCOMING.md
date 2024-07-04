# Upcoming Changes In Release 1.5, End July 2024 (Updated)
In the 1.5 release of `ubxlib`, due end July 2024, we would like to inform you of the removal of two platforms and we will also make a number of improvements; these changes are described below.

Note that the following items were _originally_ planned to form part of release 1.5 but have now been moved to a later release:

- New platform: IRIS-W1 (NXP MCUXpresso): the NXP MCU in question, MCXN947, will be supported by Zephyr release 3.7.0 and hence we will provide support for this MCU through Zephyr once Zephyr have released 3.7.0 (expected end July) and we have successfully been able to test our integration with that Zephyr release.
- Wi-Fi calibration data: support will be provided for downloading Wi-Fi calibration data to short-range modules in `ubxlib` release 1.6.
- Firmware update for NORA-W36, u-connectExpress 2nd generation: support will be provided for downloading u-connectExpress FW to the NORA-W36 module in a later release of `ubxlib`.

As always, we welcome your feedback on the road-map of features/improvements to `ubxlib`: simply post something in the issues list of this repo and we will respond.

Note: this repo is constantly updated, hence the features below will likely arrive before the release 1.5 tag is laid down.

# Removal Of Arduino (non-PlatformIO Version) And nRF5SDK Platforms
The following platforms will be removed from this repo in release 1.5:

- [port/platform/arduino](/port/platform/arduino), i.e. the version of Arduino where we supply a Python script which copies the `ubxlib` files into a structure that Arduino understands,
- [port/platform/nrf5sdk](/port/platform/nrf5sdk), i.e. the previous generation of Nordic SDK, which Nordic have put into maintenance mode.

We continue to support Arduino through [port/platform/platformio](/port/platform/platformio); if you wish to continue to use `ubxlib` with Arduino, please move to the PlatformIO IDE.

We continue to support nRF52/nRF53 through what is now the Nordic standard nRF Connect SDK (i.e. Zephyr 3). If you use `ubxlib` with nRF52/nRF53 please move to nRF Connect.

# Chunked HTTP Client API
The HTTP Client API will be extended to permit the MCU to receive an HTTP file in chunks rather than all at once.

# BLE Pairing And Bonding
Support will be provided for pairing and bonding BLE-capable short-range modules.

# Testing Non-Nordic Zephyr Platforms
Zephyr platforms other than Nordic nRF52/nRF53 will be added inside the `ubxlib` test system, e.g. STM32.

# New Cellular Modules LEXI-R10, LEXI-R422, LEXI-R520, SARA-R520
Support will be included for the new cellular LEXI-R series modules and SARA-R520 module.