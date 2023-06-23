# Introduction
This example demonstrate how to use the pre-shared key generation u-blox security feature.  The module in use must have been security sealed for this example to work: see the comments in the device-side example code that describe how to do this.

# Usage
Follow the instructions in the directory above this to build and download the target code, setting the \#define `U_CFG_APP_FILTER` to `exampleSecPsk` (noting that NO quotation marks should be included) if you wish to run *just* this example, as opposed to all the examples and unit tests.

When the target code has run and completed successfully it will print the example generated PSK and associated PSK ID in the debug stream, something like:

```
32 bytes of PSK returned:       70665f1ba1753d36e5a412a56233507da1dbd0d8476f418423892de0895c7e9f
14 byte(s) of PSK ID returned:  11010008003f9720f38c30c88428
```