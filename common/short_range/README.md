# Introduction
This directory contains the short-range APIs, covering both Wi-Fi and BLE control/connectivity.

This API relies upon the [common/at_client](/common/at_client) component to send commands to and parse responses received from an external Wi-Fi/BLE module.

# Usage
The [api](api) directory contains the files that define the short range APIs, each API function documented in its header file.  In the [src](src) directory you will find the implementation of the APIs and in the [test](test) directory the tests for the APIs that can be run on any platform.