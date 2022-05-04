###########################################################################
# Script used for setting up a machine to run docker
#
# This script will:
# * Install udev rules needed to access J-Link, ST-Link and FTDI devices
# * Install docker + docker-compose
###########################################################################

#!/bin/bash
set -e

# Uncomment to enable debug
#set -x

BASEDIR=$(dirname "$0")

read -p "Do you want to install udev rules for J-Link, ST-Link, etc? (y/n) " RESP
if [ "$RESP" = "y" ]; then
    echo "Setting up udev rules for debuggers and COM ports"
    sudo cp ${BASEDIR}/udev/* /etc/udev/rules.d/
fi

read -p "Do you want to install Docker? (y/n) " RESP
if [ "$RESP" = "y" ]; then
    ${BASEDIR}/scripts/install_docker.sh
    DOCKER_WAS_INSTALLED=1
fi

# THIS MUST ALWAYS BE PLACED LASTLY IN THE SCRIPT!
# newgrp will spawn a subshell so any shell commands after that point will not be executed
if [ "$DOCKER_WAS_INSTALLED" == 1 ]; then
    # Normally you need to logout in order to trigger the new group, but with this command this shouldn't be needed
    newgrp docker # Calling this here will start a sub-shell and cause any remaining commands to not execute
fi
