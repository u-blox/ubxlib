# Why This Directory/File Is Here
This binding file is required only for PPP-level integration with Zephyr, which happens if `U_CFG_PPP_ENABLE` is defined when building `ubxlib`.  It goes like this:

- Zephyr's `ppp.c` wants to talk to a Zephyr UART that is named `zephyr,pppuart`.
- The Zephyr port code here implements such a driver, named `u-blox,uart-ppp` (see [u_port_ppp.c](../../src/u_port_ppp.c)).
- Connecting the two can ONLY be done in the application's `.dts` or `.overlay` file.
- Hence the driver must have a binding into the device tree.
- That's what [u-blox,uart-ppp.yaml](u-blox,uart-ppp.yaml) does: it lets the device-tree-creating part of the build process know that `u-blox,uart-ppp` is a UART driver, so that the application `.dts` or `.overlay` file can refer to it, and the macros at the bottom of [u_port_ppp.c](../../src/u_port_ppp.c) can create the implementation of it when that file is compiled.