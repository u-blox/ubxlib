# Introduction
The files in here are used internally within u-blox to automate testing of `ubxlib`.  They are not supported externally.  However, if you find them useful please help yourselves.

The complete set up of the automated test system, from scratch, is described in [SETUP.md](SETUP.md); how to request access is described in [ACCESS_REQUEST.md](ACCESS_REQUEST.md).  The remainder of this document describes how to use the system and how to run the same stuff locally on your Windows/Linux PC.

# The Instances
The automated test system runs several instances all defined in [DATABASE.md](DATABASE.md). Each row in [DATABASE.md](DATABASE.md) corresponds to an instance that runs in parallel on Jenkins. The process for each instance is divided into the stages: build, flash and test. However, some of the instances only use the test stage.

Each instance (i.e. each row in [DATABASE.md](DATABASE.md) or each parallel test) has an ID with format `x.y.z` where x-z are integer values. `x` is mandatory while `y` and `z` are optional. The `x` is used for referring to a specific HW configuration and `y` & `z` are used to for defining variants of tests performed on the same HW config.

Example:
11.0: ESP32-DevKitC HW, build, flash and test `ubxlib` for ESP-IDF
11.1: ESP32-DevKitC HW (i.e. same HW as above), build, flash and test `ubxlib` for Arduino

i.e. 11 in the above example identifies the HW and `.0` & `.1` are used for testing `ubxlib` built for both ESP-IDF and Arduino on the same HW.
When running on Jenkins the test system will run these two test instances in sequence if there is only one instance of `11` HW available.

**NOTE:** it is important to note that the values set for the instances in [DATABASE.md](DATABASE.md) are used as input to the build, flash and test stage.

# Reading The Jenkins Output
Whenever a push is made to the `ubxlib_priv` repository, Jenkins will be triggered to test it.

You can follow the testing in real-time on Jenkins. You will get a nice overview in the BlueOcean UI:

![Pipeline](/readme_images/pipeline.png)

If there is a failure the stage will be marked red, or if there is a timeout it will be marked grey; in this case click on that stage and expand the script outputs at the bottom. For all stages except the `test` stage the output there should be all you need to identify the problem. For the `test` stage the output may look something like this:

![Test output](/readme_images/test_output.png)

To understand why the test failed you will likely need the debug output from the target. For the `test` stage test outcome together with the debug log from the target are stored in JUnit XML reports parsed by Jenkins. In this way you can use the `Test` tab in BlueOcean to view the failed test to find the debug output in the `Standard Output` section:

![Test tab](/readme_images/test_tab.png)

You can also find the full target log for the specific instance under the `Artifacts` tab: `_jenkins_work/<instance>/debug.log`.

# Local Installation
To run the test automation or other PyInvoke tasks locally you need to setup a few things. The following steps should work for both Windows and Ubuntu:
1. Make sure you have Python 3.x installed and that it is in your `PATH` environment.
2. Make sure that `pip3` is accessable through your `PATH` environment.
3. For Windows make sure that your `<python3_dir>/Scripts` is accessable through your `PATH` environment.
4. Run either [setup_linux.sh](setup_linux.sh) or [setup_windows.bat](setup_windows.bat) depending on your platform. These scripts will install all Python modules needed for the test automation. The modules we are using are listed in [requirements.txt](requirements.txt)
5. Verify that you can execute the command `invoke` from your terminal. If the command is not found, for Windows double check step 3. For Ubuntu just logout and login again (Python modules are placed in `~/.local/bin` and this directory is only added to the `PATH` if it existed when the user logged in).

**NOTE** You may need to re-run `setup_windows.bat`/`setup_linux.sh` when the test automation is upgraded as new Python modules may be added.

# PyInvoke Tasks
A central part in the test automation are the [PyInvoke](https://www.pyinvoke.org/) tasks. You can view each PyInvoke task as a shell command that in our case executes a step in the test automation. The PyInvoke tasks are located in the [port/platform/common/automation/tasks](./tasks/) directory. To execute a PyInvoke task you use the [invoke](https://docs.pyinvoke.org/en/stable/invoke.html) (or `inv`) command. This command will look for a `tasks` directory in the current working directory so to execute our automation task you either need to change working directory to `port/platform/common/automation` or use the [-r](https://docs.pyinvoke.org/en/stable/invoke.html#cmdoption-r) flag to specify the automation directory to `invoke`.

## Using PyInvoke
Here is a basic guide how to use the `invoke` (/`inv`) commands.

### List PyInvoke Tasks
```sh
inv -r <ubxlibdir>/port/platform/common/automation --list
Available tasks:

  arduino.build                    Build an Arduino based application
  arduino.check-installation       Check Arduino installation
  arduino.clean                    Remove all files for a nRF5 SDK build
  arduino.flash                    Flash an Arduino based application
  arduino.log                      Open a log terminal
  automation.build                 Build the firmware for an automation instance
  automation.export                Output the u_package environment
  automation.flash                 Flash the firmware for an automation instance
  ...

# OR if you already in the automation directory:
inv --list
```

### Show Help For A PyInvoke Tasks
```sh
inv -r <ubxlibdir>/port/platform/common/automation --help automation.build
Usage: inv[oke] [--core-opts] automation.build [--options] [other tasks here ...]

Docstring:
  Build the firmware for an automation instance

Options:
  -b STRING, --build-dir=STRING
  -f STRING, --filter=STRING
  -i STRING, --instance=STRING
# OR if you already in the automation directory:
inv --help automation.build
```

### Execute A PyInvoke Task
```sh
inv -r <ubxlibdir>/port/platform/common/automation automation.build 12.0
=== Loading u_packages ===
Found esp_idf v4.3.2
Setting IDF_PATH to '/home/anan/.ubxlibpkg/esp_idf-v4.3.2'
Detecting the Python interpreter
Checking "python" ...
Python 3.8.10
"python" has been detected
Adding ESP-IDF tools to PATH...
Using Python interpreter in /home/anan/.ubxlibpkg/esp_idf_tools-v4.3.2/python_env/idf4.3_py3.8_env/bin/python
Checking if Python packages are up to date...
Python requirements from /home/anan/.ubxlibpkg/esp_idf-v4.3.2/requirements.txt are satisfied.
Added the following directories to PATH:
  /home/anan/.ubxlibpkg/esp_idf-v4.3.2/components/esptool_py/esptool
  /home/anan/.ubxlibpkg/esp_idf-v4.3.2/components/espcoredump
  ...
# OR if you already in the automation directory:
inv --help automation.build 12.0
```

### Shell Tab Completion For Invoke Command
Since the PyInvoke task name are quite long, it can be convenient to enable shell tab completion.
The `invoke` command is already prepared for this, but you need to do some steps to enable it:
1. Generate the needed shell script for your shell by using executing `invoke --print-completion-script <name of your shell> > ~/.invoke-completion.sh`.
I.e. for bash you would run:
```
invoke --print-completion-script bash > ~/.invoke-completion.sh
```
2. You need to source the script generated in step 1 at startup of each shell session. For bash you can do this by adding the following line to `~/.bashrc`:
```
source ~/.invoke-completion.sh
```

## `automation` Tasks
The Jenkins pipeline will only use the `automation` PyInvoke tasks. The flow in Jenkins is:
1. Decide what instances and tests to run by calling `automation.get-test-selection`.
2. For each instance from step 1 call `automation.build --filter=<test_filter> <instance>` to build the firmware.
3. For each instance from step 1 call `automation.flash <instance>` to flash the firmware.
4. For each instance from step 1 call `automation.test --filter=<test_filter> <instance>` to start the tests.

So if you need to run the test automation locally you can invoke `automation.build`, `automation.flash` and/or `automation.test` with the instance ID as argument.
As default all tests will be executed, but if you only want to run specific test you can use the `--filter` flag.

The `automation` tasks works as an abstract layer to the platform (i.e. `arduino.<command>`, `nrf5.<command>`, ...) tasks. This means that when you call `automation.build 12` the task will check [DATABASE.md](DATABASE.md) to find what platform instance 12 is. In this case it will be `ESP-IDF` so then the `automation.build` task will in turn call `esp-idf.build` to build the firmware.

# u_packages
When you run the PyInvoke tasks one of the first things that will happen is to check if the required toolchains are installed. If they are not, the required toolchains will automatically downloaded and installed. This is done by our u_packages module (see [scripts/packages](scripts/packages)). The input to this module is the [u_packages.yml](u_packages.yml) described below.

## [u_packages.yml](u_packages.yml)
This file is used to tell what toolchain packages to install. By default these packages will be placed in `${HOME}/.ubxlibpkg` for Linux and `${UserProfile}/.ubxlibpkg` for Windows. If you want to place the packages somewhere else you can use the `UBXLIB_PKG_DIR` environmental variable.
Each package has a version number and each version of a package is stored in a separate folder.
When you switch version in `u_packages.yml` the package manager will first check if the specific version can be found in the local package dir and if not the user will be prompted whether to automatically install it.

# CodeChecker
[CodeChecker](https://github.com/Ericsson/codechecker) is a frame work for static code analysis using [Clang Static Analyzer](https://clang-analyzer.llvm.org/). We currently support running CodeChecker for STM32Cube and nRFConnect platforms.

In `DATABASE.md` the CodeChecker is run by adding `CodeChecker:` prefix to the `Platform` field. All defines and configurations will be regarded exactly like building a firmware for the specific platform.

If you are running CodeChecker locally you need to `pip3 install codechecker` and, on Windows, install at least [LLVM version 14](https://github.com/llvm/llvm-project/releases/tag/llvmorg-14.0.0), adding the `bin` directory to your path.

## Reports
As a general rule Clang Static Analyzer rarely gives you false positive so look closely at the feedback. Clang-tidy, on the other hand, is a Linter so there you will see some false positives.

For each report you can get some extra help by clicking on the `Checker name`:
![Checker](/readme_images/codechecker_checker_name.png)

There you will also see if the report came from Clang-tidy (https://clang.llvm.org/extra/clang-tidy/checks/) or Clang Static Analyzer (https://clang.llvm.org/docs/) which is relvant if you need to suppress a warning.

## Suppressing Clang-tidy Warning
If the report generated by Clang-tidy (see above how to identify), you can suppress warnings according to this documentation:
https://clang.llvm.org/extra/clang-tidy/#suppressing-undesired-diagnostics
If you want to completely turn off a specific checker you can do this by adding `- --disable=<name_of_checker>` to [port/platform/common/automation/cfg/codechecker/codechecker.yml](./cfg/codechecker/codechecker.yml).

## Suppressing Clang Static Analyzer Warning
This can only be done by the normal compiler ways. Please see Clang Static Analyzer [FAQ](https://clang-analyzer.llvm.org/faq.html).

# Running A Specified Test Instance
As described in the previous section Jenkins uses the `automation.<command>` PyInvoke tasks. These tasks can be run locally and this is described in the [`automation` Tasks](#automation-tasks) section.

**NOTE:** If you want to use hardware dependent task such as `automation.flash` you will likely need to adjust the files in `$HOME/.ubx_automation`.
In this directory you will find `settings_v2_agent_specific.json` which among other things contains a list of COM-ports and debugger serial to use for each instance.
The file will automatically created with default value if it doesn't exist. In this case the settings will use a `_FIX_ME` postfix.
To get local testing working you will need to adjust the `CONNECTION_INSTANCE_<instance>_FIX_ME` for the instance you want to run.
When you have configured COM port and/or debugger serial number you should remove the `_FIX_ME` postfix.
For instance, to run instance 16 locally you might open that file and change:

```
  "CONNECTION_INSTANCE_16_FIX_ME": {
    "serial_port": "COM7",
    "debugger": "683920969"
  }
```

...to:

```
  "CONNECTION_INSTANCE_16": {
    "serial_port": "COM3",
    "debugger": None
  }
```

By setting `debugger` to `None`, the script will simply pick the one and only connected board. Should there be multiple boards connected to the PC, one must specify the correct serial number for the debugger.

# Jenkins Test Selection
As a first step in the Jenkins pipeline there is a script that decides what instances and tests suites to run. This is handled by the `automation.get-test-selection` PyInvoke task. This task returns a JSON struct with a list of instances to run and a test filter. The input to this script is the last commit message and what files have been modified like (for more details on how to use PyInvoke tasks see [PyInvoke Tasks](#pyinvoke-tasks) section):

`invoke automation.get-test-selection --message="some text" --files="<a list of file paths, unquoted, separated with spaces>"`

The "some text" is intended to be the text of the last commit message.  When `automation.get-test-selection` is called from Jenkins, using [Jenkinsfile](Jenkinsfile), the most recent commit text is grabbed by the [Jenkinsfile](Jenkinsfile) script.  This text is parsed for a line that starts with `test: foo bar`, where `foo` is either `*` or a series of digits separated by spaces or full stops, e.g. `0` `0 1` or `1.1` or `4.0.2 6.1.3` and `bar` is some more optional text, so for example `test: 1 example` or `test: 1 exampleSec`.  `foo` indicates the instance IDs to run from [DATABASE.md](DATABASE.md), separated by spaces, and `bar` is a filter string, indicating that only examples/tests that begin with that string should be run; note that this is the name used in the test definition, the second parameter to `U_PORT_TEST_FUNCTION`, it is not the name of the sub-directory the file is in (e.g. to select tests of the AT client use `atClient` not `at_client`).  For instance `test: 1 2 3 example` would run all things that begin with `example` on instance IDs `1`, `2` and `3`, or `test: * port` would run all things that begin with `port` (i.e. the porting tests) on **all** instances, `test: * exampleSec` would run all things that begin with `exampleSec` (i.e. all of the security examples) or `test: *` would run everything on all instance IDs, etc.  Please make sure you use the **exact** syntax (e.g. don't add commas between the instances or a full stop on the end or quotation marks etc.) as it is strictly applied in order to avoid accidentally picking up lines not intended as test directives.  Alternatively `test: None` can be specified to stop any tests being run e.g. because the change affects the automation code itself and could bring the test system tumbling down, or if the code is a very early version and there is no point in wasting valuable test time.  `bar` can contain multiple filter strings; simply separate them with a full stop `.` (but no spaces), for instance `exampleSec.exampleSocketsTls`; think of the full stop as an "or".

So, when submitting a branch the u-blox Jenkins configuration will parse the commit text to find a `test:` line and conduct those tests on the branch, returning the results to Github.

If the commit text does *not* contain a line starting with `test:` (the usual case) then the file list must be provided.  Again, when `automation.get-test-selection` is called from Jenkins, [Jenkinsfile](Jenkinsfile), will determine the likely base branch (`master` or `development`) using [u_get_likely_base_branch.py](./scripts/u_get_likely_base_branch.py), grab the list of changed files between this base and the branch from Github.  `automation.get-test-selection` then calls the [u_select.py](./scripts/u_select.py) script to determine what tests should be run on which instance IDs to verify that the branch is good.  See the comments in [u_select.py](./scripts/u_select.py) to determine how it does this.  It is worth noting that the list of tests/instances selected by [u_select.py](./scripts/u_select.py) will always be the largest one: e.g. if a file in the `ble` directory has been changed then all the instances in [DATABASE.md](DATABASE.md) that have `ble` in their "APIs available" column will be selected.  To narrow the tests/instances that are run further, use the `test:` line in your most recent commit message to specify it yourself.

# Script Descriptions
[Jenkinsfile](Jenkinsfile): tells Jenkins what to do, written in Groovy stages.  Key point to note is that the archived files will be `summary.txt` for a quick overview, `test_report.xml` for an XML formatted report on any tests that are executed on the target HW and `debug.txt` for the full detailed debug on each build/download/run.

[scripts/u_connection.py](./scripts/u_connection.py): contains a record of how each item of HW is connected into the test system (COM port, debugger serial number, etc. collected from the settings) and functions to access this information.  All of the connections are retrieved-from/stored-in the settings file (see below).

[scripts/u_data.py](u_data.py): functions to parse [DATABASE.md](DATABASE.md).

[scripts/u_monitor.py](u_monitor.py): monitors the output from an instance that is executing tests, checking the output for passes/failures and optionally writing debug logs and an XML report to file. To run separatly call `python -m scripts.u_monitor` from the `automation` directory.

[scripts/u_report.py](./scripts/u_report.py): write reports on progress.

[scripts/u_settings.py](./scripts/u_settings.py): stores and retrieves the paths etc. used by the various scripts.  The settings files are stored in a directory named `.ubx_automation` off the current user's home directory and are called `settings_v2.json` for general stuff where the defaults are usually fine and `settings_v2_agent_specific.json` for things which you must usually configure yourself.  If no settings files exists default ones are first written with `_FIX_ME` added to the values in `settings_v2_agent_specific.json` which you must set: simply edit the value as necessary and then remove the `_FIX_ME` from the name.  If you **modify** an existing setting in [u_settings.py](./scripts/u_settings.py) it will not have any effect since the values that apply are those read from the settings files; either edit the values in the settings files or, if you don't have access to them (e.g. because you are running on a test agent via the automated test system) temporarily add a value to [u_settings.py](./scripts/u_settings.py) with the post-fix `_TEST_ONLY_TEMP`; e.g. to temporarily change the value of `JLINK_SWO_PORT` you would add an entry `JLINK_SWO_PORT_TEST_ONLY_TEMP` and set it to your desired value.  Obviously don't check such a change into `master`, this is purely for testing on your branch without affecting anyone else or changing the values written to the settings file.  This works as an override, so in the example `JLINK_SWO_PORT` would be left exactly as it is and the value used would be that from `JLINK_SWO_PORT_TEST_ONLY_TEMP`.  More details on the ins-and-outs of settings can be found in [u_settings.py](./scripts/u_settings.py) itself.

[scripts/u_run_astyle.py](./scripts/u_run_astyle.py): run an AStyle check; called by `automation.test` PyInvoke task.  To AStyle your files before submission, install [AStyle](http://astyle.sourceforge.net/) version 3.1 and, from the `ubxlib` root directory, run the Python script [astyle.py](/astyle.py).

[scripts/u_run_doxygen.py](./scripts/u_run_doxygen.py): run Doxygen to check that there are no documentation errors; called by `automation.test` PyInvoke task.

[scripts/u_run_static_size.py](./scripts/u_run_static_size.py): run a static check of code/RAM size; called by called by `automation.test` PyInvoke task.

[scripts/u_run_no_floating_point.py](./scripts/u_run_no_floating_point.py): run a static size check without floating point support, checking that no floating point has crept in; called by [u_run.py](u_run.py).

[scripts/u_run_pylint.py](./scripts/u_run_pylint.py): run Pylint on all of these Python automation scripts; called by `automation.test` PyInvoke task.

[scripts/u_run_check_ubxlib_h.py](./scripts/u_run_check_ubxlib_h.py): check that no API header files have been missed out of `ubxlib.h`.

[scripts/u_run_windows.py](./scripts/u_run_windows.py): build and run on Windows; called by `automation.test` PyInvoke task.

[scripts/u_run_linux.py](./scripts/u_run_linux.py): build and run on Linux; called by `automation.test` PyInvoke task.  In particular, if you install the Linux `socat` utility this script will automatically handle mapping of the `/dev/pts/x` UARTs of the `ubxlib` Linux application to real Linux devices.  For instance, to make `UART_0` the `U_CFG_TEST_UART_A` loop-back UART, used by the porting tests, then simply pass the \#define `U_CFG_TEST_UART_A=0` into the build. Similarly, to make `UART_1` the `U_CFG_TEST_UART_B` loop-back UART (used for the scenario in the AT command and chip-to-chip security tests where `U_CFG_TEST_UART_A` is looped back to `U_CFG_TEST_UART_B`) then you would also pass \#define `U_CFG_TEST_UART_B=1` into the build.  And finally, if you have a real module connected to a real device on Linux, let's say a cellular module on `/dev/tty/5`, and you want to connect it to `ubxlib` as `UART_1`, then as well as passing the \#define `U_CFG_APP_CELL_UART=1` into the build you would also pass `U_CFG_APP_CELL_UART_DEV=/dev/tty/5`.  The \#defines `U_CFG_APP_GNSS_UART` and `U_CFG_APP_SHORT_RANGE_UART` can be used similarly.

[scripts/u_select.py](./scripts/u_select.py): see above.

[scripts/u_utils.py](./scripts/u_utils.py): utility functions used by all of the above.

[scripts/u_get_likely_base_branch.py](./scripts/u_get_likely_base_branch.py): script used for guessing the base branch for a commit. This is used by `Jenkinsfile` to determine if the base branch is `development` or `master`.

[scripts/u_get_arm_toolchain.py](./scripts/u_get_arm_toolchain.py): script used by vscode to get the ARM toolchain path via `u_packages`.

# Maintenance
- If you add a new API make sure that it is listed in the `APIs available` column of at least one row in [DATABASE.md](DATABASE.md), otherwise [u_select.py](./scripts/u_select.py) will **not**  select it for automated-testing of a branch.
- If you add a new board to the test system, update [u_connection.py](./scripts/u_connection.py) to include it.
- If you add a new platform or test suite, add it to [DATABASE.md](DATABASE.md) and make sure that the result is parsed correctly by [u_data.py](./scripts/u_data.py) (e.g. by running `automation.<command>` PyInvoke tasks) from the command-line and checking that everything is correct).
- If you add a new item in the range 0 to 9 (i.e. a checker with no platform), update [automation.py](./tasks/automation.py) to include it.
- If you add a new directory OFF THE ROOT of `ubxlib`, i.e. something like `ubxlib/blah`, add it to the `ASTYLE_DIRS` variable of the [u_run_astyle.py](./scripts/u_run_astyle.py) script.