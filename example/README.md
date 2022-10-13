# Introduction
These directories provide the source code examples that show how to use the various APIs.  To build and run these source files on a supported platform you need to travel down into the [port/platform](/port/platform)`/<platform>/mcu/<mcu>` directory of your choice and follow the instructions in the `README.md` there.  For instance to build the examples on an ESP32 chip you would go to [port/platform/esp-idf/mcu/esp32](/port/platform/esp-idf/mcu/esp32) and follow the instructions in the `README.md` there to both install the Espressif development environment and build/run the examples.

For each MCU you will find a `runner` build.  This builds and runs all of these examples and all of the unit tests.

# Examples

- [sockets](sockets) contains examples of how to bring up a network (cellular or Wi-Fi) and use it to make a UDP or TCP socket connection to a server on the public internet.
- [security](security) contains examples of how to use the u-blox security features.
- [location](location) contains examples of how to get a location fix.
- [mqtt_client](mqtt_client) contains an example of how to use the MQTT client API to contact an MQTT broker on the public internet.
- [http_client](http_client) contains an example of how to use the HTTP client API.
- [cell](cell) contains examples specific to u-blox cellular modules (e.g. SARA-U201, SARA-R4 or SARA-R5).
- [gnss](gnss) contains examples specific to u-blox GNSS chips (e.g. M8, M9).
- [utilities/c030_module_fw_update](utilities/c030_module_fw_update) is not so much an example as a program that is required if you need to update the firmware of the cellular module on a C030-R5 or C030-R4xx board.