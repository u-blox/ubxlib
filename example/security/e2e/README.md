# Introduction
This example demonstrate how to use the end to end data protection u-blox security feature.  The module in use must have been security sealed for this example to work: see the comments in the device-side example code that describe how to do this.

# Usage
Follow the instructions in the directory above this to build and download the target code, setting the \#define `U_CFG_APP_FILTER` to `exampleSecE2e` (noting that NO quotation marks should be included) if you wish to run *just* this example, as opposed to all the examples and unit tests.

When the target code has run and completed successfully it will print the encrypted message in the debug stream, something like:

```
76 byte(s) of data returned.
11010008003f97203b08c787d1b200006423b8017f96548d65fcaf1036f21d10bb18bd86b57178685ffbaf471162e5bf5a7445ff568290fbb8cce57d75d5ae4830aad00da6cfd589c6795691
```

To obtain the plain text once more, edit the lines at the top of `e2edecrypt.py` to fill in your `API_KEY` and `API_SECRET` and set `ENC_DATA` to be the encrypted data.  Run the Python script and it should return to you the un-encrypted data once more.
