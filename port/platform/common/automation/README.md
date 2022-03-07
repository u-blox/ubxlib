# Introduction
The files in here are used internally within u-blox to automate testing of `ubxlib`.  They are not supported externally.  However, if you find them useful please help yourselves.

# The Test Instances
The automated testystem runs several test instances all defined in [DATABASE.md](DATABASE.md). Each row in [DATABASE.md](DATABASE.md) corresponds to an instance that runs in parallel on Jenkins. The test process for each instance is devided into the stages: build, flash and test. However, some of the instances only use the test stage.

Each test instance (i.e. each row in [DATABASE.md](DATABASE.md) or each parallel test) has an ID with format `x.y.z` where x-z are integer values. `x` is mandatory while `y` and `z` are optional. The `x` is used for referring to a specific HW configuration and `y` & `z` are used to for defining variants of tests performed on the same HW config.

Example:
11.0: ESP32-DevKitC HW, Build, flash and test ubxlib for ESP-IDF
11.1: ESP32-DevKitC HW (i.e. same HW as above), Build, flash and test ubxlib for Arduino

I.e. 11 in the above example identifies the HW and `.0` & `.1` are used for testing ubxlib built for both ESP-IDF and Arduino on the same HW.
When running on Jenkins the test system will run these two test instances in sequence if there are not another `11` HW available.

**NOTE:** It is important to note that the values set for the instances in [DATABASE.md](DATABASE.md) are used as input to the build, flash and test stage.


# Reading The Jenkins Output
You can follow the testing in realtime on Jenkins. You will get a nice overview in the BlueOcean UI:

![Pipeline](/readme_images/pipeline.png)

If there is a failure the stage will be marked red and in this case click on that stage and expand the script outputs at the bottom. For all stages except the `test` stage the output there should be all you need to identify the problem. For the `test` stage the output may look something like this:

![Test output](/readme_images/test_output.png)

To understand why the test failed you will likely need the debug output from the target. For the `test` stage test outcome together with the debug log from the target are stored in JUnit XML reports parsed by Jenkins. In this way you can use the `Test` tab in BlueOcean to view the failed test to find the debug output in the `Standard Output` section:

![Test tab](/readme_images/test_tab.png)

You can also find the full target log for the specific instance under the `Artifacts` tab: `_jenkins_work/<instance>/debug.log`.

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

### Show Help for a PyInvoke Tasks
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

### Execute a PyInvoke Tasks
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

## `automation` Tasks
The Jenkins pipeline will only use the `automation` PyInvoke tasks. The flow in Jenkins is:
1. Decide what instances and tests to run by calling `automation.get-test-selection`.
2. For each instance from step 1 call `automation.build --filter=<test_filter> <instance>` to build the firmware.
3. For each instance from step 1 call `automation.flash <instance>` to flash the firmware.
4. For each instance from step 1 call `automation.test --filter=<test_filter> <instance>` to start the tests.

So if you need to run the test automation locally you can invoke `automation.build`, `automation.flash` and/or `automation.test` with the instance ID as argument.
As default all tests will be executed, but if you only want to run specific test you can use the `--filter` flag.

The `automation` tasks works as an abstract layer to the platform (i.e. `arduino.<command>`, `nrf5.<command>`, ...) tasks.
This means that when you call `automation.build 12` the task will check [DATABASE.md](DATABASE.md) to find what platform instance 12 is. In this case it will be `ESP-IDF` so then the `automation.build` task will in turn call `esp-idf.build` to build the firmware.

# Running Tests Locally
As described in the previous section Jenkins uses the `automation.<command>` PyInvoke tasks. These tasks can be run locally and this is described in [`automation` Tasks](#automation-tasks) section.

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

By setting `debugger` to `None`, the script will simply pick the one and only connected board. Would there be multiple boards connected to the PC, one must specify corresponding serial number for debugger.

# Jenkins Test Selection

As a first step in the Jenkins pipeline there is a script that decides what instances and tests suites to run. This is handled by the `automation.get-test-selection` PyInvoke task. This task returns a JSON struct with a list of instances to run and a test filter. The input to this script is the last commit message and what files have been modified like (for more details on how to use PyInvoke tasks see [PyInvoke Tasks](#pyinvoke-tasks) section):

`invoke automation.get-test-selection --message="some text" --files="<a list of file paths, unquoted, separated with spaces>"`

The "some text" is intended to be the text of the last commit message.  When `automation.get-test-selection` is called from Jenkins, using [Jenkinsfile](Jenkinsfile), the most recent commit text is grabbed by the [Jenkinsfile](Jenkinsfile) script.  This text is parsed for a line that starts with `test: foo bar`, where `foo` is either `*` or a series of digits separated by spaces or full stops, e.g. `0` `0 1` or `1.1` or `4.0.2 6.1.3` and `bar` is some more optional text, so for example `test: 1 example` or `test: 1 exampleSec`.  `foo` indicates the instance IDs to run from [DATABASE.md](DATABASE.md), separated by spaces, and `bar` is a filter string, indicating that only examples/tests that begin with that string should be run; note that this is the name used in the test definition, the second parameter to `U_PORT_TEST_FUNCTION`, it is not the name of the sub-directory the file is in (e.g. to select tests of the AT client use `atClient` not `at_client`).  For instance `test: 1 2 3 example` would run all things that begin with `example` on instance IDs `1`, `2` and `3`, or `test: * port` would run all things that begin with `port` (i.e. the porting tests) on **all** instances, `test: * exampleSec` would run all things that begin with `exampleSec` (i.e. all of the security examples) or `test: *` would run everything on all instance IDs, etc.  Please make sure you use the **exact** syntax (e.g. don't add commas between the instances or a full stop on the end or quotation marks etc.) as it is strictly applied in order to avoid accidentally picking up lines not intended as test directives.  Alternatively `test: None` can be specified to stop any tests being run e.g. because the change affects the automation code itself and could bring the test system tumbling down, or if the code is a very early version and there is no point in wasting valuable test time.  `bar` can contain multiple filter strings; simply separate them with a full stop `.` (but no spaces), for instance `exampleSec.exampleSocketsTls`; think of the full stop as an "or".

So, when submitting a branch the u-blox Jenkins configuration will parse the commit text to find a `test:` line and conduct those tests on the branch, returning the results to Github.

If the commit text does *not* contain a line starting with `test:` (the usual case) then the file list must be provided.  Again, when `automation.get-test-selection` is called from Jenkins, [Jenkinsfile](Jenkinsfile), will determine the likely base branch (`master` or `sho_master`) using [u_get_likely_base_branch.py](./scripts/u_get_likely_base_branch.py), grab the list of changed files between this base and the branch from Github.  `automation.get-test-selection` then calls the [u_select.py](./scripts/u_select.py) script to determine what tests should be run on which instance IDs to verify that the branch is good.  See the comments in [u_select.py](./scripts/u_select.py) to determine how it does this.  It is worth noting that the list of tests/instances selected by [u_select.py](./scripts/u_select.py) will always be the largest one: e.g. if a file in the `ble` directory has been changed then all the instances in [DATABASE.md](DATABASE.md) that have `ble` in their "APIs available" column will be selected.  To narrow the tests/instances that are run further, use the `test:` line in your most recent commit message to specify it yourself.

# Script Descriptions
[Jenkinsfile](Jenkinsfile): tells Jenkins what to do, written in Groovy stages.  Key point to note is that the archived files will be `summary.txt` for a quick overview, `test_report.xml` for an XML formatted report on any tests that are executed on the target HW and `debug.txt` for the full detailed debug on each build/download/run.

[scripts/u_connection.py](./scripts/u_connection.py): contains a record of how each item of HW is connected into the test system (COM port, debugger serial number, etc. collected from the settings) and functions to access this information.  All of the connections are retrieved-from/stored-in the settings file (see below).

[scripts/u_data.py](u_data.py): functions to parse [DATABASE.md](DATABASE.md).

[scripts/u_monitor.py](u_monitor.py): monitors the output from an instance that is executing tests, checking the output for passes/failures and optionally writing debug logs and an XML report to file. To run separatly call `python -m scripts.u_monitor` from the `automation` directory.

[scripts/u_report.py](./scripts/u_report.py): write reports on progress.

[scripts/u_settings.py](./scripts/u_settings.py): stores and retrieves the paths etc. used by the various scripts.  The settings files are stored in a directory named `.ubx_automation` off the current user's home directory and are called `settings_v2.json` for general stuff where the defaults are usually fine and `settings_v2_agent_specific.json` for things which you must usually configure yourself.  If no settings files exists default ones are first written with `_FIX_ME` added to the values in `settings_v2_agent_specific.json` which you must set: simply edit the value as necessary and then remove the `_FIX_ME` from the name.  If you **modify** an existing setting in [u_settings.py](./scripts/u_settings.py) it will not have any effect since the values that apply are those read from the settings files; either edit the values in the settings files or, if you don't have access to them (e.g. because you are running on a test agent via the automated test system) temporarily add a value to [u_settings.py](./scripts/u_settings.py) with the post-fix `_TEST_ONLY_TEMP`; e.g. to temporarily change the value of `JLINK_SWO_PORT` you would add an entry `JLINK_SWO_PORT_TEST_ONLY_TEMP` and set it to your desired value.  Obviously don't check such a change into `master`, this is purely for testing on your branch without affecting anyone else or changing the values written to the settings file.  This works as an override, so in the example `JLINK_SWO_PORT` would be left exactly as it is and the value used would be that from `JLINK_SWO_PORT_TEST_ONLY_TEMP`.  More details on the ins-and-outs of settings can be found in [u_settings.py](./scripts/u_settings.py) itself.

[scripts/u_run_astyle.py](./scripts/u_run_astyle.py): run an advisory-only (i.e. no error will be thrown ever) AStyle check; called by `automation.test` PyInvoke task.  To AStyle your files before submission, install [AStyle](http://astyle.sourceforge.net/) version 3.1 and, from the `ubxlib` root directory, run:

```
astyle --options=astyle.cfg --suffix=none --verbose --errors-to-stdout --recursive *.c,*.h,*.cpp,*.hpp
```

[scripts/u_run_doxygen.py](./scripts/u_run_doxygen.py): run Doxygen to check that there are no documentation errors; called by `automation.test` PyInvoke task.

[scripts/u_run_lint.py](u_run_lint.py): run a Lint check; called by called by `automation.test` PyInvoke task.  NOTE: if you add a NEW DIRECTORY containing a PLATFORM INDEPENDENT `.c` or `.cpp` file anywhere in the `ubxlib` tree YOU MUST ALSO ADD it to the `LINT_DIRS` variable of this script.  All the Lint error/warning/information messages available for GCC are included with the exception of those suppressed by the [ubxlib.lnt](/port/platform/lint/ubxlib.lnt) configuration file kept in the [port/platform/lint](/port/platform/lint) directory.  To run Lint yourself you must install a GCC compiler and `flexelint`: read [u_run_lint.py](u_run_lint.py) to determine how to go about using the tools.  A NOTE ON STUBS: by convention, any source file ending in `_stub.c` or  `_stub.cpp` is ommitted from checking; in order to check the stub files add the flag `U_CFG_LINT_USE_STUBS` to the instance being run: with this defined the stub files will be used and the file that is being stubbed will be omitted.  For instance, with `U_CFG_LINT_USE_STUBS` defined `blah_stub.c` would replace `blah.c`.

[scripts/u_run_static_size.py](./scripts/u_run_static_size.py): run a static check of code/RAM size; called by called by `automation.test` PyInvoke task.

[scripts/u_run_no_floating_point.py](./scripts/u_run_no_floating_point.py): run a static size check without floating point support, checking that no floating point has crept in; called by [u_run.py](u_run.py).

[scripts/u_run_pylint.py](./scripts/u_run_pylint.py): run Pylint on all of these Python automation scripts; called by `automation.test` PyInvoke task.

[scripts/u_process_wrapper.py](./scripts/u_process_wrapper.py), [u_process_checker.py](u_process_checker.py), [u_process_checker_is_running.py](u_process_checker_is_running.py), [u_process_settings.py](u_process_settings.py): these files are used in the MKII system only and form a workaround for the Jenkins "abort" shortcoming, which is that Jenkins sends `SIGTERM` to all process when the little red "abort" button is pressed and then, on Windows at least, forcibly terminates the lot 20 seconds later; this is a problem when the things might not like being terminated rudely and, in the MKII case, are actually running on different machines.  The way it works is that Jenkins runs [u_process_wrapper.py](u_process_wrapper.py) which launches [u_process_checker.py](u_process_checker.py) (via `start`, as close to `fork` as Windows gets), which launches the thing that is actually meant to run.  [u_process_checker.py](u_process_checker.py) connects back to [u_process_wrapper.py](u_process_wrapper.py) over a socket and so it knows whether [u_process_wrapper.py](u_process_wrapper.py) is still running or not.  When [u_process_wrapper.py](u_process_wrapper.py) receives `SIGTERM` it exits immediately; [u_process_checker.py](u_process_checker.py) notices this and conducts an organised, handshaked abort of the thing that it ran before it exits.  [u_process_checker_is_running.py](u_process_checker_is_running.py) can be used (e.g. in a [Jenkinsfile](Jenkinsfile) `finally` block) to check if [u_process_checker.py](u_process_checker.py) is still running and prevent Jenkins processes falling over one another.  Thanks go to Morné Joubert for having figured out the innards of Jenkins and providing the pattern for this workaround.  [u_process_settings.py](u_process_settings.py) contains a few variables that are common to all three scripts.

[scripts/u_select.py](./scripts/u_select.py): see above.

[scripts/u_utils.py](./scripts/u_utils.py): utility functions used by all of the above.

[scripts/u_get_likely_base_branch.py](./scripts/u_get_likely_base_branch.py): script used for guessing the base branch for a commit. This is used by `Jenkinsfile` to determine if the base branch is `sho_master` or `master`.

# Maintenance
- If you add a new API make sure that it is listed in the `APIs available` column of at least one row in [DATABASE.md](DATABASE.md), otherwise [u_select.py](./scripts/u_select.py) will **not**  select it for testing on a Pull Request.
- If you add a new board to the test machine or change the COM port or debugger serial number that an existing board uses on the test machine, update [u_connection.py](./scripts/u_connection.py) to match.
- If you add a new platform or test suite, add it to [DATABASE.md](DATABASE.md) and make sure that the result is parsed correctly by [u_data.py](./scripts/u_data.py) (e.g. by running `automation.<command>` PyInvoke tasks) from the command-line and checking that everything is correct).
- If you add a new item in the range 0 to 9 (i.e. a checker with no platform), update [automation.py](./tasks/automation.py) to include it.
- If you add a new directory OFF THE ROOT of `ubxlib`, i.e. something like `ubxlib/blah`, add it to the `ASTYLE_DIRS` variable of the [u_run_astyle.py](./scripts/u_run_astyle.py) script.
- If you add a new directory that contains PLATFORM INDEPENDENT `.c` or `.cpp` files anywhere in the `ubxlib` tree, add it to the `LINT_DIRS` variable of the [u_run_lint.py](./scripts/u_run_lint.py) script.
- If you add a new source file in platform-independent, non-test, non-example code make sure that the source file list (and if necessary the include file list) down in [port/platform/static_size](/port/platform/static_size) is updated to include it, or it will be missed out of the size estimate.
