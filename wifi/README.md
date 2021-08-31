# Introduction
This directory contains the Wi-Fi APIs for control and data.

The Wi-Fi APIs are split into the following groups:

<to be completed>

HOWEVER this is the detailed API; if all you would like to do is bring up a Wi-Fi bearer as simply as possible and then get on with exchanging data, please consider using the [common/network](/common/network) API, along with the [common/sock](/common/sock) API.  You may still dip down into this API from the network level as the handles used at the network level are the ones generated here.

In general the APIs here are a relatively thin layer, calling into [common/short_range](/common/short_range) where the bulk of the work for both this API and the [ble](/ble) API is carried out.

# Usage
The [api](api) directory contains the files that define the Wi-Fi APIs, each API function documented in its header file.  In the [src](src) directory you will find the implementation of the APIs and in the [test](test) directory the tests for the APIs that can be run on any platform.

A simple usage example is given below.

```
< to be completed>
```