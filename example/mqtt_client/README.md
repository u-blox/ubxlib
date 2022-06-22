# Introduction
This example shows how to connect to an MQTT broker on the public internet using the [common/mqtt_client](/common/mqtt_client) API.

# Usage
To build and run this example on a supported platform you need to travel down into the [port/platform](/port/platform)`/<platform>/mcu/<mcu>` directory of your choice and find the `runner` build.  The instructions there will tell you how to set/override \#defines.  The following \#defines are relevant:

`U_CFG_APP_FILTER`: set this to `exampleMqttClient` (noting that NO quotation marks should be included) to run *just* this example, as opposed to all the examples and unit tests.

For the remainder of the \#defines you may either override their values in the same way or, if you are only running this example, you may edit the values directly in [mqtt_main.c](mqtt_main.c) before compiling.

## Using A Cellular Module

`U_CFG_TEST_CELL_MODULE_TYPE`: consult [u_cell_module_type.h](/cell/api/u_cell_module_type.h) to determine the type name for the cellular module you intend to use.  For instance, to use SARA-R5 you would set `U_CFG_TEST_CELL_MODULE_TYPE` to `CELL_CFG_MODULE_SARA_R5`.

`U_CFG_APP_PIN_CELL_xxx`: the default values for the MCU pins connecting your cellular module to your MCU are \#defined in the file [port/platform](/port/platform)`/<platform>/mcu/<mcu>/cfg/cfg_app_platform_specific.h`.  You should check if these are correct for your board and, if not, override the values of the \#defines (where -1 means "not connected").

`U_CFG_APP_CELL_UART`: this sets the internal HW UART block that your chosen MCU will use to talk to the cellular module.  The default is usually acceptable but if you wish to change it then consult the file [port/platform](/port/platform)`/<platform>/mcu/<mcu>/cfg/cfg_hw_platform_specific.h` for other options.

Obviously you will need a SIM in your board, an antenna connected and you may need to know the APN associated with the SIM (though accepting the network default often works).

## Using A Wi-Fi Module

`U_CFG_TEST_SHORT_RANGE_MODULE_TYPE`: consult [u_short_range_module_type.h](/common/short_range/api/u_short_range_module_type.h) to determine the type name for the short range module you intend to use.
For instance, to use NINA-W15 you would set `U_CFG_TEST_SHORT_RANGE_MODULE_TYPE` to U_SHORT_RANGE_MODULE_TYPE_NINA_W15`.

`U_CFG_APP_SHORT_RANGE_UART`: this sets the internal HW UART block that your chosen MCU will use to talk to the short range module. If you wish to change the default value refer to the file `u_cfg_app_platform_specific.h` under your chosen [port/platform](/port/platform).

Make sure antenna is connected on to the board and you should be connected to wifi access point for MQTT to work.