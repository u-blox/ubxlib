# Test peer for the ubxlib WiFi captive portal

This directory contains an esp32 Arduino sketch which
can be used to test the WiFi captive portal in ubxlib.

It will constantly try to connect to a specified captive
portal and once connected it will send the required credentials
for the portal to connect to an access point.

This application should be possible to run on any esp32 board.

In order to build form source  the environment available at:
 https://github.com/plerup/makeEspArduino
is needed. However the sub directory *firmware* contains prebuilt binaries which
can be flashed directly to the esp32 device. In order to do so the *esptool*
program is needed. To perform the flashing use something like the following command:

    esptool   \
    --chip esp32 --port "PORT_NAME" --baud 921600  \
    --before default_reset --after hard_reset write_flash \
    -z --flash_mode dio --flash_freq 40m --flash_size 4MB \
    0xe000 "./firmware/boot_app0.bin" \
    0x1000 "./firmware/bootloader.bin" \
    0x8000 "./firmware/partitions.bin" \
    0x10000 "./firmware/app.bin"

Replace PORT_NAME with appropriate uart name.

The applications accepts some commands incoming from the uart. Use the command
*help* or just an empty return to get a list of the available ones.

    Available commands:
      cred:<SSID> <Password>
        Set the credentials to be sent to the captive portal
      port_name:<PortalName>
        Set SSID of the captive portal to connect to
      factory
        Remove previous set credentials and portal SSID
      restart
        Restart this module

Please note that no connection will be made until credentials have been defined.
The default name for the portal name is the one used in ../u_wifi_captive_portal_test.c

The credentials and portal name are saved into the flash file system of the esp32 and
hence are persistent also after a restart of the module.