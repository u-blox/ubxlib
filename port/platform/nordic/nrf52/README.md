# Introduction
These directories provide the implementation of the porting layer on the Nordic NRF52 platform plus the associated build and board configuration information:

- `cfg`: contains the configuration files for the MCU, the OS, for the application and for testing (mostly which MCU pins are connected to which module pins).
- `sdk`: contains the files to build/test for the Nordic NRF5 platform:
  - `gcc`: contains the build and test files for the Nordic SDK, nRF5 under GCC.
  - `ses`: contains the build and test files for the Nordic SDK, nRF5 under Segger Embedded Studio.
- `src`: contains the implementation of the porting layer for NRF52.
- `app`: contains the code that runs the application (both examples and unit tests) on the NRF52 platform.

# Hardware Requirements
In order to preserve valuable HW resources this code is configured to send trace output over the SWDIO (AKA RTT) port which a Segger J-Link debugger can interpret.

If you are using a the NRF52840 DK board such a debugger is *already* included on the board.  If you're working to a bare NF52 chip or a bare u-blox NINA-B1 module you should equip yourself with a Segger [J-Link Base](https://www.segger.com/products/debug-probes/j-link/models/j-link-base/) debugger and the right cable to connect it to your board.

For debugging you will need the Segger J-Link tools, of which the Windows ones can be found here:

https://www.segger.com/downloads/jlink/JLink_Windows.exe

If you don't have an NRF52 board with Segger J-Link built in or you have a bare module etc. and are without a Segger J-Link box, you can modify the `sdk_config.h` file in the `cfg` directory to send trace out of a spare UART instead.  You would need to enable a UART port, switch off `NRF_LOG_BACKEND_RTT_ENABLED` and modify the `NRF_LOG_BACKEND_UART_ENABLED`, `NRF_LOG_BACKEND_UART_TX_PIN` and `NRF_LOG_BACKEND_UART_BAUDRATE` variables.

# Chip Resource Requirements
This code requires the use of a `TIMER` peripheral as a timer tick source.

If you connect a u-blox module via one of the UARTs an additional `TIMER` peripheral will be required per UART to count received characters via a PPI (Programmable Peripheral Interconnect) channel.

The default `TIMER` choices are specified in `cellular_cfg_hw_platform_specific.h` and can be overriden at compile time.