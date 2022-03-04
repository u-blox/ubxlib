#!/usr/bin/env python

'''Perform a Pylint check on these Python scripts.'''

import os              # For sep, listdir, isfile, join
import subprocess
from time import sleep
from logging import Logger
from scripts import u_report, u_utils
from scripts.u_logging import ULog

# Prefix to put at the start of all prints
PROMPT = "u_run_pylint"

# The logger
U_LOG: Logger = None

# Paths to look for .py files in, starting at ubxlib_dir
PYTHON_PATHS = [ u_utils.AUTOMATION_DIR + "/scripts" ]

# The minimum pylint rating to require
MIN_RATING = 9

# Disabled PyLint messages
DISABLED = {
    # These are warnings about using modern format strings
    # for logging events. The only reason I can figure out
    # why PyLint complains about it is due to some performance
    # optimization as described here:
    # https://docs.python.org/3/howto/logging.html#optimization
    # Since all the log events we generate will be processed
    # we will see no gain of using the now deprecated formatter
    # used in the logging module.
    "logging-format-interpolation",
    "logging-fstring-interpolation"
}

# Python modules that PyLint should ignore
IGNORED_MODULES = {
    "scripts.u_settings"
}

def run(ubxlib_dir, reporter):
    '''Run Pylint'''
    return_value = 1
    got_pylint = False
    min_rating = 10

    # "global" should be avoided, but we make an exception for the logger
    global U_LOG # pylint: disable=global-statement
    U_LOG = ULog.get_logger(PROMPT)

    # Print out what we've been told to do
    text = "running Pylint from ubxlib directory \"" + ubxlib_dir + "\""
    text += ", checking for minimum rating " + str(MIN_RATING)
    U_LOG.info(text)

    reporter.event(u_report.EVENT_TYPE_CHECK,
                   u_report.EVENT_START,
                   "Pylint")
    got_pylint = u_utils.exe_where("pylint", \
                        "ERROR: can't find pylint, please make"     \
                        " sure that it is installed and on the path.", \
                        logger=U_LOG)

    if got_pylint:
        # Run Pylint on all the .py files in PYTHON_PATHS
        return_value = 0
        for py_path in PYTHON_PATHS:
            if os.path.exists(py_path):
                U_LOG.info(f"CD to {py_path}...")
                with u_utils.ChangeDir(py_path):
                    popen_keywords = {
                        'stderr': subprocess.STDOUT,
                        'shell': True # Stop Jenkins hanging
                    }
                    for py_file in os.listdir(py_path):
                        if py_file.endswith(".py") and not py_file.startswith("__"):
                            U_LOG.info(f"running Pylint on {py_file}...")
                            got_rating = False
                            try:
                                # ignore u_settings module as it sets members programatically and
                                # will thus generate a bunch of lint warnings
                                cmd = u_utils.subprocess_osify(["pylint",
                                                                "--exit-zero",
                                                                f'--disable={",".join(DISABLED)}',
                                                                f'--ignored-modules={",".join(IGNORED_MODULES)}',
                                                                py_file])
                                text = subprocess.check_output(cmd, **popen_keywords)

                                rating = 0
                                for line in text.splitlines():
                                    line = line.decode()
                                    U_LOG.info(line)
                                    # See if there is a rating in this line
                                    outcome = line.rpartition("Your code has been rated at ")
                                    if len(outcome) == 3:
                                        outcome = outcome[2].split("/")
                                        # Get the bit before the "/" in the "x/y" rating
                                        try:
                                            rating = float(outcome[0])
                                            got_rating = True
                                            if rating < min_rating:
                                                min_rating = rating
                                            if rating < MIN_RATING:
                                                return_value += 1
                                        except ValueError:
                                            # Can't have been a rating line
                                            pass
                                # Let other things in
                                sleep(0.01)
                                if got_rating:
                                    if rating < MIN_RATING:
                                        reporter.event(u_report.EVENT_TYPE_CHECK,
                                                       u_report.EVENT_ERROR,
                                                       "rating {} < minimum ({})".  \
                                                       format(rating, MIN_RATING))
                                        for line in text.splitlines():
                                            line = line.strip().decode()
                                            if line:
                                                reporter.event_extra_information(line)
                                else:
                                    reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                                                   u_report.EVENT_FAILED,
                                                   "Pylint returned no rating for file \"{}\"". \
                                                   format(py_file))
                                    U_LOG.info("Pylint returned no rating.")
                                    # No rating returned, flag an error
                                    return_value += 1
                            except subprocess.CalledProcessError as error:
                                reporter.event(u_report.EVENT_TYPE_CHECK,
                                               u_report.EVENT_FAILED)
                                U_LOG.error(f"Pylint returned error {error.returncode}:")
                                for line in error.output.splitlines():
                                    line = line.strip().decode()
                                    if line:
                                        reporter.event_extra_information(line)
                                        U_LOG.error(line)
            else:
                # Flag an error if a path doesn't exist
                err_msg = f"path {py_path} does not exit"
                reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                               u_report.EVENT_FAILED,
                               err_msg)
                U_LOG.error(err_msg)
                return_value += 1

        U_LOG.info(f"Pylint check complete, minimum rating {min_rating}.")
        if min_rating < MIN_RATING:
            reporter.event(u_report.EVENT_TYPE_CHECK,
                           u_report.EVENT_FAILED)
        else:
            reporter.event(u_report.EVENT_TYPE_CHECK,
                           u_report.EVENT_PASSED)

    else:
        reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                       u_report.EVENT_FAILED,
                       "there is a problem with the Pylint installation")

    return return_value
