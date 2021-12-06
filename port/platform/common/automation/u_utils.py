#!/usr/bin/env python

'''Generally useful bits and bobs.'''

import queue                    # For PrintThread and exe_run
from time import sleep, time, gmtime, strftime   # For lock timeout, exe_run timeout and logging
from multiprocessing import RLock
from copy import copy
import threading                # For PrintThread
import sys
import os                       # For ChangeDir, has_admin
import stat                     # To help deltree out
from collections import deque   # For storing a window of debug
from telnetlib import Telnet    # For talking to JLink server
import socket
import shutil                   # To delete a directory tree
import signal                   # For CTRL_C_EVENT
import subprocess
import platform                 # Figure out current OS
import re                       # Regular Expression
import serial                   # Pyserial (make sure to do pip install pyserial)
import psutil                   # For killing things (make sure to do pip install psutil)
import requests                 # For HTTP comms with a KMTronic box (do pip install requests)
import u_settings

# Since this function is used by the global variables below it needs
# to be placed here.
def is_linux():
    '''Returns True when system is Linux'''
    return platform.system() == 'Linux'

# Since this function is used by the global variables below it needs
# to be placed here.
def pick_by_os(linux=None, other=None):
    '''
    This is a convenience function for selecting a value based on platform.
    As an example the line below will print out "Linux" when running on a
    Linux platform and "Not Linux" when running on some other platform:
        print( u_utils.pick_by_os(linux="Linux", other="Not Linux") )
    '''
    if is_linux():
        return linux
    return other

# The port that this agent service runs on
# Deliberately NOT a setting, we need to be sure
# everyone uses the same value
AGENT_SERVICE_PORT = 17003

# The maximum number of characters that an agent will
# use from controller_name when constructing a directory
# name for a ubxlib branch to be checked out into
AGENT_WORKING_SUBDIR_CONTROLLER_NAME_MAX_LENGTH = 4

# How long to wait for an install lock in seconds
INSTALL_LOCK_WAIT_SECONDS = u_settings.INSTALL_LOCK_WAIT_SECONDS #(60 * 60)

# The URL for Unity, the unit test framework
UNITY_URL = u_settings.UNITY_URL #"https://github.com/ThrowTheSwitch/Unity"

# The sub-directory that Unity is usually put in
# (off the working directory)
UNITY_SUBDIR = u_settings.UNITY_SUBDIR #"Unity"

# The path to DevCon, a Windows tool that allows
# USB devices to be reset, amongst other things
DEVCON_PATH = u_settings.DEVCON_PATH #"devcon.exe"

# The path to jlink.exe (or just the name 'cos it's on the path)
JLINK_PATH = u_settings.JLINK_PATH #"jlink.exe"

# The port number for SWO trace capture out of JLink
JLINK_SWO_PORT = u_settings.JLINK_SWO_PORT #19021

# The port number for GDB control of ST-LINK GDB server
STLINK_GDB_PORT = u_settings.STLINK_GDB_PORT #61200

# The port number for SWO trace capture out of ST-LINK GDB server
STLINK_SWO_PORT = u_settings.STLINK_SWO_PORT #61300

# The format string passed to strftime()
# for logging prints
TIME_FORMAT = u_settings.TIME_FORMAT #"%Y-%m-%d_%H:%M:%S"

# The default guard time waiting for a platform lock in seconds
PLATFORM_LOCK_GUARD_TIME_SECONDS = u_settings.PLATFORM_LOCK_GUARD_TIME_SECONDS #60 * 60

# The default guard time for downloading to a target in seconds
DOWNLOAD_GUARD_TIME_SECONDS = u_settings.DOWNLOAD_GUARD_TIME_SECONDS #60

# The default guard time for running tests in seconds
RUN_GUARD_TIME_SECONDS = u_settings.RUN_GUARD_TIME_SECONDS #60 * 60

# The default inactivity timer for running tests in seconds
RUN_INACTIVITY_TIME_SECONDS = u_settings.RUN_INACTIVITY_TIME_SECONDS #60 * 5

# The name of the #define that forms the filter string
# for which tests to run
FILTER_MACRO_NAME = u_settings.FILTER_MACRO_NAME #"U_CFG_APP_FILTER"

# The name of the environment variable that indicates we're running under automation
ENV_UBXLIB_AUTO = "U_UBXLIB_AUTO"

# The time for which to wait for something from the
# queue in exe_run().  If this is too short, in a
# multiprocessing world or on a slow machine, it is
# possible to miss things as the task putting things
# on the queue may be blocked from doing so until
# we've decided the queue has been completely emptied
# and moved on
EXE_RUN_QUEUE_WAIT_SECONDS = u_settings.EXE_RUN_QUEUE_WAIT_SECONDS #1

# The number of seconds a USB cutter and the bit positions of
# a KMTronic box are switched off for
HW_RESET_DURATION_SECONDS = u_settings.HW_RESET_DURATION_SECONDS # e.g. 5

# A marker to use to indicate that DTR/RTS should be held
# off when monitoring the serial output, required if the
# host is a NINA-W1 board
MONITOR_DTR_RTS_OFF_MARKER = "U_CFG_MONITOR_DTR_RTS_OFF"

# Executable file extension. This will be "" for Linux
# and ".exe" for Windows
EXE_EXT = pick_by_os(linux="", other=".exe")

def safe_print(string):
    '''Print a string avoiding pesky code-page decode errors'''
    try:
        print(string)
    except UnicodeEncodeError:
        for item in string:
            try:
                print(item, end='')
            except UnicodeEncodeError:
                print('?', end='')

def keep_going(flag, printer=None, prompt=None):
    '''Check a keep_going flag'''
    do_not_stop = True
    if flag is not None and not flag.is_set():
        do_not_stop = False
        if printer and prompt:
            printer.string("{}aborting as requested.".format(prompt))
    return do_not_stop

# subprocess arguments behaves a little differently on Linux and Windows
# depending if a shell is used or not, which can be read here:
# https://stackoverflow.com/a/15109975
# This function will compensate for these deviations
def subprocess_osify(cmd, shell=True):
    ''' expects an array of strings being [command, param, ...] '''
    if is_linux() and shell:
        line = ''
        for item in cmd:
            # Put everything in a single string and quote args containing spaces
            if ' ' in item:
                line += '\"{}\" '.format(item)
            else:
                line += '{} '.format(item)
        cmd = line
    return cmd

def split_command_line_args(cmd_line):
    ''' Will split a command line string into a list of arguments.
        Quoted arguments will be preserved as one argument '''
    return [p for p in re.split("( |\\\".*?\\\"|'.*?')", cmd_line) if p.strip()]

def get_actual_path(path):
    '''Given a drive number return real path if it is a subst'''
    actual_path = path
    if is_linux():
        return actual_path

    if os.name == 'nt':
        # Get a list of substs
        text = subprocess.check_output("subst",
                                       stderr=subprocess.STDOUT,
                                       shell=True)  # Jenkins hangs without this
        for line in text.splitlines():
            # Lines should look like this:
            # Z:\: => C:\projects\ubxlib_priv
            # So, in this example, if we were given z:\blah
            # then the actual path should be C:\projects\ubxlib_priv\blah
            text = line.decode()
            bits =  text.rsplit(": => ")
            if (len(bits) > 1) and (len(path) > 1) and \
              (bits[0].lower()[0:2] == path[0:2].lower()):
                actual_path = bits[1] + path[2:]
                break

    return actual_path

def get_instance_text(instance):
    '''Return the instance as a text string'''
    instance_text = ""

    for idx, item in enumerate(instance):
        if idx == 0:
            instance_text += str(item)
        else:
            instance_text += "." + str(item)

    return instance_text

# Get a list of instances as a text string separated
# by spaces.
def get_instances_text(instances):
    '''Return the instances as a text string'''
    instances_text = ""
    for instance in instances:
        if instance:
            instances_text += " {}".format(get_instance_text(instance))
    return instances_text

def remove_readonly(func, path, exec_info):
    '''Help deltree out'''
    del exec_info
    os.chmod(path, stat.S_IWRITE)
    func(path)

def deltree(directory, printer, prompt):
    '''Remove an entire directory tree'''
    tries = 3
    success = False

    if os.path.isdir(directory):
        # Retry this as sometimes Windows complains
        # that the directory is not empty when it
        # it really should be, some sort of internal
        # Windows race condition
        while not success and (tries > 0):
            try:
                # Need the onerror bit on Winders, see
                # this Stack Overflow post:
                # https://stackoverflow.com/questions/1889597/deleting-directory-in-python
                shutil.rmtree(directory, onerror=remove_readonly)
                success = True
            except OSError as ex:
                if printer and prompt:
                    printer.string("{}ERROR unable to delete \"{}\" {}: \"{}\"".
                                   format(prompt, directory,
                                          ex.errno, ex.strerror))
                sleep(1)
            tries -= 1
    else:
        success = True

    return success

# Some list types aren't quite list types: for instance,
# the lists returned by RPyC look like lists but they
# aren't of type list and so "in", for instance, will fail.
# This converts an instance list (i.e. a list-like object
# containing items that are each another list-like object)
# into a plain-old two-level list.
def copy_two_level_list(instances_in):
    '''Convert instances_in into a true list'''
    instances_out = []
    if instances_in:
        for item1 in instances_in:
            instances_out1 = []
            for item2 in item1:
                instances_out1.append(item2)
            instances_out.append(copy(instances_out1))
    return instances_out

# Check if admin privileges are available, from:
# https://stackoverflow.com/questions/2946746/python-checking-if-a-user-has-administrator-privileges
def has_admin():
    '''Check for administrator privileges'''
    admin = False

    if os.name == 'nt':
        try:
            # only Windows users with admin privileges can read the C:\windows\temp
            if os.listdir(os.sep.join([os.environ.get("SystemRoot", "C:\\windows"), "temp"])):
                admin = True
        except PermissionError:
            pass
    else:
        # Pylint will complain about the following line but
        # that's OK, it is only executed if we're NOT on Windows
        # and there the geteuid() method will exist
        if "SUDO_USER" in os.environ and os.geteuid() == 0:
            admin = True

    return admin

# Reset a USB port with the given Device Description
def usb_reset(device_description, printer, prompt):
    ''' Reset a device'''
    instance_id = None
    found = False
    success = False

    try:
        # Run devcon and parse the output to find the given device
        printer.string("{}running {} to look for \"{}\"...".   \
                       format(prompt, DEVCON_PATH, device_description))
        cmd = [DEVCON_PATH, "hwids", "=ports"]
        text = subprocess.check_output(subprocess_osify(cmd),
                                       stderr=subprocess.STDOUT,
                                       shell=True) # Jenkins hangs without this
        for line in text.splitlines():
            # The format of a devcon entry is this:
            #
            # USB\VID_1366&PID_1015&MI_00\6&38E81674&0&0000
            #     Name: JLink CDC UART Port (COM45)
            #     Hardware IDs:
            #         USB\VID_1366&PID_1015&REV_0100&MI_00
            #         USB\VID_1366&PID_1015&MI_00
            #     Compatible IDs:
            #         USB\Class_02&SubClass_02&Prot_00
            #         USB\Class_02&SubClass_02
            #         USB\Class_02
            #
            # Grab what we hope is the instance ID
            line = line.decode()
            if line.startswith("USB"):
                instance_id = line
            else:
                # If the next line is the Name we want then we're done
                if instance_id and ("Name: " + device_description in line):
                    found = True
                    printer.string("{}\"{}\" found with instance ID \"{}\"".    \
                                   format(prompt, device_description,
                                          instance_id))
                    break
                instance_id = None
        if found:
            # Now run devcon to reset the device
            printer.string("{}running {} to reset device \"{}\"...".   \
                           format(prompt, DEVCON_PATH, instance_id))
            cmd = [DEVCON_PATH, "restart", "@" + instance_id]
            text = subprocess.check_output(subprocess_osify(cmd),
                                           stderr=subprocess.STDOUT,
                                           shell=False) # Has to be False or devcon won't work
            for line in text.splitlines():
                printer.string("{}{}".format(prompt, line))
            success = True
        else:
            printer.string("{}device with description \"{}\" not found.".   \
                           format(prompt, device_description))
    except subprocess.CalledProcessError:
        printer.string("{} unable to find and reset device.".format(prompt))

    return success

# Open the required serial port.
def open_serial(serial_name, speed, printer, prompt, dtr_set_on=None, rts_set_on=None):
    '''Open serial port'''
    serial_handle = None
    text = "{}: trying to open \"{}\" as a serial port".    \
           format(prompt, serial_name)
    if dtr_set_on is not None:
        text += ", DTR forced"
        if dtr_set_on:
            text += " on"
        else:
            text += " off"
    if rts_set_on is not None:
        text += ", RTS forced"
        if rts_set_on:
            text += " on"
        else:
            text += " off"
    text += "..."
    try:
        return_value = serial.Serial(baudrate=speed, timeout=0.05)
        if dtr_set_on is not None:
            return_value.dtr = dtr_set_on
        if rts_set_on is not None:
            return_value.rts = rts_set_on
        return_value.port = serial_name
        return_value.open()
        serial_handle = return_value
        printer.string("{} opened.".format(text))
    except (ValueError, serial.SerialException) as ex:
        printer.string("{}{} while accessing port {}: {}.".
                       format(prompt, type(ex).__name__,
                              serial_handle.name, str(ex)))
    return serial_handle

def open_telnet(port_number, printer, prompt):
    '''Open telnet port on localhost'''
    telnet_handle = None
    text = "{}trying to open \"{}\" as a telnet port on localhost...".  \
           format(prompt, port_number)
    try:
        telnet_handle = Telnet("localhost", int(port_number), timeout=5)
        if telnet_handle is not None:
            printer.string("{} opened.".format(text))
        else:
            printer.string("{} failed.".format(text))
    except (socket.error, socket.timeout, ValueError) as ex:
        printer.string("{}{} failed to open telnet {}: {}.".
                       format(prompt, type(ex).__name__,
                              port_number, str(ex)))
    return telnet_handle

def install_lock_acquire(install_lock, printer, prompt, keep_going_flag=None):
    '''Attempt to acquire install lock'''
    timeout_seconds = INSTALL_LOCK_WAIT_SECONDS
    success = False

    if install_lock:
        printer.string("{}waiting for install lock...".format(prompt))
        while not install_lock.acquire(False) and (timeout_seconds > 0) and \
              keep_going(keep_going_flag, printer, prompt):
            sleep(1)
            timeout_seconds -= 1

        if timeout_seconds > 0:
            printer.string("{}got install lock.".format(prompt))
            success = True
        else:
            printer.string("{}failed to aquire install lock.".format(prompt))
    else:
        printer.string("{}warning, there is no install lock.".format(prompt))

    return success

def install_lock_release(install_lock, printer, prompt):
    '''Release install lock'''

    if install_lock:
        install_lock.release()
    printer.string("{}install lock released.".format(prompt))

def fetch_repo(url, directory, branch, printer, prompt, submodule_init=True, force=False):
    '''Fetch a repo: directory can be relative or absolute, branch can be a hash'''
    got_code = False
    success = False

    dir_text = directory
    if dir_text == ".":
        dir_text = "this directory"
    if printer and prompt:
        printer.string("{}in directory {}, fetching"
                       " {} to {}.".format(prompt, os.getcwd(),
                                          url, dir_text))
    if not branch:
        branch = "master"
    if os.path.isdir(directory):
        # Update existing code
        with ChangeDir(directory):
            if printer and prompt:
                printer.string("{}updating code in {}...".
                               format(prompt, dir_text))
            target = branch
            if branch.startswith("#"):
                # Actually been given a branch, lose the
                # preceding #
                target = branch[1:len(branch)]
            # Try this once and, if it fails and force is set,
            # do a git reset --hard and try again
            tries = 1
            if force:
                tries += 1
            while tries > 0:
                try:
                    call_list = []
                    call_list.append("git")
                    call_list.append("fetch")
                    call_list.append("origin")
                    call_list.append(target)
                    if printer and prompt:
                        text = ""
                        for item in call_list:
                            if text:
                                text += " "
                            text += item
                        printer.string("{}in {} calling {}...".
                                       format(prompt, os.getcwd(), text))
                    # Try to pull the code
                    text = subprocess.check_output(subprocess_osify(call_list),
                                                   stderr=subprocess.STDOUT,
                                                   shell=True) # Jenkins hangs without this
                    for line in text.splitlines():
                        if printer and prompt:
                            printer.string("{}{}".format(prompt, line))
                    got_code = True
                except subprocess.CalledProcessError as error:
                    if printer and prompt:
                        printer.string("{}git returned error {}: \"{}\"".
                                       format(prompt, error.returncode,
                                              error.output))
                if got_code:
                    tries = 0
                else:
                    if force:
                        # git reset --hard
                        printer.string("{}in directory {} calling git reset --hard...".   \
                                       format(prompt, os.getcwd()))
                        try:
                            text = subprocess.check_output(subprocess_osify(["git", "reset",
                                                            "--hard"]),
                                                           stderr=subprocess.STDOUT,
                                                           shell=True) # Jenkins hangs without this
                            for line in text.splitlines():
                                if printer and prompt:
                                    printer.string("{}{}".format(prompt, line))
                        except subprocess.CalledProcessError as error:
                            if printer and prompt:
                                printer.string("{}git returned error {}: \"{}\"".
                                               format(prompt, error.returncode,
                                                      error.output))
                        force = False
                    tries -= 1
        if not got_code:
            # If we still haven't got the code, delete the
            # directory for a true clean start
            deltree(directory, printer, prompt)
    if not os.path.isdir(directory):
        # Clone the repo
        if printer and prompt:
            printer.string("{}cloning from {} into {}...".
                           format(prompt, url, dir_text))
        try:
            text = subprocess.check_output(subprocess_osify(["git", "clone", "-q",
                                                             url, directory]),
                                           stderr=subprocess.STDOUT,
                                           shell=True) # Jenkins hangs without this
            for line in text.splitlines():
                if printer and prompt:
                    printer.string("{}{}".format(prompt, line))
            got_code = True
        except subprocess.CalledProcessError as error:
            if printer and prompt:
                printer.string("{}git returned error {}: \"{}\"".
                               format(prompt, error.returncode,
                                      error.output))

    if got_code and os.path.isdir(directory):
        # Check out the correct branch and recurse submodules
        with ChangeDir(directory):
            target = "origin/" + branch
            if branch.startswith("#"):
                # Actually been given a branch, so lose the
                # "origin/" and the preceding #
                target = branch[1:len(branch)]
            if printer and prompt:
                printer.string("{}checking out {}...".
                               format(prompt, target))
            try:
                call_list = ["git", "-c", "advice.detachedHead=false",
                             "checkout", "--no-progress"]
                if submodule_init:
                    call_list.append("--recurse-submodules")
                    printer.string("{}also recursing sub-modules (can take some time" \
                                   " and gives no feedback).".format(prompt))
                call_list.append(target)
                if printer and prompt:
                    text = ""
                    for item in call_list:
                        if text:
                            text += " "
                        text += item
                    printer.string("{}in {} calling {}...".
                                   format(prompt, os.getcwd(), text))
                text = subprocess.check_output(subprocess_osify(call_list),
                                               stderr=subprocess.STDOUT,
                                               shell=True) # Jenkins hangs without this
                for line in text.splitlines():
                    if printer and prompt:
                        printer.string("{}{}".format(prompt, line))
                success = True
            except subprocess.CalledProcessError as error:
                if printer and prompt:
                    printer.string("{}git returned error {}: \"{}\"".
                                   format(prompt, error.returncode,
                                          error.output))
    return success

def exe_where(exe_name, help_text, printer, prompt, set_env=None):
    '''Find an executable using where.exe or which on linux'''
    success = False

    try:
        printer.string("{}looking for \"{}\"...".          \
                       format(prompt, exe_name))
        # See here:
        # https://stackoverflow.com/questions/14928860/passing-double-quote-shell-commands-in-python-to-subprocess-popen
        # ...for why the construction "".join() is necessary when
        # passing things which might have spaces in them.
        # It is the only thing that works.
        if is_linux():
            cmd = ["which {}".format(exe_name.replace(":", "/"))]
            printer.string("{}detected linux, calling \"{}\"...".format(prompt, cmd))
        else:
            cmd = ["where", "".join(exe_name)]
            printer.string("{}detected nonlinux, calling \"{}\"...".format(prompt, cmd))
        text = subprocess.check_output(cmd, stderr=subprocess.STDOUT,
                                       env=set_env,
                                       shell=True) # Jenkins hangs without this
        for line in text.splitlines():
            printer.string("{}{} found in {}".format(prompt, exe_name, line))
        success = True
    except subprocess.CalledProcessError:
        if help_text:
            printer.string("{}ERROR {} not found: {}".  \
                           format(prompt, exe_name, help_text))
        else:
            printer.string("{}ERROR {} not found".      \
                           format(prompt, exe_name))

    return success

def exe_version(exe_name, version_switch, printer, prompt, set_env=None):
    '''Print the version of a given executable'''
    success = False

    if not version_switch:
        version_switch = "--version"
    try:
        text = subprocess.check_output(subprocess_osify(["".join(exe_name), version_switch]),
                                       stderr=subprocess.STDOUT, env=set_env,
                                       shell=True)  # Jenkins hangs without this
        for line in text.splitlines():
            printer.string("{}{}".format(prompt, line))
        success = True
    except subprocess.CalledProcessError:
        printer.string("{}ERROR {} either not found or didn't like {}". \
                       format(prompt, exe_name, version_switch))

    return success

def exe_terminate(process_pid):
    '''Jonathan's killer'''
    process = psutil.Process(process_pid)
    for proc in process.children(recursive=True):
        proc.terminate()
    process.terminate()

def read_from_process_and_queue(process, read_queue):
    '''Read from a process, non-blocking'''
    while process.poll() is None:
        string = process.stdout.readline().decode().encode("ascii", errors="replace").decode()
        if string and string != "":
            read_queue.put(string)
        else:
            sleep(0.1)

def queue_get_no_exception(the_queue, block=True, timeout=None):
    '''A version of queue.get() that doesn't throw an Empty exception'''
    thing = None

    try:
        thing = the_queue.get(block=block, timeout=timeout)
    except queue.Empty:
        pass

    return thing

def capture_env_var(line, env, printer, prompt):
    '''A bit of exe_run that needs to be called from two places'''
    # Find a KEY=VALUE bit in the line,
    # parse it out and put it in the dictionary
    # we were given
    pair = line.split('=', 1)
    if len(pair) == 2:
        env[pair[0]] = pair[1].rstrip()
    else:
        printer.string("{}WARNING: not an environment variable: \"{}\"".
                       format(prompt, line))

# Note: if returned_env is given then "set"
# will be executed after the exe and the environment
# variables will be returned in it.  The down-side
# of this is that the return value of the exe is,
# of course, lost.
def exe_run(call_list, guard_time_seconds=None, printer=None, prompt=None,
            shell_cmd=False, set_env=None, returned_env=None,
            bash_cmd=False, keep_going_flag=None):
    '''Call an executable, printing out what it does'''
    success = False
    start_time = time()
    flibbling = False
    kill_time = None
    read_time = start_time

    if returned_env is not None:
        # The caller wants the environment after the
        # command has run, so, from this post:
        # https://stackoverflow.com/questions/1214496/how-to-get-environment-from-a-subprocess
        # append a tag that we can detect
        # to the command and then call set,
        # from which we can parse the environment
        call_list.append("&&")
        call_list.append("echo")
        call_list.append("flibble")
        call_list.append("&&")
        if is_linux():
            call_list.append("env")
            bash_cmd = True
        else:
            call_list.append("set")
        # I've seen output from set get lost,
        # possibly because the process ending
        # is asynchronous with stdout,
        # so add a delay here as well
        call_list.append("&&")
        call_list.append("sleep")
        call_list.append("2")

    try:
        popen_keywords = {
            'stdout': subprocess.PIPE,
            'stderr': subprocess.STDOUT,
            'shell': shell_cmd,
            'env': set_env,
            'executable': "/bin/bash" if bash_cmd else None
        }
        # Call the thang
        # Note: used to have bufsize=1 here but it turns out
        # that is ignored 'cos the output is considered
        # binary.  Seems to work in any case, I guess
        # Winders, at least, is in any case line-buffered.

        process = subprocess.Popen(subprocess_osify(call_list, shell=shell_cmd),
                                   **popen_keywords)

        if printer:
            printer.string("{}{}, pid {} started with guard time {} second(s)". \
                           format(prompt, call_list[0], process.pid,
                                  guard_time_seconds))
        # This is over complex but, unfortunately, necessary.
        # At least one thing that we try to run, nrfjprog, can
        # crash silently: just hangs and sends no output.  However
        # it also doesn't flush and close stdout and so read(1)
        # will hang, meaning we can't read its output as a means
        # to check that it has hung.
        # So, here we poll for the return value, which is normally
        # how things will end, and we start another thread which
        # reads from the process's stdout.  If the thread sees
        # nothing for guard_time_seconds then we terminate the
        # process.
        read_queue = queue.Queue()
        read_thread = threading.Thread(target=read_from_process_and_queue,
                                       args=(process, read_queue))
        read_thread.start()
        while process.poll() is None:
            if keep_going_flag is None or keep_going(keep_going_flag, printer, prompt):
                if guard_time_seconds and (kill_time is None) and   \
                   ((time() - start_time > guard_time_seconds) or
                    (time() - read_time > guard_time_seconds)):
                    kill_time = time()
                    if printer:
                        printer.string("{}guard time of {} second(s)." \
                                       " expired, stopping {}...".
                                       format(prompt, guard_time_seconds,
                                              call_list[0]))
                    exe_terminate(process.pid)
            else:
                exe_terminate(process.pid)
            line = queue_get_no_exception(read_queue, True, EXE_RUN_QUEUE_WAIT_SECONDS)
            read_time = time()
            while line is not None:
                line = line.rstrip()
                if flibbling:
                    capture_env_var(line, returned_env, printer, prompt)
                else:
                    if returned_env is not None and "flibble" in line:
                        flibbling = True
                    else:
                        printer.string("{}{}".format(prompt, line))
                line = queue_get_no_exception(read_queue, True, EXE_RUN_QUEUE_WAIT_SECONDS)
                read_time = time()
            sleep(0.1)

        # Can't join() read_thread here as it might have
        # blocked on a read() (if nrfjprog has anything to
        # do with it).  It will be tidied up when this process
        # exits.

        # There may still be stuff on the queue, read it out here
        line = queue_get_no_exception(read_queue, True, EXE_RUN_QUEUE_WAIT_SECONDS)
        while line is not None:
            line = line.rstrip()
            if flibbling:
                capture_env_var(line, returned_env, printer, prompt)
            else:
                if returned_env is not None and "flibble" in line:
                    flibbling = True
                else:
                    printer.string("{}{}".format(prompt, line))
            line = queue_get_no_exception(read_queue, True, EXE_RUN_QUEUE_WAIT_SECONDS)

        # There may still be stuff in the buffer after
        # the application has finished running so flush that
        # out here
        line = process.stdout.readline().decode().encode("ascii", errors="replace").decode()
        while line:
            line = line.rstrip()
            if flibbling:
                capture_env_var(line, returned_env, printer, prompt)
            else:
                if returned_env is not None and "flibble" in line:
                    flibbling = True
                else:
                    printer.string("{}{}".format(prompt, line))
            line = process.stdout.readline().decode().encode("ascii", errors="replace").decode()

        if (process.poll() == 0) and kill_time is None:
            success = True
        if printer:
            printer.string("{}{}, pid {} ended with return value {}.".    \
                           format(prompt, call_list[0],
                                  process.pid, process.poll()))
    except ValueError as ex:
        if printer:
            printer.string("{}failed: {} while trying to execute {}.". \
                           format(prompt, type(ex).__name__, str(ex)))
    except KeyboardInterrupt as ex:
        process.kill()
        raise KeyboardInterrupt from ex

    return success

def set_process_prio_high():
    '''Set the priority of the current process to high'''
    if is_linux():
        print("Setting process priority currently not supported for Linux")
        # It should be possible to set prio with:
        #  psutil.Process().nice(-10)
        # However we get "[Errno 13] Permission denied" even when run as root
    else:
        psutil.Process().nice(psutil.HIGH_PRIORITY_CLASS)

def set_process_prio_normal():
    '''Set the priority of the current process to normal'''
    if is_linux():
        print("Setting process priority currently not supported for Linux")
        # It should be possible to set prio with:
        #  psutil.Process().nice(0)
        # However we get "[Errno 13] Permission denied" even when run as root
    else:
        psutil.Process().nice(psutil.NORMAL_PRIORITY_CLASS)

class ExeRun():
    '''Run an executable as a "with:"'''
    def __init__(self, call_list, printer=None, prompt=None, shell_cmd=False, with_stdin=False):
        self._call_list = call_list
        self._printer = printer
        self._prompt = prompt
        self._shell_cmd = shell_cmd
        self._with_stdin=with_stdin
        self._process = None
    def __enter__(self):
        if self._printer:
            text = ""
            for idx, item in enumerate(self._call_list):
                if idx == 0:
                    text = item
                else:
                    text += " {}".format(item)
            self._printer.string("{}starting {}...".format(self._prompt,
                                                           text))
        try:
            # Start exe
            popen_keywords = {
                'stdout': subprocess.PIPE,
                'stderr': subprocess.STDOUT,
                'shell': self._shell_cmd
            }
            if not is_linux():
                popen_keywords['creationflags'] = subprocess.CREATE_NEW_PROCESS_GROUP
            if self._with_stdin:
                popen_keywords['stdin'] = subprocess.PIPE

            self._process = subprocess.Popen(subprocess_osify(self._call_list,
                                                              shell=self._shell_cmd),
                                             **popen_keywords)

            if self._printer:
                self._printer.string("{}{} pid {} started".format(self._prompt,
                                                                  self._call_list[0],
                                                                  self._process.pid))
        except (OSError, subprocess.CalledProcessError, ValueError) as ex:
            if self._printer:
                self._printer.string("{}failed: {} to start {}.". \
                                     format(self._prompt,
                                            type(ex).__name__, str(ex)))
        except KeyboardInterrupt as ex:
            self._process.kill()
            raise KeyboardInterrupt from ex
        return self._process
    def __exit__(self, _type, value, traceback):
        del _type
        del value
        del traceback
        # Stop exe
        if self._printer:
            self._printer.string("{}stopping {}...". \
                                 format(self._prompt,
                                        self._call_list[0]))
        return_value = self._process.poll()
        if not return_value:
            retry = 5
            while (self._process.poll() is None) and (retry > 0):
                # Try to stop with CTRL-C
                if is_linux():
                    sig = signal.SIGINT
                else:
                    sig = signal.CTRL_BREAK_EVENT
                self._process.send_signal(sig)
                sleep(1)
                retry -= 1
            return_value = self._process.poll()
            if not return_value:
                # Terminate with a vengeance
                self._process.terminate()
                while self._process.poll() is None:
                    sleep(0.1)
                if self._printer:
                    self._printer.string("{}{} pid {} terminated".format(self._prompt,
                                                                         self._call_list[0],
                                                                         self._process.pid))
            else:
                if self._printer:
                    self._printer.string("{}{} pid {} CTRL-C'd".format(self._prompt,
                                                                       self._call_list[0],
                                                                       self._process.pid))
        else:
            if self._printer:
                self._printer.string("{}{} pid {} already ended".format(self._prompt,
                                                                        self._call_list[0],
                                                                        self._process.pid))

        return return_value

# Simple SWO decoder: only handles single bytes of application
# data at a time, i.e. what ITM_SendChar() sends.
class SwoDecoder():
    '''Take the contents of a byte_array and decode it as SWO'''
    def __init__(self, address, replaceLfWithCrLf=False):
        self._address = address
        self._replace_lf_with_crlf = replaceLfWithCrLf
        self._expecting_swit = True

    def decode(self, swo_byte_array):
        '''Do the decode'''
        decoded_byte_array = bytearray()
        if swo_byte_array:
            for data_byte in swo_byte_array:
                # We're looking only for "address" and we also know
                # that CMSIS only offers ITM_SendChar(), so packet length
                # is always 1, and we only send ASCII characters,
                # so the top bit of the data byte must be 0.
                #
                # For the SWO protocol, see:
                #
                # https://developer.arm.com/documentation/ddi0314/h/
                # instrumentation-trace-macrocell/
                # about-the-instrumentation-trace-macrocell/trace-packet-format
                #
                # When we see SWIT (SoftWare Instrumentation Trace
                # I think, anyway, the bit that carries our prints
                # off the target) which is 0bBBBBB0SS, where BBBBB is
                # address and SS is the size of payload to follow,
                # in our case 0x01, we know that the next
                # byte is probably data and if it is ASCII then
                # it is data.  Anything else is ignored.
                # The reason for doing it this way is that the
                # ARM ITM only sends out sync packets under
                # special circumstances so it is not a recovery
                # mechanism for simply losing a byte in the
                # transfer, which does happen occasionally.
                if self._expecting_swit:
                    if ((data_byte & 0x03) == 0x01) and ((data_byte & 0xf8) >> 3 == self._address):
                        # Trace packet type is SWIT, i.e. our
                        # application logging
                        self._expecting_swit = False
                else:
                    if data_byte & 0x80 == 0:
                        if (data_byte == 10) and self._replace_lf_with_crlf:
                            decoded_byte_array.append(13)
                        decoded_byte_array.append(data_byte)
                    self._expecting_swit = True
        return decoded_byte_array

class PrintThread(threading.Thread):
    '''Print thread to organise prints nicely'''
    def __init__(self, print_queue, file_handle=None,
                 window_file_handle=None, window_size=10000,
                 window_update_period_seconds=1):
        self._queue = print_queue
        self._lock = RLock()
        self._queue_forwards = []
        self._running = False
        self._file_handle = file_handle
        self._window = None
        self._window_file_handle = window_file_handle
        if self._window_file_handle:
            self._window = deque(self._window_file_handle, maxlen=window_size)
        self._window_update_pending = False
        self._window_update_period_seconds = window_update_period_seconds
        self._window_next_update_time = time()
        threading.Thread.__init__(self)
    def _send_forward(self, flush=False):
        # Send from any forwarding buffers
        # self._lock should be acquired before this is called
        queue_idxes_to_remove = []
        for idx, queue_forward in enumerate(self._queue_forwards):
            if flush or time() > queue_forward["last_send"] + queue_forward["buffer_time"]:
                string_forward = ""
                len_queue_forward = len(queue_forward["buffer"])
                count = 0
                for item in queue_forward["buffer"]:
                    count += 1
                    if count < len_queue_forward:
                        item += "\n"
                    if queue_forward["prefix_string"]:
                        item = queue_forward["prefix_string"] + item
                    string_forward += item
                queue_forward["buffer"] = []
                if string_forward:
                    try:
                        queue_forward["queue"].put(string_forward)
                    except TimeoutError:
                        pass
                    except (OSError, EOFError, BrokenPipeError):
                        queue_idxes_to_remove.append(idx)
                queue_forward["last_send"] = time()
        for idx in queue_idxes_to_remove:
            self._queue_forwards.pop(idx)
    def add_forward_queue(self, queue_forward, prefix_string=None, buffer_time=0):
        '''Forward things received on the print queue to another queue'''
        self._lock.acquire()
        already_done = False
        for item in self._queue_forwards:
            if item["queue"] == queue_forward:
                already_done = True
                break
        if not already_done:
            item = {}
            item["queue"] = queue_forward
            item["prefix_string"] = prefix_string
            item["buffer"] = []
            item["buffer_time"] = buffer_time
            item["last_send"] = time()
            self._queue_forwards.append(item)
        self._lock.release()
    def remove_forward_queue(self, queue_forward):
        '''Stop forwarding things received on the print queue to another queue'''
        self._lock.acquire()
        queues = []
        self._send_forward(flush=True)
        for item in self._queue_forwards:
            if item["queue"] != queue_forward:
                queues.append(item)
        self._queue_forwards = queues
        self._lock.release()
    def stop_thread(self):
        '''Helper function to stop the thread'''
        self._lock.acquire()
        self._running = False
        # Write anything remaining to the window file
        if self._window_update_pending:
            self._window_file_handle.seek(0)
            for item in self._window:
                self._window_file_handle.write(item)
            self._window_file_handle.flush()
            self._window_update_pending = False
            self._window_next_update_time = time() + self._window_update_period_seconds
        self._lock.release()
    def run(self):
        '''Worker thread'''
        self._running = True
        while self._running:
            # Print locally and store in any forwarding buffers
            try:
                my_string = self._queue.get(block=False, timeout=0.5)
                print(my_string)
                if self._file_handle:
                    self._file_handle.write(my_string + "\n")
                self._lock.acquire()
                if self._window is not None:
                    # Note that my_string can contain multiple lines,
                    # hence the need to split it here to maintain the
                    # window
                    for line in my_string.splitlines():
                        self._window.append(line + "\n")
                    self._window_update_pending = True
                for queue_forward in self._queue_forwards:
                    queue_forward["buffer"].append(my_string)
                self._lock.release()
            except queue.Empty:
                sleep(0.1)
            except (OSError, EOFError, BrokenPipeError):
                # Try to restore stdout
                sleep(0.1)
                sys.stdout = sys.__stdout__
            self._lock.acquire()
            # Send from any forwarding buffers
            self._send_forward()
            # Write the window to file if required
            if self._window_update_pending and time() > self._window_next_update_time:
                # If you don't do this you can end up with garbage
                # at the end of the file
                self._window_file_handle.truncate()
                self._window_file_handle.seek(0)
                for item in self._window:
                    self._window_file_handle.write(item)
                self._window_update_pending = False
                self._window_next_update_time = time() + self._window_update_period_seconds
            self._lock.release()

class PrintToQueue():
    '''Print to a queue, if there is one'''
    def __init__(self, print_queue, file_handle, include_timestamp=False):
        self._queues = []
        self._lock = RLock()
        if print_queue:
            self._queues.append(print_queue)
        self._file_handle = file_handle
        self._include_timestamp = include_timestamp
    def add_queue(self, print_queue):
        '''Add a queue to the list of places to print to'''
        self._lock.acquire()
        already_done = False
        for item in self._queues:
            if item == print_queue:
                already_done = True
                break
        if not already_done:
            self._queues.append(print_queue)
        self._lock.release()
    def remove_queue(self, print_queue):
        '''Remove a queue from  the list of places to print to'''
        self._lock.acquire()
        queues = []
        for item in self._queues:
            if item != print_queue:
                queues.append(item)
        self._queues = queues
        self._lock.release()
    def string(self, string, file_only=False):
        '''Print a string to the queue(s)'''
        if self._include_timestamp:
            string = strftime(TIME_FORMAT, gmtime()) + " " + string
        if not file_only:
            self._lock.acquire()
            queue_idxes_to_remove = []
            if self._queues:
                for idx, print_queue in enumerate(self._queues):
                    try:
                        print_queue.put(string)
                    except (EOFError, BrokenPipeError):
                        queue_idxes_to_remove.append(idx)
                for idx in queue_idxes_to_remove:
                    self._queues.pop(idx)
            else:
                safe_print(string)
            self._lock.release()
        if self._file_handle:
            self._file_handle.write(string + "\n")
            self._file_handle.flush()

# This stolen from here:
# https://stackoverflow.com/questions/431684/how-do-i-change-the-working-directory-in-python
class ChangeDir():
    '''Context manager for changing the current working directory'''
    def __init__(self, new_path):
        self._new_path = os.path.expanduser(new_path)
        self._saved_path = None
    def __enter__(self):
        '''CD to new_path'''
        self._saved_path = os.getcwd()
        os.chdir(self._new_path)
    def __exit__(self, etype, value, traceback):
        '''CD back to saved_path'''
        os.chdir(self._saved_path)

class Lock():
    '''Hold a lock as a "with:"'''
    def __init__(self, lock, guard_time_seconds,
                 lock_type, printer, prompt, keep_going_flag=None):
        self._lock = lock
        self._guard_time_seconds = guard_time_seconds
        self._lock_type = lock_type
        self._printer = printer
        self._prompt = prompt
        self._keep_going_flag = keep_going_flag
        self._locked = False
    def __enter__(self):
        if not self._lock:
            return True
        # Wait on the lock
        if not self._locked:
            timeout_seconds = self._guard_time_seconds
            self._printer.string("{}waiting up to {} second(s)"      \
                                 " for a {} lock...".                \
                                 format(self._prompt,
                                        self._guard_time_seconds,
                                        self._lock_type))
            count = 0
            while not self._lock.acquire(False) and                            \
                ((self._guard_time_seconds == 0) or (timeout_seconds > 0)) and \
                keep_going(self._keep_going_flag, self._printer, self._prompt):
                sleep(1)
                timeout_seconds -= 1
                count += 1
                if count == 30:
                    self._printer.string("{}still waiting {} second(s)"     \
                                         " for a {} lock (locker is"        \
                                         " currently {}).".                 \
                                         format(self._prompt, timeout_seconds,
                                                self._lock_type, self._lock))
                    count = 0
            if (self._guard_time_seconds == 0) or (timeout_seconds > 0):
                self._locked = True
                self._printer.string("{}{} lock acquired ({}).".              \
                                     format(self._prompt, self._lock_type,
                                            self._lock))
        return self._locked
    def __exit__(self, _type, value, traceback):
        del _type
        del value
        del traceback
        if self._lock and self._locked:
            try:
                self._lock.release()
                self._locked = False
                self._printer.string("{}released a {} lock.".format(self._prompt,
                                                                    self._lock_type))
            except RuntimeError:
                self._locked = False
                self._printer.string("{}{} lock was already released.". \
                                     format(self._prompt, self._lock_type))

def wait_for_completion(_list, purpose, guard_time_seconds,
                        printer, prompt, keep_going_flag):
    '''Wait for a completion list to empty'''
    completed = False
    if len(_list) > 0:
        timeout_seconds = guard_time_seconds
        printer.string("{}waiting up to {} second(s)"      \
                       " for {} completion...".          \
                       format(prompt, guard_time_seconds, purpose))
        count = 0
        while (len(_list) > 0) and                                 \
              ((guard_time_seconds == 0) or (timeout_seconds > 0)) and \
              keep_going(keep_going_flag, printer, prompt):
            sleep(1)
            timeout_seconds -= 1
            count += 1
            if count == 30:
                list_text = ""
                for item in _list:
                    if list_text:
                        list_text += ", "
                    list_text += str(item)
                printer.string("{}still waiting {} second(s)"   \
                               " for {} to complete (waiting"   \
                               " for {}).".                     \
                               format(prompt, timeout_seconds,
                                      purpose, list_text))
                count = 0
        if len(_list) == 0:
            completed = True
            printer.string("{}{} completed.".format(prompt, purpose))
    return completed

def reset_nrf_target(connection, printer, prompt):
    '''Reset a Nordic NRFxxx target'''
    call_list = []

    printer.string("{}resetting target...".format(prompt))
    # Assemble the call list
    call_list.append("nrfjprog")
    call_list.append("--reset")
    if connection and "debugger" in connection and connection["debugger"]:
        call_list.append("-s")
        call_list.append(connection["debugger"])

    # Print what we're gonna do
    tmp = ""
    for item in call_list:
        tmp += " " + item
    printer.string("{}in directory {} calling{}".         \
                   format(prompt, os.getcwd(), tmp))

    # Call it
    return exe_run(call_list, 60, printer, prompt)

def usb_cutter_reset(usb_cutter_id_strs, printer, prompt):
    '''Cut and then un-cut USB cables using Cleware USB cutters'''

    # First switch the USB cutters off
    action = "1"
    count = 0
    call_list_root = ["usbswitchcmd"]
    call_list_root.append("-s")
    call_list_root.append("-n")
    while count < 2:
        for usb_cutter_id_str in usb_cutter_id_strs:
            call_list = call_list_root.copy()
            call_list.append(usb_cutter_id_str)
            call_list.append(action)

            # Print what we're gonna do
            tmp = ""
            for item in call_list:
                tmp += " " + item
            if printer:
                printer.string("{}in directory {} calling{}".         \
                               format(prompt, os.getcwd(), tmp))

            # Set shell to keep Jenkins happy
            exe_run(call_list, 0, printer, prompt, shell_cmd=True)

        # Wait 5ish seconds
        if printer:
            printer.string("{}waiting {} second(s)...".         \
                           format(prompt, HW_RESET_DURATION_SECONDS))
        sleep(HW_RESET_DURATION_SECONDS)

        # "0" to switch the USB cutters on again
        action = "0"
        count += 1

def kmtronic_reset(ip_address, hex_bitmap, printer, prompt):
    '''Cut and then un-cut power using a KMTronic box'''

    # KMTronic is a web relay box which will be controlling
    # power to, for instance, EVKs  The last byte of the URL
    # is a hex bitmap of the outputs where 0 sets off and 1
    # sets on

    # Take only the last two digits of the hex bitmap
    hex_bitmap_len = len(hex_bitmap)
    hex_bitmap = hex_bitmap[hex_bitmap_len - 2:hex_bitmap_len]
    kmtronic_off = "http://" + ip_address + "FFE0" + hex_bitmap
    kmtronic_on = "http://" + ip_address + "FFE0" + "{0:x}".format(int(hex_bitmap, 16) ^ 0xFF)

    try:
        # First switch the given bit positions off
        if printer:
            printer.string("{}sending {}".         \
                           format(prompt, kmtronic_off))
        response = requests.get(kmtronic_off)
        # Wait 5ish seconds
        if printer:
            printer.string("{}...received response {}, waiting {} second(s)...". \
                           format(prompt, response.status_code, HW_RESET_DURATION_SECONDS))
        sleep(HW_RESET_DURATION_SECONDS)
        # Switch the given bit positions on
        if printer:
            printer.string("{}sending {}".format(prompt, kmtronic_on))
        response = requests.get(kmtronic_on)
        if printer:
            printer.string("{}...received response {}.". \
                           format(prompt, response.status_code))
    except requests.ConnectionError:
        if printer:
            printer.string("{}unable to connect to KMTronic box at {}.". \
                           format(prompt, ip_address))

# Look for a single line anywhere in message
# beginning with "test: ".  This must be followed by
# "x.y.z a.b.c m.n.o" (i.e. instance IDs space separated)
# and then an optional "blah" filter string, or just "*"
# and an optional "blah" filter string or "None".
# Valid examples are:
#
# test: 1
# test: 1 3 7
# test: 1.0.3 3 7.0
# test: 1 2 example
# test: 1.1 8 portInit
# test: *
# test: * port
# test: none
#
# Filter strings must NOT begin with a digit.
# There cannot be more than one * or a * with any other instance.
# There can only be one filter string.
# Only whitespace is expected after this on the line.
# Anything else is ignored.
# Populates instances with the "0 4.5 13.5.1" bit as instance
# entries [[0], [4, 5], [13, 5, 1]] and returns the filter
# string, if any.
def commit_message_parse(message, instances, printer=None, prompt=None):
    '''Find stuff in a commit message'''
    instances_all = False
    instances_local = []
    filter_string_local = None
    found = False

    if message:
        # Search through message for a line beginning
        # with "test:"
        if printer:
            printer.string("{}### parsing message to see if it contains a test directive...". \
                           format(prompt))
        lines = message.split("\\n")
        for idx1, line in enumerate(lines):
            if printer:
                printer.string("{}text line {}: \"{}\"".format(prompt, idx1 + 1, line))
            if line.lower().startswith("test:"):
                found = True
                instances_all = False
                # Pick through what follows
                parts = line[5:].split()
                for part in parts:
                    if instances_all and (part[0].isdigit() or part == "*" or part.lower() == "none"):
                        # If we've had a "*" and this is another one
                        # or it begins with a digit then this is
                        # obviously not a "test:" line,
                        # leave the loop and try again.
                        instances_local = []
                        filter_string_local = None
                        if printer:
                            printer.string("{}...badly formed test directive, ignoring.". \
                                           format(prompt))
                            found = False
                        break
                    if filter_string_local:
                        # If we've had a filter string then nothing
                        # must follow so this is not a "test:" line,
                        # leave the loop and try again.
                        instances_local = []
                        filter_string_local = None
                        if printer:
                            printer.string("{}...extraneous characters after test directive," \
                                           " ignoring.".format(prompt))
                            found = False
                        break
                    if part[0].isdigit():
                        # If this part begins with a digit it could
                        # be an instance containing numbers
                        instance = []
                        bad = False
                        for item in part.split("."):
                            try:
                                instance.append(int(item))
                            except ValueError:
                                # Some rubbish, not a test line so
                                # leave the loop and try the next
                                # line
                                bad = True
                                break
                        if bad:
                            instances_local = []
                            filter_string_local = None
                            if printer:
                                printer.string("{}...badly formed test directive, ignoring.". \
                                               format(prompt))
                            found = False
                            break
                        if instance:
                            instances_local.append(instance[:])
                    elif part == "*":
                        if instances_local:
                            # If we've already had any instances
                            # this is obviously not a test line,
                            # leave the loop and try again
                            instances_local = []
                            filter_string_local = None
                            if printer:
                                printer.string("{}...badly formed test directive, ignoring.". \
                                               format(prompt))
                                found = False
                            break
                        # If we haven't had any instances and
                        # this is a * then it means "all"
                        instances_local.append(part)
                        instances_all = True
                    elif part.lower() == "none":
                        if instances_local:
                            # If we've already had any instances
                            # this is obviously not a test line,
                            # leave the loop and try again
                            if printer:
                                printer.string("{}...badly formed test directive, ignoring.". \
                                               format(prompt))
                                found = False
                        instances_local = []
                        filter_string_local = None
                        break
                    elif instances_local and not part == "*":
                        # If we've had an instance and this
                        # is not a "*" then this must be a
                        # filter string
                        filter_string_local = part
                    else:
                        # Found some rubbish, not a "test:"
                        # line after all, leave the loop
                        # and try the next line
                        instances_local = []
                        filter_string_local = None
                        if printer:
                            printer.string("{}...badly formed test directive, ignoring.". \
                                           format(prompt))
                            found = False
                        break
                if found:
                    text = "found test directive with"
                    if instances_local:
                        text += " instance(s)" + get_instances_text(instances_local)
                        if filter_string_local:
                            text += " and filter \"" + filter_string_local + "\""
                    else:
                        text += " instances \"None\""
                    if printer:
                        printer.string("{}{}.".format(prompt, text))
                    break
                if printer:
                    printer.string("{}no test directive found".format(prompt))

    if found and instances_local:
        instances.extend(instances_local[:])

    return found, filter_string_local
