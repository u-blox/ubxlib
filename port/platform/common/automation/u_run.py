#!/usr/bin/env python

'''Run an instance of ubxlib automation and report results.'''

import sys # For exit() and stdout
import argparse
from os import environ, getcwd
from multiprocessing import Process, freeze_support # Needed to make Windows behave
                                                    # when doing multiprocessing,
from signal import signal, SIGINT                   # For CTRL-C handling
import u_data # Gets and displays the instance database
import u_connection # The connection to use for a given instance
import u_run_log # The main test result logger used for all external HW
import u_run_windows # Build/run stuff on Windows
import u_run_lint # Run Lint check
import u_run_doxygen # Run a Doxygen check
import u_run_astyle # Run AStyle check
import u_run_pylint # Run a Pylint check
import u_run_static_size # Run static sizing check
import u_run_no_floating_point # Run no floating point check
import u_report # reporting
import u_utils
import u_settings

# Prefix to put at the start of all prints
PROMPT = "u_run: "

# Default BRANCH to use
BRANCH_DEFAULT = u_settings.BRANCH_DEFAULT #"origin/master"

# The environment variable that may contain some defines
# we should use
UBXLIB_DEFINES_VAR = "U_UBXLIB_DEFINES"

# The name of the macro that we use to pass around the
# filter string
FILTER_MACRO_NAME = u_utils.FILTER_MACRO_NAME

# The environment variable to define to indicate that
# things are being built/run under automation
ENV_UBXLIB_AUTO = u_utils.ENV_UBXLIB_AUTO

def signal_handler(sig, frame):
    '''CTRL-C Handler'''
    del sig
    del frame
    sys.stdout.write('\n')
    print("{}CTRL-C received, EXITING.".format(PROMPT))
    sys.exit(-1)

def main(database, instance, filter_string, clean,
         ubxlib_dir, working_dir, connection_lock,
         platform_lock, misc_locks, print_queue,
         report_queue, summary_report_file_path,
         test_report_file_path, debug_file_path,
         keep_going_flag, running_flag, unity_dir):
    '''Main as a function'''
    return_value = 1
    connection = None
    platform = None
    summary_report_handle = None
    test_report_handle = None
    debug_handle = None
    instance_text = u_utils.get_instance_text(instance)
    printer_text = []

    if not working_dir:
        working_dir = getcwd()
    with u_utils.ChangeDir(working_dir):

        if running_flag:
            # We're off
            running_flag.set()

        # Create the files
        if summary_report_file_path:
            summary_report_handle = open(summary_report_file_path, "w")
            if summary_report_handle:
                printer_text.append("{}writing summary report to \"{}\".".  \
                                    format(PROMPT, summary_report_file_path))
            else:
                printer_text.append("{}unable to open file \"{}\" for summary report.".   \
                                    format(PROMPT, summary_report_file_path))
        if test_report_file_path:
            test_report_handle = open(test_report_file_path, "w")
            if test_report_handle:
                printer_text.append("{}writing test report to \"{}\".".  \
                                    format(PROMPT, test_report_file_path))
            else:
                printer_text.append("{}unable to open file \"{}\" for test report.".   \
                                    format(PROMPT, test_report_file_path))
        if debug_file_path:
            debug_handle = open(debug_file_path, "w")
            if debug_handle:
                printer_text.append("{}writing log output to \"{}\".".  \
                                    format(PROMPT, debug_file_path))
            else:
                printer_text.append("{}unable to open file \"{}\" for log"       \
                                    " output.".format(PROMPT, debug_file_path))

        # Create a printer and send the initial printer text there
        printer = u_utils.PrintToQueue(print_queue, debug_handle, True)
        for line in printer_text:
            printer.string(line)

        # Print out what we've been told to do
        text = "running instance " + instance_text
        if filter_string:
            text += " with filter_string \"" + filter_string + "\""
        if clean:
            text += ", clean build"
        if ubxlib_dir:
            text += ", ubxlib directory \"" + ubxlib_dir + "\""
        if working_dir:
            text += ", working directory \"" + working_dir + "\""
        printer.string("{}{}.".format(PROMPT, text))

        # Get the connection for this instance
        connection = u_connection.get_connection(instance)

        # Get the #defines for this instance
        defines = u_data.get_defines_for_instance(database, instance)
        if not defines:
            defines = []

        # Defines may be provided via an environment
        # variable, in a list separated with semicolons, e.g.:
        # set U_UBXLIB_DEFINES=THING_1;ANOTHER_THING=123;ONE_MORE=boo
        # Add these in.
        if UBXLIB_DEFINES_VAR in environ and environ[UBXLIB_DEFINES_VAR].strip():
            defines.extend(environ[UBXLIB_DEFINES_VAR].strip().split(";"))

        # Merge in any filter string we might have
        defines = u_utils.merge_filter(defines, filter_string)

        # It is sometimes useful for the platform tools to be able
        # to detect that they are running under automation (e.g. this
        # is used to switch ESP-IDF to using u_runner rather than the
        # usual ESP-IDF unit test menu system).
        # For this purpose we add ENV_UBXLIB_AUTO to the environment
        environ[ENV_UBXLIB_AUTO] = "1"

        # With a reporter
        with u_report.ReportToQueue(report_queue, instance,
                                    summary_report_handle,
                                    printer) as reporter:
            if connection:
                # Run the type of build/test specified
                platform = u_data.get_platform_for_instance(database, instance)
                if platform:
                    # Since there will be many different platforms, add
                    # the description from the database to the report
                    description = u_data.get_description_for_instance(database,
                                                                      instance)
                    mcu = u_data.get_mcu_for_instance(database, instance)
                    # Zephyr requires a board name also
                    board = u_data.get_board_for_instance(database, instance)
                    toolchain = u_data.get_toolchain_for_instance(database, instance)
                    if description:
                        reporter.event(u_report.EVENT_TYPE_BUILD,
                                       u_report.EVENT_NAME,
                                       description)

                    # A NOTE ABOUT keep_going_flag: the keep_going_flag is passed
                    # into any instance that will take more than a few seconds
                    # to run.  Each instance receiving it should ensure that
                    # anything that can be safely stopped and is likely to run
                    # for more than about 10ish seconds should be stopped
                    # if the flag is cleared, in case the user decides to abort
                    # a test run.
                    if platform.lower() == "windows":
                        return_value = u_run_windows.run(instance, mcu, toolchain, connection,
                                                         connection_lock, platform_lock, misc_locks,
                                                         clean, defines, printer, reporter,
                                                         test_report_handle,
                                                         keep_going_flag, unity_dir)
                    elif platform != "":
                        # For all external HW the firmware has already been built and flashed
                        # so we just need to listen for the log output
                        return_value = u_run_log.run(instance, printer, reporter, test_report_handle, keep_going_flag)
                    else:
                        printer.string("{}don't know how to handle platform \"{}\".".    \
                                    format(PROMPT, platform))
                else:
                    printer.string("{}this instance has no platform.".format(PROMPT))
            else:
                # No connection, must be a local thing
                if instance[0] == 0:
                    return_value = u_run_lint.run(instance, defines, ubxlib_dir,
                                                  printer, reporter,
                                                  keep_going_flag, unity_dir)
                elif instance[0] == 1:
                    return_value = u_run_doxygen.run(instance, ubxlib_dir,
                                                     printer, reporter)
                elif instance[0] == 2:
                    return_value = u_run_astyle.run(instance, ubxlib_dir,
                                                    printer, reporter)
                elif instance[0] == 3:
                    return_value = u_run_pylint.run(instance, ubxlib_dir,
                                                    printer, reporter, keep_going_flag)
                elif instance[0] == 4:
                    return_value = u_run_static_size.run(instance, defines, ubxlib_dir,
                                                         printer, reporter,
                                                         keep_going_flag)
                elif instance[0] == 5:
                    return_value = u_run_no_floating_point.run(instance, defines, ubxlib_dir,
                                                               printer, reporter,
                                                               keep_going_flag)
                elif instance[0] == 6:
                    printer.string("{}reserved, nothing to do.".format(PROMPT))
                    return_value = 0
                elif instance[0] == 7:
                    printer.string("{}reserved, nothing to do.".format(PROMPT))
                    return_value = 0
                elif instance[0] == 8:
                    printer.string("{}reserved, nothing to do.".format(PROMPT))
                    return_value = 0
                elif instance[0] == 9:
                    printer.string("{}reserved, nothing to do.".format(PROMPT))
                    return_value = 0
                else:
                    printer.string("{}instance {} has no connection and isn't a"     \
                                   " local thing.".format(PROMPT, instance_text))

        if platform:
            printer.string("{}instance {}, platform {} EXITING with"    \
                           " return value {}.".format(PROMPT, instance_text, platform,
                                                      return_value))
        elif connection:
            printer.string("{}instance {}, EXITING with return value {}.". \
                           format(PROMPT, instance_text, return_value))
        else:
            printer.string("{}instance {} EXITING with return value {}.".        \
                           format(PROMPT, instance_text, return_value))

        if summary_report_handle:
            summary_report_handle.close()
        if test_report_handle:
            test_report_handle.close()
        if debug_handle:
            debug_handle.close()

        if running_flag:
            # We're done
            running_flag.clear()

    return return_value

if __name__ == "__main__":
    RETURN_VALUE = 1
    INSTANCE = []
    BRANCH = BRANCH_DEFAULT
    DATABASE = []

    # Switch off traceback to stop the horrid developmenty prints
    #sys.tracebacklimit = 0
    PARSER = argparse.ArgumentParser(description="A script to"      \
                                     " run examples/tests on an"    \
                                     " instance of ubxlib hardware" \
                                     " and report the outcome.")
    PARSER.add_argument("-l", action='store_true', help="list the"  \
                        " instances that this script knows about"   \
                        " and exit.")
    PARSER.add_argument("-s", help="a summary report should be"     \
                        " written to the given file, e.g."          \
                        " -s summary.txt, any existing file will"   \
                        " be over-written.")
    PARSER.add_argument("-t", help="an XML test report should be"   \
                        " written to the given file, e.g."          \
                        " -t report.xml, any existing file will"    \
                        " be over-written.")
    PARSER.add_argument("-d", help="debug output should be"         \
                        " written to the given file, e.g."          \
                        " -d debug.txt, any existing file will"     \
                        " be over-written.")
    PARSER.add_argument("-c", action='store_true', help="clean"     \
                        " first.")
    PARSER.add_argument("-f", help="use a filter_string on the"     \
                        " items executed, e.g. -f example.")
    PARSER.add_argument("-u", help="the root directory of ubxlib.")
    PARSER.add_argument("-w", help="an empty working directory to"  \
                        " use.")
    PARSER.add_argument("instance", nargs="?", default=None,        \
                        help="the instance to use in the form"      \
                        " x.y.z where x is the HW instance, y the"  \
                        " variant (if there is more than one) and"  \
                        " z the sub-variant (again, if there is"    \
                        " more than one). x MUST be specified, y"   \
                        " and z may be omitted, e.g. 1 is valid"    \
                        " and 1.2 or 1.2.3 are also valid.")
    ARGS = PARSER.parse_args()

    # Check the command-line arguments
    if ARGS.l:
        # Just list the items and exit
        u_data.display(u_data.get(u_data.DATA_FILE))
        RETURN_VALUE = 0
    else:
        # Check that an instance has been given
        if ARGS.instance:
            # Make sure the instance is a valid string
            # and parse it into a list
            for string in ARGS.instance.split("."):
                try:
                    INSTANCE.append(int(string))
                except ValueError:
                    print("{}instance \"{}\" is not of the form 1.2.3" \
                          " as expected.".format(PROMPT, ARGS.instance))
                    del INSTANCE[:]
                    break
            if INSTANCE:
                # Get the instance database by parsing the data file
                DATABASE = u_data.get(u_data.DATA_FILE)

                # Call main()
                RETURN_VALUE = main(DATABASE, INSTANCE, ARGS.f, ARGS.c,
                                    ARGS.u, ARGS.w, None, None, None,
                                    None, None, ARGS.s, ARGS.t, ARGS.d,
                                    None, None, None)
        else:
            print("{}must supply an instance.".format(PROMPT))
            PARSER.print_help()

    sys.exit(RETURN_VALUE)

# A main is required because Windows needs it in order to
# behave when this module is called during multiprocessing
# see https://docs.python.org/2/library/multiprocessing.html#windows
if __name__ == '__main__':
    freeze_support()

    signal(SIGINT, signal_handler)

    PROCESS = Process(target=main)
    PROCESS.start()
