# Introduction
This directory contains the API for the MQTT client implementation inside u-blox modules.

IMPORTANT: this common API is currently only mapped to u-blox cellular modules where it is supported on SARA-R5; it may be subject to small changes when it is mapped to u-blox Wifi/BLE modules.  This text will be updated when more is known about those changes and the associated timescales.

# MQTT And Cellular SARA-R4 Modules
The u-blox SARA-R4 modules support MQTT but the syntax at the AT interface is different depending on exactly which version of SARA-R4 module is employed.  Code to support SARA-R4 is included here but we have not yet been able to test it on all of the flavours of SARA-R4 module.  Until we do that no SARA-R4 module is marked as supported in this code.

If you need support for MQTT on SARA-R4, try editing the `gUCellPrivateModuleList` in [u_cell_private.c](/cell/src/u_cell_private.c) to remove the comments from the MQTT features for your SARA-R4 module type, then run the `cellMqtt` and `mqttClient` test cases and fix/re-run until they pass.  It will require some debugging/fixing to get right but will be quicker than starting from an empty `.c` file.

# Usage
The `api` directory defines the MQTT client API.  The `test` directory contains tests for that API that can be run on any platform.