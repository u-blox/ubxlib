#!/bin/bash

###########################################################################
# This will setup packages needed for ubxlib
###########################################################################

set -e

# Get the script directory
SCRIPT_DIR=$(dirname "$0")

echo "Update APT package cache..."
sudo apt update
echo ""
echo "Install basic stuff needed for ubxlib"
sudo apt install make python3 python3-pip doxygen graphviz astyle libncurses5 socat libasan5
echo ""
echo "Install Zephyr stuff"
sudo apt install --no-install-recommends git cmake ninja-build gperf \
  ccache dfu-util device-tree-compiler wget \
  python3-dev python3-pip python3-setuptools python3-tk python3-wheel xz-utils file \
  make gcc gcc-multilib g++-multilib libsdl2-dev

echo ""
echo "Install Python modules needed for ubxlib"
pip3 install -r $SCRIPT_DIR/requirements.txt

echo ""
read -p "Do you want to set python3 as default python command? (y/n) " RESP
if [ "$RESP" = "y" ]; then
    sudo update-alternatives --install /usr/bin/python python /usr/bin/python3 1
fi

echo ""
read -p "Do you want to install Zephyr python modules? (y/n) " RESP
if [ "$RESP" = "y" ]; then
    pip install -r https://raw.githubusercontent.com/zephyrproject-rtos/zephyr/main/scripts/requirements.txt
fi