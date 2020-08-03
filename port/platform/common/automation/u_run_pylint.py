#!/usr/bin/env python

'''Perform a Pylint check on these Python scripts.'''

import os              # For sep, listdir, isfile, join
import subprocess
import u_report
import u_utils

# Prefix to put at the start of all prints
PROMPT = "u_run_pylint_"

# Paths to look for .py files in, starting at ubxlib_dir
PYTHON_PATHS = ["port" + os.sep + "platform" + os.sep + "common" +  \
                os.sep + "automation",]

# The minimum pylint rating to require
MIN_RATING = 9

def run(instance, ubxlib_dir, working_dir, printer, reporter):
    '''Run Pylint'''
    return_value = 1
    got_pylint = False
    min_rating = 10
    instance_text = u_utils.get_instance_text(instance)

    prompt = PROMPT + instance_text + ": "

    # Print out what we've been told to do
    text = "running Pylint from ubxlib directory \"" + ubxlib_dir + "\""
    if working_dir:
        text += ", working directory \"" + working_dir + "\""
    text += ", checking for minimum rating " + str(MIN_RATING)
    printer.string("{}{}.".format(prompt, text))

    reporter.event(u_report.EVENT_TYPE_CHECK,
                   u_report.EVENT_START,
                   "Pylint")
    try:
        # Check that Pylint is on the path
        text = subprocess.check_output(["where", "/q", "pylint"])
        got_pylint = True
    except subprocess.CalledProcessError:
        printer.string("{}ERROR: can't find Pylint, please make"     \
                       " sure that it is installed and on the path.". \
                       format(prompt))

    if got_pylint:
        # Run Pylint on all the .py files in PYTHON_PATHS
        return_value = 0
        for py_path in PYTHON_PATHS:
            abs_py_path = ubxlib_dir + os.sep + py_path
            if os.path.exists(abs_py_path):
                printer.string("{}CD to {}...".format(prompt, abs_py_path))
                with u_utils.ChangeDir(abs_py_path):
                    for py_file in os.listdir(abs_py_path):
                        if py_file.endswith(".py"):
                            printer.string("{}running Pylint on {}...".format(prompt, py_file))
                            got_rating = False
                            try:
                                text = subprocess.check_output(["pylint", "--exit-zero",
                                                                py_file],
                                                               stderr=subprocess.STDOUT,
                                                               shell=True) # Stop Jenkins hanging
                                rating = 0
                                for line in text.splitlines():
                                    printer.string("{}{}".format(prompt, line))
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
                                if got_rating:
                                    if rating < MIN_RATING:
                                        reporter.event(u_report.EVENT_TYPE_CHECK,
                                                       u_report.EVENT_ERROR,
                                                       "rating {} < minimum ({})".  \
                                                       format(rating, MIN_RATING))
                                        for line in text.splitlines():
                                            line = line.strip()
                                            if line:
                                                reporter.event_extra_information(line)
                                else:
                                    reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                                                   u_report.EVENT_FAILED,
                                                   "Pylint returned no rating for file \"{}\"". \
                                                   format(py_file))
                                    printer.string("{}Pylint returned no rating.". \
                                                   format(prompt))
                                    # No rating returned, flag an error
                                    return_value += 1
                            except subprocess.CalledProcessError as error:
                                reporter.event(u_report.EVENT_TYPE_CHECK,
                                               u_report.EVENT_FAILED)
                                printer.string("{}Pylint returned error {}:".
                                               format(prompt, error.returncode))
                                for line in error.output.splitlines():
                                    line = line.strip()
                                    if line:
                                        reporter.event_extra_information(line)
                                        printer.string("{}{}".format(prompt, line))
            else:
                # Flag an error if a path doesn't exist
                reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                               u_report.EVENT_FAILED,
                               "path {} does not exit".format(abs_py_path))
                printer.string("{}path \"{}\" does not exist.".   \
                               format(prompt, abs_py_path))
                return_value += 1

        printer.string("{}Pylint check complete, minimum rating {}.".
                       format(prompt, min_rating))
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
