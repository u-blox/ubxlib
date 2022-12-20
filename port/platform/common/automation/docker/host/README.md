# Docker Host Setup: UBUNTU ONLY, NO LONGER USED
The files here are used for preparing a Ubuntu machine for running `ubxlib_builder` docker container.  They are NO LONGER USED in the test system, but are kepy here in case they are of use in the future.

Simply run `host_setup.sh`:

```sh
$ cd ~/ubxlib/port/platform/common/automation/docker/host
$ ./host_setup.sh
```

This script will:
* Install udev rules needed to access J-Link, ST-Link and FTDI devices
* Install docker + docker-compose

