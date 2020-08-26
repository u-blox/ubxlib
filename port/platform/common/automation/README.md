# Introduction
The files in here are used internally within u-blox to automate testing of `ubxlib`.  They are not supported externally. However, if you find them useful please help yourselves.

# Reading The Jenkins Output
Since much of the test execution is inside these Python scripts the Jenkins level Groovy script `Jenkinsfile` doesn't do a great deal.  To see how testing has gone look in the Jenkins artifacts for files named `summary.log`.  There should be one at the top level and another inside the folder for each instance.  Check the top level `summary.log` to see how the run went.  The `summary.log` file inside each instance folder contains the portion of the summary for that instance but adds no aditional information so, if there is a problem, check the `debug.log` file inside the instance folder for more detailed information.

# Script Usage
The main intended entry point into automation is the `u_pull_request.py` Python script.  You can run it with parameter `-h` to obtain usage information but basically the form is:

`u_pull_request.py "some text" <a list of file paths, unquoted, separated with spaces>`

The text is intended to be that submitted with the pull request.  When `u_pull_request.py` is called from Jenkins, using `Jenkinsfile`, the pull request text is grabbed from Github by the `Jenkinsfile` script.  This text is parsed for a line that starts with `test: foo bar`, where `foo` is either `*` or a series of digits separated by spaces or full stops, e.g. `0` `0 1` or `1.1` or `4.0.2 6.1.3` and `bar` is some more optional text, e.g. `test: 1 example`.  `foo` indicates the instance IDs to run from `DATABASE.md`, separate by spaces, and `bar` is a filter string, indicating that only examples/tests that begin with that string should be run.  For instance `test: 1 2 3 example` would run all things that begin with `example` on instance IDs `1`, `2` and `3`, or `test: * port` would run *all* things that begin with `port` (i.e. the porting tests) on *all* instances, or `test: *` would run everything on all instance IDs, etc.

So, when submitting a pull request if a such a line of text is included with it then the u-blox Jenkins configuration will parse the text, find the `test:` line and conduct those tests on the pull request, returning the results to Github.

Note: on the ESP32 platform, which comes with its own unit test implementation, the filter string must be the full name of a category, e.g. `port` or `example`, partial matches are not supported.

If a line starting with `test:` is *not* included then the file list must be provided.  Again, when `u_pull_request.py` is called from Jenkins, `Jenkinsfile`, will grab the list of changed files from Github.  `u_pull_request.py` then calls the `u_select.py` script to determine what tests should be run on which instance IDs to verify that the pull request is good.  See the comments in `u_select.py` to determine how it does this.  It is worth noting that the list of tests/instances selected by `u_select.py` will always be the largest one: e.g. if a `.h` file in the `port` API of `ubxlib` has been changed then all the `port` tests on all instance IDs will be selected.  To narrow the tests/instanaces that are run, use the `test:` line to specify it yourself.  Note that though you can edit the pull request submission text unfortunately this does not retrigger testing, only pushing another submit to the pull request will do that.

For each instance ID the `u_run.py` script, the thing that ultimately does the work, will return a value which is zero for success or otherwise the number of failures that occurred during the run.  Search the output from the script for the word `EXITING` to find its return value.

# Script Descriptions
`Jenkinsfile`: tells Jenkins what to do, written in Groovy stages.  Key point to note is that the archived files will be `summary.log` for a quick overview, `test_report.xml` for an XML formatted report on any tests that are executed on the target HW and `debug.log` for the full detailed debug on each build/download/run (which is also spewed to the console by Jenkins).

`swo_decoder.py`: used by the STM2F4 tests to decode debug prints sent over SWO.

`u_connection.py`: contains a record of how each item of HW is connected into the test system (COM port, debugger serial number, etc.) and functions to access this information.

`u_data.py`: functions to parse `DATABASE.md`.

`u_monitor.py`: monitors the output from an instance that is executing tests, checking the output for passes/failures and optionally writing debug logs and an XML report to file.

`u_pull_request.py`: see above.

`u_report.py`: write reports on progress.

`u_run.py`: handles the build/download/run of tests on a given instance by calling one of the `u_run_xxx.p` scripts below; this script is called multiple times in parallel by `u_pull_request.py`, each running in a Python process of its own.

`u_run_astyle.py`: run an advisory-only (i.e. no error will be thrown ever) AStyle check; called by `u_run.py`.  NOTE: because of the way AStyle directory selection works, if you add a new directory off the `ubxlib` root directory (i.e. you add something like `ubxlib\blah`) YOU MUST ALSO ADD it to the `ASTYLE_DIRS` variable of this script.  To AStyle your files before submission, install `AStyle` version 3.1 and, from the `ubxlib` root directory, run:

```
astyle --options=astyle.cfg --suffix=none --verbose --errors-to-stdout --recursive *.c,*.h,*.cpp,*.hpp
```

`u_run_doxygen.py`: run Doxygen to check that there are no documentation errors; called by `u_run.py`.

`u_run_esp32.py`: build/download/run tests on the ESP32 platform; called by `u_run.py`.

`u_run_lint.py`: run a Lint check; called by `u_run.py`.  NOTE: if you add a NEW DIRECTORY containing a PLATFORM INDEPENDENT `.c` or `.cpp` file anywhere in the `ubxlib` tree YOU MUST ALSO ADD it to the `LINT_DIRS` variable of this script.  All the Lint error/warning/information messages available for GCC are included with the exception of those suppressed by the `ubxlib.lnt` configuration file kept in the `port\platform\lint` directory.  To run Lint yourself you must install a GCC compiler and `flexelint`: read `u_run_lint.py` to determine how to go about using the tools.

`u_run_nrf52.py`: build/download/run tests on the NRF52 platform; called by `u_run.py`.

`u_run_pylint.py`: run Pylint on all of these Python automation scripts; called by `u_run.py`.

`u_run_stm32f4.py`: build/download/run tests on the STM2F4 platform; called by `u_run.py`.

`u_select.py`: see above.

`u_utils.py`: utility functions used by all of the above.

# Maintenance
- If you add a new board to the test machine or change the COM port or debugger serial number that an existing board uses on the test machine, update `u_connection.py` to match.
- If you add a new platform or test suite, add it to `DATABASE.md` and make sure that the result is parsed correctly by `u_data.py` (e.g. by running `u_pull_request.py` from the command-line and checking that everything is correct).
- If you add a new item in the range 0 to 4 (i.e. a checker with no platform), update `u_pull_request.py` to include it.
- If you add a new directory OFF THE ROOT of `ubxlib`, i.e. something like `ubxlib\blah`, add it to the `ASTYLE_DIRS` variable of the `u_run_astyle.py` script.
- If you add a new directory that contains PLATFORM INDEPENDENT `.c` or `.cpp` files anywhere in the `ubxlib` tree, add it to the `LINT_DIRS` variable of the `u_run_lint.py` script.