# u_runner Visual Studio Code workspace
This is an experimental [Visual Studio Code](https://code.visualstudio.com/download) project which is used internally at u-blox to build and run the `runner` application that executes all of our tests and examples.
The vscode workspace supports both Linux and Windows. However, in Windows there will be some limitations such as not beeing able to run the test automation using Docker container.

Note: currently building/running/debugging is only supported on the following platforms:

- `nrf5340dk` [[zephyr](/port/platform/zephyr)]
- `evkninab3` [[zephyr](/port/platform/zephyr)]
- `evbninab3` [[nrf5sdk](/port/platform/nrf5sdk)]
- `esp32` [[esp-idf](/port/platform/esp-idf)]
- `u-blox C030` [[stm32cubef4](/port/platform/stm32cube)]
- `Windows` [[windows](/port/platform/windows)]

## Setup
1. Make sure you have Python 3.x installed and that it is in your `PATH` environment. Also make sure that `pip3` is accessable through your `PATH` environment.
2. Run either [setup_linux.sh](../port/platform/common/automation/setup_linux.sh) or [setup_windows.bat](../port/platform/common/automation/setup_windows.sh) located in [port/platform/common/automation](../port/platform/common/automation) depending on your platform.
3. In vscode use `Open workspace from file` and open [ubxlib-runner.code-workspace](/ubxlib-runner.code-workspace).
4. Make sure that all the extensions recommended by the workspace are installed.

## [u_packages.yml](u_packages.yml)
This file is used to tell what toolchain packages to install. By default these packages will be placed in `${HOME}/.ubxlibpkg` for Linux and `${UserProfile}/.ubxlibpkg` for Windows. If you want to place the packages somewhere else you can use the `UBXLIB_PKG_DIR` environmental variable.
Each package has a version number and each version of a package is stored in a separate folder.
When you switch version in `u_packages.yml` the package manager will first check if the specific version can be found in the local package dir and if not the user will be prompted whether to automatically install it.

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
If you need help on how to use a certain command you can use `inv --help <task>`, example:
```
> inv --help nrf5.build

Usage: inv[oke] [--core-opts] nrf5.build [--options] [other tasks here ...]

Docstring:
  Build a nRF5 SDK based application

Options:
  -b STRING, --build-dir=STRING      Output build directory (default: _build/nrf5)
  -j INT, --jobs=INT                 The number of Makefile jobs (default: 8)
  -m STRING, --makefile-dir=STRING   Makefile project directory to build (default: /home/ante/git/ubxlib/ubxlib_priv/port/platform/nrf5sdk/mcu/nrf52/gcc/runner)
  -o STRING, --output-name=STRING    An output name (build sub folder, default: runner_ubx_evkninab3_nrf52840
  -u STRING, --u-flags=STRING        Extra u_flags (when this is specified u_flags.yml will not be used)
```
You can also show all shell commands that are executed by a task by using the `inv -e <task>`, example:
```
> inv -e nrf5.flash

=== Loading u_packages ===
Found make
Found unity v2.5.0
Found arm_embedded_gcc 10-2020-q4-major
Found nrf5sdk 15.3.0_59ac345
nrfjprog -f nrf52 --program /home/ante/git/ubxlib/ubxlib_priv/.vscode/_build/nrf5/runner_ubx_evkninab3_nrf52840/nrf52840_xxaa.hex --chiperase --verify
ERROR: There is no debugger connected to the PC.
```

