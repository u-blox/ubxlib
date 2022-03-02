# Introduction
These directories provide the configuration and build metadata for the NRF52 MCU under the nRF5 SDK.  The configuration here is sufficient to run the `ubxlib` tests and examples, no attempt is made to optimise the MCU RAM/flash etc. sizes, you need to know how to do that yourself.

- [cfg](cfg): contains the configuration files for the MCU, for the application and for testing (mostly which MCU pins are connected to which module pins).
- [gcc](gcc): contains the installation instructions and build files for the nRF5 SDK with GCC/Make as the toolchain.

# Hardware Requirements
In order to preserve valuable HW resources this code is configured to send trace output over the SWDIO (AKA RTT) port which a Segger J-Link debugger can interpret.

If you are using an NRF52840 DK board such a debugger is *already* included on the board.  If you're working to a bare NF52 chip or a bare u-blox NINA-B3 module you should equip yourself with a Segger [J-Link Base](https://www.segger.com/products/debug-probes/j-link/models/j-link-base/) debugger and the right cable to connect it to your board.

For debugging you will need the Segger J-Link tools, of which the Windows ones can be found here:

https://www.segger.com/downloads/jlink/JLink_Windows.exe

If you don't have an NRF52 board with Segger J-Link built in or you have a bare module etc. and are without a Segger J-Link box, you can modify the `sdk_config.h` file in the `cfg` directory to send trace out of a spare UART instead.  You would need to enable a UART port, switch off `NRF_LOG_BACKEND_RTT_ENABLED` and modify the `NRF_LOG_BACKEND_UART_ENABLED`, `NRF_LOG_BACKEND_UART_TX_PIN` and `NRF_LOG_BACKEND_UART_BAUDRATE` variables.

# Chip Resource Requirements
This code requires the use of a `TIMER` peripheral as a timer tick source.

If you connect a u-blox module via one of the UARTs an additional `TIMER` peripheral will be required per UART to count received characters via a PPI (Programmable Peripheral Interconnect) channel.

The default `TIMER` choices are specified in `cellular_cfg_hw_platform_specific.h` and can be overriden at compile time.