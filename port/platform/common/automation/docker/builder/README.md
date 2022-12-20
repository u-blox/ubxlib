#`ubxlib_builder` Docker Container
The `ubxlib_builder` container is used by all the Linux machines used in the test automation for `ubxlib`. The image is based on Ubuntu 20.04 and contains all the dependencies that are required for `ubxlib` automation except the things that are handled by [u_packages](../../u_packages.yml).


# Building `ubxlib_builder` Locally
If you have installed `Docker` and `docker-compose` (for Ubuntu you can use [host_setup.sh](../host/host_setup.sh)) you can build `ubxlib_builder` by running the following command in this folder:

```sh
$ docker-compose build
```

# Running `ubxlib_builder` Locally
To run a command inside `ubxlib_builder` you use `docker-compose run`. Here is a hello world example:

```sh
$ docker-compose run --rm ubxlib_builder echo hello world
Creating builder_ubxlib_builder_run ... done
hello world
```

You can also use the container interactivly by opening a shell inside the container:

```sh
$ docker-compose run --rm ubxlib_builder sh
Creating builder_ubxlib_builder_run ... done
$ echo hello world
hello world
```

To use it with `ubxlib` you also need to mount the `ubxlib` work and home directories:

```sh
$ docker-compose run --rm -v $HOME/git/ubxlib_priv:/workdir/ubxlib -v $HOME:/home/ubxlib ubxlib_builder sh
Creating builder_ubxlib_builder_run ... done
$ inv -r ubxlib/port/platform/common/automation --list
Available tasks:

  arduino.build                    Build an Arduino based application
  arduino.check-installation       Check Arduino installation
  arduino.clean                    Remove all files for a nRF5 SDK build
  arduino.flash                    Flash an Arduino based application
  arduino.log                      Open a log terminal
  automation.build                 Build the firmware for an automation instance
  automation.export                Output the u_package environment
  automation.flash                 Flash the firmware for an automation instance
  automation.get-test-selection
...
```

# Updating the Linux Jenkins Slaves
For various reasons we are currently not using a Docker repo for handling our `ubxlib_builder`.

Instead a Jenkins job have been setup that will build the `ubxlib_builder` image for all the `ubxlib` Linux-based Jenkins slaves which can be found in the Jenkin classic mode under `Dashboard -> ubxlib -> ubxlib_docker_build`. To build the image press `Build with Parameters`, enter the branch where the new `Dockerfile` is located in the field `UBXLIB_PRIV_REV`, then press `Build`. When the images has been successfully built it will get used next time a `ubxlib` Jenkins job has been started.

NOTE: before updating the slaves it is a good idea to first try building locally as described above.
