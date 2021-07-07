#!/usr/bin/env python

'''Process wrapper to help with Jenkins integration; a version of Morné Joubert's great idea.'''

import sys
import os
from signal import signal, SIGTERM
import threading
import socket
import subprocess
from time import time, sleep
import argparse
import u_utils
import u_process_settings

PROMPT = "u_process_wrapper: "

# The name of the script which forms the other half
# of the process wrapper/process checker pair.
PROCESS_CHECKER = u_process_settings.CHECKER_NAME # e.g. "u_process_checker.py"

# The default port number to use
PROCESS_PORT = u_process_settings.PORT # e.g. 50123

# What to use to invoke Python
PROCESS_PYTHON = u_process_settings.PYTHON # e.g. "python"

# The special prefix used before a return value
# sent on a socket by u_process_checker
RETURN_VALUE_PREFIX = u_process_settings.RETURN_VALUE_PREFIX # e.g. "#!ubx!# RETURN_VALUE:"

# The receive socket buffer size to use
SOCKET_BUFFER_LENGTH = 4096

# The amount of silence on the socket which
# constitutes a timeout
SOCKET_RECEIVE_GUARD_TIME_SECONDS = 3600

class ConnectToProcessChecker(threading.Thread):
    '''Class to connect to process checker on the given port number'''
    def __init__(self, port, printing=True, receive_timeout=None):
        self._port = port
        self._printing = printing
        self._running = False
        self._connected = False
        self._receive_timeout = receive_timeout
        self._guard_timed_out = False
        self._return_value = None
        threading.Thread.__init__(self)
    def stop_thread(self):
        '''Helper function to stop the thread'''
        self._running = False
    def connected(self):
        '''Are we connected?'''
        return self._connected
    def guard_timed_out(self):
        '''Return true if we're no longer receiving stuff on the socket'''
        return self._guard_timed_out
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
                self._connected = True
                # Set connection to non-blocking so that we
                # check self._running every so often
                sock.settimeout(0)
                last_receive_time = time()
                # Now just wait either to be told to stop, for
                # the connection to drop or for a return value
                # to be sent to us
                while self._running and self._connected:
                    message = bytes()
                    try:
                        # Receive all we can on the socket
                        part = sock.recv(SOCKET_BUFFER_LENGTH)
                        while part:
                            message += part
                            part = sock.recv(SOCKET_BUFFER_LENGTH)
                    except BlockingIOError:
                        # This is fine, the socket is there and
                        # we have received nothing
                        sleep(0.1)
                    except socket.error as ex:
                        self._connected = False
                        print("{}connection to {} closed (socket error {} {}).". \
                              format(PROMPT, PROCESS_CHECKER, type(ex).__name__, str(ex)))
                    if message:
                        last_receive_time = time()
                        # Pick out the return value and print the stream if asked
                        try:
                            process_checker_said = message.decode("utf8")
                            if process_checker_said:
                                try:
                                    self._return_value = int(process_checker_said.split(RETURN_VALUE_PREFIX)[1])
                                except (IndexError, ValueError):
                                    if self._printing:
                                        # Somewhere along the way line endings are being
                                        # turned into "\r\n" which Jenkins interprets as
                                        # two linefeeds (<sigh>) so strip them off here.
                                        print(process_checker_said.replace("\r\n","\n"), end="")
                        except UnicodeDecodeError:
                            pass
                    if self._receive_timeout is not None and \
                       time() > last_receive_time + self._receive_timeout:
                        self._guard_timed_out = True
            except (socket.error, ConnectionRefusedError):
                pass

def sigterm_handler():
    ''' Just exit on receipt of SIGTERM'''
    print("{}received SIGTERM, exiting.".format(PROMPT))
    sys.exit(-1)

if __name__ == "__main__":
    RETURN_VALUE = -1
    CONNECT_THREAD = None

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
    PARSER.add_argument("-g", type=int, default=SOCKET_RECEIVE_GUARD_TIME_SECONDS,
                        help="change the default guard timer on lack of activity" \
                        " from " + PROCESS_CHECKER + ", default " +               \
                        str(SOCKET_RECEIVE_GUARD_TIME_SECONDS) + " seconds.")
    PARSER.add_argument("script", default=None, help="the name of the Python" \
                        " script to execute.")
    PARSER.add_argument("params", nargs=argparse.REMAINDER, default=None,    \
                        help="parameters to go with the script.")
    ARGS = PARSER.parse_args()

    # Trap SIGTERM, which Jenkins can send
    signal(SIGTERM, sigterm_handler)

    if ARGS.t and ARGS.r:
        print("Cannot specify -t and -r at the same time.")
    else:
        # Launch PROCESS_CHECKER with the script and its parameters
        CALL_LIST = []
        if not u_utils.is_linux():
            # We run process checker via "start" (the closest Windows
            # gets to fork): this way if process wrapper is terminated
            # by Jenkins, process checker continues running.
            # Note however, that means we don't get to see its output
            # at all, so process checker must send it back over the
            # socket.
            CALL_LIST.append("cmd")
            CALL_LIST.append("/c")
            CALL_LIST.append("start")
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
            # Now, this step is CRITICAL.  Morné Joubert deserves credit for
            # finding this Jenkins detail, I've no idea how he did it.
            # Jenkins sets an environment variable named "JENKINS_SERVER_COOKIE"
            # in each process it launches, presumably as a cross-platform mechanism
            # to find its processes.  It can ask the OS for the environment for each
            # process and find the things that belong to it in order to kill them.
            # To prevent process checker from being killed, this environment variable
            # must be set to "ignore"
            process_checker_env = os.environ
            process_checker_env["JENKINS_SERVER_COOKIE"] = "ignore"
            PROCESS = subprocess.Popen(u_utils.subprocess_osify(CALL_LIST),
                                       env=process_checker_env)
            # The process should exit immediately, having forked
            # the thing we actually want
            while PROCESS.poll() is None:
                sleep(0.1)
            PROCESS_RETURN_VALUE = PROCESS.poll()
            if PROCESS_RETURN_VALUE == 0:
                # Start a thread that attempts to connect
                # to PROCESS_CHECKER on the given port
                CONNECT_THREAD = ConnectToProcessChecker(ARGS.p,
                                                         printing=True,
                                                         receive_timeout=ARGS.g)
                CONNECT_THREAD.start()

                COUNT = 0
                while not CONNECT_THREAD.connected() and COUNT < 10:
                    sleep(1)
                    COUNT += 1
                if CONNECT_THREAD.connected():
                    # Now we wait for either a timeout in
                    # the receipt of stuff on the socket
                    # or the return value to arrive on
                    # the socket
                    while CONNECT_THREAD.return_value() is None and \
                        not CONNECT_THREAD.guard_timed_out():
                        sleep(1)
                    if CONNECT_THREAD.return_value() is not None:
                        RETURN_VALUE = CONNECT_THREAD.return_value()
                    else:
                        if CONNECT_THREAD.guard_timed_out():
                            print("{}ERROR: nothing from {} on port {}"\
                                  " in {} second(s).".format(PROMPT,
                                                             PROCESS_CHECKER,
                                                             ARGS.p, ARGS.g))
                else:
                    print("{}ERROR: unable to connect to {} on port {}.".format(PROMPT,
                                                                                PROCESS_CHECKER,
                                                                                ARGS.p))
            else:
                print("{}ERROR: process returned {}.". \
                      format(PROMPT, PROCESS_RETURN_VALUE))
        except ValueError as ex:
            print("{}ERROR: {} while trying to execute {}.". \
                  format(PROMPT, type(ex).__name__, str(ex)))
        except KeyboardInterrupt:
            print("{}received CTRL-C, exiting and leaving {}"
                  " to do its work.".format(PROMPT, PROCESS_CHECKER))

        # Tidy up
        if CONNECT_THREAD:
            # Give it a moment
            sleep(1)
            CONNECT_THREAD.stop_thread()
            CONNECT_THREAD.join()

        print("{}return value {}".format(PROMPT, RETURN_VALUE))

    sys.exit(RETURN_VALUE)
