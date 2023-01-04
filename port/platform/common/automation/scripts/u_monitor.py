#!/usr/bin/env python

'''Monitor the running of a ubxlib example or test and report results.'''

import sys
import argparse
import re
import codecs
import queue
import threading
import os
from datetime import datetime
from time import time, ctime, sleep
from typing import List
from dataclasses import dataclass, field
from curses import ascii
import traceback
import subprocess
import psutil
import serial                # Pyserial (make sure to do pip install pyserial)
from lxml import etree
from scripts import u_report, u_utils
from scripts.u_logging import ULog

# Prefix to put at the start of all prints
PROMPT = "u_monitor"
# Global logging instance
U_LOG = None

# The connection types
CONNECTION_NONE = 0
CONNECTION_SERIAL = 1
CONNECTION_TELNET = 2
CONNECTION_PROCESS = 3
CONNECTION_RTT = 4

# These are used for the XML generator
@dataclass
class TestCaseResult:
    '''Test case result data.'''
    name: str = None
    start_time: datetime = None
    end_time: datetime = None
    duration: float = None
    status: str = None
    stdout: str = ""
    message: str = None

@dataclass
class TestResults:
    '''Test result data. Used for the complete test run.'''
    finished: bool = False
    reboots: int = 0
    errors: int = 0
    items_run: int = 0
    items_failed: int = 0
    items_ignored: int = 0
    overall_start_time: datetime = None
    test_cases: List[TestCaseResult] = field(default_factory=list)
    current: TestCaseResult = None

def delayed_finish(*args):
    '''Function to set "finished" after a time delay'''

    # We will have been passed the "results" list as the
    # single entry in the args array.
    args[0].finished = True

def test_error(_results: TestResults, reporter, message):
    if reporter:
        reporter.event(u_report.EVENT_TYPE_TEST,
                       u_report.EVENT_ERROR,
                       message)

def reboot_callback(match, user_parameter, results: TestResults, reporter):
    '''Handler for reboots occuring unexpectedly'''
    del match
    del user_parameter

    msg = "target has rebooted"
    record_outcome(results, "ERROR", reporter, msg)
    results.reboots += 1

    # After one of these messages the target can often spit-out
    # useful information so wait a second or two before stopping
    # capture
    finish_timer = threading.Timer(2, delayed_finish, args=[results])
    finish_timer.start()

def run_callback(match, user_parameter, results: TestResults, reporter):
    '''Handler for an item beginning to run'''
    del user_parameter

    name = match.group(1)
    if results.current:
        # There shouldn't be any current test case but this can happen
        # occasionally on Nordic NRF52 platforms (both the NRF5SDK and
        # the Zephyr flavour) due to loss of logging, where the line
        # giving the outcome of the previous test is lost.  There is
        # no pattern to this, it just happens some times, it doesn't
        # appear to be related to logging load.  Since it is (a) rare
        # enough not to be a worry in terms of checking code quality
        # and (b) frequent enough to give irritating false negatives,
        # each one of which has to be checked, we flag a warning when
        # this happens but not an error.
        msg = "** WARNING *** new test started before last one completed"

    results.current = TestCaseResult(name=name, start_time=datetime.now())
    U_LOG.info("progress update - item {}() started on {}.".     \
               format(match.group(1), results.current.start_time.time()))

    results.items_run += 1

def record_outcome(results: TestResults, status, reporter, message=None):
    '''What it says'''
    ret = results.current
    if status == "PASS":
        evt = u_report.EVENT_PASSED
    elif status == "FAIL":
        results.items_failed += 1
        evt = u_report.EVENT_FAILED
    else:
        results.errors += 1
        evt = u_report.EVENT_ERROR

    if results.current:
        results.current.end_time = datetime.now()
        results.current.duration = (results.current.end_time - \
            results.current.start_time).total_seconds()
        results.current.status = status
        results.current.message = message
        results.test_cases.append(results.current)
        string = "{}() {} on {} after running for {:.0f} second(s)".  \
                format(results.current.name, \
                       "passed" if status == "PASS" else status, \
                       results.current.end_time.time(), \
                       results.current.duration)
        U_LOG.info(f"progress update - item {string}.")

        if message:
            string += f": {message}"

        if reporter:
            reporter.event(u_report.EVENT_TYPE_TEST, evt, string)

        results.current = None
    else:
        # There are no current test running but if there is an error we need
        # to report it here
        if reporter and evt == u_report.EVENT_ERROR:
            reporter.event(u_report.EVENT_TYPE_TEST, evt, message)

    return ret


def pass_callback(_match, user_parameter, results: TestResults, reporter):
    '''Handler for an item passing'''
    del user_parameter
    record_outcome(results, "PASS", reporter)

def fail_callback(match, user_parameter, results: TestResults, reporter):
    '''Handler for a test failing'''
    del user_parameter
    msg = match.group(2)
    record_outcome(results, "FAIL", reporter, msg)

def finish_callback(match, user_parameter, results: TestResults, reporter):
    '''Handler for a run finishing'''
    del user_parameter

    end_time = datetime.now()
    diff_sec = int((end_time - results.overall_start_time).total_seconds())
    duration_hours = int(diff_sec / 3600)
    duration_minutes = int((diff_sec % 3600) / 60)
    duration_seconds = int((diff_sec % 3600) % 60)
    results.items_run = int(match.group(1))
    results.items_failed = int(match.group(2))
    results.items_ignored = int(match.group(3))
    U_LOG.info("run completed on {}, {} item(s) run, {} item(s) failed,"\
               " {} item(s) ignored, run took {}:{:02d}:{:02d}.". \
               format(end_time.time(), results.items_run,
                      results.items_failed, results.items_ignored,
                      duration_hours, duration_minutes, duration_seconds))
    if reporter:
        reporter.test_suite_completed_event(results.items_run, results.items_failed,
                                            results.items_ignored,
                                            "run took {}:{:02d}:{:02d}".            \
                                            format(duration_hours, duration_minutes,
                                                   duration_seconds))
    results.finished = True

# List of regex strings to look for in each line returned by
# the test output and a function to call when the regex
# is matched.  The regex result is passed to the callback.
# Regex tested at https://regex101.com/ selecting Python as the flavour
INTERESTING = [[r"abort()", reboot_callback, None],
               # This one for ESP32 aborts
               [r"Guru Meditation Error", reboot_callback, None],
               # This one for ESP32 asserts
               [r"assert failed:", reboot_callback, None],
               # This one for NRF52 aborts
               [r"<error> hardfault", reboot_callback, None],
               # This one for Zephyr aborts
               [r">>> ZEPHYR FATAL ERROR", reboot_callback, None],
               # Match, for example "BLAH: Running getSetMnoProfile..."
               # capturing the "getSetMnoProfile" part
               [r"(?:^.*Running) +([^\.]+(?=\.))...$", run_callback, None],
               # Match, for example "C:/temp/file.c:890:connectedThings:PASS"
               # capturing the "connectThings" part
               [r"(?:^.*?(?:\.c:))(?:[0-9]*:)(.*?):PASS$", pass_callback, None],
               # Match, for example "C:/temp/file.c:900:tcpEchoAsync:FAIL:Function sock.
               # Expression Evaluated To FALSE" capturing the "connectThings" part
               [r"(?:^.*?(?:\.c:))(?:[0-9]*:)(.*?):FAIL:(.*)", fail_callback, None],
               # Match, for example "22 Tests 1 Failures 0 Ignored" capturing the numbers
               [r"(^[0-9]+) Test(?:s*) ([0-9]+) Failure(?:s*) ([0-9]+) Ignored", finish_callback, None]]

def readline_and_queue(results, read_queue, in_handle, connection_type,
                       terminator, reporter):
    '''Read lines from the input and queue them'''
    error_msg = None
    try:
        while not results.finished:
            line = pwar_readline(in_handle, connection_type, terminator)
            if line:
                read_queue.put(line)
            # Let others in
            sleep(0.01)
    except subprocess.CalledProcessError as ex:
        if ex.returncode != 0:
            U_LOG.error(f"{ex}")
            error_msg = f"Monitored process terminated with error code: {ex.returncode}"
        else:
            U_LOG.warning(f'Monitored process "{ex.cmd}" terminated')
    except Exception as ex:
        U_LOG.error(traceback.format_exc())
        error_msg = f'Caught exception: "{ex}"'

    if error_msg:
        results.errors += 1
        if reporter:
            reporter.event(u_report.EVENT_TYPE_TEST,
                           u_report.EVENT_ERROR,
                           error_msg)

    results.finished = True

# Read lines from input, returns the line as
# a string when terminator or '\n' is encountered.
# Does NOT return the terminating character
# If a read timeout occurs then None is returned.
def pwar_readline(in_handle, connection_type, terminator=None):
    '''Phil Ware's marvellous readline function'''
    return_value = None
    line = ""
    if terminator is None:
        terminator = "\n"
    if connection_type == CONNECTION_TELNET:
        terminator_bytes = bytes(terminator, 'ascii')
        # I was hoping that all sources of data
        # would have a read() function but it turns
        # out that Telnet does not, it has read_until()
        # which returns a whole line with a timeout
        # Note: deliberately don't handle the exception that
        # the Telnet port has been closed here, allow it
        # to stop us entirely
        # Long time-out as we don't want partial lines
        try:
            line = in_handle.read_until(terminator_bytes, 1). \
                decode('ascii', errors='backslashreplace')
        except UnicodeDecodeError:
            # Just ignore it.
            pass
        if line != "":
            # To make this work the same way as the
            # serial and exe cases, need to remove the terminator
            # and remove any dangling \n left on the front
            line = line.rstrip(terminator)
            line = line.lstrip("\n")
        else:
            line = None
        return_value = line
        # Serial ports just use read()
    elif connection_type in (CONNECTION_SERIAL, CONNECTION_RTT):
        eol = False
        try:
            while not eol and line is not None:
                buf = in_handle.read(1)
                if buf:
                    character = buf.decode('ascii', errors='backslashreplace')
                    eol = character == terminator
                    if not eol:
                        line = line + character
                else:
                    line = None
                    # Since this is a busy/wait we sleep a bit if there is no data
                    # to offload the CPU
                    sleep(0.01)
            if eol:
                line = line.rstrip()
        except UnicodeDecodeError:
            # Just ignore it.
            pass
        return_value = line
        # For pipes, need to keep re-reading even
        # when nothing is there to avoid reading partial
        # lines as the pipe is being filled
    elif connection_type == CONNECTION_PROCESS:
        eol = False
        while not eol:
            return_code = in_handle.poll()
            buf = in_handle.stdout.read(1)
            if buf:
                character = buf.decode('ascii', errors='backslashreplace')
                eol = character == terminator
                if not eol:
                    line = line + character
            elif return_code is not None:
                # We got the return code and read returned None which means
                # that process has terminated and all characters has been read
                cmd = " ".join(in_handle.args) if isinstance(in_handle.args, list) \
                    else in_handle.args
                raise subprocess.CalledProcessError(return_code, cmd)

        if eol:
            line = line.rstrip()

        return_value = line

    if return_value is None or return_value == "":
        # Typically this function is called from a busy/wait loop
        # so if there currently no data available we sleep a bit
        # to offload the CPU
        sleep(0.01)

    return return_value

# Start the required executable.
def start_exe(exe_name):
    '''Launch an executable as a sub-process'''
    return_value = None
    text = "trying to launch \"{}\" as an executable...".     \
           format(exe_name)
    try:
        popen_keywords = {
            'stdout': subprocess.PIPE,
            'stderr': subprocess.STDOUT,
            'shell': True # Jenkins hangs without this
        }
        return_value = subprocess.Popen(exe_name, **popen_keywords)
    except (ValueError, serial.SerialException, WindowsError):
        U_LOG.error(f"{text} failed.")
    return return_value

# Send the given string before running, only used on ESP32 platforms.
def esp32_send_first(send_string, in_handle, connection_type):
    '''Send a string before running tests, used on ESP32 platforms'''
    success = False
    try:
        line = ""
        # Read the opening splurge from the target
        # if there is any
        U_LOG.info("reading initial text from input...")
        sleep(1)
        # Wait for the test selection prompt
        while line is not None and \
              line.find("Press ENTER to see the list of tests.") < 0:
            line = pwar_readline(in_handle, connection_type, "\r")
            if line is not None:
                U_LOG.info(line)
        # For debug purposes, send a newline to the unit
        # test app to get it to list the tests
        U_LOG.info("listing items...")
        in_handle.write("\r\n".encode("ascii"))
        line = ""
        while line is not None:
            line = pwar_readline(in_handle, connection_type, "\r")
            if line is not None:
                U_LOG.info(line)
        # Now send the string
        U_LOG.info(f"sending {send_string}")
        in_handle.write(send_string.encode("ascii"))
        in_handle.write("\r\n".encode("ascii"))
        U_LOG.info(f"run started on {ctime(time())}.")
        success = True
    except serial.SerialException as ex:
        U_LOG.error("{} while accessing port {}: {}.".
                       format(type(ex).__name__,
                              in_handle.name, str(ex)))
    return success

def remove_unprintable_chars(text):
    """Replace unprintable characters with '?'"""
    return str(''.join(ascii.isprint(c) and c or '?' for c in text))

def terminate(in_handle):
    """Helper function for terminating a process and all its children"""
    try:
        process = psutil.Process(in_handle.pid)
    except (psutil.NoSuchProcess, ProcessLookupError):
        return
    for proc in process.children(recursive=True):
        proc.terminate()
    process.terminate()

def timeout(connection_type, in_handle):
    """Timeout event"""
    if connection_type == CONNECTION_PROCESS:
        terminate(in_handle)


# Watch the output from the items being run
# looking for INTERESTING things.
def watch_items(in_handle, connection_type, results: TestResults,
                guard_time_seconds, inactivity_time_seconds,
                terminator, reporter):
    '''Watch output'''
    return_value = -1
    start_time = time()
    last_activity_time = time()

    U_LOG.info("watching output until run completes...")

    # Start a thread to read lines from in_handle
    # This is done in a separate thread as it can block or
    # hang; this way we get to detect that and time out.
    read_queue = queue.Queue()
    readline_thread = threading.Thread(target=readline_and_queue,
                                       args=(results, read_queue, in_handle,
                                             connection_type, terminator,
                                             reporter))
    readline_thread.start()

    try:
        while not results.finished:

            # Check for timeouts
            if guard_time_seconds and (time() - start_time >= guard_time_seconds):
                msg = f"guard timer ({guard_time_seconds} second(s)) expired."
                record_outcome(results, "ERROR", reporter, msg)
                timeout(connection_type, in_handle)
                break
            if inactivity_time_seconds and (time() - last_activity_time >= inactivity_time_seconds):
                msg = f"inactivity timer ({inactivity_time_seconds} second(s)) expired."
                record_outcome(results, "ERROR", reporter, msg)
                timeout(connection_type, in_handle)
                break

            try:
                line = read_queue.get(timeout=0.5)
                last_activity_time = time()
                U_LOG.info(line)
                if results.current:
                    timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]
                    results.current.stdout += timestamp + " " + remove_unprintable_chars(line) + "\n"
                for entry in INTERESTING:
                    match = re.match(entry[0], line)
                    if match:
                        entry[1](match, entry[2], results, reporter)
            except queue.Empty:
                pass
            # Let others in
            sleep(0.01)
        # Set this to stop the read thread
        results.finished = True
        readline_thread.join()
        return_value = results.items_failed + results.reboots + results.errors
    except (serial.SerialException, EOFError) as ex:
        U_LOG.error("{} while accessing port {}: {}.".
                       format(type(ex).__name__,
                              in_handle.name, str(ex)))
    finally:
        results.finished = True

    return return_value

# Add a callback function, which will be called when
# regex_string is matched and will get the outcome of
# the regex match as a parameter, followed by the
# user parameter and then the test results (which may
# be empty at this point) and the reporter
def callback(function, regex_string, user_parameter=None):
    '''Add a callback to be called when regex_string is matched'''
    INTERESTING.append([regex_string, function, user_parameter])

def main(connection_handle, connection_type, guard_time_seconds,
         inactivity_time_seconds, terminator, instance,
         reporter, test_report_path=None, send_string=None):
    '''Main as a function'''
    # Dictionary in which results are stored
    results =  TestResults()
    return_value = -1

    if instance:
        prompt = PROMPT + "_" + u_utils.get_instance_text(instance)
    else:
        prompt = PROMPT

    # "global" should be avoided, but we make an exception for the logger
    global U_LOG # pylint: disable=global-statement
    U_LOG = ULog.get_logger(prompt)

    # Make sure we delete any old test report before we start
    if test_report_path and os.path.exists(test_report_path):
        os.remove(test_report_path)

    try:
        # If we have a serial interface we have can chose which tests to run
        # (at least, on the ESP32 platform) else the lot will just run
        if (send_string is None or esp32_send_first(send_string, connection_handle,
                                                    connection_type)):
            results.overall_start_time = datetime.now()
            return_value = watch_items(connection_handle, connection_type, results,
                                       guard_time_seconds, inactivity_time_seconds,
                                       terminator, reporter)
    finally:
        # Always try to write the report
        if test_report_path:
            U_LOG.info("writing report file...")
            if instance:
                instance_str = u_utils.get_instance_text(instance)
            else:
                instance_str = "Unknown"
            # JUnit separates packages using ".".
            # Since we want to present each instance as one test suite we therefor
            # replace all "." with "_"
            instance_str = instance_str.replace(".", "_")

            ts_el = etree.Element("testsuite", name=f"instance {instance_str}",
                                  tests=str(results.items_run),
                                  failures=str(results.items_failed),
                                  errors=str(results.errors))
            for tc in results.test_cases:
                tc_el = etree.SubElement(ts_el, "testcase", classname=f"ubxlib.instance_{instance_str}",
                                         name=tc.name, time=str(tc.duration), status=tc.status)
                if tc.status == "FAIL":
                    etree.SubElement(tc_el, "failure", message=tc.message)
                elif tc.status == "ERROR":
                    etree.SubElement(tc_el, "error", message=tc.message)
                # Dump stdout
                etree.SubElement(tc_el, "system-out").text = tc.stdout

            tree = etree.ElementTree(ts_el)
            with open(test_report_path, "wb") as file:
                # Now write the XML tree to file
                tree.write(file, encoding='utf-8', pretty_print=True)

    U_LOG.info(f"end with return value {return_value}.")

    return return_value

if __name__ == "__main__":
    SUCCESS = True
    PROCESS_HANDLE = None
    RETURN_VALUE = -1
    LOG_HANDLE = None
    TEST_REPORT_PATH = None
    CONNECTION_TYPE = CONNECTION_NONE

    PARSER = argparse.ArgumentParser(description="A script to"    \
                                     " run tests/examples and"    \
                                     " detect the outcome, communicating" \
                                     " with the target running"   \
                                     " the build.  Return value"  \
                                     " is zero on success"        \
                                     " negative on unable to run" \
                                     " positive indicating number"\
                                     " of failed test cases")
    PARSER.add_argument("port", help="the source of data: either the"   \
                        " COM port on which the build is communicating," \
                        " e.g. COM1 (baud rate fixed at 115,200)" \
                        " or a port number (in which case a Telnet" \
                        " session is opened on localhost to grab the" \
                        " output) or an executable to run which should" \
                        " spew  the output from the unit test.")
    PARSER.add_argument("-s", help="send the given string to the"
                        " target before starting monitoring.  This"
                        " is only supported on the ESP32 platform"
                        " where the interface is a serial port; it"
                        " can be used to filter which items are"
                        " to be run.")
    PARSER.add_argument("-t", type=int, help="set a guard timer in seconds; if"
                        " the guard time expires this script will"
                        " stop and return a negative value.")
    PARSER.add_argument("-i", type=int, help="set an inactivity timer in seconds;"
                        " if the target emits nothing for this many"
                        " seconds this script will stop and return"
                        " a negative value.")
    PARSER.add_argument("-l", help="the file name to write the output to;" \
                        " any existing file will be overwritten.")
    PARSER.add_argument("-x", help="the file name to write an XML-format" \
                        " report to; any existing file will be overwritten.")
    ARGS = PARSER.parse_args()

    # The following line works around weird encoding problems where Python
    # doesn't like code page 65001 character encoding which Windows does
    # See https://stackoverflow.com/questions/878972/windows-cmd-encoding-change-causes-python-crash
    codecs.register(lambda name: codecs.lookup('utf-8') if name == 'cp65001' else None)

    # Open the log file
    if ARGS.l:
        ULog.setup_logging(debug_file=ARGS.l)
        print(f'writing log output to "{ARGS.l}".')
    if SUCCESS:
        U_LOG = ULog.get_logger(PROMPT)
        # Make the connection
        CONNECTION_TYPE = CONNECTION_SERIAL
        CONNECTION_HANDLE = u_utils.open_serial(ARGS.port, 115200, U_LOG)
        if CONNECTION_HANDLE is None:
            CONNECTION_TYPE = CONNECTION_TELNET
            CONNECTION_HANDLE = u_utils.open_telnet(ARGS.port, U_LOG)
        if CONNECTION_HANDLE is None:
            CONNECTION_TYPE = CONNECTION_PROCESS
            CONNECTION_HANDLE = start_exe(ARGS.port)
        if CONNECTION_HANDLE is not None:
            # Open the report file
            if ARGS.x:
                TEST_REPORT_PATH = ARGS.x
                print(f'writing test report to "{ARGS.x}".')
            if SUCCESS:
                # Run things
                RETURN_VALUE = main(CONNECTION_HANDLE, CONNECTION_TYPE, ARGS.t,
                                    ARGS.i, "\n", None, None,
                                    TEST_REPORT_PATH, send_string=ARGS.s)

            # Tidy up
            if CONNECTION_TYPE == CONNECTION_PROCESS:
                terminate(CONNECTION_HANDLE)
            else:
                CONNECTION_HANDLE.close()

    sys.exit(RETURN_VALUE)
