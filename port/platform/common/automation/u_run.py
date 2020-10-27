#!/usr/bin/env python

'''Run an instance of ubxlib automation and report results.'''

import sys # For exit() and stdout
import argparse
from multiprocessing import Process, freeze_support # Needed to make Windows behave
                                                    # when doing multiprocessing,
from signal import signal, SIGINT                   # For CTRL-C handling
import threading # For locks
import u_data # Gets and displays the instance database
import u_connection # The connection to use for a given instance
import u_run_esp32 # Build/run stuff on ESP32
import u_run_nrf52 # Build/run stuff on NRF52
import u_run_nrf53 # Build/run stuff on NRF53
import u_run_stm32f4 # Build/run stuff on STM32F4
import u_run_lint # Run Lint check
import u_run_doxygen # Run a Doxygen check
import u_run_astyle # Run AStyle check
import u_run_pylint # Run a Pylint check
import u_report # reporting
import u_utils
import u_settings

# Prefix to put at the start of all prints
PROMPT = "u_run: "

# Default BRANCH to use
BRANCH_DEFAULT = u_settings.BRANCH_DEFAULT #"origin/master"

def signal_handler(sig, frame):
    '''CTRL-C Handler'''
    del sig
    del frame
    sys.stdout.write('\n')
    print("{}CTRL-C received, EXITING.".format(PROMPT))
    sys.exit(-1)

def main(database, instance, filter_string, clean,
         ubxlib_dir, working_dir, connection_lock,
         system_lock, platform_lock, print_queue,
         report_queue, summary_report_file_path,
         test_report_file_path, debug_file_path):
    '''Main as a function'''
    return_value = 1
    connection = None
    platform = None
    summary_report_handle = None
    test_report_handle = None
    debug_handle = None
    instance_text = u_utils.get_instance_text(instance)
    printer_text = []

    signal(SIGINT, signal_handler)

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
    if filter_string:
        # The filter_string is just another #define so
        # add it to the list
        defines.append(u_utils.FILTER_MACRO_NAME + "=" + \
                       filter_string)
    # If there is a cellular module on this instance, add its
    # name to the defines list
    cellular_module_name = u_data.get_cellular_module_for_instance(database, instance)
    if cellular_module_name:
        defines.append("U_CFG_TEST_CELL_MODULE_TYPE=" + cellular_module_name)

    # Also, when running testing it is best to run the
    # the "port" tests first as, if there's a problem with the
    # port, you want to notice it first.
    defines.append("U_RUNNER_TOP_STR=port")

    # With a reporter
    with u_report.ReportToQueue(report_queue, instance,
                                summary_report_handle,
                                printer) as reporter:
        if connection:
            # Run the type of build/test specified
            platform = u_data.get_platform_for_instance(database, instance)
            # Get the SDK for the instance (only really needed by NRF52)
            sdk = u_data.get_sdk_for_instance(database, instance)
            if platform:
                # Since there will be many different platforms, add
                # the description from the database to the report
                description = u_data.get_description_for_instance(database,
                                                                  instance)
                if description:
                    reporter.event(u_report.EVENT_TYPE_BUILD,
                                   u_report.EVENT_NAME,
                                   description)
                if platform.lower() == "esp32":
                    return_value = u_run_esp32.run(instance, sdk, connection,
                                                   connection_lock, platform_lock,
                                                   clean, defines, ubxlib_dir,
                                                   working_dir, system_lock,
                                                   printer, reporter,
                                                   test_report_handle)
                elif platform.lower() == "nrf52":
                    return_value = u_run_nrf52.run(instance, sdk, connection,
                                                   connection_lock, platform_lock,
                                                   clean, defines, ubxlib_dir,
                                                   working_dir, system_lock, printer,
                                                   reporter, test_report_handle)
                elif platform.lower() == "nrf53":
                    return_value = u_run_nrf53.run(instance, sdk, connection,
                                                   connection_lock, platform_lock,
                                                   clean, defines, ubxlib_dir,
                                                   working_dir, system_lock, printer,
                                                   reporter, test_report_handle)
                elif platform.lower() == "stm32f4":
                    return_value = u_run_stm32f4.run(instance, sdk, connection,
                                                     connection_lock, platform_lock,
                                                     clean, defines, ubxlib_dir,
                                                     working_dir, system_lock, printer,
                                                     reporter, test_report_handle)
                else:
                    printer.string("{}don't know how to handle platform \"{}\".".    \
                                   format(PROMPT, platform))
            else:
                printer.string("{}this instance has no platform.".format(PROMPT))
        else:
            # No connection, must be a local thing
            if instance[0] == 0:
                return_value = u_run_lint.run(instance, ubxlib_dir, working_dir,
                                              printer, reporter)
            elif instance[0] == 1:
                return_value = u_run_doxygen.run(instance, ubxlib_dir, working_dir,
                                                 printer, reporter)
            elif instance[0] == 2:
                return_value = u_run_astyle.run(instance, ubxlib_dir, working_dir,
                                                printer, reporter)
            elif instance[0] == 3:
                return_value = u_run_pylint.run(instance, ubxlib_dir, working_dir,
                                                printer, reporter)
            elif instance[0] == 4:
                printer.string("{}reserved, nothing to do.".format(PROMPT))
                return_value = 0
            elif instance[0] == 5:
                printer.string("{}reserved, nothing to do.".format(PROMPT))
                return_value = 0
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
                        " -d results.log, any existing file will"   \
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

                # Create a lock to cover install processes
                # that any thread of main() may need
                # to perform
                INSTALL_LOCK = threading.Lock()

                # The platform lock is meaningless if
                # called from here (since this will only
                # be a single instance) but, just so that
                # the code that is called doesn't complain
                # about a None value, give it one
                PLATFORM_LOCK = threading.Lock()

                # Call main()
                RETURN_VALUE = main(DATABASE, INSTANCE, ARGS.f, ARGS.c,
                                    ARGS.u, ARGS.w, None, INSTALL_LOCK,
                                    PLATFORM_LOCK, None, None, ARGS.s, ARGS.t, ARGS.d)
        else:
            print("{}must supply an instance.".format(PROMPT))
            PARSER.print_help()

    sys.exit(RETURN_VALUE)

# A main is required because Windows needs it in order to
# behave when this module is called during multiprocessing
# see https://docs.python.org/2/library/multiprocessing.html#windows
if __name__ == '__main__':
    freeze_support()
    PROCESS = Process(target=main)
    PROCESS.start()
