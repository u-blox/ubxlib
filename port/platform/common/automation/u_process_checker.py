#!/usr/bin/env python

'''Process checker; the other half of u_process_wrapper.py.'''

import sys
import os
import signal
import psutil
import socket
import subprocess
import argparse
from time import sleep, time
import u_settings
import u_utils

# The name of the script which forms the other half
# of the process wrapper/process checker pair.
PROCESS_WRAPPER = u_settings.PROCESS_WRAPPER # e.g. "u_process_wrapper.py"

# The default port number to use
PROCESS_PORT = u_settings.PROCESS_PORT # e.g. 50123

# What to use to invoke Python
PROCESS_PYTHON = u_settings.PROCESS_PYTHON # e.g. "python"

def end_process(process_pid, signal_to_send, kill_timeout_seconds=0, wait_for_end=True):
    '''End the process and its children in the given way'''
    start_time = time()
    process_list = []
    wait_for_end = True
    kill_now = False

    try:
        # Find all the processes and send them the signal
        process = psutil.Process(process_pid)
        process_list.append(process)
        
        for proc in process.children(recursive=True):
            process_list.append(proc)
            proc.send_signal(signal_to_send)
        process.send_signal(signal_to_send)

        # Wait for the processes to end and kill if requested
        while wait_for_end:
            if ((kill_timeout_seconds > 0) and (time() - start_time > kill_timeout_seconds)):
                kill_now = True
            wait_for_end = False
            for proc in process_list:
                if proc.is_running():
                    if kill_now:
                        proc.terminate()
                    else:
                        wait_for_end = True
                        sleep(0.1)
                        break
    except psutil.NoSuchProcess:
        pass

if __name__ == "__main__":
    RETURN_VALUE = -1
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
    PARSER.add_argument("-k", type=int, default=0, help="send SIGKILL if the"\
                        " script does not terminate in the given number of"  \
                        " seconds after being sent the termination signal.")
    PARSER.add_argument("-p", type=int, default=PROCESS_PORT, help="the port"\
                        " number to listen on, default " + str(PROCESS_PORT) + ".")
    PARSER.add_argument("script", default=None, help="the name of the Python" \
                        " script to execute.")
    PARSER.add_argument("params", nargs=argparse.REMAINDER, default=None,    \
                        help="parameters to go with the script.")
    ARGS = PARSER.parse_args()

    if ARGS.t:
        SIGNAL = signal.SIGTERM

    # Listen on the port for a connection from u_process_wrapper.py
    print("Listening for connection from {} on port {}...".format(PROCESS_WRAPPER,
                                                                  ARGS.p))
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as SOCKET:
        SOCKET.bind(("127.0.0.1", ARGS.p))
        SOCKET.listen()
        CONNECTION, ADDRESS = SOCKET.accept()
        with CONNECTION:
            print("Connected on {}.".format(ADDRESS))
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
            print("In directory \"{}\" calling \"{}\"...".format(os.getcwd(), TMP))

            try:
                # Set shell to True to keep Jenkins happy
                PROCESS = subprocess.Popen(u_utils.subprocess_osify(CALL_LIST, shell=True),
                                           stdout=subprocess.PIPE,
                                           stderr=subprocess.STDOUT,
                                           shell=True)
                # Wait for the process to finish
                while PROCESS.poll() is None:
                        string = PROCESS.stdout.readline()
                        if string:
                            print("{}".format(string.decode()), end="")
                        else:
                            try:
                                # Do a receive on the socket: we don't
                                # ever expect to receive anything, we
                                # are simply checking that the far end
                                # is still there
                                CONNECTION.recv(1)
                            except BlockingIOError:
                                # This is fine, the socket is there and
                                # we have received nothing
                                pass
                            except socket.error:
                                # Any other error means PROCESS_WRAPPER
                                # has been taken down: terminate
                                # the script and any children in the
                                # selected manner
                                end_process(PROCESS.pid, SIGNAL, ARGS.k)
                # Set the return value
                if PROCESS.poll() is not None:
                    RETURN_VALUE = PROCESS.poll()
            except ValueError as ex:
                print("ERROR: {} while trying to execute {}.". \
                      format(type(ex).__name__, str(ex)))
            except KeyboardInterrupt:
                if PROCESS:
                    # Terminate the process and all children in the
                    # selected manner
                    end_process(PROCESS.pid, SIGNAL, ARGS.k)

            # Send the return value back if the connection is there
            try:
                CONNECTION.sendall(str(RETURN_VALUE).encode())
            except (BlockingIOError, socket.error):
                pass

    print("Return value {}".format(RETURN_VALUE))

    sys.exit(RETURN_VALUE)
