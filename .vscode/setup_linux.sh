#!/bin/bash

###########################################################################
# This will setup pakages needed for ubxlib
###########################################################################

set -e

sudo apt install make python3 python3-pip
pip3 install -r requirements.txt