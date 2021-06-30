This folder contains Docker related things that is used by the test automation.
Due to software license reasons the Docker image used is only available internally at u-blox.

A short description of the folder content:
- `agent`: Things needed for creating the ubxlib agent container.
- `host`: Contains a script for setting up a new machine to run ubxlib docker containers.
- `agent_start.sh`: This script is used for starting the ubxlib agent container.

Before the agent can be started you should run the [./host/host_setup.sh](./host/host_setup.sh) script.