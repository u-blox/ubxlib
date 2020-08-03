#!/usr/bin/env python

'''Build/run ubxlib for ESP32 and report results.'''

import os            # For sep, getcwd(), isdir() and environ()
from time import clock
import u_connection
import u_monitor
import u_report
import u_utils

# Prefix to put at the start of all prints
PROMPT = "u_run_esp32_"

# The esp-idf URL and sub-directory for latest
# Espressif code
ESP_IDF_LOCATION_LATEST = {"url": "https://github.com/espressif/esp-idf",
                           "subdir": "esp-idf-latest",
                           "branch": "release/v4.1"}

# The esp-idf URL and sub-directory for NINA-W.
# NINA-W1 must use u-blox fork as the flash
# device is not supported by latest esp-idf
ESP_IDF_LOCATION_NINA_W1 = {"url": "https://github.com/u-blox/esp-idf",
                            "subdir": "esp-idf-u-blox",
                            "branch": "master"}

# A table of esp-idf URL/sub-directory to use
# for each ESP32 HW instance.
ESP_IDF_LOCATION = [None,                     # 0 (not esp-idf)
                    None,                     # 1 (not esp-idf)
                    None,                     # 2 (not esp-idf)
                    None,                     # 3 (not esp-idf)
                    None,                     # 4 (not esp-idf)
                    ESP_IDF_LOCATION_NINA_W1, # 5
                    ESP_IDF_LOCATION_LATEST,  # 6
                    ESP_IDF_LOCATION_LATEST]  # 7

# The place where the IDF tools should be found/installed
IDF_TOOLS_PATH = "C:\\Program Files\\Espressif\\ESP-IDF Tools latest"

# Build sub-directory
BUILD_SUBDIR = "build"

# Batch that installs tools, builds and runs the build
MAIN_BATCH_FILE = "u_run_esp32.bat"

# The guard time for this build in seconds:
# this can take ages when tools first have to be installed
BUILD_GUARD_TIME_SECONDS = 60 * 30

# The guard time waiting for a lock on the HW connection seconds
CONNECTION_LOCK_GUARD_TIME_SECONDS = u_connection.CONNECTION_LOCK_GUARD_TIME_SECONDS

# The guard time for running tests in seconds
RUN_GUARD_TIME_SECONDS = u_utils.RUN_GUARD_TIME_SECONDS

# The inactivity time for running tests in seconds
RUN_INACTIVITY_TIME_SECONDS = u_utils.RUN_INACTIVITY_TIME_SECONDS

def get_esp_idf_location(instance):
    '''Return the esp-idf URL and sub-directory'''
    esp_idf_location = None

    if instance[0] < len(ESP_IDF_LOCATION):
        esp_idf_location = ESP_IDF_LOCATION[instance[0]]

    return esp_idf_location

def install_build_download(esp_idf_dir, ubxlib_dir, build_dir,
                           defines, serial_port, script_dir,
                           install_lock, clean,
                           printer, prompt, reporter):
    '''Install the esp-idf tools, build and download'''
    call_list = []
    defines_text = ""
    success = False
    # Set up the environment variables IDF_TOOLS_PATH and
    # U_FLAGS
    for idx, define in enumerate(defines):
        if idx == 0:
            defines_text += "-D" + define
        else:
            defines_text += " -D" + define
    printer.string("{}setting environment variables"
                   " IDF_TOOLS_PATH={}, U_FLAGS={}".
                   format(prompt, IDF_TOOLS_PATH, defines_text))
    my_env = os.environ
    my_env["IDF_TOOLS_PATH"] = IDF_TOOLS_PATH
    my_env["U_FLAGS"] = defines_text

    # Assemble the call list for the batch file
    call_list.append(script_dir + os.sep + MAIN_BATCH_FILE)
    if clean:
        call_list.append("/c")
    call_list.append(esp_idf_dir)
    call_list.append(ubxlib_dir)
    call_list.append(build_dir)
    call_list.append(serial_port)

    printer.string("{}installing the esp-idf tools to \"{}\" and"  \
                   " then building/running code".                  \
                   format(prompt, IDF_TOOLS_PATH))
    if u_utils.install_lock_acquire(install_lock, printer, prompt):
        # Print what we're gonna do
        tmp = ""
        for item in call_list:
            tmp += " " + item
        printer.string("{}in directory {} calling{}".            \
                       format(prompt, os.getcwd(), tmp))

        # Call the batch file to do the build,
        # set shell to True to keep Jenkins happy
        success = u_utils.exe_run(call_list, BUILD_GUARD_TIME_SECONDS,
                                  printer, prompt, shell_cmd=True)
        u_utils.install_lock_release(install_lock, printer, prompt)
    else:
        reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                       u_report.EVENT_FAILED,
                       "could not acquire install lock")

    return success

def run(instance, sdk, connection, connection_lock, platform_lock,
        clean, defines, ubxlib_dir, working_dir, install_lock,
        printer, reporter, test_report_handle):
    '''Build/run on ESP32'''
    return_value = -1
    instance_text = u_utils.get_instance_text(instance)
    filter_string = "*\r\n"

    # Only one SDK for ESP2 and no issues with running in parallel
    del sdk
    del platform_lock

    prompt = PROMPT + instance_text + ": "

    # Print out what we've been told to do
    text = "running ESP32"
    if connection and connection["serial_port"]:
        text += ", on serial port " + connection["serial_port"]
    if clean:
        text += ", clean build"
    if defines:
        text += ", with #define(s)"
        for idx, define in enumerate(defines):
            if idx == 0:
                text += " \"" + define + "\""
            else:
                text += ", \"" + define + "\""
    if ubxlib_dir:
        text += ", ubxlib directory \"" + ubxlib_dir + "\""
    if working_dir:
        text += ", working directory \"" + working_dir + "\""
    printer.string("{}{}.".format(prompt, text))

    reporter.event(u_report.EVENT_TYPE_BUILD_DOWNLOAD,
                   u_report.EVENT_START,
                   "ESP32")
    script_dir = os.getcwd()
    printer.string("{}CD to {}...".format(prompt, working_dir))
    with u_utils.ChangeDir(working_dir):
        # Fetch esp-idf into the right sub directory
        esp_idf_location = get_esp_idf_location(instance)
        if esp_idf_location:
            if u_utils.fetch_repo(esp_idf_location["url"],
                                  esp_idf_location["subdir"],
                                  esp_idf_location["branch"],
                                  printer, prompt):
                build_dir = working_dir + os.sep + BUILD_SUBDIR
                # Install the esp-idf tools, build and run
                install_build_download_start_time = clock()
                if install_build_download(esp_idf_location["subdir"],
                                          ubxlib_dir, build_dir,
                                          defines, connection["serial_port"],
                                          script_dir, install_lock, clean,
                                          printer, prompt, reporter):
                    reporter.event(u_report.EVENT_TYPE_BUILD_DOWNLOAD,
                                   u_report.EVENT_PASSED,
                                   "install/build/download took {:.0f} second(s)". \
                                   format(clock() - install_build_download_start_time))
                    with u_connection.Lock(connection, connection_lock,
                                           CONNECTION_LOCK_GUARD_TIME_SECONDS,
                                           printer, prompt) as locked:
                        if locked:
                            reporter.event(u_report.EVENT_TYPE_TEST,
                                           u_report.EVENT_START)
                            # Search the defines list to see if it includes a
                            # "U_CFG_APP_FILTER=blah" item.  On ESP32 the tests
                            # that are run are not selected at compile time,
                            # they are selected by sending the "blah" string
                            # over the COM port where it must match a "module name",
                            # a thing in [square brackets] which our naming convention
                            # dictates will be an API name (e.g. "port") or "example".
                            for define in defines:
                                tmp = u_utils.FILTER_MACRO_NAME + "="
                                if define.startswith(tmp):
                                    filter_string = define[len(tmp):]
                                    reporter.event(u_report.EVENT_TYPE_TEST,
                                                   u_report.EVENT_INFORMATION,
                                                   "only running module \"" +
                                                   filter_string + "\"")
                                    printer.string("{} will use filter [{}].".   \
                                                   format(prompt, filter_string))
                                    # Add the top and tail it needs for sending
                                    filter_string = "[" + filter_string + "]\r\n"
                                    break
                            # Open the COM port to get debug output
                            serial_handle = u_utils.open_serial(connection["serial_port"],
                                                                115200, printer, prompt)
                            if serial_handle is not None:
                                # Monitor progress
                                return_value = u_monitor.main(serial_handle,
                                                              u_monitor.CONNECTION_SERIAL,
                                                              RUN_GUARD_TIME_SECONDS,
                                                              RUN_INACTIVITY_TIME_SECONDS,
                                                              instance, printer, reporter,
                                                              test_report_handle,
                                                              send_string=filter_string)
                                serial_handle.close()
                            else:
                                reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                                               u_report.EVENT_FAILED,
                                               "unable to open serial port " +      \
                                               connection["serial_port"])
                            if return_value == 0:
                                reporter.event(u_report.EVENT_TYPE_TEST,
                                               u_report.EVENT_COMPLETE)
                            else:
                                reporter.event(u_report.EVENT_TYPE_TEST,
                                               u_report.EVENT_FAILED)
                        else:
                            reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                                           u_report.EVENT_FAILED,
                                           "unable to lock a connection")
                else:
                    reporter.event(u_report.EVENT_TYPE_BUILD_DOWNLOAD,
                                   u_report.EVENT_FAILED,
                                   "unable to build/download/run, check" +      \
                                   " debug log for details")
            else:
                reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                               u_report.EVENT_FAILED,
                               "unable to fetch " + esp_idf_location["url"])
        else:
            reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                           u_report.EVENT_FAILED,
                           "don't have esp-idf URL for this ESP32 instance")
            printer.string("{}error: don't have esp-idf URL for ESP32 instance {}.".
                           format(prompt, instance_text))

    return return_value
