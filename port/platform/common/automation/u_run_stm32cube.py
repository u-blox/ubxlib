#!/usr/bin/env python

'''Build/run ubxlib for STM32Cube IDE and report results.'''

import os
from time import sleep, time
from multiprocessing import Process  # To launch swo_decoder.py
import socket
import subprocess
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

# Location of the STM32F4 Cube FW (i.e. their C drivers)
# directory
STM32CUBE_FW_PATH = u_settings.STM32CUBE_STM32CUBE_FW_PATH # e.g. "C:\\STM32Cube_FW_F4"

# Location of the STM32Cube IDE directory
STM32CUBE_IDE_PATH = u_settings.STM32CUBE_STM32CUBE_IDE_PATH # e.g. "C:\\ST\\STM32CubeIDE_1.4.0\\STM32CubeIDE"

# Location of STM32_Programmer_CLI.exe in
# the STM32Cube IDE directory
STM32_PROGRAMMER_CLI_PATH = u_settings.STM32CUBE_STM32_PROGRAMMER_CLI_PATH #

# Location of the OpenOCD executable, which can
# be found deep in the STM32Cube IDE directories
OPENOCD_PATH = u_settings.STM32CUBE_OPENOCD_PATH #

# Location of the OpenOCD scripts directory, which can
# also be found deep in the STM32Cube IDE directories
OPENOCD_SCRIPTS_PATH = u_settings.STM32CUBE_OPENOCD_SCRIPTS_PATH #

# The OpenOCD script file provided by ST for their STLink interface
OPENOCD_STLINK_INTERFACE_SCRIPT = u_settings.STM32CUBE_OPENOCD_STLINK_INTERFACE_SCRIPT #

# The OpenOCD script file provided by ST for their STM32F4 target
OPENOCD_STM32F4_TARGET_SCRIPT = u_settings.STM32CUBE_OPENOCD_STM32F4_TARGET_SCRIPT #

# The subdirectory of the working directory in which the
# STM32F4 Cube IDE should create the workspace
STM32CUBE_IDE_WORKSPACE_SUBDIR = u_settings.STM32CUBE_IDE_WORKSPACE_SUBDIR #

# The STM32F4 cube SDK directory in ubxlib
SDK_DIR = u_settings.STM32CUBE_SDK_DIR #

# The project name to build
PROJECT_NAME = u_settings.STM32CUBE_PROJECT_NAME #

# The prefix that forms the modified project name that
# is created with updated paths
UPDATED_PROJECT_NAME_PREFIX = u_settings.STM32CUBE_UPDATED_PROJECT_NAME_PREFIX #

# The name of the configuration to build
PROJECT_CONFIGURATION = u_settings.STM32CUBE_PROJECT_CONFIGURATION #

# The maximum number of U_FLAGx defines that the STM32F4Cube
# project file can take
MAX_NUM_DEFINES = u_settings.STM32CUBE_MAX_NUM_DEFINES #

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
                    "itm port 0 on", # ITM on
                    # Write SWO data to file at the given rate
                    "tpiu config internal " + SWO_DATA_FILE +         \
                    " uart off " + str(SYSTEM_CORE_CLOCK_HZ) + " " +  \
                    str(SWO_CLOCK_HZ),
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
PATHS_LIST = [{"name": "STM32CUBE_IDE_PATH",
               "path_string": STM32CUBE_IDE_PATH,
               "hint": "can't find the STM32Cube IDE. Please either"        \
                       " install it from"                                   \
                       " https://www.st.com/en/development-tools/stm32cubeide.html" \
                       " or change STM32CUBE_IDE_PATH to point to where it" \
                       " is."},
              {"name": "STM32_PROGRAMMER_CLI_PATH",
               "path_string": STM32_PROGRAMMER_CLI_PATH,
               "hint": "can't find the STM32 programmer CLI, it should be"  \
                       " provided as part of the STM32Cube IDE. Maybe you"  \
                       " upgraded it and haven't update the paths?"},
              {"name": "OPENOCD_PATH",
               "path_string": OPENOCD_PATH,
               "hint": "can't find OpenOCD, it should be provided as part"  \
                       " of the STM32Cube IDE. Maybe you upgraded it and"   \
                       " haven't update the paths?"},
              {"name": "OPENOCD_SCRIPTS_PATH",
               "path_string": OPENOCD_SCRIPTS_PATH,
               "hint": "can't find the ST configuration scripts for OpenOCD" \
                       " they should be provided as part of the STM32Cube"   \
                       " IDE. Maybe you upgraded it and haven't update the"  \
                       " paths?"},
              {"name": "OPENOCD_STLINK_INTERFACE_SCRIPT",
               "path_string": OPENOCD_SCRIPTS_PATH + os.sep + "interface" + \
                              os.sep + OPENOCD_STLINK_INTERFACE_SCRIPT,
               "hint": "can't find the STLink interface script for OpenOCD." \
                       " It should be provided as part of the STM32Cube"     \
                       " IDE. Maybe you upgraded it and haven't update the"  \
                       " paths?"},
              {"name": "OPENOCD_STM32F4_TARGET_SCRIPT",
               "path_string": OPENOCD_SCRIPTS_PATH + os.sep + "target" + \
                              os.sep + OPENOCD_STM32F4_TARGET_SCRIPT,
               "hint": "can't find the STM32F4 target script for OpenOCD." \
                       " It should be provided as part of the STM32Cube"     \
                       " IDE. Maybe you upgraded it and haven't update the"  \
                       " paths?"}]

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

def replace_variable_list_value(string, old_value, new_value):
    '''Replace location URIs with absolute locations in .project'''
    # Find <locationURI>$%7Bold_value%7D...</locationURI>
    # and replace it with <location>new_value...</location>
    new_string = ""
    find_string = "<locationURI>$%7B" + old_value + "%7D"
    find_string_len = len(find_string)
    replace_string = "<location>" + new_value.replace("\\", "/")

    for line in string.splitlines():
        pos1 = line.find(find_string)
        if pos1 >= 0:
            # Find the start of </locationURI>
            pos2 = line.find("</locationURI>")
            if pos2 >= 0:
                # This is one we want to replace
                # Take up to the start of find_string first
                new_string += line[:pos1]
                # Then put in the replacement string bit
                new_string += replace_string
                # Copy in the bits between the end of find_string
                # and the start of <locationURI>
                new_string += line[pos1 + find_string_len:pos2]
                # Add <location> on the end
                new_string += "</location>\n"
        else:
            # Not an interesting line, let it pass through
            new_string += line + "\n"

    return new_string

def create_project(project_path, old_project_name, new_project_name,
                   stm32cube_fw_path, unity_dir,
                   printer, prompt):
    '''Create a new project with the right paths'''
    new_project_path = project_path + os.sep + new_project_name
    success = False

    # If there is already a project with our intended name,
    # delete it
    if u_utils.deltree(new_project_path, printer, prompt):

        # Create the new project directory
        printer.string("{}creating {}...".format(prompt,
                                                 new_project_path))
        os.makedirs(new_project_path)

        # Read the .cproject file from the old project
        printer.string("{}reading .cproject file`...".format(prompt))
        file_handle = open(project_path + os.sep + old_project_name +
                           os.sep + ".cproject", "r")
        string = file_handle.read()
        file_handle.close()

        # Write it out to the new
        printer.string("{}writing .cproject file...".format(prompt))
        file_handle = open(project_path + os.sep + new_project_name +
                           os.sep + ".cproject", "w")
        file_handle.write(string)
        file_handle.close()

        # Read the .project file from the old project
        printer.string("{}reading .project file...".format(prompt))
        file_handle = open(project_path + os.sep + old_project_name +
                           os.sep + ".project", "r")
        string = file_handle.read()
        file_handle.close()

        # Replace "<name>blah</name>" with "<name>test_only_blah</name>
        printer.string("{}changing name in .cproject file from \"{}\"" \
                       " to \"{}\"...".format(prompt, old_project_name,
                                              new_project_name))
        string = string.replace("<name>" + old_project_name + "</name>",
                                "<name>" + new_project_name + "</name>", 1)

        # Replace STM32CUBE_FW_PATH
        printer.string("{}updating STM32CUBE_FW_PATH to \"{}\"...".    \
                       format(prompt, stm32cube_fw_path))
        string = replace_variable_list_value(string, "STM32CUBE_FW_PATH",
                                             stm32cube_fw_path)
        # Replace UNITY_PATH
        printer.string("{}updating UNITY_PATH to \"{}\"...".           \
                       format(prompt, unity_dir))
        string = replace_variable_list_value(string, "UNITY_PATH",
                                             unity_dir)

        # Write it out to the new
        printer.string("{}writing .project file...".format(prompt))
        file_handle = open(project_path + os.sep + new_project_name +
                           os.sep + ".project", "w")
        file_handle.write(string)
        file_handle.close()

        # Write in a warning file just in case anyone
        # wonders what the hell this weird project is
        file_handle = open(project_path + os.sep + new_project_name +
                           os.sep + "ignore_this_directory.txt", "w")
        file_handle.write("See u_run_stm32cube.py for an explanation.")
        file_handle.close()

        success = True

    return success

def build_binary(mcu_dir, workspace_subdir, project_name, clean, defines,
                 printer, prompt):
    '''Build'''
    call_list = []
    build_dir = mcu_dir + os.sep + project_name + os.sep + PROJECT_CONFIGURATION
    num_defines = 0
    too_many_defines = False
    elf_path = None

    # The STM32Cube IDE doesn't provide
    # a mechanism to override the build
    # output directory in the .cproject file
    # from the command-line so I'm afraid
    # all output will end up in a
    # sub-directory with the name of the
    # PROJECT_CONFIGURATION off the project
    # directory.  <sigh>
    printer.string("{}building in {}.".format(prompt, build_dir))

    if not clean or u_utils.deltree(build_dir,
                                    printer, prompt):
        for idx, define in enumerate(defines):
            # Add the #defines as environment variables
            # Note that these must be deleted afterwards
            # in case someone else is going to use the
            # worker that this was run in
            if idx >= MAX_NUM_DEFINES:
                too_many_defines = True
                printer.string("{}{} #defines"          \
                               " supplied but only"     \
                               " {} are supported by"   \
                               " this STM32Cube IDE"    \
                               " project file".format(prompt,
                                                      len(defines),
                                                      MAX_NUM_DEFINES))
                break
            os.environ["U_FLAG" + str(idx)] = "-D" + define
            num_defines += 1

        # Print the environment variables for debug purposes
        printer.string("{}environment is:".format(prompt))
        text = subprocess.check_output(["set",], shell=True)
        for line in text.splitlines():
            printer.string("{}{}".format(prompt, line.decode()))

        if not too_many_defines:
            # Delete the workspace sub-directory first if it is there
            # to avoid the small chance that the name has been used
            # previously, in which case the import would fail
            u_utils.deltree(workspace_subdir, printer, prompt)

            # Assemble the whole call list
            #
            # The documentation for command-line, AKA
            # headless, use of Eclipse can be found here:
            # https://gnu-mcu-eclipse.github.io/advanced/headless-builds/
            #
            # And you can get help by running stm32cubeidec with
            # the command-line:
            #
            # stm32cubeidec.exe --launcher.suppressErrors -nosplash
            # -application org.eclipse.cdt.managedbuilder.core.headlessbuild
            # -data PATH_TO_YOUR_WORKSPACE -help
            #
            # This information found nailed to the door of the
            # bog in the basement underneath the "beware of the
            # leopard" sign
            call_list.append(STM32CUBE_IDE_PATH + os.sep + "stm32cubeidec.exe")
            call_list.append("--launcher.suppressErrors")
            call_list.append("-nosplash")
            call_list.append("-application")
            call_list.append("org.eclipse.cdt.managedbuilder.core.headlessbuild")
            call_list.append("-data")
            call_list.append(workspace_subdir)
            call_list.append("-import")
            call_list.append(mcu_dir + os.sep + project_name)
            call_list.append("-no-indexer")
            call_list.append("-build")
            call_list.append(project_name + "/" + PROJECT_CONFIGURATION)
            call_list.append("-console")

            # Print what we're gonna do
            tmp = ""
            for item in call_list:
                tmp += " " + item
            printer.string("{}in directory {} calling{}".         \
                           format(prompt, os.getcwd(), tmp))

            # Call stm32cubeidec.exe to do the build
            if (u_utils.exe_run(call_list, BUILD_GUARD_TIME_SECONDS,
                                printer, prompt)):
                # The binary should be
                elf_path = build_dir + os.sep + project_name + ".elf"

        # Delete the environment variables again
        while num_defines > 0:
            num_defines -= 1
            del os.environ["U_FLAG" + str(num_defines)]

    return elf_path

def download(connection, guard_time_seconds, elf_path, printer, prompt):
    '''Download the given binary file'''
    call_list = []

    call_list.append(STM32_PROGRAMMER_CLI_PATH)
    call_list.append("-q") # no progress bar
    call_list.append("-c")       # connect
    call_list.append("port=SWD") # via SWD
    if connection and "debugger" in connection and connection["debugger"]:
        # Connect to the given debugger
        call_list.append("sn=" + connection["debugger"])
    call_list.append("-w")       # write the
    call_list.append(elf_path)   # ELF file
    call_list.append("-rst")     # and reset the target

    # Print what we're gonna do
    tmp = ""
    for item in call_list:
        tmp += " " + item
    printer.string("{}in directory {} calling{}".         \
                   format(prompt, os.getcwd(), tmp))

    # Call it
    return u_utils.exe_run(call_list, guard_time_seconds, printer, prompt)

def open_ocd(commands, connection):
    '''Assemble the command line for OpenOCD: it's a doozy'''
    call_list = []

    call_list.append(OPENOCD_PATH)
    call_list.append("-f")
    call_list.append(OPENOCD_SCRIPTS_PATH + os.sep + "interface" + \
                     os.sep + OPENOCD_STLINK_INTERFACE_SCRIPT)
    call_list.append("-f")
    call_list.append(OPENOCD_SCRIPTS_PATH + os.sep + "target" + \
                     os.sep + OPENOCD_STM32F4_TARGET_SCRIPT)
    call_list.append("-s")
    call_list.append(OPENOCD_SCRIPTS_PATH)
    if connection and "debugger" in connection and connection["debugger"]:
        call_list.append("-c")
        call_list.append("hla_serial " + connection["debugger"])
    for command in commands:
        call_list.append("-c")
        call_list.append(command)

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
                file_handle_out.write(decoded_data)
                file_handle_out.flush()
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
        working_dir, printer, reporter, test_report_handle):
    '''Build/run on STM32Cube'''
    return_value = -1
    mcu_dir = ubxlib_dir + os.sep + SDK_DIR + os.sep + "mcu" + os.sep + mcu
    instance_text = u_utils.get_instance_text(instance)
    # Create a unique project name prefix in case more than
    # one process is running this
    updated_project_name_prefix = UPDATED_PROJECT_NAME_PREFIX + str(os.getpid()) + "_"
    workspace_subdir = STM32CUBE_IDE_WORKSPACE_SUBDIR + "_" + str(os.getpid())
    elf_path = None
    downloaded = False
    running = False
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
    # Switch to the working directory
    with u_utils.ChangeDir(working_dir):
        # Check that everything we need is installed
        if check_installation(PATHS_LIST, printer, prompt):
            # Fetch Unity
            if u_utils.fetch_repo(u_utils.UNITY_URL,
                                  u_utils.UNITY_SUBDIR,
                                  None, printer, prompt):
                # I've no idea why but on every other
                # build STM32Cube loses track of where
                # most of the files are: you'll see it
                # say that it can't find u_cfg_sw.h and
                # fail.  Until we find out why just
                # give it two goes, deleting the project
                # we created before trying again.
                retries = 2
                while (elf_path is None) and (retries > 0):
                    # The STM32Cube IDE, based on Eclipse
                    # has no mechanism for overriding the locations
                    # of things so here we read the .project
                    # file and replace the locations of the
                    # STM32Cube SDK and Unity files as
                    # appropriate
                    if create_project(mcu_dir, PROJECT_NAME,
                                      updated_project_name_prefix + PROJECT_NAME,
                                      STM32CUBE_FW_PATH,
                                      working_dir + os.sep + u_utils.UNITY_SUBDIR,
                                      printer, prompt):
                        # Do the build
                        build_start_time = time()
                        elf_path = build_binary(mcu_dir, workspace_subdir,
                                                updated_project_name_prefix + PROJECT_NAME,
                                                clean, defines, printer, prompt)
                        if elf_path is None:
                            reporter.event(u_report.EVENT_TYPE_BUILD,
                                           u_report.EVENT_INFORMATION,
                                           "unable to build, will retry")
                            printer.string("{}if the compilation."       \
                                           " failure was because"        \
                                           " the build couldn't"         \
                                           " even find u_cfg_sw.h"       \
                                           " then ignore it, Eclipse"    \
                                           " lost its head, happens"     \
                                           " a lot, we will try again.". \
                                           format(prompt))
                    else:
                        reporter.event(u_report.EVENT_TYPE_BUILD,
                                       u_report.EVENT_WARNING,
                                       "unable to create STM32Cube project, will retry")
                    retries -= 1
                if elf_path:
                    reporter.event(u_report.EVENT_TYPE_BUILD,
                                   u_report.EVENT_PASSED,
                                   "build took {:.0f} second(s)".format(time() -
                                                                        build_start_time))
                    # Lock the connection.
                    with u_connection.Lock(connection, connection_lock,
                                           CONNECTION_LOCK_GUARD_TIME_SECONDS,
                                           printer, prompt) as locked_connection:
                        if locked_connection:
                            # I have seen download failures occur if two
                            # ST-Link connections are initiated at the same time.
                            with u_utils.Lock(platform_lock, PLATFORM_LOCK_GUARD_TIME_SECONDS,
                                              "platform", printer, prompt) as locked_platform:
                                if locked_platform:
                                    reporter.event(u_report.EVENT_TYPE_DOWNLOAD,
                                                   u_report.EVENT_START)
                                    # Do the download.  I have seen the STM32F4 debugger
                                    # barf on occasions so give this two bites of
                                    # the cherry
                                    retries = 2
                                    while not downloaded and (retries > 0):
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
                                    if downloaded:
                                        # Remove us from the list of pending downloads
                                        if download_list:
                                            download_list.remove(instance_text)
                                        reporter.event(u_report.EVENT_TYPE_DOWNLOAD,
                                                       u_report.EVENT_COMPLETE)
                                        # Wait for all the other downloads to complete before
                                        # starting SWO logging
                                        u_utils.wait_for_completion(download_list,
                                                                    "download",
                                                                    DOWNLOADS_COMPLETE_GUARD_TIME_SECONDS,
                                                                    printer, prompt)
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
                                            # Two bites at the cherry again
                                            retries = 2
                                            while not running and (retries > 0):
                                                # Now start Open OCD to reset the target
                                                # and capture SWO output
                                                sleep(1)
                                                with u_utils.ExeRun(open_ocd(OPENOCD_COMMANDS,
                                                                             connection),
                                                                    printer, prompt):
                                                    running = True
                                                    # Open the SWO decoded text file for
                                                    # reading, binary to prevent the line
                                                    # endings being munged.
                                                    file_handle = open(SWO_DECODED_TEXT_FILE,
                                                                       "rb")
                                                    # Monitor progress based on the decoded
                                                    # SWO text
                                                    return_value = u_monitor.          \
                                                                   main(file_handle,
                                                                        u_monitor.CONNECTION_PIPE,
                                                                        RUN_GUARD_TIME_SECONDS,
                                                                        RUN_INACTIVITY_TIME_SECONDS,
                                                                        instance, printer,
                                                                        reporter,
                                                                        test_report_handle)
                                                    file_handle.close()
                                                retries -= 1
                                                if not running:
                                                    sleep(5)
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
                                            reporter.event(u_report.EVENT_TYPE_TEST,
                                                           u_report.EVENT_FAILED)
                                    else:
                                        reporter.event(u_report.EVENT_TYPE_DOWNLOAD,
                                                       u_report.EVENT_FAILED)
                        else:
                            reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                                           u_report.EVENT_FAILED,
                                           "unable to lock a connection")
                else:
                    reporter.event(u_report.EVENT_TYPE_BUILD,
                                   u_report.EVENT_FAILED,
                                   "check debug log for details")

                # To avoid a build up of stuff, delete the temporary build and
                # workspace on exit
                tmp = mcu_dir + os.sep + updated_project_name_prefix + PROJECT_NAME
                if os.path.exists(tmp):
                    printer.string("{}deleting temporary build directory {}...".   \
                                   format(prompt, tmp))
                    u_utils.deltree(tmp, printer, prompt)
                if os.path.exists(workspace_subdir):
                    printer.string("{}deleting temporary workspace directory {}...". \
                                   format(prompt, workspace_subdir))
                    u_utils.deltree(workspace_subdir, printer, prompt)

            else:
                reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                               u_report.EVENT_FAILED,
                               "unable to fetch Unity")
        else:
            reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                           u_report.EVENT_FAILED,
                           "there is a problem with the tools installation for STM32F4")

    # Remove us from the list of pending downloads for safety
    try:
        misc_locks["stm32f4_downloads_list"].remove(instance_text)
    except (AttributeError, ValueError, TypeError):
        pass

    return return_value
