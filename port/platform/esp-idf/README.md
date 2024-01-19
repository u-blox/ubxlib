# Introduction
These directories provide the implementation of the porting layer on the Espressif ESP-IDF platform plus the associated build and board configuration information.  Instructions on how to install the necessary tools and perform the build can be found in the MCU directories below.

- [app](app): contains the code that runs the application (both examples and unit tests) on the ESP-IDF platform.
- [src](src): contains the implementation of the porting layers for ESP-IDF.
- [mcu](mcu): contains the configuration and build files for the MCUs supported on the ESP-IDF platform.
- [u_cfg_os_platform_specific.h](u_cfg_os_platform_specific.h): task priorities and stack sizes for the platform, built into this code.
- [test](test): contains tests that use ESP-IDF application APIs to check out the integration of `ubxlib` into ESP-IDF, e.g. at PPP level beneath LWIP.

# PPP-Level Integration With Cellular
`ubxlib` depends on very few ESP-IDF components (see the directories beneath this one for how to perform a build) but note that, if you wish to include a PPP-level integration at the base of the ESP-IDF LWIP stack, allowing use of native ESP-IDF clients (e.g. MQTT) with a cellular connection, then you must define `U_CFG_PPP_ENABLE` when building `ubxlib` and you must switch on the following in your `sdkconfig` file:

- `CONFIG_LWIP_PPP_SUPPORT`
- `CONFIG_ESP_NETIF_TCPIP_LWIP`
- `CONFIG_LWIP_PPP_PAP_SUPPORT`
- if your network operator requires a user name and password along with the APN **AND** requires CHAP authentication, then also switch on `CONFIG_LWIP_PPP_CHAP_SUPPORT`

If you are minimising the components built into your main application then you should add the ESP-IDF component `esp_netif` to the list.  If you fail to do this your code will either fail to link because `_g_esp_netif_netstack_default_ppp` could not be found, or the initial PPP LCP negotiation phase with the module will fail.

Note that adding the above configuration items seems to increase task stack size requirements in general; beware!

You must also call `esp_netif_init()` and `esp_event_loop_create_default()` at start of day from your application code, otherwise ESP-IDF will not work correctly or will go "bang" somewhere in the IP stack at the point that a cellular connection is made.  You can find an example of how to do all this and make a sockets connection in [main_ppp_espidf.c](/example/sockets/main_ppp_espidf.c).