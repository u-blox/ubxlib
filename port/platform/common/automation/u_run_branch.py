#!/usr/bin/env python

'''Decide what ubxlib testing to do on a branch and do it.'''

import sys # for exit()
from signal import signal, SIGINT, SIG_IGN         # for signal_handler
import argparse
import u_data   # Gets the instance database
import u_select # Decide what to run for ourselves
import u_utils  # For commit_message_parse()
import u_agent

# Prefix to put at the start of all prints
PROMPT = "u_run_branch: "

# Ignore instance/filter, run the lot: a safety switch
RUN_EVERYTHING = False

if __name__ == "__main__":
    __spec__ = None
    RETURN_VALUE = 0
    DATABASE = []
    INSTANCES = []
    FILTER_STRING = None

    # Switch off traceback to stop the horrid developmenty prints
    #sys.tracebacklimit = 0
    PARSER = argparse.ArgumentParser(description="A script to"      \
                                     " run examples/tests on an"    \
                                     " instance of ubxlib hardware.")
    PARSER.add_argument("-s", help="a summary report should be"      \
                        " written to the given file, e.g."           \
                        " -s summary.txt; any existing file will be" \
                        " over-written.")
    PARSER.add_argument("-t", help="an XML test report should be"   \
                        " written to the given file, e.g."          \
                        " -t report.xml; any existing file will be" \
                        " over-written. If multiple instances are"  \
                        " being run separate directories will be"   \
                        " created for each instance.")
    PARSER.add_argument("-d", help="debug output should be"         \
                        " written to the given file, e.g."          \
                        " -d debug.txt, created in the working"     \
                        " directory if one is given; any existing"  \
                        " file will be over-written. If multiple"   \
                        " instances are being run separate"         \
                        " directories will be created for each"     \
                        " instance.")
    PARSER.add_argument("-u", help="the root directory of ubxlib.")
    PARSER.add_argument("-w", help="an empty working directory"     \
                        " to use.")
    PARSER.add_argument("-c", action='store_true', help="clean"     \
                        " all working directories first.")
    PARSER.add_argument("message", help="the text from the commit"  \
                        " message; please replace any occurrence"   \
                        " of \" with, say, ` and linefeeds with"    \
                        "\"\\n\" so that the text survives being"   \
                        " passed as a parameter.")
    PARSER.add_argument("file", nargs='*', help="the file path(s)"  \
                        " changed.")
    ARGS = PARSER.parse_args()

    # Get the instance DATABASE by parsing the data file
    DATABASE = u_data.get(u_data.DATA_FILE)

    if RUN_EVERYTHING:
        # Safety switch has been thrown, run the lot
        print("{}settings \"RUN_EVERYTHING\" is True.".format(PROMPT))
        INSTANCES = u_data.get_instances_all(DATABASE)
    else:
        # Parse the message
        FOUND, FILTER_STRING = u_utils.commit_message_parse(ARGS.message, INSTANCES)
        if FOUND:
            if INSTANCES:
                # If there is a user instance, do what we're told
                if INSTANCES[0][0] == "*":
                    print("{}running everything ".format(PROMPT), end="")
                    if FILTER_STRING:
                        print("on API \"{}\" ".format(FILTER_STRING), end="")
                    print("at user request.")
                    del INSTANCES[:]
                    INSTANCES = u_data.get_instances_all(DATABASE)
        else:
            # No instance specified by the user, decide what to run
            FILTER_STRING = u_select.select(DATABASE, INSTANCES, ARGS.file)

    if INSTANCES:
        # From this post:
        # https://stackoverflow.com/questions/11312525/catch-ctrlc-sigint-and-exit-multiprocesses-gracefully-in-python
        # ...create a pool of worker processes to run our
        # instances, then they will handle sigint correctly
        # and tidy up after themselves.

        # SIGINT is ignored while the pool is created
        ORIGINAL_SIGINT_HANDLER = signal(SIGINT, SIG_IGN)
        PROCESS_POOL = u_agent.NoDaemonPool(len(INSTANCES))
        signal(SIGINT, ORIGINAL_SIGINT_HANDLER)

        # Initialise the agent
        u_agent.init()
        u_agent.init_context_lock()

        try:
            # Run them thar' instances
            RETURN_VALUE = u_agent.session_run(DATABASE, INSTANCES,
                                               FILTER_STRING, ARGS.u, ARGS.w,
                                               ARGS.c, ARGS.s, ARGS.t, ARGS.d,
                                               PROCESS_POOL)

        except KeyboardInterrupt:
            print("{}caught CTRL-C, stopping.".format(PROMPT))
            # Note: close() rather than terminate() for a graceful shut-down
            PROCESS_POOL.close()
            PROCESS_POOL.join()

        # Deinitialise the agent
        u_agent.deinit_context_lock()
        u_agent.deinit()
    else:
        print("{}*** WARNING: no instances to run! ***".format(PROMPT))
        RETURN_VALUE = 0

    sys.exit(RETURN_VALUE)
