#!/usr/bin/env python

'''Perform a Doxygen check on ubxlib report results.'''

import os          # For sep
import subprocess
import u_report
import u_utils

# Prefix to put at the start of all prints
PROMPT = "u_run_doxygen_"

# The name of the Doxygen configuration file to use
DOXYFILE = "Doxyfile"

def run(instance, ubxlib_dir, working_dir, printer, reporter):
    '''Run Doxygen'''
    return_value = 1
    got_doxygen = False
    instance_text = u_utils.get_instance_text(instance)

    prompt = PROMPT + instance_text + ": "

    # Print out what we've been told to do
    text = "running Doxygen from ubxlib directory \"" + ubxlib_dir + "\""
    if working_dir:
        text += ", working directory \"" + working_dir + "\""
    printer.string("{}{}.".format(prompt, text))

    reporter.event(u_report.EVENT_TYPE_CHECK,
                   u_report.EVENT_START,
                   "Doxygen")
    try:
        # Check that Doxygen is on the path
        text = subprocess.check_output(["where", "/q", "doxygen"])
        got_doxygen = True
    except subprocess.CalledProcessError:
        printer.string("{}ERROR: can't find Doxygen, please make"     \
                       " sure that it is installed and on the path.". \
                       format(prompt))

    if got_doxygen:
        # Run Doxygen
        config_path = ubxlib_dir + os.sep + DOXYFILE
        printer.string("{}CD to {}...".format(prompt, ubxlib_dir))
        with u_utils.ChangeDir(ubxlib_dir):
            printer.string("{}in directory {} calling doxygen {}.".    \
                           format(prompt, os.getcwd(), config_path))
            try:
                text = subprocess.check_output(["doxygen", config_path],
                                               stderr=subprocess.STDOUT,
                                               shell=True) # Jenkins hangs without this
                reporter.event(u_report.EVENT_TYPE_CHECK,
                               u_report.EVENT_PASSED)
                for line in text.splitlines():
                    printer.string("{}{}".format(prompt, line.decode()))
                return_value = 0
            except subprocess.CalledProcessError as error:
                reporter.event(u_report.EVENT_TYPE_CHECK,
                               u_report.EVENT_FAILED)
                printer.string("{}Doxygen returned error {}:".
                               format(prompt, error.returncode))
                for line in error.output.splitlines():
                    line = line.strip().decode()
                    if line:
                        reporter.event_extra_information(line)
                        printer.string("{}{}".format(prompt, line))
    else:
        reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                       u_report.EVENT_FAILED,
                       "there is a problem with the Doxygen installation")

    return return_value
