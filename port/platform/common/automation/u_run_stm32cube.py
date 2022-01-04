#!/usr/bin/env python

'''Build/run ubxlib for STM32Cube IDE and report results.'''

import os
from time import sleep, time
from multiprocessing import Process  # To launch swo_decoder.py
import socket
import u_monitor
import u_connection
import u_report
import u_utils
import u_settings

# Note: the tools provided with the STM32Cube IDE
# are entirely GUI based.  However, under the hood,
# there are command-line executables which can be employed.
# This script calls upon OpenOCD, which is provided
# as part of the STM32Cube IDE but is rather well
# hidden, along with the configuration files that
# ST provide for their processors, also well hidden.
#
# You will see quite a few VERY long filenames here,
# 'cos that's just the way the STM32Cube IDE is
# structured. Apologies.

# Prefix to put at the start of all prints
PROMPT = "u_run_stm32cube_"

# Location of the ARM Embedded Toolchain directory
ARM_GCC_TOOLCHAIN_PATH = u_settings.STM32CUBE_ARM_GNU_INSTALL_ROOT

# Location of the STM32F4 Cube FW (i.e. their C drivers)
# directory
STM32CUBE_FW_PATH = u_settings.STM32CUBE_STM32CUBE_FW_PATH # e.g. "C:\\STM32Cube_FW_F4"

# Location of the OpenOCD executable
OPENOCD_PATH = u_settings.STM32CUBE_XPACK_OPENOCD_PATH

# The STM32F4 cube SDK directory in ubxlib
SDK_DIR = "port/platform/stm32cube"

# The core clock rate of the STM32F4 processor, i.e. SystemCoreClock
SYSTEM_CORE_CLOCK_HZ = u_settings.STM32CUBE_SYSTEM_CORE_CLOCK_HZ #

# The SWO data rate: should be the same as U_CFG_HW_SWO_CLOCK_HZ
# from u_cfg_hw_platform_specific.h for the STM32F4 platform,
# if that is provided.
SWO_CLOCK_HZ = u_settings.STM32CUBE_SWO_CLOCK_HZ #

# The name of the temporary file in which SWO data is stored
SWO_DATA_FILE = u_settings.STM32CUBE_SWO_DATA_FILE #

# The name of the temporary file in which decoded SWO text
# is stored by swo_decode_process
SWO_DECODED_TEXT_FILE = u_settings.STM32CUBE_SWO_DECODED_TEXT_FILE #

# The list of commands to be sent to OpenOCD when it is opened
OPENOCD_COMMANDS = ["init", # Always required at the start
                    # Write SWO data to file at the given rate
                    "tpiu config internal " + SWO_DATA_FILE +         \
                    " uart off " + str(SYSTEM_CORE_CLOCK_HZ) + " " +  \
                    str(SWO_CLOCK_HZ),
                    "itm port 0 on", # ITM on
                    "reset init",  # Reset the processor
                    "resume"]  # Start the processor

# The guard time for this build in seconds,
# noting that it can be quite long when
# many builds are running in parallel.
BUILD_GUARD_TIME_SECONDS = u_settings.STM32CUBE_BUILD_GUARD_TIME_SECONDS # e.g. 60 * 30

# The guard time waiting for a lock on the HW connection seconds
CONNECTION_LOCK_GUARD_TIME_SECONDS = u_connection.CONNECTION_LOCK_GUARD_TIME_SECONDS

# The guard time to lock the whole STM32Cube platform in seconds
PLATFORM_LOCK_GUARD_TIME_SECONDS = u_utils.PLATFORM_LOCK_GUARD_TIME_SECONDS

# The download guard time for this build in seconds
DOWNLOAD_GUARD_TIME_SECONDS = u_utils.DOWNLOAD_GUARD_TIME_SECONDS

# The time allowed for all STM32F4 downloads to complete
DOWNLOADS_COMPLETE_GUARD_TIME_SECONDS = 3600

# The guard time for running tests in seconds
RUN_GUARD_TIME_SECONDS = u_utils.RUN_GUARD_TIME_SECONDS

# The inactivity time for running tests in seconds
RUN_INACTIVITY_TIME_SECONDS = u_utils.RUN_INACTIVITY_TIME_SECONDS

# Table of paths that must exist plus hints as to how to
# install the things if they're not
PATHS_LIST = [
    {"name": "OPENOCD_PATH",
             "path_string": OPENOCD_PATH,
             "hint": "can't find OpenOCD. Please install OpenOCD v0.11"  \
                     " and make sure STM32CUBE_XPACK_OPENOCD_PATH is"   \
                     " set to the correct path."}
]

CFG_DIR = os.path.abspath(os.path.dirname(__file__) + "/cfg")

class UbxError(Exception):
    def __init__(self, type, message=None):
        self.type = type
        self.message = message

def check_installation(paths_list, printer, prompt):
    '''Check that everything required has been installed'''
    success = True

    # Check for the paths
    printer.string("{}checking paths...".format(prompt))
    for item in paths_list:
        if not os.path.exists(item["path_string"]):
            printer.string("{}{} ({}): {}".format(prompt,
                                                  item["path_string"],
                                                  item["name"],
                                                  item["hint"]))
            success = False

    return success


def build_gcc(clean, makefile_dir, build_subdir, ubxlib_dir, unity_dir,
              defines, printer, prompt, reporter, keep_going_flag):
    '''Build on GCC'''
    call_list = []
    elf_path = None

    outputdir = os.getcwd() + os.sep + build_subdir

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
        call_list += ["make", "-j8", "-C", makefile_dir]
        call_list.append("ARM_GCC_TOOLCHAIN_PATH=" + ARM_GCC_TOOLCHAIN_PATH)
        call_list.append("STM32CUBE_FW_PATH=" + STM32CUBE_FW_PATH)
        call_list.append("UNITY_PATH=" + unity_dir.replace("\\", "/"))
        if defines:
            call_list.append("CFLAGS=" + cflags)
        call_list.append("OUTPUT_DIRECTORY=" + outputdir)

        # Call make to do the build
        # Set shell to keep Jenkins happy
        if u_utils.exe_run(call_list, BUILD_GUARD_TIME_SECONDS,
                           printer, prompt, shell_cmd=True,
                           keep_going_flag=keep_going_flag):
            elf_path = outputdir + os.sep + "runner.elf"
    else:
        reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                        u_report.EVENT_FAILED,
                        "unable to clean build directory")

    return elf_path


def download(connection, guard_time_seconds, elf_path, printer, prompt):
    '''Download the given binary file'''
    call_list = [
        OPENOCD_PATH,
        "-f", f"{CFG_DIR}/stm32f4.cfg",
    ]

    if connection and "debugger" in connection and connection["debugger"]:
        # Connect to the given debugger
        call_list += [ "-c", "hla_serial " + connection["debugger"] ]

    elf_path = elf_path.replace("\\", "/")
    call_list += [
        "-c", f"program {elf_path} reset",
        "-c", "exit"
    ]

    # Call it
    return u_utils.exe_run(call_list, guard_time_seconds, printer, prompt)

def open_ocd(commands, connection):
    '''Assemble the command line for OpenOCD: it's a doozy'''
    call_list = [
        OPENOCD_PATH,
        "-f", f"{CFG_DIR}/stm32f4.cfg"
    ]

    if connection and "debugger" in connection and connection["debugger"]:
        call_list += [ "-c", "hla_serial " + connection["debugger"] ]
    for command in commands:
        call_list += [ "-c", command ]

    return call_list

def swo_decode_process(swo_data_file, swo_decoded_text_file):
    '''Grab SWO data from a file, decode it and write it to another file'''
    file_handle_in = open(swo_data_file, "rb")
    file_handle_out = open(swo_decoded_text_file, "wb")
    decoder = u_utils.SwoDecoder(0, True)
    try:
        # Will exit if on CTRL-C
        while True:
            try:
                decoded_data = decoder.decode(file_handle_in.read(1024))
                if len(decoded_data) > 0:
                    file_handle_out.write(decoded_data)
                    file_handle_out.flush()
                else:
                    # Since this is a busy/wait we sleep a bit if there is no data
                    # to offload the CPU
                    sleep(0.01)
            except socket.timeout:
                pass
    except (KeyboardInterrupt, OSError, ConnectionAbortedError):
        file_handle_out.close()
        file_handle_in.close()

# IMPORTANT: Eclipse, on which the STM32Cube is based
# has a very peculiar and immutable way of dealing with paths.
# To work around this while preventing Git from thinking we've
# made changes to code, we make a copy of the project
# from "blah" to "test_only_blah", which is in the
# .gitignore file, and build that instead of "runner".

def run(instance, mcu, toolchain, connection, connection_lock,
        platform_lock, misc_locks, clean, defines, ubxlib_dir,
        working_dir, printer, reporter, test_report_handle,
        keep_going_flag=None, unity_dir=None):
    '''Build/run on STM32Cube'''
    return_value = -1
    mcu_dir = ubxlib_dir + os.sep + SDK_DIR + os.sep + "mcu" + os.sep + mcu.lower()
    runner_dir = mcu_dir + os.sep + "runner"
    instance_text = u_utils.get_instance_text(instance)
    elf_path = None
    downloaded = False
    download_list = None

    # Only one toolchain for STM32Cube
    del toolchain

    prompt = PROMPT + instance_text + ": "

    # Print out what we've been told to do
    text = "running STM32Cube for " + mcu
    if connection and "debugger" in connection and connection["debugger"]:
        text += ", on STLink debugger serial number " + connection["debugger"]
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

    # On STM32F4 we can get USB errors if we try to do a download
    # on one platform while another is performing SWO logging.
    # Since each board only runs a single instance of stuff we
    # can work around this be ensuring that all downloads are
    # completed before SWO logging begins.
    # Add us to the list of pending downloads
    if misc_locks and ("stm32f4_downloads_list" in misc_locks):
        download_list = misc_locks["stm32f4_downloads_list"]
        download_list.append(instance_text)

    reporter.event(u_report.EVENT_TYPE_BUILD,
                   u_report.EVENT_START,
                   "STM32Cube")
    if not working_dir:
        working_dir = "."

    try:
        # Switch to the working directory
        with u_utils.ChangeDir(working_dir):
            # Check that everything we need is installed
            if u_utils.keep_going(keep_going_flag, printer, prompt):
                if not check_installation(PATHS_LIST, printer, prompt):
                    raise UbxError(u_report.EVENT_TYPE_INFRASTRUCTURE,
                                   "there is a problem with the tools installation for STM32F4")

            # Fetch Unity, if necessary
            if u_utils.keep_going(keep_going_flag, printer, prompt) and not unity_dir:
                if not u_utils.fetch_repo(u_utils.UNITY_URL,
                                          u_utils.UNITY_SUBDIR,
                                          None, printer, prompt,
                                          submodule_init=False):
                    raise UbxError(u_report.EVENT_TYPE_INFRASTRUCTURE,
                                   "unable to fetch Unity")

                unity_dir = os.getcwd() + os.sep + u_utils.UNITY_SUBDIR

            if u_utils.keep_going(keep_going_flag, printer, prompt):
                # Do the build
                build_start_time = time()
                elf_path = build_gcc(clean, runner_dir, "build", ubxlib_dir, \
                                    unity_dir, defines, printer,prompt, \
                                    reporter, keep_going_flag)

                if elf_path is None:
                    raise UbxError(u_report.EVENT_TYPE_BUILD,
                                   "check debug log for details")
                reporter.event(u_report.EVENT_TYPE_BUILD,
                               u_report.EVENT_PASSED,
                               "build took {:.0f} second(s)".format(time() -
                                                                    build_start_time))

            if u_utils.keep_going(keep_going_flag, printer, prompt):
                # Lock the connection.
                with u_connection.Lock(connection, connection_lock,
                                    CONNECTION_LOCK_GUARD_TIME_SECONDS,
                                    printer, prompt, keep_going_flag) as locked_connection:
                    if not locked_connection:
                        raise UbxError(u_report.EVENT_TYPE_INFRASTRUCTURE,
                                        "unable to lock a connection")

                    # I have seen download failures occur if two
                    # ST-Link connections are initiated at the same time.
                    with u_utils.Lock(platform_lock, PLATFORM_LOCK_GUARD_TIME_SECONDS,
                                    "platform", printer, prompt,
                                    keep_going_flag) as locked_platform:
                        if locked_platform:
                            reporter.event(u_report.EVENT_TYPE_DOWNLOAD,
                                        u_report.EVENT_START)
                            # Do the download.  I have seen the STM32F4 debugger
                            # barf on occasions so give this two bites of
                            # the cherry
                            retries = 2
                            while u_utils.keep_going(keep_going_flag,
                                                    printer, prompt) and \
                                not downloaded and (retries > 0):
                                downloaded = download(connection,
                                                    DOWNLOAD_GUARD_TIME_SECONDS,
                                                    elf_path, printer, prompt)
                                retries -= 1
                                if not downloaded:
                                    if connection and "serial_port" in connection \
                                    and connection["serial_port"]:
                                        # Before retrying, reset the USB port
                                        u_utils.usb_reset("STMicroelectronics STLink" \
                                                        "Virtual COM Port (" +
                                                        connection["serial_port"] +
                                                        ")", printer, prompt)
                                    sleep(5)
                            if platform_lock:
                                # Once the download has been done (or not) the platform lock
                                # can be released, after a little safety sleep
                                sleep(1)
                                platform_lock.release()

                            if not downloaded:
                                raise UbxError(u_report.EVENT_TYPE_DOWNLOAD)

                            reporter.event(u_report.EVENT_TYPE_DOWNLOAD,
                                           u_report.EVENT_COMPLETE)
                            # Remove us from the list of pending downloads
                            if download_list:
                                download_list.remove(instance_text)
                                # Wait for all the other downloads to complete before
                                # starting SWO logging
                                u_utils.wait_for_completion(download_list,
                                                            "STM32F4 downloads",
                                                            DOWNLOADS_COMPLETE_GUARD_TIME_SECONDS,
                                                            printer, prompt, keep_going_flag)
                            # So that all STM32Cube instances don't start up at
                            # once, which can also cause problems, wait the
                            # instance-number number of seconds.
                            hold_off = instance[0]
                            if hold_off > 30:
                                hold_off = 30
                            sleep(hold_off)
                            # Create and empty the SWO data file and decoded text file
                            file_handle = open(SWO_DATA_FILE, "w").close()
                            file_handle = open(SWO_DECODED_TEXT_FILE, "w").close()
                            reporter.event(u_report.EVENT_TYPE_TEST,
                                        u_report.EVENT_START)
                            try:
                                # Start a process which reads the
                                # SWO output from a file, decodes it and
                                # writes it back to a file
                                process = Process(target=swo_decode_process,
                                                args=(SWO_DATA_FILE,
                                                        SWO_DECODED_TEXT_FILE))
                                process.start()
                                # Now start Open OCD to reset the target
                                # and capture SWO output
                                sleep(1)
                                with u_utils.ExeRun(open_ocd(OPENOCD_COMMANDS,
                                                            connection),
                                                    printer, prompt):
                                    # Open the SWO decoded text file for
                                    # reading, binary to prevent the line
                                    # endings being munged.
                                    file_handle = open(SWO_DECODED_TEXT_FILE, "rb")
                                    # Monitor progress based on the decoded
                                    # SWO text
                                    return_value = u_monitor.          \
                                                main(file_handle,
                                                     u_monitor.CONNECTION_PIPE,
                                                     RUN_GUARD_TIME_SECONDS,
                                                     RUN_INACTIVITY_TIME_SECONDS,
                                                     "\r", instance, printer,
                                                     reporter,
                                                     test_report_handle,
                                                     keep_going_flag=keep_going_flag)
                                    file_handle.close()
                                process.terminate()
                            except KeyboardInterrupt:
                                # Tidy up process on SIGINT
                                printer.string("{}caught CTRL-C, terminating...".
                                            format(prompt))
                                process.terminate()
                                return_value = -1
                            if return_value == 0:
                                reporter.event(u_report.EVENT_TYPE_TEST,
                                               u_report.EVENT_COMPLETE)
                            else:
                                raise UbxError(u_report.EVENT_TYPE_TEST)


    except UbxError as error:
        reporter.event(error.type,
                       u_report.EVENT_FAILED,
                       error.message)
    finally:
        # Remove us from the list of pending downloads for safety
        try:
            misc_locks["stm32f4_downloads_list"].remove(instance_text)
        except (AttributeError, ValueError, TypeError):
            pass

    return return_value
