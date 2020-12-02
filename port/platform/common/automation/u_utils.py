#!/usr/bin/env python

'''Generally useful bits and bobs.'''

import queue                    # For PrintThread and exe_run
from time import sleep, time, gmtime, strftime   # For lock timeout, exe_run timeout and logging
import threading                # For PrintThread
import os                       # For ChangeDir, has_admin
import stat                     # To help deltree out
from telnetlib import Telnet # For talking to JLink server
import socket
import shutil                   # To delete a directory tree
import signal                   # For CTRL_C_EVENT
import subprocess
import platform                 # Figure out current OS
import serial                   # Pyserial (make sure to do pip install pyserial)
import psutil                   # For killing things (make sure to do pip install psutil)
import u_settings

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

# The time for which to wait for something from the
# queue in exe_run().  If this is too short, in a
# multiprocessing world or on a slow machine, it is
# possible to miss things as the task putting things
# on the queue may be blocked from doing so until
# we've decided the queue has been completely emptied
# and moved on
EXE_RUN_QUEUE_WAIT_SECONDS = u_settings.EXE_RUN_QUEUE_WAIT_SECONDS #1

def subprocess_osify(cmd):
    ''' expects an array of strings being [command, param, ...] '''
    if platform.system() == "Linux":
        return [ ' '.join(cmd) ]
    else:
        return cmd


def get_actual_path(path):
    '''Given a drive number return real path if it is a subst'''
    actual_path = path

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
                printer.string("{}{}".format(prompt, line.decode()))
            success = True
        else:
            printer.string("{}device with description \"{}\" not found.".   \
                           format(prompt, device_description))
    except subprocess.CalledProcessError:
        printer.string("{} unable to find and reset device.".format(prompt))

    return success

# Open the required serial port.
def open_serial(serial_name, speed, printer, prompt):
    '''Open serial port'''
    serial_handle = None
    text = "{}: trying to open \"{}\" as a serial port...".    \
           format(prompt, serial_name)
    try:
        return_value = serial.Serial(serial_name, speed, timeout=0.05)
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

def fetch_repo(url, directory, branch, printer, prompt):
    '''Fetch a repo: directory can be relative or absolute'''
    got_code = False
    checked_out = False
    success = False

    printer.string("{}in directory {}, fetching"
                   " {} to directory {}".format(prompt, os.getcwd(),
                                                url, directory))
    if not branch:
        branch = "master"
    if os.path.isdir(directory):
        # Update existing code
        with ChangeDir(directory):
            printer.string("{}updating code in {}...".
                           format(prompt, directory))
            try:
                text = subprocess.check_output(subprocess_osify(["git", "pull",
                                                "origin", branch]),
                                               stderr=subprocess.STDOUT,
                                               shell=True) # Jenkins hangs without this
                for line in text.splitlines():
                    printer.string("{}{}".format(prompt, line.decode()))
                got_code = True
            except subprocess.CalledProcessError as error:
                printer.string("{}git returned error {}: \"{}\"".
                               format(prompt, error.returncode,
                                      error.output))
    else:
        # Clone the repo
        printer.string("{}cloning from {} into {}...".
                       format(prompt, url, directory))
        try:
            text = subprocess.check_output(subprocess_osify(["git", "clone", url, directory]),
                                           stderr=subprocess.STDOUT,
                                           shell=True) # Jenkins hangs without this
            for line in text.splitlines():
                printer.string("{}{}".format(prompt, line.decode()))
            got_code = True
        except subprocess.CalledProcessError as error:
            printer.string("{}git returned error {}: \"{}\"".
                           format(prompt, error.returncode,
                                  error.output))

    if got_code and os.path.isdir(directory):
        # Check out the correct branch and recurse submodules
        with ChangeDir(directory):
            printer.string("{}checking out branch {}...".
                           format(prompt, branch))
            try:
                text = subprocess.check_output(subprocess_osify(["git", "-c",
                                                "advice.detachedHead=false",
                                                "checkout",
                                                "origin/" + branch]),
                                               stderr=subprocess.STDOUT,
                                               shell=True) # Jenkins hangs without this
                for line in text.splitlines():
                    printer.string("{}{}".format(prompt, line.decode()))
                checked_out = True
            except subprocess.CalledProcessError as error:
                printer.string("{}git returned error {}: \"{}\"".
                               format(prompt, error.returncode,
                                      error.output))

            if checked_out:
                printer.string("{}recursing sub-modules (can take some time" \
                               " and gives no feedback).".format(prompt))
                try:
                    text = subprocess.check_output(subprocess_osify(["git", "submodule",
                                                    "update", "--init",
                                                    "--recursive"]),
                                                   stderr=subprocess.STDOUT,
                                                   shell=True) # Jenkins hangs without this
                    for line in text.splitlines():
                        printer.string("{}{}".format(prompt, line.decode()))
                    success = True
                except subprocess.CalledProcessError as error:
                    printer.string("{}git returned error {}: \"{}\"".
                                   format(prompt, error.returncode,
                                          error.output))

    return success

def exe_where(exe_name, help_text, printer, prompt):
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
        if platform.system() == "Linux":
            cmd = ["which {}".format(exe_name)]
            printer.string("{}detected linux, calling \"{}\"...".format(prompt, cmd))
        else:
            cmd = ["where", "".join(exe_name)]
            printer.string("{}detected nonlinux, calling \"{}\"...".format(prompt, cmd))
        text = subprocess.check_output(cmd,
                                       stderr=subprocess.STDOUT,
                                       shell=True) # Jenkins hangs without this
        for line in text.splitlines():
            printer.string("{}{} found in {}".format(prompt, exe_name,
                                                     line.decode()))
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
        text = subprocess.check_output(subprocess_osify(["".join(exe_name), version_switch]),
                                       stderr=subprocess.STDOUT,
                                       shell=True)  # Jenkins hangs without this
        for line in text.splitlines():
            printer.string("{}{}".format(prompt, line.decode()))
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
        string = process.stdout.readline().decode()
        if string:
            read_queue.put(string)

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
def exe_run(call_list, guard_time_seconds, printer, prompt,
            shell_cmd=False, set_env=None, returned_env=None):
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
        call_list.append("set")
        # I've seen output from set get lost,
        # possibly because the process ending
        # is asynchronous with stdout,
        # so add a delay here as well
        call_list.append("&&")
        call_list.append("sleep")
        call_list.append("2")

    try:
        # Call the thang
        # Note: used to have bufsize=1 here but it turns out
        # that is ignored 'cos the output is considered
        # binary.  Seems to work in any case, I guess
        # Winders, at least, is in any case line-buffered.
        process = subprocess.Popen(call_list,
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT,
                                   shell=shell_cmd,
                                   env=set_env)
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
            if guard_time_seconds and (kill_time is None) and   \
               ((time() - start_time > guard_time_seconds) or
                (time() - read_time > guard_time_seconds)):
                kill_time = time()
                printer.string("{}guard time of {} second(s)." \
                               " expired, stopping {}...".
                               format(prompt, guard_time_seconds,
                                      call_list[0]))
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
        line = process.stdout.readline().decode()
        while line:
            line = line.rstrip()
            if flibbling:
                capture_env_var(line, returned_env, printer, prompt)
            else:
                if returned_env is not None and "flibble" in line:
                    flibbling = True
                else:
                    printer.string("{}{}".format(prompt, line))
            line = process.stdout.readline().decode()

        if (process.poll() == 0) and kill_time is None:
            success = True
        printer.string("{}{}, pid {} ended with return value {}.".    \
                       format(prompt, call_list[0],
                              process.pid, process.poll()))
    except ValueError as ex:
        printer.string("{}failed: {} while trying to execute {}.". \
                       format(prompt, type(ex).__name__, str(ex)))

    return success

class ExeRun():
    '''Run an executable as a "with:"'''
    def __init__(self, call_list, printer, prompt, shell_cmd=False, with_stdin=False):
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
            if self._with_stdin:
                self._process = subprocess.Popen(self._call_list,
                                                 stdin=subprocess.PIPE,
                                                 stdout=subprocess.PIPE,
                                                 stderr=subprocess.STDOUT,
                                                 shell=self._shell_cmd,
                                                 creationflags=subprocess.CREATE_NEW_PROCESS_GROUP)
            else:
                self._process = subprocess.Popen(self._call_list,
                                                 stdout=subprocess.PIPE,
                                                 stderr=subprocess.STDOUT,
                                                 shell=self._shell_cmd,
                                                 creationflags=subprocess.CREATE_NEW_PROCESS_GROUP)
            self._printer.string("{}{} pid {} started".format(self._prompt,
                                                              self._call_list[0],
                                                              self._process.pid))
        except (OSError, subprocess.CalledProcessError, ValueError) as ex:
            if self._printer:
                self._printer.string("{}failed: {} to start {}.". \
                                     format(self._prompt,
                                            type(ex).__name__, str(ex)))
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
                self._process.send_signal(signal.CTRL_BREAK_EVENT)
                sleep(1)
                retry -= 1
            return_value = self._process.poll()
            if not return_value:
                # Terminate with a vengeance
                self._process.terminate()
                while self._process.poll() is None:
                    pass
                self._printer.string("{}{} pid {} terminated".format(self._prompt,
                                                                     self._call_list[0],
                                                                     self._process.pid))
            else:
                self._printer.string("{}{} pid {} CTRL-C'd".format(self._prompt,
                                                                   self._call_list[0],
                                                                   self._process.pid))
        else:
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
                print(my_string)
            except queue.Empty:
                pass

class PrintToQueue():
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
                print(string)
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
