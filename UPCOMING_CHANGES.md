# **BREAKING API CHANGES COMING UP IN MAY 2022**

We plan to make some significant API improvements in the next major release, v1.0.0, due end May 2022, which will REQUIRE ACTION ON YOUR PART; this is advance notice of the planned changes, which are described below.

## `u_device` API
We will add an API, named `u_device`, which will encompass the common physical characteristics of a module, the non-network-related parts such as pin definitions, UART, etc.  This new `uDevice` API will return a handle that we enable us  \[later\] to store things that will enable us to exclude unwanted code, e.g. to build without including one or more of `cell`/`gnss` or `short_range`.

## Remove Module Initialization from Network API
For a combined BLE/Wi-Fi module the concept of adding a module using the Network API is problematic; you would need to call `uNetworkAdd()` twice, with both calls including a UART configuration for the same module. This is confusing since:
- if the application calls `uNetworkAdd()` initialising BLE on UART 1 at baud rate A and then calls `uNetworkAdd()` initialising Wi-Fi on UART 1 at baud rate B, which baud rate should be used for the module on UART 1?
- if the application initialises both Wi-Fi and BLE using `uNetworkAdd()` for one module the real initialisation should happen automatically on the first call to `uNetworkAdd(0` but then some magic is required to know when to call `uNetworkRemove()` during de-initialisation.

With the `u_device` API in place we will move the initialisation of the module there and use the device handle in the Network API, think something like:

- `uDeviceUartOpen(uartConfig, moduleType, ..., &pHandle)`
- `uNetworkAdd(pHandle)`
- `uNetworkUp(pHandle, networkConfig)`

Note that the network-related configuration configuration can now move to the `uNetworkUp()` function, where it makes more sense (e.g. so that an SSID or an APN can be changed just by taking the network down and up again).

## Move `u_wifi_net` API to `u_wifi`
With the module initialization in the `u_ble` and `u_wifi` APIs removed these files now only contain `init()` and `deinit()` functions.  A user might expect `u_wifi.h` to contain the actual Wi-Fi functions such as connecting to SSID, scanning etc. But instead these API functions are located in `u_wifi_net.h`.  Hence the `u_wifi_net` APIs will be moved to `u_wifi.h`.

## `ble_data` Renamed to `ble_sps`
We will rename the SPS API from `ble_data` to `ble_sps` to make its use more obvious.

## `uNetwork` Connection Behaviour
We will add a callback that permits the user to detect when the connection (`wifi`/`ble`/`cell`) is lost.  With this in place we _may_ choose to add a parameter to `uNetworkUp()` to determine whether it should block or not.

## Add Mechanism to Reconfigure Network Settings
There is currently no way of, for instance, changing the SSID of a Wi-Fi network without de-initialize the module using `uNetworkRemove()` and then calling `uNetworkAdd()` again.  We will add a mechanism that permits switching network settings without the user having to call the above functions, assuming this doesn't just come naturally with the move of the network configuration settings to `uNetworkUp()`.

** **IF YOU DO NOT LIKE THE ABOVE, OR WOULD LIKE FURTHER CLARIFICATION ON THE ABOVE, PLEASE RAISE AN ISSUE ON THIS REPO ASAP** **