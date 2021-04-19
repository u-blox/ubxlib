# Introduction
These directories provide the source code examples that show how to use the various APIs.  To build and run these source files on a supported platform you need to travel down into the `port/platform/<platform>/mcu/<mcu>` directory of your choice and follow the instructions in the `README.md` there.  For instance to build the examples on an ESP32 chip you would go to `port/platform/esp-idf/mcu/esp32` and follow the instructions in the `README.md` there to both install the Espressif development environment and build/run the examples.

For each MCU you will find a `runner` build.  This builds and runs all of these examples and all of the unit tests.

# Examples

- `sockets` contains examples of how to bring up a network (cellular or Wifi) and use it make a UDP or TCP socket connection to a server on the public internet.
- `security` contains examples of how to use the u-blox security features.
- `mqtt_client` contains an example of how to use the MQTT client API to contact an MQTT broker on the public internet.
- `utilities/c030_module_fw_update` is not so much an example as a program that is required if you need to update the firmware of the cellular module on a C030-R5 or C030-R4xx board.- `security` contains examples of how to use the u-blox security features.
- `cell` contains examples specific to u-blox cellular modules (e.g. SARA-U201, SARA-R4 or SARA-R5).
- `utilities/c030_module_fw_update` is not so much an example as a program that is required if you need to update the firmware of the cellular module on a C030-R5 or C030-R4xx board.