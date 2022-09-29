# Introduction
This directory contains some utilities for the [SPARTN](https://www.spartnformat.org/) message protocol, permitting a SPARTN message to be validated.  The functions rely on nothing other than [common/error/api](/common/error/api) and `memcpy()`.

Note that there is NO NEED to employ these utilities for normal operation of the Point Perfect service: SPARTN messages should be received, either via MQTT or from a u-blox L-band receiver such as the NEO-D9S, and forwarded transparently to a u-blox high-precision GNSS chip, such as the ZED-F9P, which decodes the SPARTN messages itself.

# Usage
The [api](api) directory defines the SPARTN protocol utility functions.  The [test](test) directory contains tests for the SPARTN protocol utility functions that can be run on any platform.