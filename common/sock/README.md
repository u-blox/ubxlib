# Introduction
These directories provide a user data (sockets) interface.  The API presented is a Berkeley sockets API, similar to that of LWIP, allowing the files here to replace LWIP. 

# Usage
The directories include only the API and pure C source files that make no reference to a platform or an operating system.  They rely upon the `port` directory to map to a target platform and provide the necessary build/test infrastructure for that target platform; see the relevant platform directory under `port` for build and usage information.

A usage example can be found in the `README.md` file for the network API.

# Testing
The `test` directory contains generic tests for the `sock` API. Please refer to the relevant platform directory of the `port` component for instructions on how to build and run the tests.  The tests use the network API and it's test configuration data to provide a transport for the sockets testing.