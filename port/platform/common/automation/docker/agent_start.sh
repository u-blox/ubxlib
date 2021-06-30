###########################################################################
# Startup script for ubxlib agent
#
# This script is used to startup the ubxlib agent docker container.
# Before you start it you need to run host/host_setup.sh first. This
# will setup docker and "ubx" user on your host
###########################################################################

#!/bin/bash
set -e

# Uncomment to enable debug
#set -x

BASEDIR=$(dirname "$0")

# The SDKs will be placed in a bind mounted volume in the host dir below
export HOST_SDK_DIR="$HOME/sdks"

# The home directory for the ubx user inside the container will also be
# bind mounted. This is used for storing authorized public SSH keys and
# ubxlib automation settings etc
export HOST_AGENT_HOME_DIR="$HOME/ubxagent"

# ubxlib host location
export HOST_UBXLIB_DIR="$BASEDIR/../../../../../.."

# Forward the hostname of this machine to the docker container
export HOSTNAME=`hostname`

# This is the folder where ubxlib settings are stored
HOST_UBX_AUTOMATION_DIR="$HOST_AGENT_HOME_DIR/.ubx_automation"

if [ ! -d "$HOST_SDK_DIR" ] ; then
    echo "Creating $HOST_SDK_DIR"
    mkdir -p "$HOST_SDK_DIR"
fi

if [ ! -d "$HOST_UBX_AUTOMATION_DIR" ] ; then
    echo "Creating $HOST_UBX_AUTOMATION_DIR"
    mkdir -p "$HOST_UBX_AUTOMATION_DIR"
    # Copy the default settings
    cp $BASEDIR/agent/settings/* "$HOST_UBX_AUTOMATION_DIR"
fi

# The entrypoint script can switch branch when the container is started
# This can be used as a fallback mechanism
export ENTRY_UBXLIB_BRANCH="master"

# The command to execute when the container has started
export ENTRY_COMMAND="cd /workdir/ubxlib/port/platform/common/automation && python u_agent_service.py -u /opt/sdks/unity/ -n $HOSTNAME -d /workdir/debug.txt /workdir 1 2 3 4 5"

# Start the ubxlib_agent
docker-compose -f "$BASEDIR/agent/docker-compose.yml" up ubxlib_agent