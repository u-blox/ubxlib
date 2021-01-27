# Introduction
These directories provide the porting layer that allows the high level C APIs to be built and run on a supported MCU.

No attempt is made to create a full MCU porting API; only the APIs necessary to run the high level C APIs are implemented here.

# Usage
The `api` directory defines the port API, each API function documented in the header file.  The `clib` directory contains implementations of C library APIs that may be missing on some platforms (e.g. `strtok_r`, the re-entrant version of the `strtok` library function).  In the `platform` directory you will find the implementation of the porting API on various target SDKs and MCUs and the necessary instructions to create a working binary and tests on each of those target MCUs.  Please refer to the `README.md` files in your chosen target platform directory for more information.

The `test` directory contains tests for the `port` API that can be run on any platform.