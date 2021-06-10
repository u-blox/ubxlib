#!/usr/bin/env python

'''Build/run ubxlib for ESP-IDF and report results.'''

import os            # For sep, getcwd(), isdir() and environ()
from time import time, sleep
import psutil
import u_connection
import u_monitor
import u_report
import u_utils
import u_settings

# Prefix to put at the start of all prints
PROMPT = "u_run_esp_idf_"

# The root directory for ESP-IDF
ESP_IDF_ROOT = u_settings.ESP_IDF_ROOT # e.g. "c:\\esp32"

# The ESP-IDF URL and directory for latest
# Espressif code
ESP_IDF_LOCATION_LATEST = u_settings.ESP_IDF_LOCATION_LATEST

# The ESP-IDF URL and directory for NINA-W1.
# NINA-W1 must use u-blox fork as the flash
# device is not supported by latest ESP-IDF
ESP_IDF_LOCATION_NINA_W1 = u_settings.ESP_IDF_LOCATION_NINA_W1

# A table of ESP-IDF URL/sub-directory to use
# for each ESP-IDF instance.
ESP_IDF_LOCATION = [None,                     # 0 (not ESP-IDF)
                    None,                     # 1 (not ESP-IDF)
                    None,                     # 2 (not ESP-IDF)
                    None,                     # 3 (not ESP-IDF)
                    None,                     # 4 (not ESP-IDF)
                    None,                     # 5 (not ESP-IDF)
                    None,                     # 6 (not ESP-IDF)
                    None,                     # 7 (not ESP-IDF)
                    None,                     # 8 (not ESP-IDF)
                    None,                     # 9 (not ESP-IDF)
                    ESP_IDF_LOCATION_NINA_W1, # 10
                    ESP_IDF_LOCATION_LATEST,  # 11
                    ESP_IDF_LOCATION_LATEST]  # 12

# The place where the IDF tools should be found/installed
IDF_TOOLS_PATH = u_settings.ESP_IDF_TOOLS_PATH # e.g. ESP_IDF_ROOT + os.sep + "esp-idf-tools-latest"

# The sub-directory name of the project to build
PROJECT_SUBDIR = u_settings.ESP_IDF_PROJECT_SUBDIR # e.g. "runner"

# The name of the test component to build
TEST_COMPONENT = u_settings.ESP_IDF_TEST_COMPONENT # e.g. "ubxlib_runner"

# Build sub-directory
BUILD_SUBDIR = u_settings.ESP_IDF_BUILD_SUBDIR # e.g. "build"

# The guard time for the install in seconds:
# this can take ages when tools first have to be installed
INSTALL_GUARD_TIME_SECONDS = u_settings.ESP_IDF_INSTALL_GUARD_TIME_SECONDS # e.g. 60 * 60

# The guard time for this build in seconds
BUILD_GUARD_TIME_SECONDS = u_settings.ESP_IDF_BUILD_GUARD_TIME_SECONDS # e.g. 60 * 30

# The download guard time for this build in seconds
DOWNLOAD_GUARD_TIME_SECONDS = u_settings.ESP_IDF_DOWNLOAD_GUARD_TIME_SECONDS # e.g. 60 * 5

# The guard time waiting for a lock on the HW connection seconds
CONNECTION_LOCK_GUARD_TIME_SECONDS = u_connection.CONNECTION_LOCK_GUARD_TIME_SECONDS

# The guard time for running tests in seconds
RUN_GUARD_TIME_SECONDS = u_utils.RUN_GUARD_TIME_SECONDS

# The inactivity time for running tests in seconds
RUN_INACTIVITY_TIME_SECONDS = u_utils.RUN_INACTIVITY_TIME_SECONDS

def get_esp_idf_location(instance):
    '''Return the ESP-IDF URL and sub-directory'''
    esp_idf_location = None

    if instance[0] < len(ESP_IDF_LOCATION):
        esp_idf_location = ESP_IDF_LOCATION[instance[0]]

    return esp_idf_location

def print_env(returned_env, printer, prompt):
    '''Print a dictionary that contains the new environment'''
    printer.string("{}environment will be:".format(prompt))
    if returned_env:
        for key, value in returned_env.items():
            printer.string("{}{}={}".format(prompt, key, value))
    else:
        printer.string("{}EMPTY".format(prompt))

def install(esp_idf_url, esp_idf_dir, esp_idf_branch,
            system_lock, keep_going_flag, printer, prompt, reporter):
    '''Install the Espressif tools and ESP-IDF'''
    returned_env = {}
    count = 0

    # Acquire the install lock as this is a global operation
    if system_lock is None or u_utils.install_lock_acquire(system_lock, printer,
                                                           prompt, keep_going_flag):
        # Fetch the repo
        if u_utils.keep_going(keep_going_flag, printer, prompt) and \
           u_utils.fetch_repo(esp_idf_url, esp_idf_dir,
                              esp_idf_branch, printer, prompt):

            # Set up the environment variable IDF_TOOLS_PATH
            my_env = os.environ
            my_env["IDF_TOOLS_PATH"] = IDF_TOOLS_PATH

            printer.string("{}installing the Espressif tools to \"{}\" and"  \
                           " ESP-IDF to \"{}\".".                            \
                           format(prompt, IDF_TOOLS_PATH, esp_idf_dir))
            # Switch to where the stuff should have already
            # been fetched to
            with u_utils.ChangeDir(esp_idf_dir):
                if not u_utils.has_admin():
                    printer.string("{}NOTE: if install.bat fails (the return"   \
                                   " code may still be 0), then try re-running" \
                                   " as administrator.".format(prompt))
                # First call install.bat
                # set shell to True to keep Jenkins happy
                if u_utils.keep_going(keep_going_flag, printer, prompt) and \
                   u_utils.exe_run(["install.bat"], INSTALL_GUARD_TIME_SECONDS,
                                    printer, prompt, shell_cmd=True):
                    # ...then export.bat to set up paths etc.
                    # which we return attached to returned_env.
                    # It is possible for the process of extracting
                    # the environment variables to fail due to machine
                    # loading (see comments against EXE_RUN_QUEUE_WAIT_SECONDS
                    # in exe_run) so give this up to three chances to succeed
                    while u_utils.keep_going(keep_going_flag, printer, prompt) and \
                          not returned_env and (count < 3):
                        # set shell to True to keep Jenkins happy
                        u_utils.exe_run(["export.bat"], INSTALL_GUARD_TIME_SECONDS,
                                        printer, prompt, shell_cmd=True,
                                        returned_env=returned_env)
                        if not returned_env:
                            printer.string("{}warning: retrying export.bat to"     \
                                           " capture the environment variables...".
                                           format(prompt))
                        count += 1
                    if not returned_env:
                        reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                                       u_report.EVENT_FAILED,
                                       "export.bat failed")
                else:
                    reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                                   u_report.EVENT_FAILED,
                                   "install.bat failed")
        else:
            reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                           u_report.EVENT_FAILED,
                           "unable to fetch " + esp_idf_url)
        if system_lock is not None:
            u_utils.install_lock_release(system_lock, printer, prompt)
    else:
        reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                       u_report.EVENT_FAILED,
                       "could not acquire install lock")

    return returned_env

def build(esp_idf_dir, ubxlib_dir, build_dir, defines, env, clean,
          printer, prompt, reporter):
    '''Build the code'''
    call_list = []
    defines_text = ""
    success = False

    # Make sure that the build directory exists and is
    # cleaned if required
    if os.path.exists(build_dir):
        if clean:
            u_utils.deltree(build_dir, printer, prompt)
            os.makedirs(build_dir)
            # Note: used to delete sdkconfig here to
            # force it to be regenerated from the
            # sdkconfig.defaults file however we can't
            # do that with parallel builds as the file
            # might be in use.  Just need to be sure
            # that none of our builds fiddle with
            # it (which they shouldn't for consistency
            # anyway).
    else:
        os.makedirs(build_dir)

    # CCACHE is a pain in the bum: falls over on Windows
    # path length issues randomly and doesn't say where.
    # Since we're generally doing clean builds, disable it
    env["CCACHE_DISABLE"] = "1"

    if os.path.exists(build_dir):
        printer.string("{}building code...".format(prompt))
        # Set up the U_FLAGS environment variables
        for idx, define in enumerate(defines):
            if idx == 0:
                defines_text += "-D" + define
            else:
                defines_text += " -D" + define
        printer.string("{}setting environment variables U_FLAGS={}".
                       format(prompt, defines_text))
        env["U_FLAGS"] = defines_text

        # Assemble the call list for the build process
        call_list.append("python")
        call_list.append(esp_idf_dir + os.sep + "tools\\idf.py")
        call_list.append("-C")
        call_list.append(ubxlib_dir + os.sep + \
                         "port\\platform\\esp-idf\\mcu\\esp32"
                         + os.sep + PROJECT_SUBDIR)
        call_list.append("-B")
        call_list.append(build_dir)
        call_list.append("-D")
        call_list.append("TEST_COMPONENTS=" + TEST_COMPONENT)
        call_list.append("size")
        call_list.append("build")

        # Print what we're gonna do
        tmp = ""
        for item in call_list:
            tmp += " " + item
        printer.string("{}in directory {} calling{}".            \
                       format(prompt, os.getcwd(), tmp))

        # Do the build,
        # set shell to True to keep Jenkins happy
        success = u_utils.exe_run(call_list, BUILD_GUARD_TIME_SECONDS,
                                  printer, prompt, shell_cmd=True,
                                  set_env=env)
    else:
        reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                       u_report.EVENT_FAILED,
                       "could not create directory \"" + build_dir + "\"")

    return success

def download(esp_idf_dir, ubxlib_dir, build_dir, serial_port, env,
             printer, prompt):
    '''Download a build to the target'''
    call_list = []

    # Assemble the call list for the download process
    call_list.append("python")
    call_list.append(esp_idf_dir + os.sep + "tools\\idf.py")
    call_list.append("-p")
    call_list.append(serial_port)
    call_list.append("-C")
    call_list.append(ubxlib_dir + os.sep + \
                     "port\\platform\\esp-idf\\mcu\\esp32"
                     + os.sep + PROJECT_SUBDIR)
    call_list.append("-B")
    call_list.append(build_dir)
    call_list.append("flash")

    # Print what we're gonna do
    tmp = ""
    for item in call_list:
        tmp += " " + item
    printer.string("{}in directory {} calling{}".            \
                   format(prompt, os.getcwd(), tmp))

    # Give ourselves priority here or the download can fail
    psutil.Process().nice(psutil.HIGH_PRIORITY_CLASS)

    # Do the download,
    # set shell to True to keep Jenkins happy
    return_code = u_utils.exe_run(call_list, DOWNLOAD_GUARD_TIME_SECONDS,
                                  printer, prompt, shell_cmd=True, set_env=env)

    # Return priority to normal
    psutil.Process().nice(psutil.NORMAL_PRIORITY_CLASS)

    return return_code

def run(instance, mcu, toolchain, connection, connection_lock,
        platform_lock, misc_locks, clean, defines, ubxlib_dir,
        working_dir, printer, reporter, test_report_handle,
        keep_going_flag=None):
    '''Build/run on ESP-IDF'''
    return_value = -1
    instance_text = u_utils.get_instance_text(instance)

    # No issues with running in parallel on ESP-IDF
    del platform_lock

    # Only one toolchain for ESP-IDF
    del toolchain

    prompt = PROMPT + instance_text + ": "

    # Print out what we've been told to do
    text = "running ESP-IDF for " + mcu
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

    reporter.event(u_report.EVENT_TYPE_BUILD,
                   u_report.EVENT_START,
                   "ESP-IDF")
    printer.string("{}CD to {}...".format(prompt, working_dir))
    with u_utils.ChangeDir(working_dir):
        # Fetch ESP-IDF into the right sub directory
        # and install the tools
        esp_idf_location = get_esp_idf_location(instance)
        esp_idf_dir = ESP_IDF_ROOT + os.sep + esp_idf_location["subdir"]
        if esp_idf_location:
            system_lock = None
            if u_utils.keep_going(keep_going_flag, printer, prompt) and \
               misc_locks and ("system_lock" in misc_locks):
                system_lock = misc_locks["system_lock"]
            returned_env = install(esp_idf_location["url"], esp_idf_dir,
                                   esp_idf_location["branch"], system_lock,
                                   keep_going_flag, printer, prompt, reporter)
            if u_utils.keep_going(keep_going_flag, printer, prompt) and \
               returned_env:
                # From here on the ESP-IDF tools need to set up
                # and use the set of environment variables
                # returned above.
                print_env(returned_env, printer, prompt)
                # Now do the build
                build_dir = working_dir + os.sep + BUILD_SUBDIR
                build_start_time = time()
                if u_utils.keep_going(keep_going_flag, printer, prompt) and \
                   build(esp_idf_dir, ubxlib_dir, build_dir,
                         defines, returned_env, clean,
                         printer, prompt, reporter):
                    reporter.event(u_report.EVENT_TYPE_BUILD,
                                   u_report.EVENT_PASSED,
                                   "build took {:.0f} second(s)". \
                                   format(time() - build_start_time))
                    with u_connection.Lock(connection, connection_lock,
                                           CONNECTION_LOCK_GUARD_TIME_SECONDS,
                                           printer, prompt, keep_going_flag) as locked:
                        if locked:
                            # Have seen this fail, only with Python 3 for
                            # some reason, so give it a few goes
                            reporter.event(u_report.EVENT_TYPE_DOWNLOAD,
                                           u_report.EVENT_START)
                            retries = 0
                            while u_utils.keep_going(keep_going_flag, printer, prompt) and \
                                  not download(esp_idf_dir, ubxlib_dir, build_dir,
                                               connection["serial_port"], returned_env,
                                               printer, prompt) and (retries < 3):
                                reporter.event(u_report.EVENT_TYPE_DOWNLOAD,
                                               u_report.EVENT_WARNING,
                                               "unable to download, will retry...")
                                retries += 1
                                sleep(5)
                            if retries < 3:
                                reporter.event(u_report.EVENT_TYPE_DOWNLOAD,
                                               u_report.EVENT_COMPLETE)
                                reporter.event(u_report.EVENT_TYPE_TEST,
                                               u_report.EVENT_START)
                                # Search the defines list to see if it includes a
                                # "U_CFG_APP_FILTER=blah" item.  On ESP32 the tests
                                # that are run are not selected at compile time,
                                # so we can't do this filtering, just print a warning
                                # that the filter is being ignored.
                                for define in defines:
                                    tmp = u_utils.FILTER_MACRO_NAME + "="
                                    if define.startswith(tmp):
                                        filter_string = define[len(tmp):]
                                        reporter.event(u_report.EVENT_TYPE_TEST,
                                                       u_report.EVENT_INFORMATION,
                                                       "filter string \"" +
                                                       filter_string + "\" ignored")
                                        printer.string("{} filter string \"{}\""         \
                                                       " was specified but ESP-IDF uses" \
                                                       " its own test wrapper which"     \
                                                       " does not support filtering"     \
                                                       " hence it will be ignored.".     \
                                                       format(prompt, filter_string))
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
                                                                  "\r", instance, printer,
                                                                  reporter, test_report_handle,
                                                                  send_string="*\r\n",
                                                                  keep_going_flag=keep_going_flag)
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
                                reporter.event(u_report.EVENT_TYPE_DOWNLOAD,
                                               u_report.EVENT_FAILED,
                                               "unable to download to the target")
                        else:
                            reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                                           u_report.EVENT_FAILED,
                                           "unable to lock a connection")
                else:
                    reporter.event(u_report.EVENT_TYPE_BUILD,
                                   u_report.EVENT_FAILED,
                                   "unable to build, check debug log for details")
            else:
                reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                               u_report.EVENT_FAILED,
                               "tools installation failed")
        else:
            reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                           u_report.EVENT_FAILED,
                           "don't have ESP-IDF URL for this instance")
            printer.string("{}error: don't have ESP-IDF URL instance {}.".
                           format(prompt, instance_text))

    return return_value
