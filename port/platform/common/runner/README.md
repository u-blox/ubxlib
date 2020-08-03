# Introduction
The files in here are used in the execution of examples and tests.

# Description
The core of `runner` is the macro `U_RUNNER_FUNCTION`.  This macro wraps every example and every test in `ubxlib` and takes advantage of a C++ feature in compilers, namely the `constructor` attribute.  The `constructor` attribute identifies a function as one that must be run during C startup, before entry into `main()`.  The `U_RUNNER_FUNCTION` macro creates a unique function name for the function it is wrapping and, during C startup, calls a registration function which puts that unique function name into a linked list which is fully populated by the time code starts to run for real.

The other `runner` functions allow the functions in the linked list to be executed, printed, sorted, etc.

By this means all the `ubxlib` examples and tests can be compiled at the same time, loaded into the list, executed and checked for correctness, without collisions of definitions of `main()` or the need for a separate set of build metadata for each example/test/platform/SDK combination.