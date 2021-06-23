#!/usr/bin/env python

'''Process wrapper to help with Jenkins integration; a version of Morné Joubert's great idea.'''

import sys
import os
import threading
import socket
import subprocess
import argparse
import u_settings
import u_utils

# The name of the script which forms the other half
# of the process wrapper/process checker pair.
PROCESS_CHECKER = u_settings.PROCESS_CHECKER # e.g. "u_process_checker.py"

# The default port number to use
PROCESS_PORT = u_settings.PROCESS_PORT # e.g. 50123

# What to use to invoke Python
PROCESS_PYTHON = u_settings.PROCESS_PYTHON # e.g. "python"

# The return value global so that connect_to_process_checker() can update it
RETURN_VALUE = -1

# Variable to indicate we're connected
CONNECTED_TO_PROCESS_CHECKER = False

def connect_to_process_checker(port):
    '''Connect to process checker on the given port number'''
    global RETURN_VALUE, CONNECTED_TO_PROCESS_CHECKER

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.settimeout(5)
        try:
            sock.connect(("127.0.0.1", port))
            CONNECTED_TO_PROCESS_CHECKER = True
            # Now all we do is block until the integer return
            # value is sent to us as a string (or an error occurs)
            sock.settimeout(None)
            try:
                process_checker_said = sock.recv(64)
                try:
                    RETURN_VALUE = int(process_checker_said.decode("utf8"))
                except (UnicodeDecodeError, ValueError):
                    pass
            except socket.error:
                pass
        except (socket.error, ConnectionRefusedError):
            pass

if __name__ == "__main__":
    PARSER = argparse.ArgumentParser(description="Wrap a Python script so"   \
                                     " that it can be ended in a controlled" \
                                     " way if this process is killed; this"  \
                                     " wrapper actually just runs " +        \
                                     PROCESS_CHECKER + " which in turn runs" \
                                     " the Python script that is given as a" \
                                     " parameter to this script.  " +        \
                                     PROCESS_CHECKER + " and this script"    \
                                     " communicate over the given port;"     \
                                     " when " + PROCESS_CHECKER + " spots"   \
                                     " that this script has stopped it"      \
                                     " terminates the Python script using"   \
                                     " the selected mechanism (default"      \
                                     " SIGINT, AKA CTRL-C).")
    PARSER.add_argument("-t", action='store_true', help="use SIGTERM intead" \
                        " of SIGINT as the termination signal.")
    PARSER.add_argument("-k", type=int, help="send SIGKILL if the script"    \
                        " does not terminate in the given number of seconds" \
                        " after being sent the termination signal.")
    PARSER.add_argument("-p", type=int, default=PROCESS_PORT, help="the port"\
                        " number to use, default " + str(PROCESS_PORT) + ".")
    PARSER.add_argument("script", default=None, help="the name of the Python" \
                        " script to execute.")
    PARSER.add_argument("params", nargs=argparse.REMAINDER, default=None,    \
                        help="parameters to go with the script.")
    ARGS = PARSER.parse_args()

    # Start a thread that attempts to connect
    # to PROCESS_CHECKER on the given port
    connect_thread = threading.Thread(target=connect_to_process_checker,
                                      args=(ARGS.p,))
    connect_thread.start()

    # Launch PROCESS_CHECKER with the script and its parameters
    CALL_LIST = []
    # Launch as a separate process so that it is not affected
    # by the killing of us
    if PROCESS_PYTHON:
        CALL_LIST.append(PROCESS_PYTHON)
    CALL_LIST.append(PROCESS_CHECKER)
    if ARGS.t:
        CALL_LIST.append("-t")
    if ARGS.k:
        CALL_LIST.append("-k")
        CALL_LIST.append(str(ARGS.k))
    if ARGS.p:
        CALL_LIST.append("-p")
        CALL_LIST.append(str(PROCESS_PORT))
    CALL_LIST.append(ARGS.script)
    if ARGS.params:
        CALL_LIST.extend(ARGS.params)

    # Print what we're going to do
    TMP = ""
    for item in CALL_LIST:
        if TMP:
            TMP += " "
        TMP += item
    print("In directory \"{}\" calling \"{}\" and waiting to be terminated.". \
          format(os.getcwd(), TMP))

    try:
        CREATION_FLAGS = 0
        if u_utils.is_linux():
            # TODO
            pass
        else:
            CREATION_FLAGS |= subprocess.CREATE_NEW_PROCESS_GROUP
            CREATION_FLAGS |= subprocess.CREATE_NO_WINDOW
        # Set shell to True to keep Jenkins happy
        PROCESS = subprocess.Popen(u_utils.subprocess_osify(CALL_LIST, shell=True),
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT,
                                   shell=True,
                                   creationflags=CREATION_FLAGS)
        # Wait for the process to finish
        while PROCESS.poll() is None:
            string = PROCESS.stdout.readline()
            if string:
                print("{}".format(string.decode()), end="")
    except ValueError as ex:
        print("ERROR: {} while trying to execute {}.". \
              format(type(ex).__name__, str(ex)))
    except KeyboardInterrupt:
        pass

    # Finished with the connection now
    connect_thread.join()

    if not CONNECTED_TO_PROCESS_CHECKER:
        print("ERROR: unable to connect to {} on port {}.".format(PROCESS_CHECKER, ARGS.p))

    print("Return value {}".format(RETURN_VALUE))
    sys.exit(RETURN_VALUE)
