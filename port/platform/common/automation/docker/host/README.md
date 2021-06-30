Agent Host Setup
================
The files here are used for preparing a Ubuntu machine for running ubxlib agent docker container.
The steps needed are:
1. Install Ubuntu
2. Generate a new SSH key using:
    ```
    $ ssh-keygen -t ed25519 -C "dummy@u-blox.com" -N ""
    ```
3. Add the generated key to the ssh-agent:
    ```
    $ ssh-add ~/.ssh/id_ed25519
    ```
4. Add the public key `~/.ssh/id_ed25519.pub` as a *readonly* *deploy* key here:<br>
https://github.com/u-blox/ubxlib_priv/settings/keys
5. Clone the ubxlib repo preferably to your home folder:
    ```
    $ git clone git@github.com:u-blox/ubxlib_priv.git ~/ubxlib
    ```
6. Now the rest can be setup using `host_setup.sh`:
    ```
    $ cd ~/ubxlib/port/platform/common/automation/docker/host
    $ ./host_setup.sh
    ```
    This script will:
    * Install udev rules needed to access J-Link, ST-Link and FTDI devices
    * Install docker + docker-compose
    * Install SSH server

