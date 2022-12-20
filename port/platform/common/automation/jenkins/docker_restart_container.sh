#!/bin/bash
# Restart a named Docker container
if [ -n "$1" ]; then
    container_id=$(docker ps -aqf "name=$1")
    if [ -n "$container_id" ]; then
        docker restart $container_id
        echo SMEE container, ID $container_id, restarted.
    else
        echo There is no SMEE container named \"$1\" running.
    fi
else
    echo Please supply the name of the Docker container you want to start, e.g. $0 smee-client
fi
