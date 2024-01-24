# Introduction
This directory contains the short-range APIs, covering both Wi-Fi and BLE control/connectivity.

This API relies upon either [common/at_client](/common/at_client) or the `ucxclient` under [src/gen2](src/gen2) (see below) to send commands to and parse responses received from an external Wi-Fi/BLE module.

The operation of `ubxlib` does not rely on a particular version of uConnectExpress; the versions that we test with are listed in the [test](test) directory.

# Usage
The [api](api) directory contains the files that define the short range APIs, each API function documented in its header file.  In the [src](src) and [src/gen2](src/gen2) directories you will find the implementation of the APIs and in the [test](test) directory the tests for the APIs that can be run on any platform.

# uConnectExpress For NORA-W36 And Beyond
`ubxlib` relies on uConnectExpress, running on the short-range module and providing the AT interface to this MCU.  While `ubxlib` does not rely on a particular version of uConnectExpress, NORA-W3 and later modules are provided with a second generation of uConnectExpress which requires the `ubxlib` code in the [src/gen2](src/gen2) directory rather that in the [src](src) directory.

To use the [src/gen2](src/gen2) code, please **add** `short_range_gen2` to the `UBXLIB_FEATURES` variable in your `make` or `CMake` file, e.g.:

```
UBXLIB_FEATURES=cell gnss short_range short_range_gen2
```

The versions of uConnectExpress that we test with are listed in the short-range [test](/common/short_range/test) directory.

## Implementation Note
The next generation uConnectExpress for NORA-W36 is quite different to what went before, hence the new implementation files in [src/gen2](src/gen2).  In particular:
- an entirely new AT parser is used for NORA-W36, NOT [common/at_client](/common/at_client), see the `ucxclient` under [src/gen2](src/gen2); if your application made any direct calls into [common/at_client](/common/at_client) previously then, to have an effect on NORA-W36, the equivalents in the `ucxclient` under [src/gen2](src/gen2) must be called (the existing calls to [common/at_client](/common/at_client) should remain if you are also using a cellular module or a short-range module other than NORA-W36),
- EDM (Extended Data Mode) is no longer used at all.
