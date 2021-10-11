#!/usr/bin/env python

'''Build/run ubxlib under Arduino and report results.'''

import os            # For sep, getcwd(), isdir() and environ()
import subprocess
from time import time, sleep
import u_connection
import u_monitor
import u_report
import u_utils
import u_settings

# Prefix to put at the start of all prints
PROMPT = "u_run_arduino_"

# ubxlib Arduino sub-directory (off ubxlib root)
ARDUINO_SUB_DIR = u_settings.ARDUINO_SUB_DIR # e.g. "port/platform/arduino"

# The name of the Arduino CLI executable
ARDUINO_CLI_EXE = u_settings.ARDUINO_CLI_EXE # e.g. "arduino-cli"

# URLs from which to install the Arduino boards
ARDUINO_BOARDS_URLS = u_settings.ARDUINO_BOARDS_URLS

# The name to use for the libraries sub-directory
LIBRARIES_SUB_DIR = "libraries"

# The name to use for the main ubxlib library
LIBRARY_NAME = "ubxlib"

# The thing to stick on the end of things when we mean the main library
LIBRARY_NAME_LIB_POSTFIX = ""

# The thing to stick on the end of things for the test library
LIBRARY_NAME_TEST_POSTFIX = "_test"

# Build sub-directory
BUILD_SUBDIR = u_settings.ARDUINO_BUILD_SUBDIR # e.g. "build"

# The path to the sketch to build for the ubxlib test (off ARDUINO_SUB_DIR)
TEST_SKETCH_SUB_PATH = "app/app.ino"

# The #define which means that DTR and RTS should be switched
# off while monitoring the serial output, needed when the
# host is a NINA-W1 board (which must, of course, not have
# the flow control lines connected on that serial port)
MONITOR_DTR_RTS_OFF_MARKER = u_utils.MONITOR_DTR_RTS_OFF_MARKER

# The guard time for library creation in seconds
LIBRARY_CREATE_GUARD_TIME_SECONDS = u_settings.ARDUINO_LIBRARY_CREATE_GUARD_TIME_SECONDS # 60 * 5

# The guard time to lock the Arduino platform in seconds
PLATFORM_LOCK_GUARD_TIME_SECONDS = u_utils.PLATFORM_LOCK_GUARD_TIME_SECONDS

# The guard time for the install in seconds:
# this can take ages when tools first have to be installed
INSTALL_GUARD_TIME_SECONDS = u_settings.ARDUINO_INSTALL_GUARD_TIME_SECONDS # e.g. 60 * 60

# The guard time for this build in seconds
BUILD_GUARD_TIME_SECONDS = u_settings.ARDUINO_BUILD_GUARD_TIME_SECONDS # e.g. 60 * 30

# The download guard time for this build in seconds
DOWNLOAD_GUARD_TIME_SECONDS = u_settings.ARDUINO_DOWNLOAD_GUARD_TIME_SECONDS # e.g. 60 * 5

# The guard time waiting for a lock on the HW connection seconds
CONNECTION_LOCK_GUARD_TIME_SECONDS = u_connection.CONNECTION_LOCK_GUARD_TIME_SECONDS

# The guard time for running tests in seconds
RUN_GUARD_TIME_SECONDS = u_utils.RUN_GUARD_TIME_SECONDS

# The inactivity time for running tests in seconds
RUN_INACTIVITY_TIME_SECONDS = u_utils.RUN_INACTIVITY_TIME_SECONDS

# Run an external command
def run_command(call_list, guard_time_seconds, printer, prompt, keep_going_flag):
    '''Run an external command'''

    # Print what we're gonna do
    tmp = ""
    for item in call_list:
        tmp += " " + item
    printer.string("{}in directory {} calling{}".            \
                   format(prompt, os.getcwd(), tmp))

    # Do it, setting shell to True to keep Jenkins happy
    return u_utils.exe_run(call_list, guard_time_seconds,
                           printer, prompt, shell_cmd=True,
                           keep_going_flag=keep_going_flag)

# Install the necessary tools
def install(board, board_urls, printer, prompt, keep_going_flag):
    '''Install tools'''
    call_list = []
    success = False

    # First check that the Arduino CLI is there
    printer.string("{}checking...".format(prompt))

    call_list.append(ARDUINO_CLI_EXE)
    call_list.append("version")
    if run_command(call_list, DOWNLOAD_GUARD_TIME_SECONDS,
                   printer, prompt, keep_going_flag):
        # Write the config file
        call_list = []
        call_list.append(ARDUINO_CLI_EXE)
        call_list.append("config")
        call_list.append("init")
        call_list.append("--overwrite")
        call_list.append("--dest-dir")
        call_list.append(".")
        if run_command(call_list, DOWNLOAD_GUARD_TIME_SECONDS,
                       printer, prompt, keep_going_flag):
            # Install the tools
            call_list = []
            call_list.append(ARDUINO_CLI_EXE)
            call_list.append("config")
            call_list.append("add")
            call_list.append("board_manager.additional_urls")
            for item in board_urls:
                call_list.append(item)
            if run_command(call_list, INSTALL_GUARD_TIME_SECONDS,
                           printer, prompt, keep_going_flag):
                call_list = []
                call_list.append(ARDUINO_CLI_EXE)
                call_list.append("core")
                call_list.append("update-index")
                if run_command(call_list, INSTALL_GUARD_TIME_SECONDS,
                               printer, prompt, keep_going_flag):
                    call_list = []
                    call_list.append(ARDUINO_CLI_EXE)
                    call_list.append("core")
                    call_list.append("install")
                    # I'm kind of guessing the syntax here, I think the
                    # install command needs the first two parts of the
                    # FQBN, with the actual board name on the end
                    # missed off
                    core = board.split(":")
                    call_list.append(core[0] + ":" + core[1])
                    success = run_command(call_list, INSTALL_GUARD_TIME_SECONDS,
                                          printer, prompt, keep_going_flag)
    else:
        printer.string("{}can't find {}: is it installed and on the path?". \
                       format(prompt, ARDUINO_CLI_EXE))

    return success

# Clear out a pre-built library
def clear_prebuilt_library(library_path, mcu, printer, prompt):
    '''Clear a pre-built library'''
    prebuilt_dir = os.path.join(library_path, "src", mcu.lower())
    if os.path.exists(prebuilt_dir):
        printer.string("{}deleting pre-built library directory {}...\n". \
                       format(prompt, prebuilt_dir))
        u_utils.deltree(prebuilt_dir, printer, prompt)

# Create the ubxlib library for Arduino
def create_library(ubxlib_dir, arduino_dir, toolchain, library_path, postfix,
                   clean, printer, prompt, keep_going_flag):
    '''Create the ubxlib library'''
    call_list = []
    success = False

    # Delete the library directory if clean is required
    if clean:
        u_utils.deltree(library_path + postfix, printer, prompt)

    printer.string("{}creating library...".format(prompt))

    # Assemble the call list for the creation process
    call_list.append("python")
    call_list.append(arduino_dir + os.sep + "u_arduino" + postfix + ".py")
    call_list.append("-p")
    call_list.append(toolchain)
    call_list.append("-u")
    call_list.append(ubxlib_dir)
    call_list.append("-o")
    call_list.append(library_path + postfix)
    call_list.append(arduino_dir + os.sep + "source" + postfix + ".txt")
    call_list.append(arduino_dir + os.sep + "include" + postfix + ".txt")
    success = run_command(call_list, LIBRARY_CREATE_GUARD_TIME_SECONDS,
                          printer, prompt, keep_going_flag)

    return success

# Build ubxlib library for Arduino
def build(build_dir, sketch_path, library_path, mcu, board, defines,
          clean, printer, prompt, reporter, keep_going_flag):
    '''Build ubxlib for Arduino'''
    call_list = []
    defines_text = ""
    build_path = None

    # We build inside the build_dir but in a sub-directory specific
    # to the sketch
    build_dir = os.path.join(build_dir, os.path.basename(os.path.split(sketch_path)[0]))

    # Make sure that the build directory exists and is
    # cleaned if required
    if os.path.exists(build_dir):
        if clean:
            u_utils.deltree(build_dir, printer, prompt)
            os.makedirs(build_dir)
    else:
        os.makedirs(build_dir)

    if os.path.exists(build_dir):
        printer.string("{}building {} in {}...".format(prompt, sketch_path, build_dir))

        # Assemble the call list for the creation process
        call_list.append(ARDUINO_CLI_EXE)
        call_list.append("compile")
        if library_path:
            call_list.append("--libraries")
            call_list.append(library_path)
        call_list.append("--fqbn")
        call_list.append(board)
        if clean:
            call_list.append("--clean")
        call_list.append("-v")
        call_list.append("--build-path")
        call_list.append(build_dir)
        call_list.append("--build-cache-path")
        call_list.append(build_dir )
        if defines:
            for define in defines:
                if defines_text:
                    defines_text += " "
                defines_text += "\"-D" + define + "\""
            # Set the flags for our ubxlib files, which are .c
            call_list.append("--build-property")
            call_list.append("compiler.c.extra_flags=" + defines_text)
            # Set the flags for the .ino application files, which are .cpp
            call_list.append("--build-property")
            call_list.append("compiler.cpp.extra_flags=" + defines_text)
        call_list.append(sketch_path)
        if run_command(call_list, BUILD_GUARD_TIME_SECONDS,
                       printer, prompt, keep_going_flag):
            build_path = build_dir
            if library_path:
                # If that was succesful, copy the ".a" files to the correct
                # locations under each library/mcu
                for root, _directories, files in os.walk(os.path.join(build_dir,
                                                                      LIBRARIES_SUB_DIR)):
                    for file in files:
                        if file.endswith(".a"):
                            source = os.path.join(root, file)
                            library_name = os.path.basename(os.path.split(source)[0])
                            destination_dir = os.path.join(library_path, library_name,
                                                           "src", mcu.lower())
                            destination = os.path.join(destination_dir,
                                                       library_name + ".a")
                            if not os.path.isdir(destination_dir):
                                try:
                                    os.makedirs(destination_dir)
                                except OSError:
                                    reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                                                   u_report.EVENT_FAILED,
                                                   "could not create directory \"" + \
                                                   destination + "\"")
                                    build_path = None
                                    break
                            call_list = []
                            if u_utils.is_linux():
                                call_list.append("cp")
                            else:
                                call_list.append("copy")
                                call_list.append("/Y")
                            call_list.append(source)
                            call_list.append(destination)
                            try:
                                printer.string("{}copying {} to {}...".   \
                                               format(prompt, source, destination))
                                subprocess.check_output(u_utils.subprocess_osify(call_list), shell=True)
                            except subprocess.CalledProcessError as error:
                                reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                                               u_report.EVENT_FAILED,
                                               "{}error when copying {} to {}, {} {}: \"{}\"".
                                               format(prompt, source, destination, error.cmd,
                                                      error.returncode, error.output))
                                build_path = None
                                break
    else:
        reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                       u_report.EVENT_FAILED,
                       "could not create directory \"" + build_dir + "\"")

    return build_path

# Download to the target
def download(build_dir, board, serial_port, printer, prompt, keep_going_flag):
    '''Download a build to the target'''
    call_list = []
    success = False

    printer.string("{}downloading {}...".format(prompt, build_dir))

    # Assemble the call list for the creation process
    call_list.append(ARDUINO_CLI_EXE)
    call_list.append("upload")
    call_list.append("-p")
    call_list.append(serial_port)
    call_list.append("--fqbn")
    call_list.append(board)
    call_list.append("-v")
    # It can't seem to find the binary file without this for some reason
    call_list.append("--input-file")
    call_list.append(os.path.join(build_dir, os.path.basename(build_dir) + ".ino.bin"))
    call_list.append(build_dir)

    # Give ourselves priority here or the download can fail
    u_utils.set_process_prio_high()

    success = run_command(call_list, DOWNLOAD_GUARD_TIME_SECONDS,
                          printer, prompt, keep_going_flag)

    # Return priority to normal
    u_utils.set_process_prio_normal()

    return success

def run(instance, mcu, board, toolchain, connection, connection_lock,
        platform_lock, misc_locks, clean, defines, ubxlib_dir,
        working_dir, printer, reporter, test_report_handle,
        keep_going_flag=None, unity_dir=None):
    '''Build/run on Arduino'''
    return_value = -1
    monitor_dtr_rts_on = None
    installed = False
    sketch_paths = []
    build_paths = []
    return_values = []
    instance_text = u_utils.get_instance_text(instance)

    # None of the misc locks are required
    del misc_locks

    # Since we only currently support ESP-IDF we don't need
    # unity as ESP-IDF already has a copy built in
    del unity_dir

    prompt = PROMPT + instance_text + ": "

    toolchain = toolchain.lower()

    # Print out what we've been told to do and at the
    # same time check for the DTR/RTS off marker
    text = "running Arduino for " + mcu + " with SDK " + toolchain + \
           " on board with FQBN \"" + board + "\""
    if connection and connection["serial_port"]:
        text += ", serial port " + connection["serial_port"]
    if clean:
        text += ", clean build"
    if defines:
        text += ", with #define(s)"
        for idx, define in enumerate(defines):
            if define == MONITOR_DTR_RTS_OFF_MARKER:
                monitor_dtr_rts_on = False
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
                   "Arduino")
    printer.string("{}CD to {}...".format(prompt, working_dir))

    with u_utils.ChangeDir(working_dir):
        # Lock the Arduino platform while we install the tools
        with u_utils.Lock(platform_lock, PLATFORM_LOCK_GUARD_TIME_SECONDS,
                          "platform", printer, prompt,
                          keep_going_flag) as locked_platform:
            if locked_platform:
                installed = install(board, ARDUINO_BOARDS_URLS,
                                    printer, prompt, keep_going_flag)

        if installed:
            arduino_dir = os.path.join(ubxlib_dir, ARDUINO_SUB_DIR)
            library_path = os.path.join(working_dir, LIBRARIES_SUB_DIR)
            # Clear out any pre-built ubxlib library or rebuilds
            # won't pick up changes
            clear_prebuilt_library(os.path.join(library_path, LIBRARY_NAME),
                                   mcu, printer, prompt)
            # Create the ubxlib Arduino library
            if create_library(ubxlib_dir, arduino_dir, toolchain,
                              os.path.join(library_path, LIBRARY_NAME),
                              LIBRARY_NAME_LIB_POSTFIX,
                              clean, printer, prompt, keep_going_flag):
                # Create the ubxlib Arduino test library
                if create_library(ubxlib_dir, arduino_dir, toolchain,
                                  os.path.join(library_path, LIBRARY_NAME),
                                  LIBRARY_NAME_TEST_POSTFIX,
                                  clean, printer, prompt, keep_going_flag):
                    # We now build both libraries with the test sketch and we also
                    # build the examples with the just the ubxlib Arduino library.
                    # Make a list of the sketches to build
                    sketch_paths.append(os.path.join(arduino_dir, TEST_SKETCH_SUB_PATH))
                    for root, _directories, files in os.walk(library_path):
                        for file in files:
                            if os.sep + "examples" + os.sep in root and file.endswith(".ino"):
                                sketch_paths.append(os.path.join(root, file))
                    printer.string("{}{} thing(s) to build.".format(prompt, len(sketch_paths)))
                    # Build the sketches: note that the first build of the ubxlib
                    # Arduino library to a .a file will be copied back into the library
                    # directory for use in the following builds
                    build_dir = os.path.join(working_dir, BUILD_SUBDIR)
                    for sketch_path in sketch_paths:
                        build_start_time = time()
                        build_path = build(build_dir, sketch_path, library_path, mcu, board, defines,
                                           clean, printer, prompt, reporter, keep_going_flag)
                        if not u_utils.keep_going(keep_going_flag, printer, prompt) or not build_path:
                            break
                        build_paths.append(build_path)
                        reporter.event(u_report.EVENT_TYPE_BUILD,
                                       u_report.EVENT_PASSED,
                                       "build {} of {} took {:.0f} second(s)". \
                                       format(len(build_paths), len(sketch_paths),
                                              time() - build_start_time))
                    if len(build_paths) == len(sketch_paths):
                        # Download and run the builds
                        with u_connection.Lock(connection, connection_lock,
                                               CONNECTION_LOCK_GUARD_TIME_SECONDS,
                                               printer, prompt, keep_going_flag) as locked:
                            if locked:
                                for build_path in build_paths:
                                    # I have seen download failures occur if two
                                    # are initiated at the same time so lock the
                                    # platform for this
                                    downloaded = False
                                    with u_utils.Lock(platform_lock, PLATFORM_LOCK_GUARD_TIME_SECONDS,
                                                      "platform", printer, prompt,
                                                      keep_going_flag) as locked_platform:
                                        if locked_platform:
                                            # Have seen this fail, so give it a few goes
                                            reporter.event(u_report.EVENT_TYPE_DOWNLOAD,
                                                           u_report.EVENT_START)
                                            retries = 0
                                            while u_utils.keep_going(keep_going_flag, printer,
                                                                     prompt) and               \
                                                  not downloaded and (retries < 3):
                                                downloaded = download(build_path, board,
                                                                      connection["serial_port"],
                                                                      printer, prompt,
                                                                      keep_going_flag)
                                                if not downloaded:
                                                    reporter.event(u_report.EVENT_TYPE_DOWNLOAD,
                                                                   u_report.EVENT_WARNING,
                                                                   "unable to download, will" \
                                                                   " retry...")
                                                    retries += 1
                                                    sleep(5)
                                    if downloaded:
                                        reporter.event(u_report.EVENT_TYPE_DOWNLOAD,
                                                       u_report.EVENT_COMPLETE)
                                        reporter.event(u_report.EVENT_TYPE_TEST,
                                                       u_report.EVENT_START)
                                        # Open the COM port to get debug output
                                        serial_handle = u_utils.open_serial(connection["serial_port"],
                                                                            115200,
                                                                            printer,
                                                                            prompt,
                                                                            dtr_set_on=monitor_dtr_rts_on,
                                                                            rts_set_on=monitor_dtr_rts_on)
                                        if serial_handle is not None:
                                            # Monitor progress
                                            return_values.append(u_monitor.main(serial_handle,
                                                                                u_monitor.CONNECTION_SERIAL,
                                                                                RUN_GUARD_TIME_SECONDS,
                                                                                RUN_INACTIVITY_TIME_SECONDS,
                                                                                "\r", instance,
                                                                                printer,
                                                                                reporter,
                                                                                test_report_handle,
                                                                                keep_going_flag=keep_going_flag))
                                            # Delays and flushes here to make sure
                                            # that the serial port actually closes
                                            # since we might need to re-open it for
                                            # another download going around the loop
                                            serial_handle.cancel_read()
                                            sleep(1)
                                            serial_handle.reset_input_buffer()
                                            serial_handle.reset_output_buffer()
                                            sleep(1)
                                            serial_handle.close()
                                            reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                                                           u_report.EVENT_COMPLETE,
                                                           "serial port closed")
                                            sleep(5)
                                        else:
                                            reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                                                           u_report.EVENT_FAILED,
                                                           "unable to open serial port " +      \
                                                           connection["serial_port"])
                                        if return_values and return_values[-1] == 0:
                                            reporter.event(u_report.EVENT_TYPE_TEST,
                                                           u_report.EVENT_COMPLETE)
                                        else:
                                            reporter.event(u_report.EVENT_TYPE_TEST,
                                                           u_report.EVENT_FAILED)
                                    else:
                                        reporter.event(u_report.EVENT_TYPE_DOWNLOAD,
                                                       u_report.EVENT_FAILED,
                                                       "unable to download to the target")
                                if return_values:
                                    return_value = 0
                                    for item in return_values:
                                        # If a return value goes negative then
                                        # only count the negative values, i.e. the
                                        # number of infrastructure failures
                                        if (item < 0) and (return_value >= 0):
                                            return_value = item
                                        else:
                                            if (((item > 0) and (return_value >= 0)) or  \
                                                ((item < 0) and (return_value < 0))):
                                                return_value += item
                            else:
                                reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                                               u_report.EVENT_FAILED,
                                               "unable to lock a connection")
                    else:
                        reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                                       u_report.EVENT_FAILED,
                                       "failed a build")
                else:
                    reporter.event(u_report.EVENT_TYPE_BUILD,
                                   u_report.EVENT_FAILED,
                                   "unable to build library, check debug log for details")
            else:
                reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                               u_report.EVENT_FAILED,
                               "unable to create library, check debug log for details")
        else:
            reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                           u_report.EVENT_FAILED,
                           "unable to install tools, check debug log for details")

    return return_value
