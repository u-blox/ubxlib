#!/usr/bin/env python

'''Build/run ubxlib for Windows and report results.'''

import os                    # For sep(), getcwd(), listdir()
from time import time
import u_connection
import u_monitor
import u_report
import u_utils
import u_settings

# Prefix to put at the start of all prints
PROMPT = "u_run_windows_"

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

def print_env(returned_env, printer, prompt):
    '''Print a dictionary that contains the new environment'''
    printer.string("{}environment will be:".format(prompt))
    if returned_env:
        for key, value in returned_env.items():
            printer.string("{}{}={}".format(prompt, key, value))
    else:
        printer.string("{}EMPTY".format(prompt))

def check_installation(tools_list, env, printer, prompt):
    '''Check that everything required has been installed'''
    success = True

    # Check for the tools on the path
    printer.string("{}checking tools...".format(prompt))
    for item in tools_list:
        if u_utils.exe_where(item["which_string"], item["hint"],
                             printer, prompt, set_env=env):
            if item["version_switch"]:
                u_utils.exe_version(item["which_string"],
                                    item["version_switch"],
                                    printer, prompt, set_env=env)
        else:
            success = False

    return success

def set_up_environment(printer, prompt, reporter, keep_going_flag):
    '''Set up the environment ready for building'''
    returned_env = {}
    count = 0

    # Call the setup batch file and grab the environment it
    # creates.  It is possible for capturing the environment
    # variables to fail due to machine loading (see comments
    # against EXE_RUN_QUEUE_WAIT_SECONDS in exe_run) so give this
    # up to three chances to succeed
    while u_utils.keep_going(keep_going_flag, printer, prompt) and \
          not returned_env and (count < 3):
        # set shell to True to keep Jenkins happy
        u_utils.exe_run([MSVC_SETUP_BATCH_FILE], MSVC_SETUP_GUARD_TIME_SECONDS,
                        printer, prompt, shell_cmd=True,
                        returned_env=returned_env,
                        keep_going_flag=keep_going_flag)
        if not returned_env:
            printer.string("{}warning: retrying {} to"     \
                           " capture the environment variables...".
                           format(prompt, " ".join(MSVC_SETUP_BATCH_FILE)))
        count += 1
    if not returned_env:
        reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                       u_report.EVENT_FAILED,
                       "{} failed".format(" ".join(MSVC_SETUP_BATCH_FILE)))
    return returned_env

def build(clean, ubxlib_dir, unity_dir, defines, env, printer, prompt,
          reporter, keep_going_flag):
    '''Build using MSVC'''
    defines_text = ""
    call_list = []
    output_dir = os.getcwd() + os.sep + BUILD_SUBDIR
    cmakelist_dir = ubxlib_dir + os.sep + RUNNER_DIR
    exe_file_path = None

    # Clear the output folder if we're not just running
    if not clean or u_utils.deltree(output_dir,
                                    printer, prompt):
        # Set up the U_FLAGS environment variable
        for idx, define in enumerate(defines):
            if idx == 0:
                defines_text += "-D" + define
            else:
                defines_text += " -D" + define
        printer.string("{}setting environment variable U_FLAGS={}".
                       format(prompt, defines_text))
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
                           printer, prompt, shell_cmd=True,
                           set_env=env,
                           keep_going_flag=keep_going_flag):
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
                               printer, prompt, shell_cmd=True,
                               set_env=env,
                               keep_going_flag=keep_going_flag):
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

def run(instance, mcu, toolchain, connection, connection_lock, platform_lock,
        misc_locks, clean, defines, ubxlib_dir, working_dir,
        printer, reporter, test_report_handle, keep_going_flag=None,
        unity_dir=None):
    '''Build/run on Windows'''
    return_value = -1
    exe_file = None
    instance_text = u_utils.get_instance_text(instance)

    # Don't need the platform or misc locks
    del platform_lock
    del misc_locks

    # MCU is not relevant for Windows but it is included as a parameter
    # in case, in future, we wanted to use it to convey win64.
    del mcu

    # Only one toolchain for Windows (MSVC), the parameter is again
    # retained to allow us to specify a different one should we wish
    # to do so in future
    del toolchain

    prompt = PROMPT + instance_text + ": "

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
    if ubxlib_dir:
        text += ", ubxlib directory \"" + ubxlib_dir + "\""
    if working_dir:
        text += ", working directory \"" + working_dir + "\""
    if unity_dir:
        text += ", using Unity from \"" + unity_dir + "\""
    printer.string("{}{}.".format(prompt, text))

    reporter.event(u_report.EVENT_TYPE_BUILD,
                   u_report.EVENT_START,
                   "Windows")
    # Switch to the working directory
    with u_utils.ChangeDir(working_dir):
        # Set up the environment
        returned_env = set_up_environment(printer, prompt, reporter, keep_going_flag)
        if returned_env:
            # When building we must use the set of environment variables
            # returned above.
            print_env(returned_env, printer, prompt)
            # Check that everything we need is installed
            # and configured
            if u_utils.keep_going(keep_going_flag, printer, prompt) and \
               check_installation(TOOLS_LIST, returned_env, printer, prompt):
                # Fetch Unity, if necessary
                if u_utils.keep_going(keep_going_flag, printer, prompt) and \
                    not unity_dir:
                    if u_utils.fetch_repo(u_utils.UNITY_URL,
                                          u_utils.UNITY_SUBDIR,
                                          None, printer, prompt,
                                          submodule_init=False):
                        unity_dir = os.getcwd() + os.sep + u_utils.UNITY_SUBDIR
                if unity_dir:
                    # Do the build
                    build_start_time = time()
                    exe_file = build(clean, ubxlib_dir, unity_dir, defines,
                                     returned_env, printer, prompt, reporter,
                                     keep_going_flag)
                    if u_utils.keep_going(keep_going_flag, printer, prompt) and \
                       exe_file:
                        # Build succeeded, need to lock some things before we can run it
                        reporter.event(u_report.EVENT_TYPE_BUILD,
                                       u_report.EVENT_PASSED,
                                       "build took {:.0f} second(s)".format(time() -
                                                                            build_start_time))
                        # Lock the connection in order to run
                        with u_connection.Lock(connection, connection_lock,
                                               CONNECTION_LOCK_GUARD_TIME_SECONDS,
                                               printer, prompt,
                                               keep_going_flag) as locked_connection:
                            if locked_connection:
                                # Start the .exe and monitor what it spits out
                                with u_utils.ExeRun([exe_file], printer, prompt) as process:
                                    return_value = u_monitor.main(process.stdout,
                                                                  u_monitor.CONNECTION_PIPE,
                                                                  RUN_GUARD_TIME_SECONDS,
                                                                  RUN_INACTIVITY_TIME_SECONDS,
                                                                  None, instance, printer,
                                                                  reporter,
                                                                  test_report_handle,
                                                                  keep_going_flag=keep_going_flag)
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
