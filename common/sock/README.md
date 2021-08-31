# Introduction
These directories provide a user data (sockets) interface.  The API presented is a Berkeley sockets API, similar to that of LWIP, allowing the files here to replace LWIP. 

# Usage
The directories include only the API and pure C source files that make no reference to a platform or an operating system.  They rely upon the [port](/port) directory to map to a target platform and provide the necessary build/test infrastructure for that target platform; see the relevant platform directory under [port](/port) for build and usage information.

A usage example can be found in the `README.md` file for the [common/network](/common/network) API.

# Testing
The [test](test) directory contains generic tests for this API. Please refer to the relevant platform directory of the [port](/port) component for instructions on how to build and run the tests.  The tests use the [common/network](/common/network)  API and its test configuration data to provide a transport for the sockets testing.