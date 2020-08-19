#!/usr/bin/env python

'''Decide what ubxlib testing to do based on a Github pull request.'''

import sys # for exit()
import argparse
from time import sleep
import os # For sep and getcwd() and makedirs()
from signal import signal, SIGINT, SIG_IGN         # for signal_handler
from multiprocessing import Manager                # To launch u_run_blah.py instances
import multiprocessing.pool                        # Specific import for daemonic process dodge
import u_data   # Gets the instance DATABASE
import u_select # Decide what to run for ourselves
import u_connection # To initialise locks
import u_run    # Actually run stuff
import u_report # reporting
import u_utils  # utils

# Decide what to do with a pull request based on the
# message that came with the pull request and the
# list of files that have been changed.
#
# Looks for a single line anywhere in the pull request
# text beginning with "test: ".  This must be followed
# either by "x.y.x" and then "blah", or just "x.y.z"
# or "*".  Valid examples are:
#
# test: 1
# test: 1.0.3
# test: 1 example
# test: 1.0 portInit
# test: *
# test: * port
#
# Only whitespace is expected after this on the line.
# Anything else is ignored.

# Prefix to put at the start of all prints
PROMPT = "u_pull_request: "

# Prefix for the individual instance working directory
# Starts with a "u" in order that it gets sorted after
# the name we usually use for the summary log
# (summary.log)
INSTANCE_DIR_PREFIX = "u_instance_"

# The number of seconds at which to report what's still running
STILL_RUNNING_REPORT_SECONDS = 30

# These two wrapper classes stolen from:
# https://stackoverflow.com/questions/6974695/python-process-pool-non-daemonic
# Reason for it is that some of our pool processes need
# to be able to create processes themselves (e.g. for STM32F4)
# and processes that are marked as daemons, as they are
# by default, are not permitted to do that.
class NoDaemonProcess(multiprocessing.Process):
    '''Version of Process that avoids it being a Deamon'''
    # make 'daemon' attribute always return False
    def _get_daemon(self):
        return False
    def _set_daemon(self, value):
        pass
    daemon = property(_get_daemon, _set_daemon)

# Sub-class multiprocessing.pool.Pool instead of multiprocessing.Pool
# because the latter is only a wrapper function, not a proper class.
class NoDaemonPool(multiprocessing.pool.Pool):
    '''Sub-class Pool'''
    Process = NoDaemonProcess

def parse_message(message, instances):
    '''Find stuff in a pull request message'''
    instance_local = []
    filter_string_local = None

    if message:
        # Search through message for a line beginning
        # with "test:"
        print "{}parsing message to see if it contains"  \
              " a test directive...".format(PROMPT)
        lines = message.split("\\n")
        for idx1, line in enumerate(lines):
            print "{}text line {}: \"{}\"".              \
                  format(PROMPT, idx1 + 1, line)
            if line.lower().startswith("test:"):
                # Pick through what follows
                parts = line[5:].split()
                for part in parts:
                    if instance_local and filter_string_local:
                        # If we have found an instance
                        # and a filter and there's still
                        # something else then this wasn't
                        # a "test:" line at all, carry
                        # on trying
                        del instance_local[:]
                        filter_string_local = None
                        break
                    elif instance_local:
                        # If we have an instance, and we
                        # haven't got a filter it
                        # must be a filter
                        filter_string_local = part
                    elif part[0].isdigit():
                        # If we haven't had an instance
                        # and this is part begins with
                        # a digit it could be an instance
                        # containing numbers
                        for item in part.split("."):
                            try:
                                instance_local.append(int(item))
                            except ValueError:
                                del instance_local[:]
                                break
                    elif part == "*":
                        # If we haven't had an instance
                        # and this is a * then it means
                        # "all"
                        instance_local.append(part)
                    else:
                        # Found some rubbish, not a "test:"
                        # line after all
                        del instance_local[:]
                        filter_string_local = None
                        break
                if instance_local:
                    # Stop once we've seen a valid instance
                    found = "found instance "
                    for idx2, item in enumerate(instance_local):
                        if idx2 == 0:
                            found += str(item)
                        else:
                            found += "." + str(item)
                    if filter_string_local:
                        found += " and filter \"" +          \
                                 filter_string_local + "\"."
                    print "{}{}".format(PROMPT, found)
                    break

    if instance_local:
        instances.append(instance_local[:])

    return filter_string_local

def create_platform_locks(database, instances, manager, platform_locks):
    '''Create a lock per platform in platform_locks'''
    for instance in instances:
        this_platform = u_data.get_platform_for_instance(database, instance)
        if this_platform:
            done = False
            for platform in platform_locks:
                if this_platform == platform["platform"]:
                    done = True
                    break
            if not done:
                platform_locks.append({"platform": this_platform, "lock": manager.RLock()})

def run_instances(database, instances, filter_string, ubxlib_dir,
                  working_dir, clean, summary_report_file,
                  test_report_file, debug_file):
    '''Run the given instances'''
    return_value = 0
    processes = []
    platform_locks = []
    alive_count = 0
    report_thread = None
    report_queue = None
    reporter = None
    summary_report_file_path = None
    test_report_file_path = None
    debug_file_path = None
    summary_report_handle = None

    # Create a lock to cover install processes
    # that any process of u_run.main() may need
    # to perform outside of its working directory
    manager = Manager()
    install_lock = manager.RLock()

    # It is possible, on some platforms, for the SDKs
    # to be a bit pants at running in multiple instances
    # hence here we create a lock per platform and pass it
    # into the instance for it to be able to manage
    # multiplicity if required
    create_platform_locks(database, instances, manager, platform_locks)

    # Launch a thread that prints stuff out
    # nicely from multiple sources
    print_queue = manager.Queue()
    print_thread = u_utils.PrintThread(print_queue)
    print_thread.start()

    # Set up a printer for this thread to print to the queue
    printer = u_utils.PrintToQueue(print_queue, None, True)

    if summary_report_file:
        # Launch a thread that manages reporting
        # from multiple sources
        summary_report_file_path = working_dir + os.sep + summary_report_file
        summary_report_handle = open(summary_report_file_path, "w")
        if summary_report_handle:
            printer.string("{}writing overall summary report to \"{}\".".  \
                           format(PROMPT, summary_report_file_path))
        else:
            printer.string("{}unable to open file \"{}\" for overall summary report.".   \
                           format(PROMPT, summary_report_file_path))
        report_queue = manager.Queue()
        report_thread = u_report.ReportThread(report_queue, summary_report_handle)
        report_thread.start()
        reporter = u_report.ReportToQueue(report_queue, None, None, printer)
        reporter.open()

    # From this post:
    # https://stackoverflow.com/questions/11312525/catch-ctrlc-sigint-and-exit-multiprocesses-gracefully-in-python
    # ...create a pool of worker processes to run our
    # instances, then they will handle sigint correctly
    # and tidy up after themselves.

    # SIGINT is ignored while the pool is created
    original_sigint_handler = signal(SIGINT, SIG_IGN)
    # On the face of it the number of worker processes should be
    # equal to the number of instances.  However, running more
    # worker processes than there are logical processors reduces
    # the overall speed due to the churn in sharing them out and
    # running 10 or more on an 8 logical CPU machine causes
    # unexplained slow-downs.  Hence, for the moment, while
    # compilation load is predominant as the test runs take no
    # more than 30 seconds, we set the number of workers
    # to 4 as that is (by experiment) the optimal number.
    pool = NoDaemonPool(4)
    signal(SIGINT, original_sigint_handler)

    # Create locks for connections
    u_connection.init_locks(manager)

    try:
        # Set up all the instances
        for instance in instances:
            # Provide a working directory that is unique
            # for each instance and make sure it exists
            if working_dir:
                this_working_dir = working_dir + os.sep +       \
                                   INSTANCE_DIR_PREFIX + \
                                   u_utils.get_instance_text(instance).replace(".", "_")
            else:
                this_working_dir = os.getcwd() + os.sep +       \
                                   INSTANCE_DIR_PREFIX + \
                                   u_utils.get_instance_text(instance).replace(".", "_")
            if not os.path.isdir(this_working_dir):
                os.makedirs(this_working_dir)
            # Only clean the working directory if requested
            if clean:
                u_utils.deltree(this_working_dir, printer, PROMPT)
                os.makedirs(this_working_dir)

            # Create the file paths for this instance
            if summary_report_file:
                summary_report_file_path = this_working_dir + os.sep + summary_report_file
            if test_report_file:
                test_report_file_path = this_working_dir + os.sep + test_report_file
            if debug_file:
                debug_file_path = this_working_dir + os.sep + debug_file

            # Start u_run.main in each worker thread
            process = {}
            process["platform"] = u_data.get_platform_for_instance(database, instance)
            process["instance"] = instance
            process["platform_lock"] = None
            for platform_lock in platform_locks:
                if process["platform"] == platform_lock["platform"]:
                    process["platform_lock"] = platform_lock["lock"]
                    break
            process["handle"] = pool.apply_async(u_run.main,
                                                 (database, instance,
                                                  filter_string, True,
                                                  ubxlib_dir, this_working_dir,
                                                  u_connection.get_lock(instance),
                                                  install_lock,
                                                  process["platform_lock"],
                                                  print_queue, report_queue,
                                                  summary_report_file_path,
                                                  test_report_file_path,
                                                  debug_file_path))
            alive_count += 1
            processes.append(process.copy())

        # Wait for all the launched processes to complete
        printer.string("{}all instances now launched.".format(PROMPT))
        loop_count = 0
        while alive_count > 0:
            for process in processes:
                instance_text = u_utils.get_instance_text(process["instance"])
                if not "dealt_with" in process and process["handle"].ready():
                    try:
                        # If the return value has gone negative, i.e.
                        # an infrastructure failure, leave it there,
                        # else add the number of test failures to it
                        if (return_value >= 0 and process["handle"].get() > 0) or \
                            (return_value <= 0 and process["handle"].get() < 0):
                            return_value += process["handle"].get()
                    except KeyboardInterrupt:
                        raise KeyboardInterrupt
                    except Exception as ex:
                        # If an instance threw an exception then flag an
                        # infrastructure error
                        return_value = -1
                        printer.string("{}instance {} threw exception \"{}:"    \
                                       " {}\" but I can't tell you where"       \
                                       " I'm afraid.".                          \
                                       format(PROMPT, instance_text,
                                              type(ex).__name__,
                                              ex.message))
                        if reporter:
                            reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                                           u_report.EVENT_FAILED,
                                           "instance {} threw exception \"{}: {}\"". \
                                           format(instance_text, type(ex).__name__,
                                                  ex.message))
                    alive_count -= 1
                    process["dealt_with"] = True
                if not process["handle"].ready() and                         \
                   (loop_count == STILL_RUNNING_REPORT_SECONDS):
                    printer.string("{}instance {} still running.".           \
                                        format(PROMPT, instance_text))
            loop_count += 1
            if loop_count > STILL_RUNNING_REPORT_SECONDS:
                loop_count = 0
            sleep(1)

    except KeyboardInterrupt:
        # Pools can tidy themselves up on SIGINT
        printer.string("{}caught CTRL-C, terminating instances...".format(PROMPT))
        if reporter:
            reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                           u_report.EVENT_FAILED,
                           "CTRL-C received, terminating")
        pool.terminate()
        return_value = 0

    # Tidy up
    pool.close()
    pool.join()
    if reporter:
        reporter.event_extra_information("return value overall {} (0 = success, negative ="   \
                                         " probable infrastructure failure, positive ="       \
                                         " failure(s) (may still be due to infrastructure))". \
                                         format(return_value))
        reporter.close()

    # Wait for the print and report queues to empty
    # and stop the print process
    printer.string("{}all runs complete, return value {}.". \
                        format(PROMPT, return_value))
    sleep(1)
    print_thread.stop_thread()
    print_thread.join()

    # Stop the reporting process
    if report_thread:
        report_thread.stop_thread()
        report_thread.join()

    if summary_report_handle:
        summary_report_handle.close()

    return return_value

if __name__ == "__main__":
    RETURN_VALUE = 0
    DATABASE = []
    INSTANCES = []
    FILTER_STRING = None

    # Switch off traceback to stop the horrid developmenty prints
    #sys.tracebacklimit = 0
    PARSER = argparse.ArgumentParser(description="A script to"      \
                                     " run examples/tests on an"    \
                                     " instance of ubxlib hardware" \
                                     " based on a Github pull"      \
                                     " request.")
    PARSER.add_argument("-s", help="a summary report should be"      \
                        " written to the given file, e.g."           \
                        " -s summary.txt; any existing file will be" \
                        " over-written.")
    PARSER.add_argument("-t", help="an XML test report should be"   \
                        " written to the given file, e.g."          \
                        " -t report.xml; any existing file will be" \
                        " over-written.")
    PARSER.add_argument("-d", help="debug output should be"         \
                        " written to the given file, e.g."          \
                        " -d results.log, created in the working"   \
                        " directory if one is given; any existing"  \
                        " file will be over-written. If multiple"   \
                        " instances are being run separate files"   \
                        " will be created for each instance by"     \
                        " appending the instance number to the file"\
                        " name before any file extension, e.g."     \
                        " results_1_3_4.log.")
    PARSER.add_argument("-u", help="the root directory of ubxlib.")
    PARSER.add_argument("-w", help="an empty working directory"     \
                        " to use.")
    PARSER.add_argument("-c", action='store_true', help="clean"     \
                        " all working directories first.")
    PARSER.add_argument("message", help="the text from the pull"    \
                        " request submission message.")
    PARSER.add_argument("file", nargs='*', help="the file path(s)"  \
                        " changed in the pull request.")
    ARGS = PARSER.parse_args()

    # Get the instance DATABASE by parsing the data file
    DATABASE = u_data.get(u_data.DATA_FILE)

    # Parse the message
    FILTER_STRING = parse_message(ARGS.message, INSTANCES)

    if INSTANCES:
        # If there is a user instance, do what we're told
        if INSTANCES[0][0] == "*":
            print "{}running everything at user request.". \
                  format(PROMPT)
            del INSTANCES[:]
            INSTANCES = u_data.get_instances_all(DATABASE)
    else:
        # No instance specified by the user, decide what to run
        FILTER_STRING = u_select.select(DATABASE, INSTANCES, \
                                        ARGS.file)

    # Run them thar' instances
    RETURN_VALUE = run_instances(DATABASE, INSTANCES,
                                 FILTER_STRING, ARGS.u, ARGS.w,
                                 ARGS.c, ARGS.s, ARGS.t, ARGS.d)

    sys.exit(RETURN_VALUE)
