#!/usr/bin/env python

'''Process checker; the other half of u_process_wrapper.py.'''

import sys
import os
from signal import SIGTERM, CTRL_C_EVENT
import socket
import subprocess
import threading
import argparse
from time import sleep, time
import psutil
from scripts import u_utils, u_process_settings

PROMPT = "u_process_checker: "

# The name of the script which forms the other half
# of the process wrapper/process checker pair.
PROCESS_WRAPPER = u_process_settings.WRAPPER_NAME # e.g. "u_process_wrapper.py"

# The default port number to use to check if process wrapper
# is still there
PROCESS_PORT = u_process_settings.PORT # e.g. 50123

# What to use to invoke Python
PROCESS_PYTHON = u_process_settings.PYTHON # e.g. "python"

# The special prefix used before a return value
# when we send it on the socket to process wrapper
RETURN_VALUE_PREFIX = u_process_settings.RETURN_VALUE_PREFIX # e.g. "#!ubx!# RETURN_VALUE:"

# How often to check if process wrapper is still there.
PROCESS_CHECK_INTERVAL_SECONDS = 1

# The remote control command to abort a running process,
# will be understood by u_controller_client.py
REMOTE_CONTROL_COMMAND_ABORT = "abort\n"

# The ack for a command, sent by u_controller_client.py
REMOTE_CONTROL_ACK = "ack\n"

def process_forward_output(process, socket_handle):
    '''Read output from a process and queue'''
    while process.poll() is None:
        string = process.stdout.readline().decode("utf8")
        if string and string != "":
            if socket_handle is not None:
                try:
                    socket_handle.sendall(string.encode())
                except socket.error as ex:
                    print("{}error writing to port {} ({} {}).". \
                          format(PROMPT, PROCESS_PORT, type(ex).__name__, str(ex)))
                    # Don't waste our time if it's gone
                    socket_handle = None
        # Let others in
        sleep(0.01)

def wait_for_termination(process_list, kill_timeout_seconds=None):
    '''Called by end_process_with_command() and end_process_with_signal()'''
    wait_for_end = True
    start_time = time()
    kill_now = False

    while wait_for_end:
        if (kill_timeout_seconds is not None) and \
           (time() - start_time > kill_timeout_seconds):
            kill_now = True
        wait_for_end = False
        count = 0
        for proc in process_list:
            if proc.is_running() and not proc.status() == "zombie":
                count += 1
                print("{}process {} ({}) is still running.".format(PROMPT,
                                                                   proc.pid,
                                                                   proc.name()))
                if kill_now:
                    try:
                        proc.terminate()
                        print("{}terminated process {} ({}).".format(PROMPT,
                                                                     proc.pid,
                                                                     proc.name()))
                    except psutil.NoSuchProcess:
                        pass
                else:
                    wait_for_end = True
                    sleep(1)
        print("{}{} process(es) still running.".format(PROMPT, count))

    # For debug purposes, count the zombie processes
    count = 0
    for proc in process_list:
        try:
            if proc.status() == "zombie":
                count += 1
        except psutil.NoSuchProcess:
            pass
    print("{}{} zombie process(es) left.".format(PROMPT, count))

def end_process_with_command(process_pid, port, kill_timeout_seconds=None,
                             wait_for_end=True):
    '''End the process using a remote control command'''
    process_list = []
    main_process = None

    print("{}aborting process using a remote control command.".format(PROMPT))

    try:
        # Find the process
        main_process = psutil.Process(process_pid)
    except psutil.NoSuchProcess:
        print("{}no processes to clean up.".format(PROMPT))

    if main_process:
        # Connect to it
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.settimeout(10)
            try:
                sock.connect(("127.0.0.1", port))
                try:
                    # Send the abort command
                    sock.sendall(REMOTE_CONTROL_COMMAND_ABORT.encode())
                    print("{}remote control command \"{}\" sent on port {}.". \
                          format(PROMPT, REMOTE_CONTROL_COMMAND_ABORT.rstrip(), port))
                    # Wait for the ack and only then close the socket
                    try:
                        message = sock.recv(64)
                    except socket.error as ex:
                        print("{}remote control connection closed" \
                              " (socket error {} {}).". \
                              format(PROMPT, type(ex).__name__, str(ex)))
                    try:
                        if message:
                            message = message.decode("utf8")
                            print("{}received response to remote control command \"{}\".". \
                                  format(PROMPT, message.rstrip()))
                    except UnicodeDecodeError:
                        # Just ignore it.
                        pass
                    process_list.append(main_process)
                except socket.error:
                    print("{}ERROR: unable to send the remote control command" \
                          " \"{}\" on port {}.". \
                          format(PROMPT, REMOTE_CONTROL_COMMAND_ABORT.rstrip(), port))
            except (socket.error, ConnectionRefusedError):
                print("{}ERROR: unable to connect to remote control port {}.". \
                      format(PROMPT, port))

        # Wait for the processes to end and kill them if/when requested
        if wait_for_end:
            wait_for_termination(process_list, kill_timeout_seconds)

def end_process_with_signal(process_pid, signal_to_send, kill_timeout_seconds=None,
                            wait_for_end=True):
    '''End the process and its children with a signal in the given way'''
    process_list = []
    main_process = None

    print("{}aborting process using a signal.".format(PROMPT))
    print("{}[our process ID is {}]".format(PROMPT, os.getpid()))

    try:
        # Find the process
        main_process = psutil.Process(process_pid)
    except psutil.NoSuchProcess:
        print("{}no processes to clean up.".format(PROMPT))

    if main_process:
        # Hunt down its children and send them the signal
        for child_process in main_process.children(recursive=True):
            try:
                child_process.send_signal(signal_to_send)
                print("{}sent signal {} to child process {} ({}).".format(PROMPT,
                                                                          signal_to_send,
                                                                          child_process.pid,
                                                                          child_process.name()))
                process_list.append(child_process)
            except psutil.NoSuchProcess:
                pass
        try:
            # Send the main process the signal
            main_process.send_signal(signal_to_send)
            print("{}sent signal {} to main process {} ({}).".format(PROMPT,
                                                                     signal_to_send,
                                                                     main_process.pid,
                                                                     main_process.name()))
            process_list.append(main_process)
        except psutil.NoSuchProcess:
            pass

        # Wait for the processes to end and kill them if/when requested
        if wait_for_end:
            wait_for_termination(process_list, kill_timeout_seconds)

if __name__ == "__main__":
    RETURN_VALUE = -1
    NEXT_CHECK = time()
    PROCESS = None
    SIGNAL = CTRL_C_EVENT

    PARSER = argparse.ArgumentParser(description="The other half of"         \
                                     " " + PROCESS_WRAPPER + "; this script" \
                                     " is intended to be invoked by"         \
                                     " " + PROCESS_WRAPPER + ".  This script"\
                                     " runs the given Python script, with its"\
                                     " parameters, and then checks that"     \
                                     " " + PROCESS_WRAPPER + " is still"     \
                                     " running by connecting to it on the"   \
                                     " given port. As soon as it determines" \
                                     " that " + PROCESS_WRAPPER + " has"     \
                                     " exited it terminates the Python"      \
                                     " script using the selected mechanism"  \
                                     " (default SIGINT, AKA CTRL-C, unless"  \
                                     " -r or -t is specified).  The output"  \
                                     " from both this script and the launched"\
                                     " script is sent to " + PROCESS_WRAPPER + \
                                     " on the same port so that it can see"  \
                                     " what we're up to.")
    PARSER.add_argument("-t", action='store_true', help="use SIGTERM intead" \
                        " of SIGINT as the termination signal (cannot be"    \
                        " specified at the same time as -r).")
    PARSER.add_argument("-k", type=int, help="kill the script if it does not" \
                        " terminate in the given number of second after"     \
                        " being sent the termination signal.")
    PARSER.add_argument("-p", type=int, default=PROCESS_PORT, help="the port"\
                        " number to listen on, default " + str(PROCESS_PORT) + ".")
    PARSER.add_argument("-r",  type=int, help="use the given port number"    \
                        " to abort the script using a bespole remote control"\
                        " protocol (specific to u_controller_client.py)"     \
                        " instead of sending a signal (cannot be specified at"\
                        " the same time as -t).")
    PARSER.add_argument("script", default=None, help="the name of the Python" \
                        " script to execute.")
    PARSER.add_argument("params", nargs=argparse.REMAINDER, default=None,    \
                        help="parameters to go with the script.")
    ARGS = PARSER.parse_args()

    if ARGS.t and ARGS.r:
        print("Cannot specify -t and -r at the same time.")
    else:
        if ARGS.t:
            SIGNAL = SIGTERM

        # Listen on the port for a connection from u_process_wrapper.py
        print("{}listening for connection from {} on port {}...".format(PROMPT,
                                                                        PROCESS_WRAPPER,
                                                                        ARGS.p))
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as SOCKET:
            SOCKET.bind(("127.0.0.1", ARGS.p))
            SOCKET.listen()
            CONNECTION, ADDRESS = SOCKET.accept()
            with CONNECTION:
                print("{}connected on {}.".format(PROMPT, ADDRESS))
                # Set connection to non-blocking with zero timeout
                CONNECTION.settimeout(0)

                # Launch the script with its parameters
                CALL_LIST = []
                if PROCESS_PYTHON:
                    CALL_LIST.append(PROCESS_PYTHON)
                CALL_LIST.append(ARGS.script)
                if ARGS.params:
                    CALL_LIST.extend(ARGS.params)

                # Print what we're going to do
                TMP = ""
                for item in CALL_LIST:
                    if TMP:
                        TMP += " "
                    TMP += item
                print("{}in directory \"{}\" calling \"{}\"...".format(PROMPT,
                                                                       os.getcwd(),
                                                                       TMP))

                try:
                    PROCESS = subprocess.Popen(u_utils.subprocess_osify(CALL_LIST),
                                               stdout=subprocess.PIPE,
                                               stderr=subprocess.STDOUT)
                    # Run a thread to grab stuff from the process
                    # and sent it back over the socket
                    PROCESS_READ_THREAD = threading.Thread(target=process_forward_output,
                                                           args=(PROCESS, CONNECTION))
                    PROCESS_READ_THREAD.start()

                    # Wait for the process to finish
                    while PROCESS.poll() is None:
                        if time() > NEXT_CHECK:
                            NEXT_CHECK = time() + PROCESS_CHECK_INTERVAL_SECONDS
                            try:
                                # Do a receive on the socket: we don't
                                # ever expect to receive anything, we
                                # are simply checking that the far end
                                # is still there
                                CONNECTION.recv(1)
                            except BlockingIOError:
                                # This is fine, the socket is there and
                                # we have received nothing
                                sleep(0.1)
                            except socket.error as ex:
                                print("{}{} has exited (socket error {} {} on {}).". \
                                      format(PROMPT, PROCESS_WRAPPER,
                                             type(ex).__name__, str(ex), ADDRESS))
                                # Any other error means PROCESS_WRAPPER
                                # has been taken down: terminate
                                # the script in the selected manner
                                if ARGS.r:
                                    end_process_with_command(PROCESS.pid, ARGS.r,
                                                             kill_timeout_seconds=ARGS.k)
                                else:
                                    end_process_with_signal(PROCESS.pid, SIGNAL,
                                                            kill_timeout_seconds=ARGS.k)
                            except KeyboardInterrupt:
                                print("{}received CTRL-C, exiting...".format(PROMPT))
                                # Get the process to terminate in the selected manner
                                if ARGS.r:
                                    end_process_with_command(PROCESS.pid, ARGS.r,
                                                             kill_timeout_seconds=ARGS.k)
                                else:
                                    end_process_with_signal(PROCESS.pid, SIGNAL,
                                                            kill_timeout_seconds=ARGS.k)
                    # Set the return value
                    if PROCESS.poll() is not None:
                        RETURN_VALUE = PROCESS.poll()
                except ValueError as ex:
                    print("{}ERROR: {} while trying to execute {}.". \
                          format(PROMPT, type(ex).__name__, str(ex)))

                # Send the return value back if the connection is there
                try:
                    # Give the read process thread time to finish off first
                    sleep(5)
                    CONNECTION.sendall((RETURN_VALUE_PREFIX + str(RETURN_VALUE)).encode())
                except (BlockingIOError, socket.error) as ex:
                    print("{}can't send return value to {}, likely it exited ({} {}).". \
                          format(PROMPT, PROCESS_WRAPPER, type(ex).__name__, str(ex)))

        print("{}return value {}".format(PROMPT, RETURN_VALUE))

        # Can't join() PROCESS_READ_THREAD here as it might have
        # blocked on a read(); it will be tidied up when this process
        # exits.

    sys.exit(RETURN_VALUE)
