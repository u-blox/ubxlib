# Introduction
These examples demonstrates how to bring up a network connection using a u-blox module and make a TCP or UDP sockets connection, optionally over a TLS-secured link, to a server on the public internet.

# A Note On TLS Security
TLS security can be applied in at least three different ways, all of which can be performed using the `main_tls.c` example by tweaking the [settings structure](/common/security/api/u_security_tls.h#L283):

- encrypted traffic only: no checking that the server is authentic and no checking by the server that you are who you say you are.  This is the simplest form of security and requires no security credentials to operate.
- encrypted traffic plus server authentication: for this the necessary credentials must be loaded onto the module to check the server's authenticity.  For the purposes of this example the certificate of the `ubxlib` echo server is used, since the `ubxlib` echo server does not form a part of any chain of trust; in the real world with true trusted servers you would not need to do this, you load a root certificate onto the module instead and check the authenticity of the server's certificate using that.
- encrypted traffic plus server and client authentication: for this the client credentials must be loaded onto the module.  This example performs that operation however the `ubxlib` echo server does not perform any actual checking.

There are many other configurable items to play with, depending on how tight your TLS security requirements are: [u_security_tls.h](/common/security/api/u_security_tls.h) is the place to find all the options.  Remember that, as with any security system, when it doesn't work you will get very little feedback as to why; be patient and explore all the variables when debugging.

# Usage (C Examples)
To build and run these examples on a supported platform you need to travel down into the [port/platform](/port/platform)`/<platform>/mcu/<mcu>` directory of your choice and find the `runner` build.  The instructions there will tell you how to set/override \#defines.  The following \#defines are relevant:

`U_CFG_APP_FILTER`: set this to `exampleSockets` (noting that NO quotation marks should be included) to run *just* these example, as opposed to all the examples and unit tests.

For the remainder of the \#defines (see "Using A xxx Module" below) you may either override their values in the same way or, if you are only running these examples, you may edit the values directly in [main.c](main.c) and [main_tls.c](main_tls.c) before compiling.

# Usage (Arduino Example)
Follow the instructions in the [port/platform/arduino](/port/platform/arduino) directory to create the Arduino library version of `ubxlib`, which will include the example here.

# Using A Cellular Module

`U_CFG_TEST_CELL_MODULE_TYPE`: consult [u_cell_module_type.h](/cell/api/u_cell_module_type.h) to determine the type name for the cellular module you intend to use.  For instance, to use SARA-R5 you would set `U_CFG_TEST_CELL_MODULE_TYPE` to `U_CELL_MODULE_TYPE_SARA_R5`.

`U_CFG_APP_PIN_CELL_xxx`: the default values for the MCU pins connecting your cellular module to your MCU are \#defined in the file [port/platform](/port/platform)`/<platform>/mcu/<mcu>/cfg/cfg_app_platform_specific.h`.  You should check if these are correct for your board and, if not, override the values of the \#defines (where -1 means "not connected").

`U_CFG_APP_CELL_UART`: this sets the internal HW UART block that your chosen MCU will use to talk to the cellular module.  The default is usually acceptable but if you wish to change it then consult the file [port/platform](/port/platform)`<platform>/mcu/<mcu>/cfg/cfg_hw_platform_specific.h` for other options.

Obviously you will need a SIM in your board, an antenna connected and you may need to know the APN associated with the SIM (though accepting the network default often works).

# Using A Wi-Fi Module

`U_CFG_TEST_SHORT_RANGE_MODULE_TYPE`: consult [u_short_range_module_type.h](/common/short_range/api/u_short_range_module_type.h) to determine the type name for the short range module module you intend to use.  For instance, to use NINA-W13 you would set `U_CFG_TEST_SHORT_RANGE_MODULE_TYPE` to `U_SHORT_RANGE_MODULE_TYPE_NINA_W13`.

`U_CFG_APP_PIN_SHORT_RANGE_xxx`: the default values for the MCU pins connecting your short range module to your MCU are \#defined in the file [port/platform](/port/platform)`/<platform>/mcu/<mcu>/cfg/cfg_app_platform_specific.h`.  You should check if these are correct for your board and, if not, override the values of the \#defines (where -1 means "not connected").

`U_CFG_APP_SHORT_RANGE_UART`: this sets the internal HW UART block that your chosen MCU will use to talk to the Wi-Fi module.  The default is usually acceptable but if you wish to change it then consult the file [port/platform](/port/platform)`<platform>/mcu/<mcu>/cfg/cfg_hw_platform_specific.h` for other options.
