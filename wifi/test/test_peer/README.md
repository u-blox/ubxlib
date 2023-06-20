# WiFi Captive Portal Test Peer
This directory contains an ESP32 Arduino sketch which can be used to test the WiFi captive portal; it should run on any ESP32 board.

It will constantly try to connect to a specified access point (`port_name`, which will be `UBXLIB_CAPTIVE_PORTAL` (see [u_wifi_captive_portal_test.c](../u_wifi_captive_portal_test.c)) unless otherwise specified) and, once connected, it will send the credentials \[that YOU must set up (see below)\], effectively emulating the behaviour of a mobile phone that is trying to configure the WiFi captive portal to connect with the intended final destination Wifi access point.

In order to build from source, the environment [https://github.com/plerup/makeEspArduino](https://github.com/plerup/makeEspArduino) is needed.  However the sub-directory (firmware)[firmware] contains prebuilt binaries which
can be flashed directly into any ESP32 board using the Espressif `esptool` with a command something like:

```
esptool --chip esp32 --port PORT_NAME --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size 4MB 0xe000 "./firmware/boot_app0.bin" 0x1000 "./firmware/bootloader.bin" 0x8000 "./firmware/partitions.bin" 0x10000 "./firmware/app.bin"
```

...replacing `PORT_NAME` with the appropriate UART name.

The applications accepts some commands incoming from the UART \[your UART terminal software *MUST* be set to use `LF` as line terminator\].  Use the command `help` or just an empty `LF` to get a list of the available ones:

```
Available commands:
  cred:<SSID> <Password>
    Set the credentials to be sent to the captive portal
  port_name:<PortalName>
    Set SSID of the captive portal to connect to
  factory
    Remove previous set credentials and portal SSID
  restart
    Restart this module
```

Please note that no connection will be made until the credentials have been defined.

The credentials and portal name are saved into the flash file system of the ESP32 and hence are persistent across restarts of the module.