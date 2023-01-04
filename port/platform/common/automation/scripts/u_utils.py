#!/usr/bin/env python

'''Generally useful bits and bobs.'''

import queue                    # For PrintThread and exe_run
from time import sleep, time    # For lock timeout, exe_run timeout and logging
import threading                # For PrintThread
import sys
import os                       # For ChangeDir, has_admin
import stat                     # To help deltree out
from telnetlib import Telnet    # For talking to JLink server
from logging import Logger
import logging
import socket
import shutil                   # To delete a directory tree
import signal                   # For CTRL_C_EVENT
import subprocess
import platform                 # Figure out current OS
import serial                   # Pyserial (make sure to do pip install pyserial)
import psutil                   # For killing things (make sure to do pip install psutil)
import requests                 # For HTTP comms with a KMTronic box (do pip install requests)
from scripts import u_settings

DEFAULT_LOGGER = logging.getLogger()

# Since this function is used by the global variables below it needs
# to be placed here.
def is_linux():
    '''Returns True when system is Linux'''
    return platform.system() == 'Linux'

def is_automation():
    """Returns True if running automated (detected by checking if there is a TTY)"""
    return not sys.stdin.isatty()


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

# How long to wait for an install lock in seconds
INSTALL_LOCK_WAIT_SECONDS = u_settings.INSTALL_LOCK_WAIT_SECONDS #(60 * 60)

# The URL for Unity, the unit test framework
UNITY_URL = u_settings.UNITY_URL #"https://github.com/ThrowTheSwitch/Unity"

# The sub-directory that Unity is usually put in
# (off the working directory)
UNITY_SUBDIR = u_settings.UNITY_SUBDIR #"Unity"

# The port number for SWO trace capture out of JLink
JLINK_SWO_PORT = u_settings.JLINK_SWO_PORT #19021

# The format string passed to strftime()
# for logging prints
TIME_FORMAT = u_settings.TIME_FORMAT #"%Y-%m-%d_%H:%M:%S"

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

# The ubxlib root directory
UBXLIB_DIR = os.path.abspath(os.path.dirname(__file__) + "/../../../../..")
# Visual Studio Code directory
VSCODE_DIR = UBXLIB_DIR + "/.vscode"
# Platform directory
PLATFORM_DIR = UBXLIB_DIR + "/port/platform"
# Automation directory
AUTOMATION_DIR = UBXLIB_DIR + "/port/platform/common/automation"
# Automation scripts directory
SCRIPTS_DIR = AUTOMATION_DIR + "/scripts"
# OpenOCD config dir used for automation
OPENOCD_CFG_DIR = AUTOMATION_DIR + "/cfg"
# CodeChecker config dir used for automation
CODECHECKER_CFG_DIR = AUTOMATION_DIR + "/cfg/codechecker"
# CodeChecker config files
CODECHECKER_IGNORE_FILE = f"{CODECHECKER_CFG_DIR}/ignore_file.cfg"
CODECHECKER_CFG_FILE = f"{CODECHECKER_CFG_DIR}/codechecker.yml"


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
                line += f'\"{item}\" '
            else:
                line += f'{item} '
        cmd = line
    return cmd

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
            instances_text += f" {get_instance_text(instance)}"
    return instances_text

def remove_readonly(func, path, exec_info):
    '''Help deltree out'''
    del exec_info
    os.chmod(path, stat.S_IWRITE)
    func(path)

def deltree(directory, logger: Logger = DEFAULT_LOGGER):
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
                logger.error(f'ERROR unable to delete "{directory}" {ex.errno}: "{ex.strerror}"')
                sleep(1)
            tries -= 1
    else:
        success = True

    return success

# Open the required serial port.
def open_serial(serial_name, speed, logger: Logger = DEFAULT_LOGGER,
                dtr_set_on=None, rts_set_on=None):
    '''Open serial port'''
    serial_handle = None
    text = f"trying to open \"{serial_name}\" as a serial port"
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
        logger.info(f"{text} opened.")
    except (ValueError, serial.SerialException) as ex:
        logger.warning(f"{type(ex).__name__} while accessing port {serial_name}: {str(ex)}.")
    return serial_handle

def open_telnet(port_number, logger: Logger = DEFAULT_LOGGER):
    '''Open telnet port on localhost'''
    telnet_handle = None
    text = f"trying to open \"{port_number}\" as a telnet port on localhost..."
    try:
        telnet_handle = Telnet("localhost", int(port_number), timeout=5)
        if telnet_handle is not None:
            logger.info(f"{text} opened.")
        else:
            logger.warning(f"{text} failed.")
    except (socket.error, socket.timeout, ValueError) as ex:
        logger.warning(f"{type(ex).__name__} failed to open telnet {port_number}: {str(ex)}.")
    return telnet_handle

def run_call(call_list, logger: Logger = DEFAULT_LOGGER, shell_cmd=False):
    ''' Run a call_list through subprocess.check_output() '''
    success = False
    try:
        text = " ".join(call_list)
        logger.info(f"in {os.getcwd()} calling {text}...")
        # Try to pull the code
        text = subprocess.check_output(subprocess_osify(call_list),
                                       stderr=subprocess.STDOUT,
                                       shell=shell_cmd)
        for line in text.splitlines():
            logger.info(line)
        success = True
    except subprocess.CalledProcessError as error:
        logger.error(f"{call_list[0]} returned error {error}: \"{error.output}\"")
    return success

def git_cleanup(logger: Logger = DEFAULT_LOGGER, shell_cmd=False):
    ''' Antevir's recommended clean-up procedure '''
    logger.info(f"trying to clean up...")
    run_call(["git", "reset", "--hard", "HEAD"], logger=logger, shell_cmd=shell_cmd)
    run_call(["git", "submodule", "foreach", "--recursive", "git", \
              "reset", "--hard"], logger=logger, shell_cmd=shell_cmd)
    # The "double f" forces cleaning of directories with .git subdirectories
    run_call(["git", "clean", "-xfdf"], logger=logger, shell_cmd=shell_cmd)
    run_call(["git", "submodule", "foreach", "--recursive", "git", \
              "clean", "-xfdf"], logger=logger, shell_cmd=shell_cmd)
    run_call(["git", "submodule", "sync", "--recursive"], logger=logger, shell_cmd=shell_cmd)
    run_call(["git", "submodule", "update", "--init", "--recursive"], logger=logger, shell_cmd=shell_cmd)

def fetch_repo(url, directory, branch, logger: Logger = DEFAULT_LOGGER,
               submodule_init=True, force=False):
    '''Fetch a repo: directory can be relative or absolute, branch can be a hash'''
    got_code = False
    success = False

    dir_text = directory
    if dir_text == ".":
        dir_text = "this directory"

    logger.info(f"in directory {os.getcwd()}, fetching {url} to {dir_text}.")
    if not branch:
        branch = "master"
    if os.path.isdir(directory):
        # Update existing code
        with ChangeDir(directory):
            logger.info(f"updating code in {dir_text}...")
            target = branch
            if branch.startswith("#"):
                # Actually been given a branch, lose the
                # preceding #
                target = branch[1:len(branch)]
            # Jenkins can hang without True here
            got_code = run_call(["git", "fetch", "origin", target],
                                logger=logger, shell_cmd=True)
            if not got_code and force:
                # If it didn't work, clean up and try again
                git_cleanup(logger=logger, shell_cmd=True)
                got_code = run_call(["git", "fetch", "origin", target],
                                    logger=logger, shell_cmd=True)
        if force and not got_code:
            # If we still haven't got the code, delete the
            # directory for a true clean start
            deltree(directory, logger=logger)
    if not os.path.isdir(directory):
        # Clone the repo
        logger.info(f"cloning from {url} into {dir_text}...")
        call_list = ["git", "clone", "-q"]
        call_list.append(url)
        call_list.append(directory)
        got_code = run_call(call_list, logger=logger, shell_cmd=True)
        if got_code and  submodule_init:
            with ChangeDir(directory):
                logger.info("also recursing sub-modules (can take some time)")
                run_call(["git", "submodule", "update", "--init", "--recursive"],
                         logger=logger, shell_cmd=True)

    if got_code and os.path.isdir(directory):
        # Check out the correct branch and recurse submodules
        with ChangeDir(directory):
            target = "origin/" + branch
            if branch.startswith("#"):
                # Actually been given a branch, so lose the
                # "origin/" and the preceding #
                target = branch[1:len(branch)]
            logger.info(f"checking out {target}...")
            call_list = ["git", "-c", "advice.detachedHead=false",
                         "checkout", "--no-progress"]
            if submodule_init:
                call_list.append("--recurse-submodules")
                logger.info("also recursing sub-modules (can take some time" \
                                   " and gives no feedback).")
            call_list.append(target)
            success = run_call(call_list, logger=logger, shell_cmd=True)
            if not success:
                # If it didn't work, clean up and try again
                git_cleanup(logger=logger, shell_cmd=True)
                success = run_call(call_list, logger=logger, shell_cmd=True)

    return success

def exe_where(exe_name, help_text, set_env=None,
              logger: Logger = DEFAULT_LOGGER):
    '''Find an executable using where.exe or which on linux'''
    success = False

    try:
        logger.info(f'looking for "{exe_name}"...')
        # See here:
        # https://stackoverflow.com/questions/14928860/passing-double-quote-shell-commands-in-python-to-subprocess-popen
        # ...for why the construction "".join() is necessary when
        # passing things which might have spaces in them.
        # It is the only thing that works.
        if is_linux():
            cmd = [f"which {exe_name.replace(':', '/')}"]
            logger.info(f'detected linux, calling "{cmd}"...')
        else:
            cmd = ["where", "".join(exe_name)]
            logger.info(f'detected nonlinux, calling "{cmd}"...')
        text = subprocess.check_output(cmd, stderr=subprocess.STDOUT,
                                       env=set_env,
                                       shell=True) # Jenkins hangs without this
        for line in text.splitlines():
            logger.info(f"{exe_name} found in {line}")
        success = True
    except subprocess.CalledProcessError:
        if help_text:
            logger.error(f"ERROR {exe_name} not found: {help_text}")
        else:
            logger.error(f"ERROR {exe_name} not found")

    return success

def exe_version(exe_name, version_switch,
                logger: Logger=DEFAULT_LOGGER, set_env=None):
    '''Print the version of a given executable'''
    success = False

    if not version_switch:
        version_switch = "--version"
    try:
        text = subprocess.check_output(subprocess_osify(["".join(exe_name), version_switch]),
                                       stderr=subprocess.STDOUT, env=set_env,
                                       shell=True)  # Jenkins hangs without this
        for line in text.splitlines():
            logger.info(line)
        success = True
    except subprocess.CalledProcessError:
        logger.error(f"ERROR {exe_name} either not found or didn't like {version_switch}")

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

def capture_env_var(line, env, logger: Logger = DEFAULT_LOGGER):
    '''A bit of exe_run that needs to be called from two places'''
    # Find a KEY=VALUE bit in the line,
    # parse it out and put it in the dictionary
    # we were given
    pair = line.split('=', 1)
    if len(pair) == 2:
        env[pair[0]] = pair[1].rstrip()
    else:
        logger.warning(f'WARNING: not an environment variable: "{line}"')

# Note: if returned_env is given then "set"
# will be executed after the exe and the environment
# variables will be returned in it.  The down-side
# of this is that the return value of the exe is,
# of course, lost.
def exe_run(call_list, guard_time_seconds=None,
            logger: Logger = DEFAULT_LOGGER,
            shell_cmd=False, set_env=None, returned_env=None,
            bash_cmd=False):
    '''Call an executable, printing out what it does'''
    success = False
    start_time = time()
    flibbling = False
    kill_time = None
    read_time = start_time

    # Print what we're gonna do
    logger.info(f"in directory {os.getcwd()} calling {' '.join(call_list)}")

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

        logger.info(f"{call_list[0]}, pid {process.pid} started with guard" \
                    f" time {guard_time_seconds} second(s)")
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
            if guard_time_seconds and (kill_time is None) and   \
               ((time() - start_time > guard_time_seconds) or
                (time() - read_time > guard_time_seconds)):
                kill_time = time()
                logger.warning(f"guard time of {guard_time_seconds} second(s) expired," \
                               f" stopping {call_list[0]}...")
                exe_terminate(process.pid)
            line = queue_get_no_exception(read_queue, True, EXE_RUN_QUEUE_WAIT_SECONDS)
            read_time = time()
            while line is not None:
                line = line.rstrip()
                if flibbling:
                    capture_env_var(line, returned_env, logger=logger)
                else:
                    if returned_env is not None and "flibble" in line:
                        flibbling = True
                    else:
                        logger.info(line)
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
                capture_env_var(line, returned_env, logger=logger)
            else:
                if returned_env is not None and "flibble" in line:
                    flibbling = True
                else:
                    logger.info(line)
            line = queue_get_no_exception(read_queue, True, EXE_RUN_QUEUE_WAIT_SECONDS)

        # There may still be stuff in the buffer after
        # the application has finished running so flush that
        # out here
        line = process.stdout.readline().decode().encode("ascii", errors="replace").decode()
        while line:
            line = line.rstrip()
            if flibbling:
                capture_env_var(line, returned_env, logger=logger)
            else:
                if returned_env is not None and "flibble" in line:
                    flibbling = True
                else:
                    logger.info(line)
            line = process.stdout.readline().decode().encode("ascii", errors="replace").decode()

        if (process.poll() == 0) and kill_time is None:
            success = True
        logger.info(f"{call_list[0]}, pid {process.pid} ended with return" \
                    f" value {process.poll()}.")
    except ValueError as ex:
        logger.error(f"failed: {type(ex).__name__} while trying to execute {str(ex)}.")
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
    def __init__(self, call_list, logger: Logger = DEFAULT_LOGGER,
                 shell_cmd=False, with_stdin=False):
        self._call_list = call_list
        self._logger = logger
        self._shell_cmd = shell_cmd
        self._with_stdin=with_stdin
        self._process = None
    def __enter__(self):
        text = ""
        for idx, item in enumerate(self._call_list):
            if idx == 0:
                text = item
            else:
                text += f" {item}"
        self._logger.info(f'starting "{text}"...')
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

            self._logger.info(f"{self._call_list[0]} pid {self._process.pid} started")
        except (OSError, subprocess.CalledProcessError, ValueError) as ex:
            self._logger.error(f"failed: {type(ex).__name__} to start {str(ex)}.")
        except KeyboardInterrupt as ex:
            self._process.kill()
            raise KeyboardInterrupt from ex
        return self._process
    def __exit__(self, _type, value, traceback):
        del _type
        del value
        del traceback
        # Stop exe
        self._logger.info(f'stopping "{self._call_list[0]}"...')
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
                self._logger.info(f"{self._call_list[0]} pid {self._process.pid} terminated")
            else:
                self._logger.info(f"{self._call_list[0]} pid {self._process.pid} CTRL-C'd")
        else:
            self._logger.info(f"{self._call_list[0]} pid {self._process.pid} already ended")

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


def usb_cutter_reset(usb_cutter_id_strs, logger: Logger=DEFAULT_LOGGER):
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

            # Set shell to keep Jenkins happy
            exe_run(call_list, 0, logger=logger, shell_cmd=True)

        # Wait 5ish seconds
        logger.info(f"waiting {HW_RESET_DURATION_SECONDS} second(s)...")
        sleep(HW_RESET_DURATION_SECONDS)

        # "0" to switch the USB cutters on again
        action = "0"
        count += 1

def kmtronic_reset(ip_address, hex_bitmap, logger: Logger=DEFAULT_LOGGER):
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
        logger.info(f"sending {kmtronic_off}")
        response = requests.get(kmtronic_off)
        # Wait 5ish seconds

        logger.info(f"...received response {response.status_code}, waiting "  \
                    f" {HW_RESET_DURATION_SECONDS} second(s)...")
        sleep(HW_RESET_DURATION_SECONDS)
        # Switch the given bit positions on
        logger.info(f"sending {kmtronic_on}")
        response = requests.get(kmtronic_on)
        logger.info(f"...received response {response.status_code}.")
    except requests.ConnectionError:
        logger.error(f"unable to connect to KMTronic box at {ip_address}.")

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
            printer.string(f"{prompt}### parsing message to see if it contains a test directive...")
        lines = message.split("\\n")
        for idx1, line in enumerate(lines):
            if printer:
                printer.string(f"{prompt}text line {idx1 + 1}: \"{line}\"")
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
                            printer.string(f"{prompt}...badly formed test directive, ignoring.")
                            found = False
                        break
                    if filter_string_local:
                        # If we've had a filter string then nothing
                        # must follow so this is not a "test:" line,
                        # leave the loop and try again.
                        instances_local = []
                        filter_string_local = None
                        if printer:
                            printer.string(f"{prompt}...extraneous characters after test directive," \
                                           " ignoring.")
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
                                printer.string(f"{prompt}...badly formed test directive, ignoring.")
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
                                printer.string(f"{prompt}...badly formed test directive, ignoring.")
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
                                printer.string(f"{prompt}...badly formed test directive, ignoring.")
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
                            printer.string(f"{prompt}...badly formed test directive, ignoring.")
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
                        printer.string(f"{prompt}{text}.")
                    break
                if printer:
                    printer.string(f"{prompt}no test directive found")

    if found and instances_local:
        instances.extend(instances_local[:])

    return found, filter_string_local

def merge_filter(defines, filter_string):
    '''Merge the given filter string into defines'''
    defines_returned = []
    filter_list = []

    if filter_string:
        filter_list = filter_string.split(".")
    if defines:
        for define in defines:
            if define.startswith(FILTER_MACRO_NAME):
                # Find the bit after "U_CFG_APP_FILTER=" if it's there
                parts = define.split("=")
                if parts and len(parts) > 1:
                    # Find the individual parts of the filter "thinga.thingb"
                    filters = parts[1].split(".")
                    if filters and len(filters) > 0:
                        # Add them to our filter list
                        filter_list.extend(filters)
            else:
                # If it's not "U_CFG_APP_FILTER" then just add it
                defines_returned.append(define)

    # Now add the filter list back into the defines
    if len(filter_list) > 0:
        new_filter_string = ""
        for idx, item in enumerate(filter_list):
            if idx > 0:
                new_filter_string += "."
            new_filter_string += item.strip()
        defines_returned.append(FILTER_MACRO_NAME + "=" + \
                                new_filter_string)

    return defines_returned

def device_redirect_thread(device_a, device_b, baud_rate, terminateQueue, logger):
    '''Redirect thread, started by device_redirect_start()'''
    terminated = False
    baud_rate_str = ""

    if baud_rate:
        baud_rate_str = ",b" + str(baud_rate)

    call_list = ["socat", device_a + ",echo=0,raw" + baud_rate_str, \
                 device_b + ",echo=0,raw" + baud_rate_str]
    with ExeRun(call_list, logger) as process:
        while not terminated:
            try:
                terminateQueue.get(timeout=1)
                terminated = True
            except queue.Empty:
                if logger:
                    line = process.stdout.readline().decode().encode("ascii", errors="replace").decode()
                    while line:
                        line = line.rstrip()
                        logger.error(line)
                        line = process.stdout.readline().decode().encode("ascii", errors="replace").decode()
                if process.poll():
                    # The process has terminated all by itself
                    terminated = True

def device_redirect_start(device_a, device_b, baud_rate, logger: Logger=DEFAULT_LOGGER):
    '''Start a thread that redirects device_a to device_b, Linux only'''
    terminateQueue = None

    if is_linux():
        terminateQueue = queue.Queue()
        handle = threading.Thread(target=device_redirect_thread,
                                  args=(device_a, device_b, baud_rate, terminateQueue, logger))
        handle.start()
        # Pause to let socat print the opening debug; useful in case
        # it gets "permission denied" or some such back
        sleep(1)

    return terminateQueue

def device_redirect_stop(terminateQueue):
    '''Stop a thread that was doing redirection by sending it something on its terminate queue'''
    terminateQueue.put("Terminate")
