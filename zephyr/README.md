# Why This Directory Is Here
This directory is added to permit Zephyr users to include `ubxlib` as a [Zephyr module](https://docs.zephyrproject.org/latest/guides/modules.html); that is all it does, nothing else.

Please look in the [port/platform/zephyr](/port/platform/zephyr) directory for all the actual Zephyr platform support stuff.

The easiest way though is to just include the file `ubxlib.cmake` in this directory to the applications `CMakeLists.txt`.
This is the only thing required to get full access to `ubxlib`.

Doing that will also by default implicitly setup suitable configuration variables for `ubxlib`. If that is not wanted then the CMake variable UBXLIB_NO_DEF_CONF must be defined before the inclusion.

Please note that it is also possible to override the default configuration variables in the application `prj.conf` file.
