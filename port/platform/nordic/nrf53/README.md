# Introduction
These directories provide the implementation of the porting layer on the Nordic NRF5340 platform plus the associated build and board configuration information:

- `cfg`: contains the configuration files for the MCU, the OS, for the application and for testing (mostly which MCU pins are connected to which module pins).
- `sdk`: contains the files to build/test for the Nordic NRF5340 platform:
  - `nrfconnect`: contains the build and test files for the NRFconnect SDK
- `src`: contains the implementation of the porting layer for NRF5340.
- `app`: contains the code that runs the application (both examples and unit tests) on the NRF5340 platform.


# Hardware Requirements
This code is developed and tested on NRF5340pdk development board.

