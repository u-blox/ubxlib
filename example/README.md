# Introduction
These directories provide the source code examples that show how to use the various APIs.  To build and run these source files on a supported platform you need to travel down into the `port/platform/<platform>/mcu/<mcu>` directory of your choice and follow the instructions in the `README.md` there.  For instance to build the examples on an ESP32 chip you would go to `port/platform/esp-idf/mcu/esp32` and follow the instructions in the `README.md` there to both install the Espressif development environment and build/run the examples.

For each MCU you will find a `runner` build.  This builds and runs all of these examples and all of the unit tests.

# Examples

- `sockets` contains an example of how to bring up a network (cellular or Wifi) and use it make a UDP or TCP socket connection to a server on the public internet.
- `security` contains examples of how to use the u-blox security features.