# Introduction
This example demonstrate how to use the chip to chip (C2C) protection u-blox security feature.  This feature allows the communication across the AT interface between the MCU and the module to be authenticated and encrypted, preventing snooping of hardware lines.  Since the process of pairing a module with an MCU for chip-to-chip security is a one-time-only irreversible process (except by special arrangement with u-blox) this example will only run if `U_CFG_TEST_SECURITY_C2C_TE_SECRET` (a 16 byte binary value that forms part of the pairing process) is defined.

IMPORTANT: it is intended that the pairing process that enables chip to chip security is carried out in a secure environment, e.g. in your factory.  To ensure that is the case the module will ONLY allow chip to chip security pairing to be performed BEFORE the module has been security boot-strapped, something the module will do THE MOMENT it contacts the network for the first time.  In other words, the sequence must be:

1. Complete the C2C pairing process between your MCU and the module; your MCU must store the pairing keys that are used to switch C2C security on and off later as desired.
2. Allow the module to contact the network for the first time: it will bootstrap with the u-blox security servers.
3. Complete the security sealing process.

Steps 1 to 3 must be performed in the order given and should be performed in a secure environment.  With that done C2C security can be employed by your MCU at any time it wishes.

Note: in order to test this example code, we have enabled a special permission on our test devices, LocalC2CKeyPairing, which DOES permit C2C pairing to be performed on a security bootstrapped/sealed module.

# Usage
Follow the instructions in the directory above this to build and download the target code.  Before commencing your build:

- set the \#define `U_CFG_APP_FILTER` to `exampleSecC2c` (noting that NO quotation marks should be included) if you wish to run *just* this example, as opposed to all the examples and unit tests,
- as stated above, since the process of C2C pairing a module with an MCU is normally an irreversible one (except by arrangement with u-blox) this example will do nothing unless you define a value for `U_CFG_TEST_SECURITY_C2C_TE_SECRET`, again with no quotation marks around the value; for instance, we use `U_CFG_TEST_SECURITY_C2C_TE_SECRET=\x00\x01\x02\x03\x04\x05\x06\x07\xff\xfe\xfd\xfc\xfb\xfa\xf9\xf8` for our internal testing.

You will, of course, notice no difference in operation to that with C2C security disabled, since the scrambled AT comms are encrypted/decypted and integrity checked by this code: to see the effect of C2C protection you should monitor the serial lines between the MCU and the module with something like a Salaea logic probe to see that the AT communications are, in fact, scrambled when a C2C session is open.