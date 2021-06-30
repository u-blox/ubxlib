#!/bin/bash
set -e

BASEDIR=$(dirname "$0")
BRANCH=""
CMD=""

function usage()
{
    echo "Supported arguments:"
    echo "-h"
    echo "-b <ubxlib_branch>"
    echo "-c <command>"
}

while getopts "b:c:" opt; do
    case $opt in
        h)
            usage
            exit
            ;;
        b)
            BRANCH=$OPTARG
            echo "Setting branch to $BRANCH"
            ;;
        c)
            CMD=$OPTARG
            echo "Setting command to $CMD"
            ;;
        *)
            echo "ERROR: unknown parameter \"$PARAM\""
            usage
            exit 1
            ;;
    esac
done


# Check SDK installations
$BASEDIR/setup_sdks.sh $SDK_DIR

# This is necessary to prevent the "git clone" operation from failing
# with an "unknown host key" error.
if [ ! -d "$HOME/.ssh" ]; then
    mkdir -m 700 "$HOME/.ssh"
    touch -m 600 "$HOME/.ssh/known_hosts"
    ssh-add -l
    ssh-keyscan github.com > "$HOME/.ssh/known_hosts" 2>/dev/null
fi

if [ ! -d "ubxlib" ]; then
    # ubxlib directory does not exist so we clone it
    git clone git@github.com:u-blox/ubxlib_priv.git ubxlib
fi

if [ ! -z "$BRANCH" ]; then
    # The branch argument have been set so we switch branch
    if git -C ubxlib diff-index --quiet HEAD --; then
        git -C ubxlib fetch
        git -C ubxlib checkout $BRANCH
    else
        echo
        echo "************************************************************************************"
        echo "* WARNING: There are local modifications to the git repo - unable to switch branch!"
        echo "************************************************************************************"
        echo
    fi
fi

# Execute the command
echo "Executing \"$CMD\""
bash -c "$CMD"
