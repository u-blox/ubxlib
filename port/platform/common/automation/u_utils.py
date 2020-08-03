#!/usr/bin/env python

'''Generally useful bits and bobs.'''

import queue                    # For PrintThread and exe_run
from time import sleep, clock, gmtime, strftime   # For lock timeout, exe_run timeout and logging
import threading                # For PrintThread
import os                       # For ChangeDir
import stat                     # To help deltree out
from telnetlib import Telnet # For talking to JLink server
import socket
import shutil                   # To delete a directory tree
import subprocess
import serial                   # Pyserial (make sure to do pip install pyserial)
import psutil                   # For killing things (make sure to do pip install psutil)

# How long to wait for an install lock in seconds
INSTALL_LOCK_WAIT_SECONDS = (60 * 60)

# The URL for Unity, the unit test framework
UNITY_URL = "https://github.com/ThrowTheSwitch/Unity"

# The sub-directory that Unity is usually put in
# (off the working directory)
UNITY_SUBDIR = "Unity"

# The path to jlink.exe (or just the name 'cos it's on the path)
JLINK_PATH = "jlink.exe"

# The telnet port number for trace capture out of JLink
JLINK_TELNET_TRACE_PORT = 19021

# The format string passed to strftime()
# for logging prints
TIME_FORMAT = "%Y-%m-%d_%H:%M:%S"

# The default guard time waiting for a platform lock in seconds
PLATFORM_LOCK_GUARD_TIME_SECONDS = 60 * 60

# The default guard time for downloading to a target in seconds
DOWNLOAD_GUARD_TIME_SECONDS = 60

# The default guard time for running tests in seconds
RUN_GUARD_TIME_SECONDS = 60 * 60

# The default inactivity timer for running tests in seconds
RUN_INACTIVITY_TIME_SECONDS = 60 * 5

# The name of the #define that forms the filter string
# for which tests to run
FILTER_MACRO_NAME = "U_CFG_APP_FILTER"

def get_instance_text(instance):
    '''Return the instance as a text string'''
    instance_text = ""

    for idx, item in enumerate(instance):
        if idx == 0:
            instance_text += str(item)
        else:
            instance_text += "." + str(item)

    return instance_text

def remove_readonly(func, path, exec_info):
    '''Help deltree out'''
    del exec_info
    os.chmod(path, stat.S_IWRITE)
    func(path)

def deltree(directory, printer, prompt):
    '''Remove an entire directory tree'''
    tries = 2
    success = False

    if os.path.isdir(directory):
        # Retry this as sometimes Windows complains
        # that the directory is not empty when it
        # it really should be, some sort of internal
        # Windows race condition
        while not success and (tries > 0):
            try:
                # Need the onerror bit on Winders, seek
                # this Stack Overflow post:
                # https://stackoverflow.com/questions/1889597/deleting-directory-in-python
                shutil.rmtree(directory, onerror=remove_readonly)
                success = True
            except OSError as ex:
                printer.string("{}ERROR unable to delete \"{}\" {}: \"{}\"".
                               format(prompt, directory,
                                      ex.errno, ex.strerror))
            tries -= 1
    else:
        success = True

    return success

# Open the required serial port.
def open_serial(serial_name, speed, printer, prompt):
    '''Open serial port'''
    serial_handle = None
    text = "{}: trying to open \"{}\" as a serial port...".    \
           format(prompt, serial_name)
    try:
        return_value = serial.Serial(serial_name, speed,
                                     timeout=0.05)
        serial_handle = return_value
        printer.string("{} opened.".format(text))
    except (ValueError, serial.SerialException) as ex:
        printer.string("{}{} while accessing port {}: {}.".
                       format(prompt, type(ex).__name__,
                              serial_handle.name, ex.message))
    return serial_handle

def open_telnet(port_number, printer, prompt):
    '''Open telnet port on localhost'''
    telnet_handle = None
    text = "{}trying to open \"{}\" as a telnet port on localhost...".  \
           format(prompt, port_number)
    try:
        telnet_handle = Telnet("localhost", int(port_number),
                               timeout=5)
        if telnet_handle is not None:
            printer.string("{} opened.".format(text))
        else:
            printer.string("{} failed.".format(text))
    except (socket.error, socket.timeout, ValueError) as ex:
        printer.string("{}{} failed to open telnet {}: {}.".
                       format(prompt, type(ex).__name__,
                              port_number, ex.message))
    return telnet_handle

def install_lock_acquire(install_lock, printer, prompt):
    '''Attempt to acquire install lock'''
    timeout_seconds = INSTALL_LOCK_WAIT_SECONDS
    success = False

    if install_lock:
        printer.string("{}waiting for install lock...".format(prompt))
        while not install_lock.acquire(False) and (timeout_seconds > 0):
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

def fetch_repo(url, subdir, branch, printer, prompt):
    '''Fetch a repo'''
    got_code = False
    success = False

    printer.string("{}in directory {}, fetching"
                   " {} to subdirectory {}".format(prompt, os.getcwd(),
                                                   url, subdir))
    if not branch:
        branch = "master"
    if os.path.isdir(subdir):
        # Update existing code
        with (ChangeDir(subdir)):
            printer.string("{}updating code in {}...".
                           format(prompt, subdir))
            try:
                text = subprocess.check_output(["git", "pull",
                                                "origin", branch],
                                               stderr=subprocess.STDOUT,
                                               shell=True) # Jenkins hangs without this
                for line in text.splitlines():
                    printer.string("{}{}".format(prompt, line))
                got_code = True
            except subprocess.CalledProcessError as error:
                printer.string("{}git returned error {}: \"{}\"".
                               format(prompt, error.returncode,
                                      error.output))
    else:
        # Clone the repo
        printer.string("{}cloning from {} into {}...".
                       format(prompt, url, subdir))
        try:
            text = subprocess.check_output(["git", "clone", url, subdir],
                                           stderr=subprocess.STDOUT,
                                           shell=True) # Jenkins hangs without this
            for line in text.splitlines():
                printer.string("{}{}".format(prompt, line))
            got_code = True
        except subprocess.CalledProcessError as error:
            printer.string("{}git returned error {}: \"{}\"".
                           format(prompt, error.returncode,
                                  error.output))

    if got_code and os.path.isdir(subdir):
        # Check out the correct branch and recurse submodules
        with (ChangeDir(subdir)):
            printer.string("{}checking out branch {}...".
                           format(prompt, branch))
            try:
                text = subprocess.check_output(["git", "-c",
                                                "advice.detachedHead=false",
                                                "checkout",
                                                "origin/" + branch,
                                                "--recurse-submodules"],
                                               stderr=subprocess.STDOUT,
                                               shell=True) # Jenkins hangs without this
                for line in text.splitlines():
                    printer.string("{}{}".format(prompt, line))
                success = True
            except subprocess.CalledProcessError as error:
                printer.string("{}git returned error {}: \"{}\"".
                               format(prompt, error.returncode,
                                      error.output))

    return success

def exe_where(exe_name, help_text, printer, prompt):
    '''Find an executable using where.exe'''
    success = False

    try:
        printer.string("{}looking for \"{}\"...".          \
                       format(prompt, exe_name))
        # See here:
        # https://stackoverflow.com/questions/14928860/passing-double-quote-shell-commands-in-python-to-subprocess-popen
        # ...for why the construction "".join() is necessary when
        # passing things which might have spaces in them.
        # It is the only thing that works.
        text = subprocess.check_output(["where", "".join(exe_name)],
                                       stderr=subprocess.STDOUT,
                                       shell=True) # Jenkins hangs without this
        for line in text.splitlines():
            printer.string("{}{} found in {}".format(prompt, exe_name,
                                                     line))
        success = True
    except subprocess.CalledProcessError:
        if help_text:
            printer.string("{}ERROR {} not found: {}".  \
                           format(prompt, exe_name, help_text))
        else:
            printer.string("{}ERROR {} not found".      \
                           format(prompt, exe_name))

    return success

def exe_version(exe_name, version_switch, printer, prompt):
    '''Print the version of a given executable'''
    success = False

    if not version_switch:
        version_switch = "--version"
    try:
        text = subprocess.check_output(["".join(exe_name), version_switch],
                                       stderr=subprocess.STDOUT,
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
        proc.kill()
    process.kill()

def read_from_process_and_queue(process, read_queue):
    '''Read from a process, non-blocking'''
    while process.poll() is None:
        string = process.stdout.readline()
        if string:
            read_queue.put(string)

def exe_run(call_list, guard_time_seconds, printer, prompt,
            shell_cmd=False):
    '''Call an executable, printing out what it does'''
    success = False
    start_time = clock()
    kill_time = None
    read_time = start_time

    try:
        # Call the thang
        process = subprocess.Popen(call_list,
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT,
                                   shell=shell_cmd,
                                   bufsize=1)
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
        # nothing for guard_time_seconds then we kill the
        # process.
        read_queue = queue.Queue()
        read_thread = threading.Thread(target=read_from_process_and_queue,
                                       args=(process,
                                             read_queue))
        read_thread.start()
        while process.poll() is None:
            if guard_time_seconds and (kill_time is None) and   \
               ((clock() - start_time > guard_time_seconds) or
                (clock() - read_time > guard_time_seconds)):
                kill_time = clock()
                printer.string("{}guard time of {} second(s)." \
                               " expired, stopping {}...".
                               format(prompt, guard_time_seconds,
                                      call_list[0]))
                exe_terminate(process.pid)
            try:
                line = read_queue.get(block=False, timeout=0.5).rstrip()
                read_time = clock()
                if line:
                    printer.string("{}{}".format(prompt, line))
            except queue.Empty:
                pass
        # Can't join() read_thread here as it might have
        # blocked on a read() (if nrfjprog has anything to
        # do with it).  It will be tidied up when this process
        # exits.
        # There may still be stuff in the buffer after
        # the application has finished running so flush it
        # out here
        line = process.stdout.readline().rstrip()
        while line:
            printer.string("{}{}".format(prompt, line))
            line = process.stdout.readline().rstrip()
        if (process.poll() == 0) and kill_time is None:
            success = True
        printer.string("{}{}, pid {} ended with return value {}.".    \
                       format(prompt, call_list[0],
                              process.pid, process.poll()))
    except ValueError as ex:
        printer.string("{}failed: {} while trying to execute {}.". \
                       format(prompt, type(ex).__name__, ex.message))

    return success

class ExeRun(object):
    '''Run an executable as a "with:"'''
    def __init__(self, call_list, printer, prompt, shell_cmd=False):
        self._call_list = call_list
        self._printer = printer
        self._prompt = prompt
        self._shell_cmd = shell_cmd
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
            self._process = subprocess.Popen(self._call_list,
                                             stdout=subprocess.PIPE,
                                             stderr=subprocess.STDOUT,
                                             shell=self._shell_cmd,
                                             bufsize=1)
            self._printer.string("{}{} pid {} started".format(self._prompt,
                                                              self._call_list[0],
                                                              self._process.pid))
        except (OSError, subprocess.CalledProcessError, ValueError) as ex:
            if self._printer:
                self._printer.string("{}failed: {} to start {}.". \
                                     format(self._prompt,
                                            type(ex).__name__, ex.message))
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
            self._process.kill()
            while self._process.poll() is None:
                pass
            self._printer.string("{}{} pid {} ended".format(self._prompt,
                                                            self._call_list[0],
                                                            self._process.pid))
        return return_value

class PrintThread(threading.Thread):
    '''Print thread to organise prints nicely'''
    def __init__(self, print_queue):
        self._queue = print_queue
        self._running = False
        threading.Thread.__init__(self)
    def stop_thread(self):
        '''Helper function to stop the thread'''
        self._running = False
    def run(self):
        '''Worker thread'''
        self._running = True
        while self._running:
            try:
                my_string = self._queue.get(block=False, timeout=0.5)
                print my_string
            except queue.Empty:
                pass

class PrintToQueue(object):
    '''Print to a queue, if there is one'''
    def __init__(self, print_queue, file_handle, include_timestamp=False):
        self._queue = print_queue
        self._file_handle = file_handle
        self._include_timestamp = include_timestamp
    def string(self, string, file_only=False):
        '''Print a string'''
        if self._include_timestamp:
            string = strftime(TIME_FORMAT, gmtime()) + " " + string
        if not file_only:
            if self._queue:
                self._queue.put(string)
            else:
                print string
        if self._file_handle:
            self._file_handle.write(string + "\n")
            self._file_handle.flush()

# This stolen from here:
# https://stackoverflow.com/questions/431684/how-do-i-change-the-working-directory-in-python
class ChangeDir(object):
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

class Lock(object):
    '''Hold a lock as a "with:"'''
    def __init__(self, lock, guard_time_seconds,
                 lock_type, printer, prompt):
        self._lock = lock
        self._guard_time_seconds = guard_time_seconds
        self._lock_type = lock_type
        self._printer = printer
        self._prompt = prompt
        self._locked = False
    def __enter__(self):
        # Wait on the lock
        if not self._locked:
            timeout_seconds = self._guard_time_seconds
            self._printer.string("{}waiting up to {} second(s)"      \
                                 " for a {} lock...".                \
                                 format(self._prompt,
                                        self._guard_time_seconds,
                                        self._lock_type))
            count = 0
            while not self._lock.acquire(False) and                \
                ((self._guard_time_seconds == 0) or (timeout_seconds > 0)):
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
        if self._locked:
            try:
                self._lock.release()
                self._locked = False
                self._printer.string("{}released a {} lock.".format(self._prompt,
                                                                    self._lock_type))
            except RuntimeError:
                self._locked = False
                self._printer.string("{}{} lock was already released.". \
                                     format(self._prompt, self._lock_type))
