#!/bin/bash

###########################################################################
# This will setup packages needed for ubxlib
###########################################################################

set -e

sudo apt install make python3 python3-pip
pip3 install -r ../port/platform/common/automation/requirements.txt