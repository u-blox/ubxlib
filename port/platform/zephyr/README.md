# Introduction
These directories provide the implementation of the porting layer on the Zephyr platform.  Instructions on how to install the necessary tools and perform the build can be found in the [runner](runner) directory for Nordic platforms, the [runner_stm32](runner_stm32) directory for STM32 platforms and the [runner_linux](runner_linux) directory for Linux/Posix.  While this should, generically, work with any Zephyr board, you should note that we only test with the following HW:

- Nordic MCUs, which require a **specific** version/configuration of Zephyr,
- STM32 MCUs,
- Linux/posix, for debugging/development only, just like [windows](../windows).

Note: the directory structure here differs from that in the other platform directories in order to follow more closely the approach adopted by Zephyr, which is hopefully familiar to Zephyr users.

- [app](app): contains the code that runs the test application (both examples and unit tests) on the Zephyr platform.
- [cfg](cfg): contains the configuration files for the MCU, the OS, for the application and for testing (mostly which MCU pins are connected to which module pins).
- [dts](dts): device tree bindings needed by this code; for instance, the PPP UART driver, required if you want to integrate at PPP-level with cellular, see below.
- [src](src): contains the implementation of the porting layers for Zephyr platform on Nordic chips and on Linux/posix.
- [runner](runner): contains the test application configuration and build files for the Nordic MCUs supported on the Zephyr platform.
- [runner_linux](runner_linux): contains the test application configuration and build files for Linux/Posix on the Zephyr platform.
- [runner_stm32](runner_stm32): contains the test application configuration and build files for STM32 MCUs supported on the Zephyr platform.
- [boards](boards): contains custom u-blox boards that are not \[yet\] in the Zephyr repo.
- [test](test): contains tests that use Zephyr application APIs to check out the integration of `ubxlib` into Zephyr, e.g. at PPP level.

# SDK Installation For Nordic MCUs (NRF Connect)
`ubxlib` is tested with the version of Zephyr that comes with `nRFConnect SDK version 2.5.0` (Zephyr 3.4.99ncs, which includes modifications that Nordic make to Zephyr) which is the recommended version; it is intended to build with all versions nRFConnect SDK from 1.6.1 up til 2.5.0.

Follow the instructions to install the development tools:

- Install nRF connect from https://www.nordicsemi.com/Software-and-Tools/Development-Tools/nRF-Connect-for-desktop.
- Start nRFConnect and use the tool chain manager to install the recommended SDK version (see above).
- IMPORTANT: update SDK and toolchain using the dropdown menu for your SDK version.

# SDK Installation For Other MCUs (Native Zephyr)
Please follow the [Zephyr](https://docs.zephyrproject.org/latest/develop/getting_started/index.html) instructions to install Zephyr and the `west` tool that configures and builds Zephyr.

`ubxlib` is tested with native Zephyr version 3.6.0.

If you intend to use Zephyr on Linux/posix then you must also follow the instructions here: https://docs.zephyrproject.org/latest/boards/posix/native_posix/doc/index.html.

# Integration
`ubxlib` is a [Zephyr module](https://docs.zephyrproject.org/latest/guides/modules.html).

To add `ubxlib` to your Zephyr application you can either add it to your `west.yml`, or make use of [ZEPHYR_EXTRA_MODULES](https://docs.zephyrproject.org/latest/guides/modules.html#integrate-modules-in-zephyr-build-system).

To add it to your `west.yml`, add the u-blox remote to your `remotes` section and the `ubxlib` module to the `projects` section:
```yml
  remotes:
    - name: u-blox
      url-base: https://github.com/u-blox

  projects:
    - name: ubxlib
      remote: u-blox
      path: ubxlib
      import: true
      clone-depth: 1
```

You must then also enable `UBXLIB` either via [menuconfig](https://docs.zephyrproject.org/latest/guides/build/kconfig/menuconfig.html#menuconfig) or by adding the following line to your `prj.conf`:

```
CONFIG_UBXLIB=y
```

`ubxlib` also requires some Zephyr config to be enabled: check [default.conf](default.conf) for the basics or, for details to do with BLE configuration or including I2C/SPI etc., check [runner/prj.conf](runner/prj.conf)/[runner_linux/prj.conf](runner_linux/prj.conf).

If you wish to build the default `runner` build and associated tests _without_ nRF Connect SDK then you will need to bring in a copy of Unity from somewhere, identifying the Unity directory by defining the environment variable `UNITY_PATH`, e.g. `UNITY_PATH=/home/ubxlib/unity`.

# SDK Usage

## Nordic MCUs: Segger Embedded Studio
When you install nRFConnect SDK for Windows you will get a copy of Segger Embedded Studio (SES).

You can either start SES from the nRF Toolchain Manager by clicking "Open Segger Embedded Studio" or by running `toolchain/SEGGER Embedded Studio.cmd` found in your installation of nRFconnect SDK.

- Always load project from SES using file->Open nRF connect SDK project
- Select the project folder containing the `CMakeLists.txt` of the application you want to build.
- Board file should be `{your_sdk_path}/zephyr/boards/arm/nrf5340dk_nrf5340` for EVK-NORA-B1.
- Board name should be `nrf5340dk_nrf5340_cpuapp` for EVK-NORA-B1.  For a custom board e.g. `ubx_evkninab4_nrf52833`.
- Where a board-specific configuration file is available (e.g. `ubx_evkninab4_nrf52833.conf`) this will be picked up automatically.

## Linux/Posix
With this code fetched to your Linux machine all you have to do is set the correct UART number for any u-blox module which is attached to that machine.  When the executable starts running it will print something like:

```
UART_1 connected to pseudotty: /dev/pts/5
UART_0 connected to pseudotty: /dev/pts/3
```

This indicates that two UARTs, 0 and 1 (the maximum for Linux/Posix builds) are available and that they emerge on the corresponding pseudo-terminals; these pseudo-terminals are assigned by the operating system and may change on each run.  You can use the Linux utility `socat` to redirect the pseudo-terminals to real devices.  For instance:

```
socat /dev/pts/5,echo=0,raw /dev/tty/0,echo=0,raw
```

...would redirect `/dev/pts/5` to `/dev/tty/0` so, using the example above, that means UART 1 will be effectively on `/dev/tty/0`, or:

```
socat /dev/pts/3,echo=0,raw /dev/pts/3,echo=0,raw
```

...would loop `/dev/pts/3` (in the example above UART 0) back on itself.

## Additional Notes
- Always clean the build directory when upgrading to a new `ubxlib` version.
- You may override or provide conditional compilation flags to CMake without modifying `CMakeLists.txt`.  Do this by setting an environment variable `U_FLAGS`, e.g.:

  ```
  set U_FLAGS=-DMY_FLAG
  ```

  ...or:

  ```
  set U_FLAGS=-DMY_FLAG -DU_CFG_APP_PIN_CELL_ENABLE_POWER=-1
  ```

# Device Tree
Zephyr pin choices for any HW peripheral managed by Zephyr (e.g. UART, I2C, SPI, etc.) are made at compile-time in the Zephyr device tree, they cannot be passed into the functions as run-time variables.  Look in the `zephyr/zephyr.dts` file located in your build directory to find the resulting pin allocations for these peripherals.

If you want to find out more about device tree please see Zephyr "[Introduction to Device Tree](https://docs.zephyrproject.org/latest/guides/dts/intro.html)".

You will still need to pass into `ubxlib` the HW block that is used: e.g. UART 0, UART 1, etc.  The UARTs, for instance, will be named `uart0`, `uart1`... in the device tree; the ending number is the value you should use to tell `ubxlib` what device to open.

# UART/I2C/SPI Device Tree Names If You Are Not Using nRF52/nRF53
In order to obtain the device configuration for UART, I2C and SPI from the device tree, `ubxlib` assumes that a UART device is named `uart` (e.g. `uart2`), an I2C device `i2c` (e.g. `i2c1`) and an SPI device `spi` (e.g. `spi0`); it is the number on the end of that name that must be passed into the `uDeviceCfgUart_t`/`uDeviceCfgI2c_t`/`uDeviceCfgSpi_t` structure that you pass to [uDeviceOpen()](/common/device/api/u_device.h), the HW block that is used.

If you are not using nRF52/nRF53 then it is possible that the entries in the device tree are called something else, e.g. `usart` for STM32, maybe `serial`, whatever (the same is true for I2C and SPI but everyone seems to have chosen `i2c` and `spi` there).

In that case you must add a device tree `.overlay` file to your build that defines a `ubxlib` alias for the device in order to get the correct mapping. `ubxlib` will always check if there is an alias, named `ubxlib-xxxxn`, and use it when present.  For example:

```
    / {
      aliases {
        ubxlib-uart1 = &usart1;
      };
    };
```

# I2C Troubles
The I2C interface of Zephyr allows the underlying driver quite a lot of flexibility: if you experience problems using GNSS over I2C you should probably start by attaching a monitoring device (e.g. a [Saleae](https://www.saleae.com/) probe) to the I2C lines to see what is going on.  For instance, the STM32 I2C drivers have a length limitation somewhere, which means that you will not be able to successfully receive the larger messages (e.g. large configuration messages, RRLP messages) that the GNSS device sends unless you set `maxSegmentSize` in the `uDeviceCfgI2c_t` structure (see [u_device.h](/common/device/api/u_device.h)), or set `i2c-max-segment-size` in the GNSS device entry in your `.overlay` file (see below for how to do that), to something like 255.

# Specifying Device And Network Configuration Through The Device Tree
For those familiar with the Zephyr device tree, it is possible to ignore the device and network configuration structures in the C code and instead set their entire contents through the device tree, binding files for which can be found in [dts/bindings](dts/bindings).

IMPORTANT: this _only_ works if you initialise the relevant `uPort` APIs and then bring up a device by calling `uDeviceOpen()` (see the [examples](/example)); if you initialise the relevant `uPort` APIs and then bring up a device the hard way, by separately calling `uPortXxxOpen()` for the UART/I2C/SPI transport, then calling `uCellAdd()`/`uGnssAdd()`/`uShortRangeAdd()` etc. it will NOT work.

You will need an entry with `status = "okay"` for the device you intend to use ([cellular](dts/bindings/u-blox,ubxlib-device-cellular.yaml), [GNSS](dts/bindings/u-blox,ubxlib-device-gnss.yaml) or [short range](dts/bindings/u-blox,ubxlib-device-short-range.yaml)) and each device _may_ refer to a [BLE](dts/bindings/u-blox,ubxlib-network-ble.yaml), [cellular](dts/bindings/u-blox,ubxlib-network-cellular.yaml), [GNSS](dts/bindings/u-blox,ubxlib-network-gnss.yaml) or [Wi-Fi](dts/bindings/u-blox,ubxlib-network-wifi.yaml) network (e.g. to specify an SSID or an APN or use GNSS inside a cellular device).  Here is an example:

```
/ { # This puts us in the root node

    # A cellular device connected to UART0; the name can be whatever you like
    cfg-device-cellular {
        compatible = "u-blox,ubxlib-device-cellular";
        # Each ubxlib-xxx-yyy entry needs to have status = "okay"
        status = "okay";
        # The transport-type looks like the node-label you may use for
        # a UART etc. but for the "transport-type" property of a ubxlib
        # device it must be given as a STRING
        transport-type = "uart0";
        module-type = "U_CELL_MODULE_TYPE_SARA_R422";
        # The ubxlib pin number is simply an integer counting up from zero,
        # so you will need to know the number of pins per GPIO port on your
        # MCU to work it out.  For nRF52/nRF53 there are 32 pins per GPIO
        # port, so the pin for <&gpio1 0 x> would be pin 32, while for
        # STM32 there are 16 pins per GPIO port which is easiest to comprehend
        # when expressed in hex, so <&gpiob 0 x> (the STM32 GPIO device tree
        # entries use a letter rather than a number), pin PB0, would be 0x10,
        # <&gpiof 14 x>, pin PF14, would be 0x5e, etc.
        pin-pwr-on = <10>;
        pin-vint = <35>;
        # This SARA-R422 device has GNSS inside it, which is indicated by
        # the second network configuration below; the first network
        # configuration contains the APN for cellular
        # If the cellular module does not have GNSS inside it and
        # your service provider does not require you to provide an
        # APN then you do not need to provide a network property
        # at all
        network = <&label_cfg_network_cellular_thingstream &label_cfg_network_gnss_inside>;
    };

    # A GNSS device connected to SPI2
    cfg-device-gnss {
        compatible = "u-blox,ubxlib-device-gnss";
        status = "okay";
        transport-type = "spi2";
        spi-index-select = <0>;
        module-type = "U_GNSS_MODULE_TYPE_M9";
        # No network configuration is required for a standalone GNSS device
    };

    # A short-range device connected on UART1
    cfg-device-short-range {
        compatible = "u-blox,ubxlib-device-short-range";
        status = "okay";
        transport-type = "uart1";
        module-type = "U_SHORT_RANGE_MODULE_TYPE_NINA_W15";
        network = <&label_cfg_network_wifi_client_home &label_cfg_network_ble_peripheral>;
   };

    # A cellular network configuration; label required so that we can
    # reference it.  The label and name can be whatever you like.
    label_cfg_network_cellular_1nce: cfg-network-cellular-1nce {
        compatible = "u-blox,ubxlib-network-cellular";
        status = "okay";
        apn = "iot.1nce.net";
    };

    # Another cellular network configuration
    label_cfg_network_cellular_thingstream: cfg-network-cellular-thingstream {
        compatible = "u-blox,ubxlib-network-cellular";
        status = "okay";
        apn = "tsiot";
    };

    # A GNSS network configuration for the GNSS inside the SARA-R422 device
    label_cfg_network_gnss_inside: cfg-network-gnss-inside {
        compatible = "u-blox,ubxlib-network-gnss";
        status = "okay";
        module-type = "U_GNSS_MODULE_TYPE_M10";
    };

    # A Wi-Fi network configuration
    label_cfg_network_wifi_client_home: cfg-network-wifi-client-home {
        compatible = "u-blox,ubxlib-network-wifi";
        status = "okay";
        ssid = "my_home_ssid";
        authentication = 2;
        pass-phrase = "my_pass_phrase";
    };

    # A BLE network configuration
    label_cfg_network_ble_peripheral: cfg-network-ble-peripheral {
        compatible = "u-blox,ubxlib-network-ble";
        status = "okay";
        role = "U_BLE_CFG_ROLE_PERIPHERAL";
    };
};

&uart0 {
    compatible = "nordic,nrf-uarte";
    status = "okay";
}
&spi2 {
    compatible = "nordic,nrf-spim";
    status = "okay";
    cs-gpios = <&gpio1 15 GPIO_ACTIVE_LOW>;
    pinctrl-0 = <&spi2_default>;
    pinctrl-1 = <&spi2_sleep>;
    pinctrl-names = "default", "sleep";
}
&uart1 {
    compatible = "nordic,nrf-uarte";
    status = "okay";
}

...
```

Notes:
- The form of each device tree property name matches that of the corresponding member of the [uDeviceCfg_t](/common/device/api/u_device.h)/[uNetworkCfgBle_t](/common/network/api/u_network_config_ble.h)/[uNetworkCfgCell_t](/common/network/api/u_network_config_cell.h)/[uNetworkCfgGnss_t](/common/network/api/u_network_config_gnss.h)/[uNetworkCfgWifi_t](/common/network/api/u_network_config_wifi.h) structure, e.g. `transportType` becomes `transport-type`; if you have `ubxlib` debug prints switched on then the values adopted for each property name will be printed-out when a device is opened or a network interface is brought up.
- Where a structure member is to do with a transport, e.g. `baudRate` for UART or `alreadyOpen` for I2C or `frequencyHertz` for SPI, then the corresponding property name is prefixed with `uart`, `i2c` or `spi`, i.e. `uart-baud-rate`, `i2c-already-open` and `spi-frequency-hertz`.
- If you include just a single `ubxlib` cellular, GNSS or short-range device in your device tree then you need populate nothing in the `uDeviceCfg_t` structure that you pass to `uDeviceOpen()` (or `NULL` can be passed) and what you have specified in the device tree will be used.
- If you have more than one `ubxlib` device entry in your device tree but you still have only one of each type, e.g. you have a cellular device connected via UART and a GNSS device connected via SPI, you only need to populate the `uDeviceType_t` member of the `uDeviceCfg_t` structures that you pass to `uDeviceOpen()` (e.g. `U_DEVICE_TYPE_CELL` and `U_DEVICE_TYPE_GNSS`) and the relevant entry from your device tree will be used.
- If you have more than one `ubxlib` cellular, GNSS or short-range entry in your device tree then you will also need to populate the `pCfgName` member of the `uDeviceCfg_t` structures that you pass to `uDeviceOpen()` with, e.g. for the example above `cfg-device-cellular`, to match the name of the node in the device tree you want to use.
- Each `ubxlib-xxx-yyy` node needs a `status = "okay"` property in it in order to be used; this is a Zephyr device tree convention.
- For a chosen transport the `ubxlib` setting ALWAYS OVERRIDES any in the `&` node, whether explicitly set in the `ubxlib-device-xxx` entry or not; for instance, if in node `&uart0` you set `current-speed = <230400>` and you pick up that UART in a `ubxlib-device-cellular` node (`transport-type = "uart0"`) WITHOUT including the property `uart-baud-rate`, the baud rate you end up with will be the `ubxlib` default for cellular of 115200, NOT 230400.
- The `network` property in the `ubxlib` DT binding is of type `phandles`,  that is, more than one; the device tree syntax for this is to put ALL of the referenced node-labels TOGETHER inside a SINGLE pair of angle brackets `< >`, put an `&` before each and separate them ONLY WITH WHITESPACE, no commas.
- Device tree node-labels, i.e. the bit before the `:`, can contain `-` but a _reference_ to them cannot, hence you must use underscores there, unlike the convention for the rest of the syntax where `-` is normal.
- Device tree node-labels are required so that `phandle`-type properties (i.e. the `network` property in this case) can refer to those nodes; if there is no need to refer to a node then the label can be omitted.

FYI, for transports and GPIOs, what `ubxlib` is trying to get is the HW block number, for example the `0` on the end of `&uart0`; however, there is no way (see [discussion](https://github.com/zephyrproject-rtos/zephyr/issues/67046)), from within C code, to get that `0`, or even `uart0`, those references are all resolved inside the Zephyr device tree parser before any C code is compiled.  This is why the `transport-type` and `pin-xxx` labels, which would naturally just be `phandle` references, e.g. `<&uart0>` and `<gpio1 3 0>`, have to instead be strings and integers.  Should Zephyr provide a mechanism to obtain this information in future then we will adopt it and the properties will become conventional.

# PPP-Level Integration With Cellular
Before version 3.5.0, PPP support in Zephyr was marked as "experimental" and, even now, despite the presence of a PAP configuration item, there is no way to configure the authentication mode or the username/password (which are hard-coded in `zephyr/subsys/net/l2/ppp/pap.c`, see function `pap_config_info_add()`) and there doesn't appear to be any code to support CHAP authentication; hence, if your operator requires a user name and password along with the APN, you have no choice but to edit the Zephyr source code.

Also, the only integration mechanism provided at the bottom of Zephyr PPP is to a \[single\] UART (`zephyr,ppp-uart`); hence the port layer here makes the PPP stream of the \[cellular\] module appear as a UART which must be pulled-in by the application's `.dts` or a `.overlay` file to complete the connection (see below for how to do this).

And BE WARNED, because of the way the integration has to work, the transmit side of the link is relatively slow, hence the TCP window size has to be relatively small.

With all of that said, it otherwise appears to work, so if you wish to use native Zephyr applications (e.g. MQTT) with a cellular connection, you will need to (a) define `U_CFG_PPP_ENABLE` when building `ubxlib`, (b) put the settings below in your `prj.conf` file and (c) add a `zephyr,ppp-uart` entry to your `.dts` or `.overlay` file (see further down):

- `CONFIG_NETWORKING=y`
- `CONFIG_NET_DRIVERS=y`
- `CONFIG_NET_IPV6=n` (the IPCP negotiation phase will fail if IPV6 is set and the module does not have an IPV6 address)
- `CONFIG_NET_IPV4=y`
- `CONFIG_PPP_NET_IF_NO_AUTO_START=y`
- `CONFIG_NET_PPP=y`
- `CONFIG_NET_PPP_ASYNC_UART=y`
- `CONFIG_NET_L2_PPP=y`
- `CONFIG_NET_L2_PPP_PAP=y`
- `CONFIG_NET_L2_PPP_TIMEOUT=10000`
- If your network operator requires a user name and password along with the APN **AND** requires CHAP authentication, then also include `CONFIG_NET_L2_PPP_CHAP=y` and hope it works,
- `CONFIG_NET_PPP_UART_BUF_LEN=512`; 512 being a suggested receive buffer length (two will be required),
- `CONFIG_NET_PPP_ASYNC_UART_TX_BUF_LEN=512`; 512 being a suggested transmit buffer length.

If you want to use sockets from Zephyr and if you want to use BSD names for stuff (otherwise you will need a liberal sprinkling of a `zsock_` prefix), you might also want to add:

- `CONFIG_NET_TCP=y`
- `CONFIG_NET_TCP_MAX_SEND_WINDOW_SIZE=256` (since the PPP link is relatively slow, keep the window size small)
- `CONFIG_NET_TCP_MAX_RECV_WINDOW_SIZE=256`
- `CONFIG_NET_SOCKETS=y`
- `CONFIG_NET_SOCKETS_POSIX_NAMES=y` (if you do not set this then you will need to prefix all sockets calls with `zsock_`, e.g. `zsock_connect()`)

Depending on how much data you expect to receive, you may want to increase `CONFIG_NET_PPP_RINGBUF_SIZE` from the default of 256 and you may need to [tune the buffer sizes that the network stack inside Zephyr uses](https://docs.zephyrproject.org/latest/connectivity/networking/net_config_guide.html#network-buffer-configuration-options).  If you have trouble getting data through then look for TCP errors in the Zephyr logging output like "E: TCP failed to allocate buffer in retransmission" and "E: Data buffer (1034) allocation failed", which indicates that you need bigger buffers.  In our Zephyr native sockets test, `zephyrSockTcp()`, where we send 2 kbytes up and back down again, we set `CONFIG_NET_BUF_DATA_SIZE=256`.

With `U_CFG_PPP_ENABLE` defined `ubxlib` will, internally, create a Zephyr UART device named `u-blox,uart-ppp` and you must instantiate it and connect it to Zephyr PPP's `zephyr,ppp-uart` in the root section of your `.dts` or `.overlay` file with something like:

```
/* Required for PPP-level integration with ubxlib to work */
/ { // The leading / indicates that we're in the root section
    // This chooses uart instance 99 to be the zephyr,ppp-uart that ppp.c wants
    chosen {
        zephyr,ppp-uart = &uart99;
    };

    // This creates instance 99 of a uart that we will give to zephyr,ppp-uart
    uart99: uart-ppp@8000 { // The "8000" here is irrelevant but required for Zephyr to work
        compatible = "u-blox,uart-ppp"; // The important part: this is an instance of u-blox,uart-ppp
        reg = <0x8000 0x100>; // This is irrelevant but required for Zephyr to work
        status = "okay"; // Zephyr boiler-plate
    };
};
```

If this is not correct then Zephyr `ppp.c` will fail to compile in `ppp_start()` because `__device_dts_ord_DT_CHOSEN_zephyr_ppp_uart_ORD` is undeclared (i.e. a binding for `zephyr,ppp-uart` has not been found) or your application will fail to link because something like `__device_dts_ord_6` (the driver implementation in [u_port_ppp.c](src/u_port_ppp.c)) has not been found.

You can find an example of how to do all this and make a sockets connection in [main_ppp_zephyr.c](/example/sockets/main_ppp_zephyr.c).

## Zephyr PPP Issue In Zephyr Versions Before 3.6.0
Before version 3.6.0 there was an issue in Zephyr PPP, discussed in [Github issue 67627](https://github.com/zephyrproject-rtos/zephyr/issues/67627), which meant that Zephyr did not shut the PPP link down properly, leaving the module "hanging", such that it would not connect again next time around unless the module was power cycled or rebooted.  A workaround for this is included, so everything works fine, _except_ that when a cellular connection is disconnected Zephyr must be allowed 20 seconds for its side of the PPP connection to time out.  If your application naturally leaves such a delay (e.g. because you are going to switch the cellular module off anyway, or because your application is not going to connect again for at least 20 seconds) then you can ignore this issue; otherwise you may set `U_CFG_PPP_ZEPHYR_TERMINATE_WAIT_SECONDS` to, for instance, 20, and the `ubxlib` code will add that delay on PPP closure.