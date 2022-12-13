#!/usr/bin/env python

'''Build/run ubxlib for Windows and report results.'''
import os                    # For sep(), getcwd(), listdir()
from time import time
from logging import Logger
import requests
from scripts import u_connection, u_monitor, u_report, u_utils, u_settings
from scripts.u_logging import ULog

# Prefix to put at the start of all prints
PROMPT = "u_run_windows_"

# The logger
U_LOG: Logger = None

# The directory where the runner build can be found
RUNNER_DIR = os.path.join("port", "platform", "windows", "mcu", "win32", "runner")

# The name of the output sub-directory
BUILD_SUBDIR = u_settings.WINDOWS_BUILD_SUBDIR # e.g. "build"

# Batch file that sets up the MSVC environment.
MSVC_SETUP_BATCH_FILE = u_settings.WINDOWS_MSVC_SETUP_BATCH_FILE

# The guard time for the environment setup in seconds:
MSVC_SETUP_GUARD_TIME_SECONDS = u_settings.WINDOWS_MSVC_SETUP_GUARD_TIME_SECONDS # e.g. 60 * 10

# The guard time for the environment setup in seconds:
CMAKE_PATH = u_settings.WINDOWS_MSVC_CMAKE_PATH # MSVC comes with its own copy of CMake

# The generator to use for MSVC builds:
CMAKE_GENERATOR = u_settings.WINDOWS_MSVC_CMAKE_GENERATOR # e.g. "Visual Studio 17 2022"

# The guard time for runnning CMake in seconds:
CMAKE_GUARD_TIME_SECONDS = u_settings.WINDOWS_CMAKE_GUARD_TIME_SECONDS # e.g. 60 * 10

# The build target, i.e. the name of the CMakeLists.txt node that is to be built:
BUILD_TARGET = "ubxlib_test_main"

# The build configuration:
BUILD_CONFIGURATION = "Debug"

# The guard time for this build in seconds,
# noting that it can be quite long when
# many builds are running in parallel.
BUILD_GUARD_TIME_SECONDS = u_settings.WINDOWS_BUILD_GUARD_TIME_SECONDS # e.g. 60 * 30

# The guard time waiting for a lock on the HW connection seconds
CONNECTION_LOCK_GUARD_TIME_SECONDS = u_connection.CONNECTION_LOCK_GUARD_TIME_SECONDS

# The guard time waiting for a install lock in seconds
INSTALL_LOCK_WAIT_SECONDS = u_utils.INSTALL_LOCK_WAIT_SECONDS

# The guard time for running tests in seconds
RUN_GUARD_TIME_SECONDS = u_utils.RUN_GUARD_TIME_SECONDS

# The inactivity time for running tests in seconds
RUN_INACTIVITY_TIME_SECONDS = u_utils.RUN_INACTIVITY_TIME_SECONDS

# Table of "where.exe" search paths for tools required to be installed
# plus hints as to how to install the tools and how to read their version
TOOLS_LIST = [{"which_string": "cl",
               "hint": "can't find MSVC, please either install it from"     \
                       " https://aka.ms/vs/17/release/vs_BuildTools.exe"    \
                       " or add it to the path.",
               "version_switch": ""}]

# The prefix that all switch control thingies in the
# list of defines should have; will be followed by XXX
SWITCH_CONTROL_PREFIX = "U_CFG_TEST_NET_STATUS_"

# The prefix that the target should emit before XXX
# to control a switch
SWITCH_TRIGGER_PREFIX = "AUTOMATION_SET_SWITCH"

# Regex to capture a string of the form:
#
# AUTOMATION_SET_SWITCH XXX 1
#
# ...where it would return "XXX" in the first capture group and "1"
# in the second capture group. This tested on https://regex101.com/ with
# Python the selected regex flavour.
# "XXX" should then match the "XXX" in "U_CFG_TEST_NET_STATUS_XXX"
SWITCH_CONTROL_REGEX = r"(?:^" + SWITCH_TRIGGER_PREFIX + " (.*?) ([0-1]))$"

# Switches to control RF
SWITCH_LIST = u_settings.SWITCH_LIST

def print_env(returned_env):
    '''Print a dictionary that contains the new environment'''
    U_LOG.info("environment will be:")
    if returned_env:
        for key, value in returned_env.items():
            U_LOG.info(f"{key}={value}")
    else:
        U_LOG.info("EMPTY")

def check_installation(tools_list, env):
    '''Check that everything required has been installed'''
    success = True

    # Check for the tools on the path
    U_LOG.info("checking tools...")
    for item in tools_list:
        if u_utils.exe_where(item["which_string"], item["hint"],
                             logger=U_LOG, set_env=env):
            if item["version_switch"]:
                u_utils.exe_version(item["which_string"],
                                    item["version_switch"],
                                    logger=U_LOG, set_env=env)
        else:
            success = False

    return success

def set_up_environment(reporter):
    '''Set up the environment ready for building'''
    returned_env = {}
    count = 0

    # Call the setup batch file and grab the environment it
    # creates.  It is possible for capturing the environment
    # variables to fail due to machine loading (see comments
    # against EXE_RUN_QUEUE_WAIT_SECONDS in exe_run) so give this
    # up to three chances to succeed
    while not returned_env and (count < 3):
        # set shell to True to keep Jenkins happy
        u_utils.exe_run([MSVC_SETUP_BATCH_FILE], MSVC_SETUP_GUARD_TIME_SECONDS,
                        logger=U_LOG,
                        shell_cmd=True,
                        returned_env=returned_env)
        if not returned_env:
            U_LOG.warning("warning: retrying {} to"     \
                          " capture the environment variables...".
                           format(" ".join(MSVC_SETUP_BATCH_FILE)))
        count += 1
    if not returned_env:
        reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                       u_report.EVENT_FAILED,
                       f"{' '.join(MSVC_SETUP_BATCH_FILE)} failed")
    return returned_env

def switch_list_create(u_flags, logger):
    '''Parse u_flags to look for switch control things'''
    switch_list = []

    # Parse u_flags to find values which begin with
    # U_CFG_TEST_NET_STATUS_XXX and create a list of the values
    # of XXX and what they equate to in the form:
    #
    # {"xxx": "XXX", "name": "RF_SWITCH_A"}
    for flag in u_flags:
        if flag.startswith(SWITCH_CONTROL_PREFIX):
            parts1 = flag.split(SWITCH_CONTROL_PREFIX)
            if parts1 and len(parts1) > 1:
                item = {}
                item["xxx"] = parts1[1]
                message = "\"" + item["xxx"] + "\""
                parts2 = parts1[1].split("=")
                if parts2 and len(parts2) > 1:
                    item["xxx"] = parts2[0]
                    message = "\"" + item["xxx"] + "\""
                    item["name"] = parts2[1]
                    message += " will operate " + item["name"]
                logger.info("control word " + message +
                            ", trigger string will be \"" +
                            SWITCH_TRIGGER_PREFIX +
                            " " + item["xxx"] + " 0-1\"")
                switch_list.append(item)

    # Having made the list, check if we know of
    # all of the required switches; if we do then
    # add the switch information to the switch_list
    # entry, else delete the entry.
    delete_list = []
    for idx, item in enumerate(switch_list):
        if "name" in item and item["name"]:
            found = False
            for switch in SWITCH_LIST:
                if switch["name"] == item["name"]:
                    switch_list[idx].update(switch)
                    found = True
            if not found:
                delete_list.append(idx)
                logger.info("WARNING: don't know about a switch" +
                            " named \"" + item["name"] + "\"")
    # Remove the largest index items first so as not to change
    # the order of the list as we go
    delete_list.sort(reverse=True)
    for item in delete_list:
        switch_list.pop(item)

    return switch_list

def callback(match, switch_list, results, reporter):
    '''Control a switch's state'''
    url = None

    del results

    # match group 1 will contain the XXX from U_CFG_TEST_NET_STATUS_XXX
    # and match group 2 will contain the desired state, e.g. "0" or "1"
    xxx = match.group(1)
    desired_state = match.group(2)
    if xxx:
        message = "script asked to switch \"" + xxx + "\""
        if desired_state:
            message += " to state \"" + desired_state + "\""
            for switch in switch_list:
                if switch["xxx"] == xxx and desired_state in switch and "ip" in switch:
                    url = switch["ip"] + "/" + switch[desired_state]
                    message += ", " + switch["name"] + ": " + url
                    break
        if url:
            response = requests.post("http://" + url)
            message += " [response " + str(response.status_code) + "]"
        else:
            message += ": DON'T KNOW HOW TO DO THAT!"
        if reporter and message:
            reporter.event(u_report.EVENT_TYPE_TEST,
                           u_report.EVENT_INFORMATION,
                           message)


def build(clean, unity_dir, defines, env, reporter):
    '''Build using MSVC'''
    defines_text = ""
    call_list = []
    output_dir = os.getcwd() + os.sep + BUILD_SUBDIR
    cmakelist_dir = u_utils.UBXLIB_DIR + os.sep + RUNNER_DIR
    exe_file_path = None

    # Clear the output folder if we're not just running
    if not clean or u_utils.deltree(output_dir, logger=U_LOG):
        # Set up the U_FLAGS environment variable
        # Note: MSVC uses # as a way of passing = in such a
        # flag, and so the first # that appears in the value
        # of a flag will come out as = in the code
        # i.e. -DTHING=1234# will appear to the code as
        # #define THING 1234=.  Below we check if the value part
        # includes a # and, if it does, we replace the = with
        # a hash also; then, for example -DTHING#1234# will
        # appear to the code as #define THING 1234#.
        for idx, define in enumerate(defines):
            if "#" in define:
                define = define.replace("=", "#", 1)
            if idx == 0:
                defines_text += "-D" + define
            else:
                defines_text += " -D" + define
        U_LOG.info(f'setting environment variable U_FLAGS="{defines_text}"')
        env["U_FLAGS"] = defines_text
        # There is one complication with Unity, which is that the
        # CMakeLists.txt file likely works out its path, relative
        # to the ubxlib root, as being at the same level, i.e. "../Unity".
        # This won't work if the ubxlib root is a subst'ed drive, i.e.
        # "Z:/.." makes no sense.  So if there is NOT a #define UNITY_PATH
        # in the environment, add one that puts Unity in the correct
        # default place.
        if "UNITY_PATH" not in env:
            env["UNITY_PATH"] = u_utils.get_actual_path(unity_dir)

        # First run MSVC CMake to generate the metadata
        call_list += ["set", "&&"]
        call_list += ["cmake", "-G", CMAKE_GENERATOR]
        call_list += ["-T", "host=x86", "-A", "win32"]
        call_list += ["-B", output_dir]
        call_list += ["--no-warn-unused-cli"]
        call_list += [cmakelist_dir]

        # Call CMake
        # Set shell to keep Jenkins happy
        if u_utils.exe_run(call_list, CMAKE_GUARD_TIME_SECONDS,
                           logger=U_LOG, shell_cmd=True,
                           set_env=env):
            # Now run MSVC CMake again to do the build
            call_list = []
            call_list += ["cmake"]
            call_list += ["--build", output_dir]
            call_list += ["--config", BUILD_CONFIGURATION]
            call_list += ["--target", BUILD_TARGET]
            call_list += ["-j", "8"]

            # Call Make to do the build
            # Set shell to keep Jenkins happy
            if u_utils.exe_run(call_list, BUILD_GUARD_TIME_SECONDS,
                               logger=U_LOG, shell_cmd=True,
                               set_env=env):
                exe_file_path = output_dir + os.sep + BUILD_CONFIGURATION + \
                                os.sep + BUILD_TARGET + ".exe"
        else:
            reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                            u_report.EVENT_FAILED,
                            "unable to run CMake")
    else:
        reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                        u_report.EVENT_FAILED,
                        "unable to clean build directory")

    return exe_file_path

def run(instance, toolchain, connection, connection_lock,
        clean, defines, reporter, test_report_file_path,
        unity_dir=None):
    '''Build/run on Windows'''
    return_value = -1
    exe_file = None
    instance_text = u_utils.get_instance_text(instance)
    switch_list = None

    # "global" should be avoided, but we make an exception for the logger
    global U_LOG # pylint: disable=global-statement
    U_LOG = ULog.get_logger(PROMPT + instance_text)

    # Only one toolchain for Windows (MSVC), the parameter is again
    # retained to allow us to specify a different one should we wish
    # to do so in future
    del toolchain

    # Print out what we've been told to do
    text = "running Windows"
    if clean:
        text += ", clean build"
    if defines:
        text += ", with #define(s)"
        for idx, define in enumerate(defines):
            if idx == 0:
                text += " \"" + define + "\""
            else:
                text += ", \"" + define + "\""
    if unity_dir:
        text += ", using Unity from \"" + unity_dir + "\""
    U_LOG.info(text)

    reporter.event(u_report.EVENT_TYPE_BUILD,
                   u_report.EVENT_START,
                   "Windows")
    # Set up the environment
    returned_env = set_up_environment(reporter)
    if returned_env:
        # When building we must use the set of environment variables
        # returned above.
        print_env(returned_env)
        # Check that everything we need is installed
        # and configured
        if check_installation(TOOLS_LIST, returned_env):
            # Fetch Unity, if necessary
            if not unity_dir:
                if u_utils.fetch_repo(u_utils.UNITY_URL,
                                      u_utils.UNITY_SUBDIR,
                                      None, logger=U_LOG,
                                      submodule_init=False):
                    unity_dir = os.getcwd() + os.sep + u_utils.UNITY_SUBDIR
            if unity_dir:
                # Do the build
                build_start_time = time()
                exe_file = build(clean, unity_dir, defines, returned_env, reporter)
                if exe_file:
                    # Build succeeded, need to lock some things before we can run it
                    reporter.event(u_report.EVENT_TYPE_BUILD,
                                    u_report.EVENT_PASSED,
                                    "build took {:.0f} second(s)".format(time() -
                                                                        build_start_time))
                    # Lock the connection in order to run
                    with u_connection.Lock(connection, connection_lock,
                                            CONNECTION_LOCK_GUARD_TIME_SECONDS,
                                            logger=U_LOG
                                            ) as locked_connection:
                        if locked_connection:
                            switch_list = switch_list_create(defines, logger=U_LOG)
                            if switch_list:
                                # Watch out for strings from the target which
                                # ask us to control switches. Such strings are
                                # of the form:
                                #
                                # AUTOMATION_SET_SWITCH XXX 0
                                #
                                # ...where XXX will match a define of the
                                # form U_CFG_TEST_NET_STATUS_XXX in the
                                # list passed to us from DATABASE.md and
                                # the digit that follows is "0" for "off"
                                # or "1" for "on".
                                u_monitor.callback(callback, SWITCH_CONTROL_REGEX,
                                                   switch_list)
                            # Start the .exe and monitor what it spits out
                            with u_utils.ExeRun([exe_file], logger=U_LOG) as process:
                                return_value = u_monitor.main(process,
                                                              u_monitor.CONNECTION_PROCESS,
                                                              RUN_GUARD_TIME_SECONDS,
                                                              RUN_INACTIVITY_TIME_SECONDS,
                                                              None, instance,
                                                              reporter,
                                                              test_report_file_path)
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
                    return_value = 1
                    reporter.event(u_report.EVENT_TYPE_BUILD,
                                    u_report.EVENT_FAILED,
                                    "check debug log for details")
            else:
                reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                                u_report.EVENT_FAILED,
                                "unable to fetch Unity")
        else:
            reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                            u_report.EVENT_FAILED,
                            "there is a problem with the tools installation for Windows")
    else:
        return_value = 1
        reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                        u_report.EVENT_FAILED,
                        "unable to set up environment")

    return return_value
