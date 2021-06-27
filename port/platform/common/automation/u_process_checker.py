#!/usr/bin/env python

'''Process checker; the other half of u_process_wrapper.py.'''

import sys
import os
import signal
import socket
import subprocess
import queue
import threading
import argparse
from time import sleep, time
import psutil
import u_settings
import u_utils

PROMPT = "u_process_checker: "

# The name of the script which forms the other half
# of the process wrapper/process checker pair.
PROCESS_WRAPPER = u_settings.PROCESS_WRAPPER # e.g. "u_process_wrapper.py"

# The default port number to use
PROCESS_PORT = u_settings.PROCESS_PORT # e.g. 50123

# What to use to invoke Python
PROCESS_PYTHON = u_settings.PROCESS_PYTHON # e.g. "python"

# How often to check if process wrapper is still there.
PROCESS_CHECK_INTERVAL_SECONDS = 1

# Place to hook a print queue
PRINT_QUEUE = None

# Place to hook a printer
PRINTER = None

def process_read(process, read_queue):
    '''Read output from a process and queue'''
    while process.poll() is None:
        string = process.stdout.readline().decode()
        if string and string != "":
            read_queue.put(string.rstrip())

def end_process(process_pid, signal_to_send, kill_timeout_seconds=None, wait_for_end=True):
    '''End the process and its children in the given way'''
    start_time = time()
    process_list = []
    wait_for_end = True
    kill_now = False
    main_process = None

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

if __name__ == "__main__":
    RETURN_VALUE = -1
    NEXT_CHECK = time()
    PROCESS = None
    SIGNAL = signal.CTRL_C_EVENT

    PARSER = argparse.ArgumentParser(description="The other half of"         \
                                     " " + PROCESS_WRAPPER + "; this script" \
                                     " is intended to be invoked by"         \
                                     " " + PROCESS_WRAPPER + ".  It runs"    \
                                     " the given Python script, with its"    \
                                     " parameters, and then checks that"     \
                                     " " + PROCESS_WRAPPER + " is still"     \
                                     " running by connecting to it on the"   \
                                     " given port. As soon as it determines" \
                                     " that " + PROCESS_WRAPPER + " has"     \
                                     " exited it terminates the Python"      \
                                     " script using the selected mechanism"  \
                                     " (default SIGINT, AKA CTRL-C).")
    PARSER.add_argument("-t", action='store_true', help="use SIGTERM intead" \
                        " of SIGINT as the termination signal.")
    PARSER.add_argument("-k", type=int, help="kill the script if it does not" \
                        " terminate in the given number of second after"     \
                        " being sent the termination signal.")
    PARSER.add_argument("-p", type=int, default=PROCESS_PORT, help="the port"\
                        " number to listen on, default " + str(PROCESS_PORT) + ".")
    PARSER.add_argument("script", default=None, help="the name of the Python" \
                        " script to execute.")
    PARSER.add_argument("params", nargs=argparse.REMAINDER, default=None,    \
                        help="parameters to go with the script.")
    ARGS = PARSER.parse_args()

    if ARGS.t:
        SIGNAL = signal.SIGTERM

    # For the output from the script we call
    PRINT_QUEUE = queue.Queue()
    PRINT_THREAD =  u_utils.PrintThread(PRINT_QUEUE)
    PRINT_THREAD.start()
    PRINTER = u_utils.PrintToQueue(PRINT_QUEUE, None)

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
                                           stderr=subprocess.STDOUT,
                                           creationflags=0)
                # Run a thread to queue stuff from the process
                PROCESS_READ_THREAD = threading.Thread(target=process_read,
                                                       args=(PROCESS, PRINT_QUEUE))
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
                            # the script and any children in the
                            # selected manner
                            end_process(PROCESS.pid, SIGNAL, ARGS.k)
                            break
                # Set the return value
                if PROCESS.poll() is not None:
                    RETURN_VALUE = PROCESS.poll()
            except ValueError as ex:
                print("{}ERROR: {} while trying to execute {}.". \
                      format(PROMPT, type(ex).__name__, str(ex)))
            except KeyboardInterrupt:
                print("{}received CTRL-C, exiting...".format(PROMPT))
                if PROCESS and PROCESS.poll() is not None:
                    # Terminate the process and all children in the
                    # selected manner
                    end_process(PROCESS.pid, SIGNAL, ARGS.k)

            # Send the return value back if the connection is there
            try:
                CONNECTION.sendall(str(RETURN_VALUE).encode())
            except (BlockingIOError, socket.error) as ex:
                print("{}can't send return value to {}, likely it exited ({} {}).". \
                      format(PROMPT, PROCESS_WRAPPER, type(ex).__name__, str(ex)))

    print("{}return value {}".format(PROMPT, RETURN_VALUE))

    # Uncomment the two lines below when debugging with this script
    # launched in its own window (see u_process_wrapper.py for how
    # to do that) and you don't want it to evaporate or stop printing
    # at the end
    #while True:
    #    pass

    # Stop the printer
    sleep(1)
    PRINT_THREAD.stop_thread()
    PRINT_THREAD.join()
    PRINTER = None

    sys.exit(RETURN_VALUE)
