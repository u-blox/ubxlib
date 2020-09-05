#!/usr/bin/env python

'''Build/run ubxlib for STM32F4 and report results.'''

import os
from time import sleep, time
from threading import Thread
import socket
import subprocess
import u_monitor
import u_connection
import u_report
import u_utils

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

# Location of the STM32 Cube Progammer bin directory in the
# STM32Cube IDE directory
STM32_PROGRAMMER_BIN_PATH = STM32CUBE_IDE_PATH + "\\plugins\\"       \
                            "com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer."  \
                            "win32_1.3.0.202002181050\\tools\\bin"

# Location of STM32_Programmer_CLI.exe in
# the STM32Cube IDE directory
STM32_PROGRAMMER_CLI_PATH = STM32_PROGRAMMER_BIN_PATH + "\\STM32_Programmer_CLI.exe"

# Location of the ST-LINK_gdbserver executable, which can
# be found deep in the STM32Cube IDE directories
STLINK_GDB_SERVER_PATH = STM32CUBE_IDE_PATH + "\\plugins\\"                  \
                         "com.st.stm32cube.ide.mcu.externaltools.stlink-gdb-server."  \
                         "win32_1.3.0.202002181050\\tools\\bin\\ST-LINK_gdbserver.exe"

# Location of the ST-flavour GNU ARM GDB executable, which can
# be found deep in the STM32Cube IDE directories
STM32_GNU_ARM_GDB_SERVER_PATH = STM32CUBE_IDE_PATH + "\\plugins\\"            \
                                "com.st.stm32cube.ide.mcu.externaltools."     \
                                "gnu-tools-for-stm32.7-2018-q2-update.win32_1.0.0.201904181610\\" \
                                "tools\\bin\\arm-none-eabi-gdb.exe"

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

# The port number to use for GDB comms with ST-LINK GDB server
STLINK_GDB_PORT = 61234

# The port number to use for SWO output from the ST-LINK GDB server
STLINK_SWO_PORT = 61235

# The list of commands to be sent to ST-LINK_gdbserver
# when it is opened
GDB_SERVER_COMMANDS = ["-cp " + STM32_PROGRAMMER_BIN_PATH,
                       "-e",
                       "-d",
                       "-a " + str(SYSTEM_CORE_CLOCK_HZ),
                       "-b {:.0f}".format(SYSTEM_CORE_CLOCK_HZ / SWO_CLOCK_HZ)]

# The name of the temporary file in which decoded SWO text
# is stored by swo_decode_process
SWO_DECODED_TEXT_FILE = "swo.txt"

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
              {"name": "STLINK_GDB_SERVER_PATH",
               "path_string": STLINK_GDB_SERVER_PATH,
               "hint": "can't find ST-LINK_gdbserver, it should be provided" \
                       " as part of the STM32Cube IDE. Maybe you upgraded"   \
                       " it and haven't update the paths?"},
              {"name": "STM32_GNU_ARM_GDB_SERVER_PATH",
               "path_string": STM32_GNU_ARM_GDB_SERVER_PATH,
               "hint": "can't find the STM32-flavour GNU ARM GDB server," \
                       " which should be provided as part of the STM32Cube"   \
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
    num_defines = 0
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

def gdb_server(commands, swo_port, gdb_port, connection):
    '''Assemble the command line for the ST-LINK GDB server'''
    call_list = [STLINK_GDB_SERVER_PATH]

    # Stick in all the given stuff
    for item in commands:
        call_list.append(item)

    # Add the ports
    call_list.append("-p " + str(gdb_port))
    call_list.append("-z " + str(swo_port))

    # Add the debugger ID
    if connection and "debugger" in connection and connection["debugger"]:
        call_list.append("-i " + connection["debugger"])

    return call_list

def gdb_client(gdb_port):
    '''Assemble the command line for the ST-flavour GNU ARM GDB client'''
    call_list = [STM32_GNU_ARM_GDB_SERVER_PATH]

    # Switch off confirm otherwise "run" asks for confirmation
    call_list.append("--eval-command=set confirm off")
    # Extended remote so that we get to use "run"
    call_list.append("--eval-command=target extended-remote localhost:" + str(gdb_port))
    call_list.append("--eval-command=run")

    return call_list

def swo_decode_thread(swo_socket, swo_decoded_text_file):
    '''Grab SWO data from a socket, decode it and write it to file'''
    file_handle = open(swo_decoded_text_file, "wb")
    decoder = u_utils.SwoDecoder(0, True)
    try:
        # Will exit if swo_socket closes or on CTRL-C
        swo_socket.settimeout(0.1)
        while True:
            try:
                decoded_data = decoder.decode(swo_socket.recv(1024))
                # Just to be different, the ST-Link decode strips
                # line endings from CR/LF to LF.  Switch them back
                file_handle.write(decoded_data)
                file_handle.flush()
            except socket.timeout:
                pass
    except (KeyboardInterrupt, OSError, ConnectionAbortedError):
        file_handle.close()

# IMPORTANT: Eclipse, on which the STM32Cube is based
# has a very peculiar and immutable way of dealing with paths.
# To work around this while preventing Git from thinking we've
# made changes to code, we make a copy of the project
# from "blah" to "test_only_blah", which is in the
# .gitignore file, and build that instead of "runner".

def run(instance, sdk, connection, connection_lock, platform_lock, clean, defines,
        ubxlib_dir, working_dir, printer, reporter, test_report_handle):
    '''Build/run on STM32F4'''
    return_value = -1
    sdk_dir = ubxlib_dir + os.sep + SDK_DIR
    instance_text = u_utils.get_instance_text(instance)
    # Create a unique project name prefix in case more than
    # one process is running this
    updated_project_name_prefix = UPDATED_PROJECT_NAME_PREFIX + str(os.getpid()) + "_"
    workspace_subdir = STM32CUBE_IDE_WORKSPACE_SUBDIR + "_" + str(os.getpid())
    swo_port = u_utils.STLINK_SWO_PORT
    gdb_port = u_utils.STLINK_GDB_PORT
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
                        build_start_time = time()
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
                                   "build took {:.0f} second(s)".format(time() -
                                                                        build_start_time))
                    # Lock the connection.
                    with u_connection.Lock(connection, connection_lock,
                                           CONNECTION_LOCK_GUARD_TIME_SECONDS,
                                           printer, prompt) as locked:
                        if locked:
                            reporter.event(u_report.EVENT_TYPE_DOWNLOAD,
                                           u_report.EVENT_START)
                            # Do the download.  I have seen the STM32F4 debugger
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
                                reporter.event(u_report.EVENT_TYPE_TEST,
                                               u_report.EVENT_START)
                                if connection and "swo_port" in connection:
                                    swo_port = connection["swo_port"]
                                if connection and "gdb_port" in connection:
                                    gdb_port = connection["gdb_port"]

                                try:
                                    # Two bites at the cherry again
                                    retries = 2
                                    while not running and (retries > 0):
                                        # First, start the ST-LINK GDB server
                                        with u_utils.ExeRun(gdb_server(GDB_SERVER_COMMANDS,
                                                                       swo_port, gdb_port,
                                                                       connection),
                                                            printer, prompt):
                                            # Open a socket to the SWO port
                                            # of the ST-LINK GDB server
                                            with socket.socket(socket.AF_INET,
                                                               socket.SOCK_STREAM) as swo_socket:
                                                try:
                                                    swo_socket.connect(("localhost", swo_port))
                                                    # Start a thread which reads the
                                                    # SWO output, decodes it and writes it to file
                                                    read_thread = Thread(target=swo_decode_thread,
                                                                         args=(swo_socket,
                                                                               SWO_DECODED_TEXT_FILE))
                                                    read_thread.start()
                                                    # Now start the GNU ARM GDB client
                                                    # that will start the target running
                                                    with u_utils.ExeRun(gdb_client(gdb_port),
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
                                                    swo_socket.close()
                                                    read_thread.join()
                                                except socket.error:
                                                    reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                                                                   u_report.EVENT_FAILED,
                                                                   "unable to connect to port " +  \
                                                                   str(swo_port))
                                        retries -= 1
                                        if not running:
                                            sleep(5)
                                except KeyboardInterrupt:
                                    # Tidy up process on SIGINT
                                    printer.string("{}caught CTRL-C, terminating...".format(prompt))
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
