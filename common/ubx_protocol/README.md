# Introduction
This directory contains encode and decode utilities for the UBX protocol, used to communicate with a u-blox GNSS module.  The functions rely on nothing other than [common/error/api](/common/error/api) and `memcpy()`.

# Usage
The [api](api) directory defines the UBX encode/decode functions.  The [test](test) directory contains tests for the UBX protocol encode/decode functions that can be run on any platform.