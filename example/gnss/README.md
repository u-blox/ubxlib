# Introduction
These directories provide examples that are specific to u-blox GNSS chips, e.g. NEO-M8, ZED-F9P etc.  They are relevant if you are _just_ using GNSS (i.e. not cellular or WiFi/BLE) and all you want to do is configure the GNSS device and exchange message of your choice with it.

# Example
- [cfg_val_main.c](cfg_val_main.c) contains an example of how to configure a GNSS chip using the `uGnssCfgValXxx()` API which can be found in [u_gnss_cfg.h](/gnss/api/u_gnss_cfg.h); note that this API is supported ONLY on M9 modules and later, not on M8 modules; for M8 modules there are simple configuration/information APIs in [u_gnss_cfg.h](/gnss/api/u_gnss_cfg.h)/[u_gnss_info.h](/gnss/api/u_gnss_info.h) that need no examples.
- [msg_main.c](msg_main.c) contains an example of how to exchange messages of your choice with a GNSS chip that is connected directly to this MCU, i.e. not via an intermediate [cellular] module.  It uses the `uGnssMsg` API which can be found in [u_gnss_msg.h](/gnss/api/u_gnss_msg.h).

# Usage
To build and run these examples on a supported platform you need to travel down into the `port/platform/<platform>/mcu/<mcu>` directory of your choice and find the `runner` build.  The instructions there will tell you how to set/override defines.  The following \#defines are relevant:

`U_CFG_APP_FILTER`: set this to `exampleGnss` (noting that NO quotation marks should be included) to run *just* these examples, as opposed to all the examples and unit tests.

`U_CFG_TEST_GNSS_MODULE_TYPE`: consult [u_gnss_module_type.h](/gnss/api/u_gnss_module_type.h) to determine the type name for the GNSS chip you intend to use.  For instance, to use a ZED-F9P you would set `U_CFG_TEST_GNSS_MODULE_TYPE` to `U_GNSS_MODULE_TYPE_M9`.

`U_CFG_APP_GNSS_UART` / `U_CFG_APP_GNSS_I2C`: this sets the internal HW block that your chosen MCU will use to talk to the GNSS module.  The default from the file [port/platform](/port/platform)`/<platform>/mcu/<mcu>/cfg/cfg_app_platform_specific.h` may be acceptable; if you wish to change it then consult the specification for your MCU and the file [port/platform](/port/platform)`/<platform>/mcu/<mcu>/cfg/cfg_hw_platform_specific.h` for other options.

For the remainder of the \#defines you may either override their values in the same way or, if you are only running these examples, you may edit the values directly in the example source files before compiling.

`U_CFG_APP_PIN_GNSS_xxx`: the default values for the MCU pins connecting your GNSS module to your MCU are \#defined in the file [port/platform](/port/platform)`/<platform>/mcu/<mcu>/cfg/cfg_app_platform_specific.h`.  You should check if these are correct for your board and, if not, override the values of the \#defines (where -1 means "not connected").

You will also need an antenna connected to the GNSS chip.