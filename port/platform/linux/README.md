# Introduction
These directories provide the implementation of the porting layer on native Linux.

# Building
All software building for this platform is intended to be made using [CMake](https://cmake.org/).

There are typically two scenarios when it comes to building Linux application which includes `ubxlib`.

The first case is to build the test runner application within this repo. More information about this can be [found here](mcu/posix/runner/README.md).

On the other hand if you want to add `ubxlib` to an existing or new Linux application of your own you just have to add the following text your `CMakeLists.txt` file

    include(DIRECTORY_WHERE_YOU_HAVE_PUT_UBXLIB/port/platform/linux/linux.cmake)
    target_link_libraries(YOUR_APPLICATION_NAME ubxlib ${UBXLIB_REQUIRED_LINK_LIBS})
    target_include_directories(YOUR_APPLICATION_NAME PUBLIC ${UBXLIB_INC} ${UBXLIB_PUBLIC_INC_PORT})

# Visual Studio Code
Both case listed above can also be made from within Visual Studio Code (on the Linux platform).

In the first case you can just open the predefined Visual Studio project file available in the root directory of this repository, `ubxlib-runner.code-workspace`. You can then select the `Build Linux runner` build target to start a build, and then the `Linux runner` debug target to start debugging.

In the second case you have to install the [CMake extension for Visual Studio Code](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools).

More information on how to use CMake in Visual Studio Code can be [found here](https://code.visualstudio.com/docs/cpp/CMake-linux).

# PPP-Level Integration With Cellular
If you wish to use a cellular connection directly as an IP transport with Linux, you may do so using `pppd`, installed as the package `ppp` in the usual way.  The setup will look something like this:

```
                  +----------+          +------------------+       +--------------+            +-----------------+      \|/
   IP address     |          |  socket  | Your application |       |              |  UART/USB  |                 |       |
     inside  o----|   pppd   |<-------->|   using ubxlib   |<----->| /dev/ttyXXX0 |<---------->| Cellular module |<------+
     Linux        |          |          |                  |       |              |            |                 |
                  +----------+          +------------------+       +--------------+            +-----------------+
                          ^                                                                       ^
                           \                                                                     /
                             ------------------------- PPP over serial -------------------------
```

In words: `ubxlib` exposes a socket (by default `5000`) which `pppd` is able to connect to, `ubxlib` then connects on to the cellular module via a physical serial port, all of which means that `pppd` is able to make a PPP-over-serial connection to the PPP entity inside the cellular module, which has the connection to the public internet of the cellular network.

For PPP connectivity to be available you need to define `U_CFG_PPP_ENABLE` when building `ubxlib`, e.g. by including it in the `U_FLAGS` environment variable which `linux.cmake` looks for.  Your application code must configure `ubxlib` to know what serial port the cellular module is connected to (the `/dev/ttyXXX0` above, e.g. `/dev/ttyAMA0` for UART0 of a Raspberry Pi) and the Linux port layer here inside `ubxlib` will listen on socket `U_PORT_PPP_LOCAL_DEVICE_NAME` (i.e. `127.0.0.1:5000`) for a connection at the other end from `pppd`.

`pppd` should be run as follows:

```
pppd socket 127.0.0.1:5000 115200 passive persist maxfail 0 local defaultroute
```

This will cause `pppd` to try connecting to `ubxlib` on port `5000`, persistently, until it is terminated.  If you have permissions problems running `pppd`, take a look [here](https://tldp.org/HOWTO/PPP-HOWTO/root.html) to find the best way to resolve them: when we run `pppd` in the `ubxlilb` test system we have `pppd` `setuid` and we put `noauth` in the `/etc/ppp/options` file as the test system is secured at the perimeter, miscreants cannot get in; if you do not wish to rely on this in your scenario then you may need to configure a peer address within `pppd` and use `pppd call` or some such.  If you need to use a socket other than `5000`, then in your application you may do this by calling `uPortPppSetLocalDeviceName()` with a revised socket address, e.g. `127.0.0.1:6000`, **before** you call `uDeviceOpen()`.

IMPORTANT: if your cellular service provider requires you to enter a username and/or password for authentication then you must provide those parameters to `pppd`, since there is no way for the `ubxlib` code to supply them to `pppd`.

Once `pppd` is running you may start your application, which will call `ubxlib` in the usual way to open the cellular device and connect it to the network: you can find an example of how to do this in [main_ppp_linux.c](/example/sockets/main_ppp_linux.c).

You may also find [the rest of this extremely detailed HOWTO](https://tldp.org/HOWTO/PPP-HOWTO/index.html) for `pppd` useful.

# Limitations
Some limitations apply on this platform:

- Linux does not provide an implementation of critical sections, something which this code relies upon for cellular power saving (specifically, the process of waking up from cellular power-saving) hence cellular power saving cannot be used from a Linux build.
- On a Raspberry Pi (any flavour), the I2C HW implementation of the Broadcom chip does not correctly support clock stretching, which is required for u-blox GNSS devices, hence it is recommended that, if you are using I2C to talk to the GNSS device, you use the bit-bashing I2C driver to avoid data loss, e.g. by adding the line `dtoverlay=i2c-gpio,i2c_gpio_sda=2,i2c_gpio_scl=3,i2c_gpio_delay_us=2,bus=8` to `/boot/config.txt` and NOT uncommenting the `i2c_arm` line in the same file.
- Use of GPIO chips above 0 are supported but NOT when the pin in question is to be an output that must be set high at initialisation; this is because the `uPortGpioConfig()` call is what tells this code to use an index other than 0 and, to set an output pin high at initialisation, `uPortGpioSet()`, has to be called _before_ `uPortGpioConfig()`.
- All testing has been carried out on a 64-bit Raspberry Pi 4.