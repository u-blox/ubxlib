#!/usr/bin/env python

'''Process wrapper to help with Jenkins integration; a version of Morné Joubert's great idea.'''

import sys
import os
import threading
import socket
import subprocess
from time import sleep
import argparse
import u_settings
import u_utils

PROMPT = "u_process_wrapper: "

# The name of the script which forms the other half
# of the process wrapper/process checker pair.
PROCESS_CHECKER = u_settings.PROCESS_CHECKER # e.g. "u_process_checker.py"

# The default port number to use
PROCESS_PORT = u_settings.PROCESS_PORT # e.g. 50123

# What to use to invoke Python
PROCESS_PYTHON = u_settings.PROCESS_PYTHON # e.g. "python"

class ConnectToProcessChecker(threading.Thread):
    '''Class to connect to process checker on the given port number'''
    def __init__(self, port):
        self._port = port
        self._running = False
        self._has_been_connected = False
        self._return_value = None
        threading.Thread.__init__(self)
    def stop_thread(self):
        '''Helper function to stop the thread'''
        self._running = False
    def has_been_connected(self):
        '''Is we ever been connected?'''
        return self._has_been_connected
    def return_value(self):
        '''The return value from PROCESS_CHECKER'''
        return self._return_value
    def run(self):
        '''Worker thread'''
        self._running = True
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.settimeout(5)
            try:
                sock.connect(("127.0.0.1", self._port))
                self._has_been_connected = True
                # Set connection to non-blocking so that we
                # check self._running every so often
                sock.settimeout(0)
                connected = True
                process_checker_said = b""
                # Now just wait either to be told to stop, for
                # the connection to drop or for a return value
                # to be sent to us
                while self._running and connected:
                    try:
                        # Receive all we can on the socket
                        part = sock.recv(64)
                        while part:
                            process_checker_said += part
                            part = sock.recv(64)
                    except BlockingIOError:
                        # This is fine, the socket is there and
                        # we have received nothing
                        sleep(1)
                    except socket.error:
                        connected = False
                    if process_checker_said:
                        try:
                            self._return_value = int(process_checker_said.decode("utf8"))
                        except (UnicodeDecodeError, ValueError):
                            pass
            except (socket.error, ConnectionRefusedError):
                pass

if __name__ == "__main__":
    RETURN_VALUE = -1

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
                                     " SIGINT, AKA CTRL-C unless -t or -r"   \
                                     " is specified).")
    PARSER.add_argument("-t", action='store_true', help="use SIGTERM intead" \
                        " of SIGINT as the termination signal (cannot be"    \
                        " specified at the same time as -r).")
    PARSER.add_argument("-k", type=int, help="terminate the script if it"    \
                        " does not terminate in the given number of seconds" \
                        " after being sent the termination signal.")
    PARSER.add_argument("-p", type=int, default=PROCESS_PORT, help="the port"\
                        " number to use, default " + str(PROCESS_PORT) + ".")
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
        # Start a thread that attempts to connect
        # to PROCESS_CHECKER on the given port
        connect_thread = ConnectToProcessChecker(ARGS.p)
        connect_thread.start()

        # Launch PROCESS_CHECKER with the script and its parameters
        CALL_LIST = []
        if not u_utils.is_linux():
            CALL_LIST.append("cmd")
            CALL_LIST.append("/c")
            CALL_LIST.append("start")
            CALL_LIST.append("/B")
        if PROCESS_PYTHON:
            CALL_LIST.append(PROCESS_PYTHON)
        CALL_LIST.append(PROCESS_CHECKER)
        if ARGS.t:
            CALL_LIST.append("-t")
        if ARGS.k is not None:
            CALL_LIST.append("-k")
            CALL_LIST.append(str(ARGS.k))
        if ARGS.p:
            CALL_LIST.append("-p")
            CALL_LIST.append(str(ARGS.p))
        if ARGS.r:
            CALL_LIST.append("-r")
            CALL_LIST.append(str(ARGS.r))
        CALL_LIST.append(ARGS.script)
        if ARGS.params:
            CALL_LIST.extend(ARGS.params)

        # Print what we're going to do
        TMP = ""
        for item in CALL_LIST:
            if TMP:
                TMP += " "
            TMP += item
        print("{}in directory \"{}\" calling \"{}\" and waiting to be terminated.". \
              format(PROMPT, os.getcwd(), TMP))

        try:
            CREATION_FLAGS = 0
            # TODO Linux
            #if not u_utils.is_linux():
            # When debugging from the command-line on Windows
            # set this so that PROCESS_CHECKER pops up in another
            # window and you can see what's going on after we've
            # been killed
            #CREATION_FLAGS |= subprocess.DETACHED_PROCESS
            #CREATION_FLAGS |= subprocess.CREATE_NEW_PROCESS_GROUP
            # Set shell to True to keep Jenkins happy
            PROCESS = subprocess.Popen(u_utils.subprocess_osify(CALL_LIST, shell=True),
                                       stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT,
                                       shell=True,
                                       creationflags=CREATION_FLAGS)
            # Wait for the process to finish
            while PROCESS.poll() is None:
                string = PROCESS.stdout.readline().decode()
                if string and string != "":
                    print(string.rstrip())
                else:
                    sleep(0.1)
        except ValueError as ex:
            print("{}ERROR: {} while trying to execute {}.". \
                  format(PROMPT, type(ex).__name__, str(ex)))
        except KeyboardInterrupt:
            print("{}received CTRL-C, exiting...".format(PROMPT))

        if connect_thread.has_been_connected():
            # Wait a moment to make sure that the return value comes
            # through on the socket
            sleep(1)
            if connect_thread.return_value() is not None:
                RETURN_VALUE = connect_thread.return_value()
        else:
            print("{}ERROR: unable to connect to {} on port {}.".format(PROMPT,
                                                                        PROCESS_CHECKER,
                                                                        ARGS.p))

        # Finished with the connection now
        connect_thread.stop_thread()
        connect_thread.join()

        print("{}return value {}".format(PROMPT, RETURN_VALUE))

    sys.exit(RETURN_VALUE)
