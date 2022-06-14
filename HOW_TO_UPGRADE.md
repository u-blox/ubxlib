# Dealing With The Changes In 1.0.0 [End June 2022]
In the 1.0.0 release of `ubxlib`, from end June 2022, we made a number of breaking changes.  If you were using `ubxlib` BEFORE end June 2022, here's how you upgrade.

## `ubxlib.h`
Not a breaking change of itself but you can now obtain all of the necessary `ubxlib` public header files simply by including the single header file [ubxlib.h](ubxlib.h); it may be best to make this change first so that any new headers required for what follows will already be brought in.

## `uDevice` API And Consequent Changes To `uNetwork` API
This is the most significant change; it affects EVERYTHING.

We have introduced a [uDevice API](/common/device/api/u_device.h) which represents the physical module: cellular, GNSS or short-range (i.e. BLE and Wi-Fi) and its physical characteristics (pins, UART, etc.).  You open a device and then you open a network (cellular, GNSS, BLE or Wi-Fi) on that device.  This allows a single short-range device to support a BLE and a Wi-Fi network at the same time, and it allows a cellular module to support a GNSS network and a cellular network at the same time (for the case where the GNSS module is inside or attached via a cellular module).

### ALL Handles Are Now Of Type `uDeviceHandle_t`, No Longer `int32_t`
ALL API calls now take a `uDeviceHandle_t` (still 32-bit, see [u_device.h](/common/device/api/u_device.h)) and NOT an `int32_t` handle.

If you use the functions `uCellAdd()` (see [u_cell.h](/cell/api/u_cell.h)) or `uGnssAdd()` (see [u_gnss.h](/gnss/api/u_gnss.h)): these functions take an additional parameter, a pointer to a place to put the `uDeviceHandle_t` and now ONLY RETURN AN ERROR CODE; it is the `uDeviceHandle_t` which you must use in ALL API calls.  For the equivalent changes in `uBle`/`uWifi`/`uShortRange`, see [below](#ubleuwifiushortrange-changes).

### Converting
See the [examples](/example), which are all updated to use the new APIs.

`uNetworkInit()`/`uNetworkDeinit()` (see [u_network.h](/common/network/api/u_network.h)) become `uDeviceInit()`/`uDeviceDeinit()` (see [u_device.h](/common/device/api/u_device.h)).

VERY IMPORTANT: `uDeviceDeinit()`, unlike `uNetworkDeinit()`, DOES NOT perform a clean-up; you must call `uDeviceClose()` for any opened devices in order to shut `ubxlib` down cleanly.

Your single network configuration structure `uNetworkConfigurationXxx_t` is now split between a `uDeviceCfg_t` structure (see [u_device.h](/common/device/api/u_device.h)) and a `uNetworkCfgXxx_t` structure (see `u_nework_config_xxx.h` in [common/network/api](/common/network/api)).  IMPORTANT: note that a `baudRate` field, which you MUST POPULATE, has been added in `uDeviceCfg_t` (115,200 being a good default for cellular and short-range, 9600 being a good default for GNSS) and that, for cellular, a `pinDtrPowerSaving` field, which you MUST ALSO POPULATE, has been added in the `uDeviceCfgCell_t` structure (-1 being a good default); if you were previously just setting the conditional compilation flag `U_CFG_APP_PIN_CELL_DTR` then you must make sure that the value `U_CFG_APP_PIN_CELL_DTR` is now applied to the `pinDtrPowerSaving` field of the `uDeviceCfgCell_t` structure instead.  Also note that, for cellular, `pPin` has been renamed to `pSimPinCode` for clarity.

`uNetworkAdd()`/`uNetworkRemove()` become `uDeviceOpen()`/`uDeviceClose()` (see [u_device.h](/common/device/api/u_device.h)).

`uNetworkUp()`/`uNetworkDown()` become `uNetworkInterfaceUp()`/`uNetworkInterfaceDown()` (see [u_network.h](/common/network/api/u_network.h)).

IMPORTANT: previously, for cellular/GNSS, `uNetworkDown()` also powered the device down; `uNetworkInterfaceDown()` does NOT DO THIS: to power the device down you must call `uDeviceClose()` with the second parameter set to `true`.

### If You Use GNSS Inside, Or Connected Via, A Cellular Module
See [main_loc_gnss_cell.c](/example/location/main_loc_gnss_cell.c) for an example.

Populate a `uDeviceCfg_t` (see [u_device.h](/common/device/api/u_device.h)) structure for the CELLULAR device and a `uNetworkCfgGnss_t` structure (see [u_network_config_gnss.h](/common/network/api/u_network_config_gnss.h)) for the GNSS bits, call `uDeviceOpen()` on the CELLULAR device and then `uNetworkInterfaceUp()` (see [u_network.h](/common/network/api/u_network.h)) for the GNSS network on that device; there is no need to copy handles around etc. as you did previously.  Once the GNSS network interface is up you may call any of the [uGnssXxx()](/gnss/api/) functions; just use the handle of the cellular device when doing so.

### If You Use The `uLocation` API
See the [main_loc_cell_locate.c](/example/location/main_loc_cell_locate.c) or [main_loc_gnss_cloud_locate.c](/example/location/main_loc_gnss_cloud_locate.c) examples.

`networkHandleAssist` is removed from the `uLocationAssist_t` structure (see [u_location.h](/common/location/api/u_location.h)); it is no longer needed, the required information is available internally within `ubxlib`.

## `uBle`/`uWifi`/`uShortRange` Changes
If you were using the functions `uBleAdd()`/`uBleRemove()`/`uBleDetectModule()`/`uBleAtClientHandleGet()` or `uWifiAdd()`/`uWifiRemove()`/`uWifiDetectModule()`/`uWifiAtClientHandleGet()` or `uShortRangeAdd()`/`uShortRangeRemove()`, these are now removed.  Please use `uDeviceOpen()` to obtain a device handle which you may use with the [uBle](/ble/api) and [uWifi](/wifi/api) APIs; close the device with `uDeviceClose()` when done.

Not also that all of the communication with the short-range module is now carried out using Extended Data Mode (EDM).

## Also If You Use The `uWifi` API
`u_wifi_net.h` is removed; with the changes above it had very little left in it.  The residual functionality is now in [u_wifi.h](/wifi/api/u_wifi.h) with a small amount of consequential renaming.  For this change it is probably simplest to try to compile your existing code and "fix until done".

## Also If You Use The `uBle` API
`u_ble_data.h` has been renamed to [u_ble_sps.h](/ble/api/u_ble_sps.h); again the simplest thing to do here is probably to try to compile your existing code and "fix until done".

**PLEASE FEEL FREE TO RAISE AN ISSUE ON THIS REPO IF YOU HAVE PROBLEMS/QUESTIONS CONCERNING THE UPGRADE**