#!/usr/bin/env python

'''Check if u_process_checker.py is still running.'''

import sys
import os
import signal
import psutil
import argparse
import socket
from time import sleep, time
import u_settings
import u_utils

# The name of the process checker script.
PROCESS_CHECKER = u_settings.PROCESS_CHECKER # e.g. "u_process_checker.py"

# The default port number that process checker would have been using.
PROCESS_PORT = u_settings.PROCESS_PORT # e.g. 50123

if __name__ == "__main__":
    RETURN_VALUE = -1

    PARSER = argparse.ArgumentParser(description="Return zero if the"         \
                                     " " + PROCESS_CHECKER + "script (and"    \
                                     " hence likely the scripts it executed)" \
                                     " are still running; a positive value"   \
                                     " indicates it (and hence they) have"    \
                                     " ended, a negative value indicates that"\
                                     " an error as occurred.")
    PARSER.add_argument("-p", type=int, default=PROCESS_PORT, help="the port"\
                        " number " + PROCESS_CHECKER + " was using, default" \
                        " " + str(PROCESS_PORT) + ".")
    ARGS = PARSER.parse_args()

    # Try binding to the port
    print("Checking for {} on port {}...".format(PROCESS_CHECKER, ARGS.p))
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as SOCKET:
        try:
            SOCKET.bind(("127.0.0.1", ARGS.p))
            SOCKET.listen()
            # We're able to bind to it, hence process checker must have exitted
            RETURN_VALUE = 1
        except socket.error:
            RETURN_VALUE = 0

    print("Return value {}".format(RETURN_VALUE))
    sys.exit(RETURN_VALUE)
