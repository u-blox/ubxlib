# Introduction
These directories provide the implementation of the porting layer on the Zephyr platform.  Instructions on how to install the necessary tools and perform the build can be found in the [runner](runner) directory for Nordic platforms and the [runner_linux](runner_linux) directory for Linux/Posix.  While this should, generically, work with any Zephyr board, you should note that we only test with the following HW:

- Nordic MCUs, which require a **specific** version/configuration of Zephyr,
- Linux/posix, for debugging/development only, just like [windows](../windows).

Note: the directory structure here differs from that in the other platform directories in order to follow more closely the approach adopter by Zephyr, which is hopefully familiar to Zephyr users.

- [app](app): contains the code that runs the test application (both examples and unit tests) on the Zephyr platform.
- [cfg](cfg): contains the configuration files for the MCU, the OS, for the application and for testing (mostly which MCU pins are connected to which module pins).
- [dts](dts): device tree bindings needed by this code; for instance, the PPP UART driver, required if you want to integrate at PPP-level with cellular, see below.
- [src](src): contains the implementation of the porting layers for Zephyr platform on Nordic chips and on Linux/posix.
- [runner](runner): contains the test application configuration and build files for the Nordic MCUs supported on the Zephyr platform.
- [runner_linux](runner_linux): contains the test application configuration and build files for Linux/Posix on the Zephyr platform.
- [boards](boards): contains custom u-blox boards that are not \[yet\] in the Zephyr repo.
- [test](test): contains tests that use Zephyr application APIs to check out the integration of `ubxlib` into Zephyr, e.g. at PPP level.

# SDK Installation (NRF Connect)
`ubxlib` is tested with the version of Zephyr that comes with `nRFConnect SDK version 2.5.0` (Zephyr 3.4.99ncs, which includes modifications that Nordic make to Zephyr) which is the recommended version; it is intended to build with all versions nRFConnect SDK from 1.6.1 up til 2.5.0.

Follow the instructions to install the development tools:

- Install nRF connect. https://www.nordicsemi.com/Software-and-Tools/Development-Tools/nRF-Connect-for-desktop
- Start nRFConnect and use the tool chain manager to install the recommended SDK version (see above).
- IMPORTANT: update SDK and toolchain using the dropdown menu for your SDK version.

If you intend to use Zephyr on Linux/posix then you must also follow the instructions here:

https://docs.zephyrproject.org/latest/boards/posix/native_posix/doc/index.html

# Integration
`ubxlib` is a [Zephyr module](https://docs.zephyrproject.org/latest/guides/modules.html).

To add `ubxlib` to your Zephyr application you can either add it to your west.yml, or make use of [ZEPHYR_EXTRA_MODULES](https://docs.zephyrproject.org/latest/guides/modules.html#integrate-modules-in-zephyr-build-system).

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

## Device Tree
Zephyr pin choices for any HW peripheral managed by Zephyr (e.g. UART, I2C, SPI, etc.) are made at compile-time in the Zephyr device tree, they cannot be passed into the functions as run-time variables.  Look in the `zephyr/zephyr.dts` file located in your build directory to find the resulting pin allocations for these peripherals.

If you want to find out more about device tree please see Zephyr [Introduction to devicetree](https://docs.zephyrproject.org/latest/guides/dts/intro.html)

You will, though, still need to pass into `ubxlib` the HW block that is used: e.g. UART 0, UART 1, etc.  The UARTs, for instance, will be named `uart0`, `uart1`... in the device tree; the ending number is the value you should use to tell `ubxlib` what device to open.

If this is a problem, e.g. if your board uses its own naming of the devices you can add a device tree overlay file to your build and in this define aliases in order to get the correct mapping. Ubxlib will always check if there is an alias named ubxlib-xyz and use it when present. Example:

    / {
      aliases {
        ubxlib-uart1 = &usart1;
      };
    };

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

# PPP-Level Integration With Cellular
PPP support in Zephyr, at least in Zephyr version 3.4.99, is marked as "experimental": despite the presence of a PAP configuration item, there is no way to configure the authentication mode or the username/password (which are hard-coded in `zephyr/subsys/net/l2/ppp/pap.c`, see function `pap_config_info_add()`) and there doesn't appear to be any code to support CHAP authentication; hence, if your operator requires a user name and password along with the APN, you have no choice but to edit the Zephyr source code.

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
- `CONFIG_NET_SOCKETS_POSIX_NAMES=y` (if you do not set this then you will beed to prefix all sockets calls with `zsock_`, e.g. `zsock_connect()`)

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

## Zephyr PPP Issue
There is an issue in Zephyr PPP, discussed in [Github issue 67627](https://github.com/zephyrproject-rtos/zephyr/issues/67627), which means that Zephyr does not shut the PPP link down properly, leaving the module "hanging", such that it will not connect again next time around, unless the module is power cycled or rebooted.  A workaround for this is included, so everything works fine, _except_ that when a cellular connection is disconnected Zephyr must be allowed 20 seconds for its side of the PPP connection to time out; this delay is included within the ubxlib code.  If you do not want this delay (e.g. because you are going to switch the cellular module off anyway, or because your application is not going to connect again for at least 20 seconds) then you can remove it by defining `U_CFG_PPP_ZEPHYR_TERMINATE_WAIT_DISABLE`.

Once the Zephyr issue is fixed and the fix is available in a version of Zephyr that forms a part of nRFConnect SDK the delay will be removed.