# Introduction
These directories provide the implementation of the porting layer on the Espressif ESP-IDF platform plus the associated build and board configuration information.  Instructions on how to install the necessary tools and perform the build can be found in the MCU directories below.

- [app](app): contains the code that runs the application (both examples and unit tests) on the ESP-IDF platform.
- [src](src): contains the implementation of the porting layers for ESP-IDF.
- [mcu](mcu): contains the configuration and build files for the MCUs supported on the ESP-IDF platform.
- [u_cfg_os_platform_specific.h](u_cfg_os_platform_specific.h): task priorities and stack sizes for the platform, built into this code.
- [test](test): contains tests that use ESP-IDF application APIs to check out the integration of `ubxlib` into ESP-IDF, e.g. at PPP level beneath LWIP.

# PPP-Level Integration With Cellular
`ubxlib` depends on very few ESP-IDF components (see the directories beneath this one for how to perform a build) but note that, if you wish to include a PPP-level integration at the base of the ESP-IDF LWIP stack, allowing use of native ESP-IDF clients (e.g. MQTT) with a cellular connection, then you must define `U_CFG_PPP_ENABLE` when building `ubxlib`; you can do this by appending it to the `COMPILE_DEFINITIONS` ESP-IDF CMake variable in your top-level `CMakeLists.txt` file as follows:

```
add_compile_definitions(U_CFG_PPP_ENABLE)
```

...or by [including it in the U_FLAGS environment variable](mcu/esp32/README.md) which the `ubxlib` ESP-IDF component looks for.  Note that adding `U_CFG_PPP_ENABLE` to `target_compile_options()` is NOT sufficient since the target compile options are not applied to components, and `ubxlib` is brought in as an ESP-IDF component.

You can find an example of how to make a sockets connection using native Espressif function calls in [main_ppp_espidf.c](/example/sockets/main_ppp_espidf.c).

## SDK Config: `menuconfig`
To bring in the right ESP-IDF components and compilation flags, you can do that through `menuconfig` by going to `Component Config` and making sure that `ESP NETIF Adapter --> TCP/IP Stack Library (LwIP) --> LwIP` is ticked; also make sure that `LWIP --> Enable PAP support` and, if your network operator requires a user name and password along with the APN **AND** requires CHAP authentication, then also `LWIP --> Enable CHAP support`, are ticked.

## SDK Config: Manual Configuration
Alternatively, if you prefer to do this by hand, make sure that your `sdkconfig` file contains:
- `CONFIG_LWIP_PPP_SUPPORT`
- `CONFIG_ESP_NETIF_TCPIP_LWIP`
- `CONFIG_LWIP_PPP_PAP_SUPPORT`
- if your network operator requires a user name and password along with the APN **AND** requires CHAP authentication, then also switch on `CONFIG_LWIP_PPP_CHAP_SUPPORT`

...and, if you are minimising the components built into your main application then you should add the ESP-IDF component `esp_netif` to the list.  If you fail to do this your code will either fail to link because `_g_esp_netif_netstack_default_ppp` could not be found, or the initial PPP LCP negotiation phase with the module will fail.

## Run-time Configuration
You must also call `esp_netif_init()` followed by `esp_event_loop_create_default()`, once only, at start of day (i.e. before opening the `ubxlib` device) from your application code, otherwise ESP-IDF will not work correctly or will assert because of an invalid `mbox` somewhere in the IP stack at the point that a cellular connection is made.