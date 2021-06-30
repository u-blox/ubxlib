USB udev rules
==============

These udev rules will allow any user to access the following devices on a Linux system:

- Cypress KitProg1/KitProg2 in CMSIS-DAP mode
- Cypress KitProg3
- Cypress MiniProg4
- DAPLink
- STLinkV2
- STLinkV2-1
- STLinkV3
- Keil ULINKplus
- NXP LPC-LinkII
- NXP MCU-Link
- J-Link
- FTDI devices
- \+ more

In Ubuntu copy the files to `/etc/udev/rules.d/`

To list connected USB device and get their corresponding VID/PID use:
```
$ lsusb
Bus 001 Device 006: ID 0483:3748 STMicroelectronics ST-LINK/V2
Bus 001 Device 005: ID 1366:1011 SEGGER J-Link
Bus 001 Device 001: ID 1d6b:0001 Linux Foundation 1.1 root hub
```