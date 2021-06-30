This folder contains Docker related things that is needed for creating the ubxlib agent container used by the test automation.

A short description of the folder content:
- `scripts`: The entry point scripts that will run when the agent container is started/re-started
- `settings`: The default ubxlib test automation settings. These settings will be copied to host folder `~/ubxagent/.ubx_automation` when the container is started for the first time. After this point you need to configure J-Link serial numbers etc in `~/ubxagent/.ubx_automation/settings_v2_agent_specific.json`.
- `docker-compose.yml`: The docker compose file used for the ubxlib agent container. Please note that the container should be started using [../agent_start.sh](../start_agent.sh)
