# Introduction
This directory contains an AT client, providing helper functions to send commands *to* an AT interface (e.g. a V250 modem) over a UART in a standard way and parse the responses that are received back either synchronously or asynchronously as unsolicited responses.  The client is used by various of the other `ubxlib` module in carrying out their functions, it is not intended for direct use by a customer.  It sits on top of the [port](/port) API, meaning that it can be used on any platform that the [port](/port) API supports.

Note: this AT client is used by cellular and all short range modules EXCEPT NORA-W36; for NORA-W36 an entirely different AT parser is used, see the `ucxclient` sub-directory of [common/short-range/src/gen2](/common/short-range/src/gen2).

# Usage
The [api](api) directory defines the AT client API.  The [test](test) directory contains tests for that API that can be run on any platform.