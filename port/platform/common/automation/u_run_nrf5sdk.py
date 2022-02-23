#!/usr/bin/env python

'''Build/run ubxlib for nRF5 SDK and report results.'''

import os                    # For sep(), getcwd()
from time import time, sleep
import subprocess
from pathlib import Path
import u_connection
import u_monitor
import u_report
import u_utils
import u_settings
from u_rtt_reader import URttReader

# Prefix to put at the start of all prints
PROMPT = "u_run_nrf5sdk_"

# Expected bin directory of GCC ARM compiler
GNU_INSTALL_ROOT = u_settings.NRF5SDK_GNU_INSTALL_ROOT # e.g. "C:/Program Files (x86)/GNU Tools ARM Embedded/9 2019-q4-major/bin/"

# Expected prefix for GCC ARM tools
GNU_PREFIX = u_settings.NRF5SDK_GNU_PREFIX # e.g. "arm-none-eabi"

# Expected version of GCC ARM compiler
GNU_VERSION = u_settings.NRF5SDK_GNU_VERSION # e.g. "9.2.1"

# Expected location of nRF5 SDK installation
NRF5SDK_PATH = u_settings.NRF5SDK_NRF5_PATH # e.g. "C:/nrf5"

# The list of things to execute jlink.exe
RUN_JLINK = [u_utils.JLINK_PATH] + u_settings.NRF5SDK_NRF52_RUN_JLINK #

# JLink needs to be treated with kid gloves concerning shutting it down
JLINK_EXIT_DELAY_SECONDS = u_settings.JLINK_EXIT_DELAY_SECONDS # e.g. 10

# The directory where the runner build for GCC can be found
RUNNER_DIR_GCC = u_settings.NRF5SDK_NRF52_RUNNER_DIR_GCC # e.g. "port/platform/nrf5sdk/mcu/nrf52/gcc/runner"

# Prefix of the build sub-directory name for GCC
BUILD_SUBDIR_PREFIX_GCC = u_settings.NRF5SDK_BUILD_SUBDIR_PREFIX_GCC # e.g. "build_"

# The guard time for this build in seconds,
# noting that it can be quite long when
# many builds are running in parallel.
BUILD_GUARD_TIME_SECONDS = u_settings.NRF5SDK_BUILD_GUARD_TIME_SECONDS # e.g. 60 * 30

# The guard time waiting for a lock on the HW connection seconds
CONNECTION_LOCK_GUARD_TIME_SECONDS = u_connection.CONNECTION_LOCK_GUARD_TIME_SECONDS

# The guard time waiting for a install lock in seconds
INSTALL_LOCK_WAIT_SECONDS = u_utils.INSTALL_LOCK_WAIT_SECONDS

# The download guard time for this build in seconds
DOWNLOAD_GUARD_TIME_SECONDS = u_utils.DOWNLOAD_GUARD_TIME_SECONDS

# The guard time for running tests in seconds
RUN_GUARD_TIME_SECONDS = u_utils.RUN_GUARD_TIME_SECONDS

# The inactivity time for running tests in seconds
RUN_INACTIVITY_TIME_SECONDS = u_utils.RUN_INACTIVITY_TIME_SECONDS

# Table of "where.exe" search paths for tools required to be installed for
# each SDK type (None = both) plus hints as to how to install the tools
# and how to read their version
TOOLS_LIST = [{"type": "gcc", "which_string": "make",
               "hint": "can't find make, please install it"                  \
                       " (e.g. from here:"                                   \
                       " http://gnuwin32.sourceforge.net/packages/make.htm)" \
                       " and ensure it is on the path.",
               "version_switch": "--version"},
              {"type": "gcc", "which_string": GNU_INSTALL_ROOT + ":" + GNU_PREFIX + "-gcc{}".format(u_utils.EXE_EXT),
               "hint": "can't find GCC ARM compiler expected to be found"    \
                       " in GNU_INSTALL_ROOT, please EITHER"                 \
                       " install it (no need to add it to the path)"         \
                       " or change the variable GNU_INSTALL_ROOT"            \
                       " to reflect where it is (e.g."                       \
                       " C:/Program Files (x86)/GNU Tools ARM Embedded/9 2019-q4-major/bin/)" \
                       " (and GNU_PREFIX to something like arm-none-eabi).",
               "version_switch": None},
              {"type": None, "which_string": "nrfjprog{}".format(u_utils.EXE_EXT),
               "hint": "couldn't find the nRF5 SDK at NRF5_PATH,"            \
                       " please download the latest version from"            \
                       " https://www.nordicsemi.com/Software-and-tools/"     \
                       "Software/nRF5-SDK/Download#infotabs"                 \
                       " (no need to run an installer, no need for Soft"     \
                       " Device, just unzip the nRF5_blah zip file)"         \
                       " or override the environment variable NRF5_PATH"     \
                       " to reflect where it is.",
               "version_switch": "--version"},
              {"type": None, "which_string": u_utils.JLINK_PATH,
               "hint": "can't find the SEGGER tools, please install"         \
                       " the latest version of their JLink tools from"       \
                       " https://www.segger.com/downloads/jlink/JLink_Windows.exe" \
                       " and add them to the path.",
               "version_switch": None}
]

def check_installation(sdk, tools_list, printer, prompt):
    '''Check that everything required has been installed'''
    success = True

    # Print the environment variables for debug purposes
    printer.string("{}environment is:".format(prompt))
    text = subprocess.check_output(["set",], shell=True)
    for line in text.splitlines():
        printer.string("{}{}".format(prompt, line.decode()))

    # Check for the tools on the path
    printer.string("{}checking tools...".format(prompt))
    for item in tools_list:
        if not item["type"] or (item["type"].lower() == sdk.lower()):
            if u_utils.exe_where(item["which_string"], item["hint"],
                                 printer, prompt):
                if item["version_switch"]:
                    u_utils.exe_version(item["which_string"],
                                        item["version_switch"],
                                        printer, prompt)
            else:
                success = False

    return success

def download(connection, guard_time_seconds, hex_path, printer, prompt):
    '''Download the given hex file to an attached NRF52 board'''
    call_list = []

    # Assemble the call list
    call_list.append("nrfjprog")
    call_list.append("-f")
    call_list.append("nrf52")
    call_list.append("--program")
    call_list.append(hex_path)
    call_list.append("--chiperase")
    call_list.append("--verify")
    if connection and "debugger" in connection and connection["debugger"]:
        call_list.append("-s")
        call_list.append(connection["debugger"])

    # Call it
    return u_utils.exe_run(call_list, guard_time_seconds, printer, prompt)

def build_gcc(clean, build_subdir, unity_dir,
              defines, printer, prompt, reporter, keep_going_flag):
    '''Build on GCC'''
    call_list = []
    hex_file_path = None

    makefile = u_utils.UBXLIB_DIR + os.sep + RUNNER_DIR_GCC + os.sep + "Makefile"
    outputdir = os.getcwd() + os.sep + build_subdir

    # The Nordic Makefile.common that is included by our Makefile
    # is quite limited and weird behaiviours:
    # 1. It is not possible to specify an OUTPUT_DIRECTORY that
    #    is not on the same drive as the source code. In our case
    #    the source code is mounted as a subst device in the Windows
    #    case.
    # 2. Makefile.common expects having a "Makefile" in the current
    #    directory. However, since we want the build output to be placed
    #    outside the source tree and due to 1) we want to call our
    #    Makefile using "make -f $UBXLIB_DIR/$RUNNER_DIR_GCC/Makefile"
    #    from a workdir. In this case the Makefile will NOT be located
    #    in current directory. So to get nRF5 SDK Makefile.common happy
    #    we fake this Makefile with an empty file:
    Path('./Makefile').touch()

    # Clear the output folder if we're not just running
    if not clean or u_utils.deltree(outputdir,
                                    printer, prompt):
        if defines:
            # Create the CFLAGS string
            cflags = ""
            for idx, define in enumerate(defines):
                if idx == 0:
                    cflags = "-D" + define
                else:
                    cflags += " -D" + define
        # Note: when entering things from the command-line
        # if there is more than one CFLAGS parameter then
        # they must be quoted but that is specifically
        # NOT required here as the fact that CFLAGS
        # is passed in as one array entry is sufficient

        # Assemble the whole call list
        call_list += ["make", "-j8", "-f", makefile]
        call_list.append("NRF5_PATH=" + NRF5SDK_PATH)
        call_list.append("UNITY_PATH=" + unity_dir.replace("\\", "/"))
        if defines:
            call_list.append("CFLAGS=" + cflags)
        call_list.append("OUTPUT_DIRECTORY=" + build_subdir)
        call_list.append("GNU_VERSION=" + GNU_VERSION)
        call_list.append("GNU_PREFIX=" + GNU_PREFIX)
        call_list.append("GNU_INSTALL_ROOT=" + GNU_INSTALL_ROOT)

        # Call make to do the build
        # Set shell to keep Jenkins happy
        if u_utils.exe_run(call_list, BUILD_GUARD_TIME_SECONDS,
                           printer, prompt, shell_cmd=True,
                           keep_going_flag=keep_going_flag):
            hex_file_path = outputdir +  \
                            os.sep + "nrf52840_xxaa.hex"
    else:
        reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                        u_report.EVENT_FAILED,
                        "unable to clean build directory")

    return hex_file_path

def run(instance, mcu, toolchain, connection, connection_lock,
        platform_lock, misc_locks, clean, defines,
        printer, reporter, test_report_handle,
        keep_going_flag=None, unity_dir=None):
    '''Build/run on nRF5'''
    return_value = -1
    hex_file_path = None
    instance_text = u_utils.get_instance_text(instance)
    downloaded = False
    _ = (misc_locks) # Suppress unused variable

    # Don't need the platform lock
    del platform_lock

    prompt = PROMPT + instance_text + ": "

    # Print out what we've been told to do
    text = "running nRF5 for " + mcu + " under " + toolchain
    if connection and "debugger" in connection and connection["debugger"]:
        text += ", on JLink debugger serial number " + connection["debugger"]
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
    printer.string("{}{}.".format(prompt, text))

    reporter.event(u_report.EVENT_TYPE_BUILD,
                   u_report.EVENT_START,
                   "nRF5SDK/" + toolchain)
    # Check that everything we need is installed
    if u_utils.keep_going(keep_going_flag, printer, prompt) and \
        check_installation(toolchain, TOOLS_LIST, printer, prompt):
        # Fetch Unity, if necessary
        if u_utils.keep_going(keep_going_flag, printer, prompt) and \
           check_installation(toolchain, TOOLS_LIST, printer, prompt):
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
                if u_utils.keep_going(keep_going_flag, printer, prompt):
                    if toolchain.lower() == "gcc":
                        build_subdir_gcc = BUILD_SUBDIR_PREFIX_GCC + instance_text.replace(".", "_")
                        hex_file_path = build_gcc(clean, build_subdir_gcc, unity_dir,
                                                  defines, printer, prompt, reporter, keep_going_flag)
                if hex_file_path:
                    # Build succeeded, need to lock a connection to do the download
                    reporter.event(u_report.EVENT_TYPE_BUILD,
                                   u_report.EVENT_PASSED,
                                   "build took {:.0f} second(s)".format(time() -
                                                                        build_start_time))
                    # Do the download
                    with u_connection.Lock(connection, connection_lock,
                                           CONNECTION_LOCK_GUARD_TIME_SECONDS,
                                           printer, prompt,
                                           keep_going_flag) as locked_connection:
                        if locked_connection:
                            reporter.event(u_report.EVENT_TYPE_DOWNLOAD,
                                           u_report.EVENT_START)
                            # I have seen the download fail on occasion
                            # so give this two bites of the cherry
                            retries = 2
                            while not downloaded and (retries > 0):
                                downloaded = download(connection,
                                                      DOWNLOAD_GUARD_TIME_SECONDS,
                                                      hex_file_path,
                                                      printer, prompt)
                                retries -= 1
                                if not downloaded and (retries > 0):
                                    reporter.event(u_report.EVENT_TYPE_DOWNLOAD,
                                                   u_report.EVENT_WARNING,
                                                   "unable to download, will retry...")
                                    sleep(5)
                            if downloaded:
                                reporter.event(u_report.EVENT_TYPE_DOWNLOAD,
                                                u_report.EVENT_COMPLETE)

                                # Now the target can be reset
                                u_utils.reset_nrf_target(connection,
                                                            printer, prompt)
                                reporter.event(u_report.EVENT_TYPE_TEST,
                                                u_report.EVENT_START)

                                with URttReader("NRF52840_XXAA",
                                                jlink_serial=connection["debugger"],
                                                printer=printer,
                                                prompt=prompt) as rtt_reader:
                                    return_value = u_monitor.main(rtt_reader,
                                                                    u_monitor.CONNECTION_RTT,
                                                                    RUN_GUARD_TIME_SECONDS,
                                                                    RUN_INACTIVITY_TIME_SECONDS,
                                                                    "\n", instance, printer,
                                                                    reporter,
                                                                    test_report_handle)

                                if return_value == 0:
                                    reporter.event(u_report.EVENT_TYPE_TEST,
                                                    u_report.EVENT_COMPLETE)
                                else:
                                    reporter.event(u_report.EVENT_TYPE_TEST,
                                                    u_report.EVENT_FAILED)
                            else:
                                reporter.event(u_report.EVENT_TYPE_DOWNLOAD,
                                                u_report.EVENT_FAILED,
                                                "check debug log for details")
                            # Wait for a short while before giving
                            # the connection lock away to make sure
                            # that everything really has shut down
                            # in the debugger
                            sleep(5)
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
                        "there is a problem with the tools installation for nRF5 SDK")

    return return_value
