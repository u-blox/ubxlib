# Introduction
This directory and its sub-directories contain the build and build script which compiles and runs any or all of the examples and tests for the SARAR5UCPU platform.

# Installation
To run examples and test with SARAR5UCPU following tools must be installed and on the path.

- `arm toolchain`: toolchain required to build examples and tests.
- `cmake`: is a cross-platform, open-source build system generator required for build.
- `ninja`: is a small build system required for build.

# Usage
Follow the instructions to build examples and test for SARAR5UCPU platform:

Create a directory `BUILD` to the same directory level as `ubxlib` and you will also need a copy of cell_ucpu_sdk, unity, the unit test framework, and mbedtls (u-blox/mbedtls, fork of mbedtls), for cryptographic functions, get the branch `v2.26.0_modifications`. which can be Git cloned as mentioned below:

Clone cell_ucpu_sdk:
```
git clone https://github.com/u-blox/cell_ucpu_sdk.git
```

Clone Unity:
```
git clone https://github.com/ThrowTheSwitch/Unity.git
```

Clone mbedtls:
```
git clone --single-branch --branch v2.26.0_modifications https://github.com/u-blox/mbedtls.git
```

Clone it to the same directory level as `ubxlib`, i.e.:

```
..
.
BUILD
cell_ucpu_sdk
Unity
mbedtls
ubxlib
```
After following the above steps, run the following command to generate build files.
```
cmake -DCMAKE_TOOLCHAIN_FILE=path/to/armToolchian -DU_CFG_TEST_FILTER=port -GNinja ubxlib\port\platform\cell_ucpu\r5\runner
```
If the above command is successful, run the following command in the same directory to generate the executable to run on the SARA5UCPU platform.
```
ninja
```
A .bin file will be generated after the above command, which you can flash on to SARAR5UCPU platform.
Please note that if you want to run tests or examples you have to provide the name of test group or example using `U_CFG_TEST_FILTER`, if you want to run a specific test, provide it's name using `U_CFG_APP_FILTER` along with the `U_CFG_TEST_FILTER`. If you wish to write your own application you can simply edit the `u_main.c` file placed at `cell_ucpu\r5\app`. Currently supported tests and examples are mentioned in the table below.

| Tests/Examples  | U_CFG_TEST_FILTER |
| -------------   | ------------- |
| port tests  | port  |
| cellular tests  | cell  |
| network tests | network|
| mqtt client tests| mqttClient |
| security tests | security |
| socket tests | sock |
| ubx protocol tests | ubxProtocol |
| cellualar example | exampleCell|
| mqtt example | exampleMqtt|
| security example | exampleSec|
| socket example | exampleSockets |

