# Introduction
This directory contains the API for the MQTT client implementation inside u-blox modules.

IMPORTANT: this common API is currently only mapped to u-blox cellular modules where it is supported on SARA-R5 and the SARA-R4x series; it may be subject to small changes when it is mapped to u-blox Wifi/BLE modules.  This text will be updated when more is known about those changes and the associated timescales.

# Usage
The `api` directory defines the MQTT client API.  The `test` directory contains tests for that API that can be run on any platform.