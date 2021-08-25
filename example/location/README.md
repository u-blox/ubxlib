# Introduction
These examples demonstrate how to establish location in three different configurations:

- locally using a GNSS chip that is attached directly to this MCU (`main_loc_gnss.c`),
- locally using a GNSS chip that is attached via a cellular module (`main_loc_gnss_cell.c`),
- using cloud services such as Cell Locate via a cellular \[and in future Wifi\] module (`main_loc_cell_locate.c`).

# Usage
To build and run these examples on a supported platform you need to travel down into the `port/platform/<platform>/mcu/<mcu>` directory of your choice and find the `runner` build.  The instructions there will tell you how to set/override #defines.  The following #defines are relevant:

`U_CFG_APP_FILTER`: set this to `exampleLoc` (noting that NO quotation marks should be included) to run *just* these examples, as opposed to all the examples and unit tests.

For the remainder of the #defines you may either override their values in the same way or, if you are only running these examples, you may edit the values directly in `main_loc_gnss.c`, `main_loc_gnss_cell.c`  and `main_loc_cloud.c` before compiling.

## The GNSS Example `main_loc_gnss.c`
If you have a GNSS chip attached directly to this MCU then you can run the `main_loc_gnss.c` example and the following values should be set.

`U_CFG_TEST_GNSS_MODULE_TYPE`: consult [u_gnss_type.h](/gnss/api/u_gnss_type.h) to determine the type name for the GNSS module you intend to use.  For instance, to use an M8 module you would set `U_CFG_TEST_GNSS_MODULE_TYPE` to `U_GNSS_MODULE_TYPE_M8`.

`U_CFG_APP_PIN_GNSS_xxx`: the default values for the MCU pins connecting your GNSS module to your MCU are #defined in the file `port/platform/<platform>/mcu/<mcu>/cfg/cfg_app_platform_specific.h`.  You should check if these are correct for your board and, if not, override the values of the #defines (where -1 means "not connected").

`U_CFG_APP_GNSS_UART`: this sets the internal HW UART block that your chosen MCU will use to talk to the GNSS module.  The default is usually acceptable but if you wish to change it then consult the file `port/platform/<platform>/mcu/<mcu>/cfg/cfg_hw_platform_specific.h` for other options.

You will also need an antenna connected to the GNSS chip.

## The GNSS Cellular Example `main_loc_gnss_cell.c`
If you have a GNSS chip attached via a cellular module then you can run the `main_loc_gnss_cell.c` example and the following values should be set.

`U_CFG_TEST_GNSS_MODULE_TYPE`: consult [u_gnss_type.h](/gnss/api/u_gnss_type.h) to determine the type name for the GNSS module you intend to use.  For instance, to use an M8 module you would set `U_CFG_TEST_GNSS_MODULE_TYPE` to `U_GNSS_MODULE_TYPE_M8`.

`U_CFG_TEST_CELL_MODULE_TYPE`: consult [u_cell_module_type.h](/cell/api/u_cell_module_type.h) to determine the type name for the cellular module you intend to use.  For instance, to use SARA-R5 you would set `U_CFG_TEST_CELL_MODULE_TYPE` to `U_CELL_MODULE_TYPE_SARA_R5`.

`U_CFG_APP_PIN_CELL_xxx`: the default values for the MCU pins connecting your cellular module to your MCU are #defined in the file `port/platform/<platform>/mcu/<mcu>/cfg/cfg_app_platform_specific.h`.  You should check if these are correct for your board and, if not, override the values of the #defines (where -1 means "not connected").

`U_CFG_APP_CELL_UART`: this sets the internal HW UART block that your chosen MCU will use to talk to the cellular module.  The default is usually acceptable but if you wish to change it then consult the file `port/platform/<platform>/mcu/<mcu>/cfg/cfg_hw_platform_specific.h` for other options.

## The Cell Locate Example `main_loc_cell_locate.c`
If you do not have a GNSS chip you may establish approximate location (e.g. within a kilometre) via a cellular module using the Cell Locate service as shown in the `main_loc_cell_locate.c` example.

For this example the settings of the GNSS Cellular Example above must be followed and then in addition:

`U_CFG_APP_CELL_LOC_AUTHENTICATION_TOKEN`: must be set to a valid authentication token for the u-blox Cell Locate service, obtainable from your [Thingstream portal](https://portal.thingstream.io/app/location-services), noting that NO quotation marks should be included.

You will need a SIM in your board, a cellular antenna connected and you may need to know the APN associated with the SIM (though accepting the network default often works).