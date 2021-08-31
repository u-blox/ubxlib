# libfibonacci - `ubxlib` library example

This is a trivial `ubxlib` library which can do three things:
* calculate a number from the fibonacci series
* return last calculated number
* return a hello world string

It serves the purpose of showing how a `ubxlib` library can be implemented.

# File structure

`api/` - the library public header

`src/` - the library implementation

`example-app/` - platform specific example apps using the library

`platform/` - platform specific utility scripts for building the library

`Makefile` - will build the library using module `common/lib_common` library build utilities