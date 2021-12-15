# u_runner Visual Studio Code workspace
This is an experimental [Visual Studio Code](https://code.visualstudio.com/download) project which is used internally at u-blox to build and run the `runner` application that executes all of our tests and examples.
The vscode workspace supports both Linux and Windows. However, in Windows there will be some limitations such as not beeing able to run the test automation using Docker container.

Note: currently building/running/debugging is only supported on the following platforms:

- `nrf5340dk` [[zephyr](/port/platform/zephyr)]
- `evkninab3` [[zephyr](/port/platform/zephyr)]
- `evbninab3` [[nrf5sdk](/port/platform/nrf5sdk)]
- `esp32` [[esp-idf](/port/platform/esp-idf)]
- `u-blox C030` [[stm32cubef4](/port/platform/stm32cube)]

## Setup
1. Make sure you have Python 3.x installed and that it is in your `PATH` environment. Also make sure that `pip3` is accessable through your `PATH` environment.
2. Run either [setup_linux.sh](setup_linux.sh) or [setup_windows.bat](setup_windows.sh) depending on your platform.
3. In vscode use `Open workspace from file` and open [ubxlib-runner.code-workspace](/ubxlib-runner.code-workspace).
4. Make sure that all the extensions recommended by the workspace are installed.

## [config.yml](config.yml)
This file is used to tell what SDK versions should be downloaded and where to install them. If you already have some SDKs installed you can point to their location. If not you will be asked for confirmation when you try to build a target if you want to download the required software.

## `u_flags.yml`
When you try to build any of the targets a `u_flags.yml` will be created in the `.vscode` directory. This can be used for setting `U_FLAGS` defines (or any C define for that matter).
You can set different flags for each boards. Here is an example that will enable EDM stream colored logging for `nrfconnect` target `runner_nrf5340dk_nrf5340_cpuapp`:

```yml
nrfconnect:
  runner_nrf5340dk_nrf5340_cpuapp:
    u_flags:
      - U_CFG_SHORT_RANGE_EDM_STREAM_DEBUG
      - U_CFG_SHORT_RANGE_EDM_STREAM_DEBUG_COLOR
```

## Tasks
We use `PyInvoke` to define tasks for building and cleaning different targets. You can list the available tasks by executing `inv --list` in the `.vscode` directory. You will find the full documentation of `PyInvoke` [here](https://docs.pyinvoke.org/en/stable/).

