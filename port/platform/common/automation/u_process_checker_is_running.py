#!/usr/bin/env python

'''Check if u_process_checker.py is still running.'''

import sys
from time import time, sleep
import argparse
import socket
import u_process_settings

PROMPT = "u_process_checker_is_running: "

# The name of the process checker script.
PROCESS_CHECKER = u_process_settings.CHECKER_NAME # e.g. "u_process_checker.py"

# The default port number that process checker would have been using.
PROCESS_PORT = u_process_settings.PORT # e.g. 50123

if __name__ == "__main__":
    RETURN_VALUE = -1
    START_TIME = None

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
    PARSER.add_argument("-t", type=int, default=0, help="keep checking"      \
                        " for the specified number of seconds.")
    ARGS = PARSER.parse_args()

    # Try binding to the port
    print("{}checking if {} is running on port {}...".format(PROMPT,
                                                             PROCESS_CHECKER,
                                                             ARGS.p))
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as SOCKET:
        end_time = time() + ARGS.t
        while RETURN_VALUE <= 0:
            try:
                SOCKET.bind(("127.0.0.1", ARGS.p))
                SOCKET.listen()
                # We're able to bind to it, hence process checker must have exited
                if START_TIME:
                    print("{}{} exited after {} second(s).".format(PROMPT,
                                                                   PROCESS_CHECKER,
                                                                   int(time() - START_TIME)))
                else:
                    print("{}{} is not running.".format(PROMPT, PROCESS_CHECKER))
                RETURN_VALUE = 1
            except socket.error:
                if START_TIME is None and ARGS.t > 0:
                    print("{}{} is running, waiting up to {} second(s) for"   \
                          " it to exit (best NOT to \"forcibly terminate\","  \
                          " let it finish in its own time)...".               \
                          format(PROMPT, PROCESS_CHECKER, ARGS.t))
                    START_TIME = time()
                # Can't bind to the socket, infer the process checker is running
                RETURN_VALUE = 0
                if ((ARGS.t > 0) and (time() < end_time)):
                    sleep(1)
                else:
                    break
            # This script doesn't output much so give it a shove
            # to make the output appear in a Jenkins console
            sys.stdout.flush()
    sys.exit(RETURN_VALUE)
