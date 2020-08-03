#!/usr/bin/env python

'''Build/run ubxlib for STM32F4 and report results.'''

import sys # For stdout redirection when wrapping swo_decoder.py
import os
from time import sleep, clock
from multiprocessing import Process  # To launch swo_decoder.py
import subprocess
import psutil
import u_monitor
import u_connection
import u_report
import u_utils
import swo_decoder

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
PROMPT = "u_run_stm32f4_"

# Location of the STM32F4 Cube FW (i.e. their C drivers)
# directory
STM32CUBE_FW_PATH = "C:\\STM32Cube_FW_F4"

# Location of the STM32Cube IDE directory
STM32CUBE_IDE_PATH = "C:\\ST\\STM32CubeIDE_1.3.0\\STM32CubeIDE"

# Location of STM32_Programmer_CLI.exe in
# the STM32Cube IDE directory
STM32_PROGRAMMER_CLI_PATH = STM32CUBE_IDE_PATH + "\\plugins\\"       \
                            "com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer."  \
                            "win32_1.3.0.202002181050\\tools\\bin\\" \
                            "STM32_Programmer_CLI.exe"

# Location of the OpenOCD executable, which can
# be found deep in the STM32Cube IDE directories
OPENOCD_PATH = STM32CUBE_IDE_PATH + "\\plugins\\"                  \
               "com.st.stm32cube.ide.mcu.externaltools.openocd."   \
               "win32_1.3.0.202002181050\\tools\\bin\\openocd.exe"

# Location of the OpenOCD scripts directory, which can
# also be found deep in the STM32Cube IDE directories
OPENOCD_SCRIPTS_PATH = STM32CUBE_IDE_PATH +  "\\plugins\\"       \
                       "com.st.stm32cube.ide.mcu.debug."         \
                       "openocd_1.3.0.202002181050\\resources\\openocd\\"     \
                       "st_scripts"

# The OpenOCD script file provided by ST for their STLink interface
OPENOCD_STLINK_INTERFACE_SCRIPT = "stlink.cfg"

# The OpenOCD script file provided by ST for their STM32F4 target
OPENOCD_STM32F4_TARGET_SCRIPT = "stm32f4x.cfg"

# The subdirectory of the working directory in which the
# STM32F4 Cube IDE should create the workspace
STM32CUBE_IDE_WORKSPACE_SUBDIR = "workspace_1.3.0"

# The STM32F4 cube SDK directory in ubxlib
SDK_DIR = "port\\platform\\stm\\stm32f4\\sdk\\cube"

# The project name to build
PROJECT_NAME = "runner"

# The prefix that forms the modified project name that
# is created with updated paths
UPDATED_PROJECT_NAME_PREFIX = "test_only_"

# The name of the configuration to build
PROJECT_CONFIGURATION = "Debug"

# The maximum number of U_FLAGx defines that the STM32F4Cube
# project file can take
MAX_NUM_DEFINES = 20

# The core clock rate of the STM32F4 processor, i.e. SystemCoreClock
SYSTEM_CORE_CLOCK_HZ = 168000000

# The SWO data rate: should be the same as U_CFG_HW_SWO_CLOCK_HZ
# from u_cfg_hw_platform_specific.h for the STM32F4 platform,
# if that is provided.
SWO_CLOCK_HZ = 2000000

# The name of the temporary file in which SWO data is stored
SWO_DATA_FILE = "swo.dat"

# The name of the temporary file in which decoded SWO text
# is stored by swo_decoder
SWO_DECODED_TEXT_FILE = "swo.txt"

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
BUILD_GUARD_TIME_SECONDS = 60 * 30

# The guard time waiting for a lock on the HW connection seconds
CONNECTION_LOCK_GUARD_TIME_SECONDS = u_connection.CONNECTION_LOCK_GUARD_TIME_SECONDS

# The download guard time for this build in seconds
DOWNLOAD_GUARD_TIME_SECONDS = u_utils.DOWNLOAD_GUARD_TIME_SECONDS

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

def create_project(sdk_path, old_project_name, new_project_name,
                   stm32cube_fw_path, unity_dir,
                   printer, prompt):
    '''Create a new project with the right paths'''
    new_project_path = sdk_path + os.sep + new_project_name
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
        file_handle = open(sdk_path + os.sep + old_project_name +
                           os.sep + ".cproject", "r")
        string = file_handle.read()
        file_handle.close()

        # Write it out to the new
        printer.string("{}writing .cproject file...".format(prompt))
        file_handle = open(sdk_path + os.sep + new_project_name +
                           os.sep + ".cproject", "w")
        file_handle.write(string)
        file_handle.close()

        # Read the .project file from the old project
        printer.string("{}reading .project file...".format(prompt))
        file_handle = open(sdk_path + os.sep + old_project_name +
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
        file_handle = open(sdk_path + os.sep + new_project_name +
                           os.sep + ".project", "w")
        file_handle.write(string)
        file_handle.close()

        # Write in a warning file just in case anyone
        # wonders what the hell this weird project is
        file_handle = open(sdk_path + os.sep + new_project_name +
                           os.sep + "ignore_this_directory.txt", "w")
        file_handle.write("See u_run_stm32f4.py for an explanation.")
        file_handle.close()

        success = True

    return success

def build_binary(sdk_dir, workspace_subdir, project_name, clean, defines,
                 printer, prompt):
    '''Build'''
    call_list = []
    build_dir = sdk_dir + os.sep + project_name + os.sep + PROJECT_CONFIGURATION
    too_many_defines = False
    elf_path = None

    # The STM2F4 Cube IDE doesn't provide
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

        # Print the environment variables for debug purposes
        printer.string("{}environment is:".format(prompt))
        text = subprocess.check_output(["set",], shell=True)
        for line in text.splitlines():
            printer.string("{}{}".format(prompt, line))

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
            call_list.append(sdk_dir + os.sep + project_name)
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
    call_list.append("-v")       # verify the download
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

def swo_decoder_wrapper(swo_decoded_text_file,
                        swo_data_file, address, sync, timeout):
    '''Wrapper for swo_decoder that redirects stdout, to be called in a Process'''
    # Redirect output to the SWO decoded output text file
    stdout_saved = sys.stdout
    sys.stdout = open(swo_decoded_text_file, "w", buffering=0)
    # Call the SWO decoder script with the remaining arguments
    try:
        swo_decoder.main(swo_data_file, address, sync, timeout)
    except KeyboardInterrupt:
        sys.stdout = stdout_saved

# IMPORTANT: Eclipse, on which the STM32Cube is based
# has very peculiar and immutable way of dealing with paths.
# To work around this while preventing Git from thinking we've
# made changes to code, we make a copy of the project
# from "blah" to "test_only_blah", which is in the
# .gitignore file, and build that instead of "runner".

def run(instance, sdk, connection, connection_lock, platform_lock, clean, defines,
        ubxlib_dir, working_dir, printer, reporter, test_report_handle):
    '''Build/run on STM32F4'''
    return_value = 1
    sdk_dir = ubxlib_dir + os.sep + SDK_DIR
    instance_text = u_utils.get_instance_text(instance)
    # Create a unique project name prefix in case more than
    # one process is running this
    updated_project_name_prefix = UPDATED_PROJECT_NAME_PREFIX + str(os.getpid()) + "_"
    workspace_subdir = STM32CUBE_IDE_WORKSPACE_SUBDIR + "_" + str(os.getpid())
    elf_path = None
    downloaded = False
    running = False

    # Only one SDK for STM32F4 and no issues with running in parallel
    del sdk
    del platform_lock

    prompt = PROMPT + instance_text + ": "

    # Print out what we've been told to do
    text = "running STM32F4"
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

    reporter.event(u_report.EVENT_TYPE_BUILD,
                   u_report.EVENT_START,
                   "STM32F4")
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
                    # The STM32F4 Cube IDE, based on Eclipse
                    # has no mechanism for overriding the locations
                    # of things so here we read the .project
                    # file and replace the locations of the
                    # STM2F4 Cube SDK and Unity files as
                    # appropriate
                    if create_project(sdk_dir, PROJECT_NAME,
                                      updated_project_name_prefix + PROJECT_NAME,
                                      STM32CUBE_FW_PATH,
                                      working_dir + os.sep + u_utils.UNITY_SUBDIR,
                                      printer, prompt):
                        # Do the build
                        build_start_time = clock()
                        elf_path = build_binary(sdk_dir, workspace_subdir,
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
                                   "build took {:.0f} second(s)".format(clock() -
                                                                        build_start_time))
                    # Lock the connection.
                    with u_connection.Lock(connection, connection_lock,
                                           CONNECTION_LOCK_GUARD_TIME_SECONDS,
                                           printer, prompt) as locked:
                        if locked:
                            reporter.event(u_report.EVENT_TYPE_DOWNLOAD,
                                           u_report.EVENT_START)
                            # Do the download.  I have see the STM32F4 debugger
                            # barf on occasions so give this two bites of
                            # the cherry
                            retries = 2
                            while not downloaded and (retries > 0):
                                downloaded = download(connection, DOWNLOAD_GUARD_TIME_SECONDS,
                                                      elf_path, printer, prompt)
                                retries -= 1
                                if not downloaded:
                                    sleep(5)
                            if downloaded:
                                reporter.event(u_report.EVENT_TYPE_DOWNLOAD,
                                               u_report.EVENT_COMPLETE)
                                # Empty the SWO data file and decoded text files
                                file_handle = open(SWO_DATA_FILE, "w").close()
                                file_handle = open(SWO_DECODED_TEXT_FILE, "w").close()
                                reporter.event(u_report.EVENT_TYPE_TEST,
                                               u_report.EVENT_START)
                                try:
                                    # Start the parser on the SWO data file
                                    # in its own process and in a wrapper
                                    # that redirects its decoded output to
                                    # a file
                                    process = Process(target=swo_decoder_wrapper,
                                                      args=(SWO_DECODED_TEXT_FILE,
                                                            SWO_DATA_FILE, 0, True, -1))
                                    process.start()
                                    # Start OpenOCD to read from SWO on the target
                                    # and dump it into the SWO data file
                                    # Give ourselves priority here or OpenOCD can lose
                                    # characters
                                    psutil.Process().nice(psutil.HIGH_PRIORITY_CLASS)
                                    # Two bites at the cherry again
                                    retries = 2
                                    while not running and (retries > 0):
                                        with u_utils.ExeRun(open_ocd(OPENOCD_COMMANDS,
                                                                     connection),
                                                            printer, prompt):
                                            running = True
                                            # Open the SWO decoded text file for reading
                                            # Open the file in binary mode to prevent line
                                            # endings being munged by Python
                                            file_handle = open(SWO_DECODED_TEXT_FILE, "rb")
                                            # Monitor progress based on the decoded SWO text
                                            return_value = u_monitor.main(file_handle,
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
                                    printer.string("{}caught CTRL-C, terminating...".format(prompt))
                                    process.terminate()
                                    return_value = 1
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
                tmp = sdk_dir + os.sep + updated_project_name_prefix + PROJECT_NAME
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

    return return_value
