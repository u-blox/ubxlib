# Introduction
These directories provide the implementation of the porting layer on the Espressif ESP32 platform plus the associated build and board configuration information:

- `cfg`: contains the configuration files for the MCU, the OS, for the application and for testing (mostly which MCU pins are connected to which module pins).
- `sdk`: contains the files to build/test for the Espressif ESP32 platform.
- `src`: contains the implementation of the porting layers for ESP32.
- `app`: contains the code that runs the application (both examples and unit tests) on the ESP32 platform.

# Hardware Requirements
None aside from the standard ESP32 trace log point over UART.

# Chip Resource Requirements
None over and above those used for your chosen example/test, e.g. a UART to talk to a u-blox module.