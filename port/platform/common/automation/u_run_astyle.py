#!/usr/bin/env python

'''Check-only  run of AStyle and on ubxlib and report results.'''

import os          # For sep
import subprocess
import u_report
import u_utils
import u_settings

# Prefix to put at the start of all prints
PROMPT = "u_run_astyle_"

# The name of the AStyle configuration file to look for
# in the root of the ubxlib directory
CONFIG_FILE = u_settings.ASTYLE_CONFIG_FILE

# File extensions to include
ASTYLE_FILE_EXTENSIONS = u_settings.ASTYLE_FILE_EXTENSIONS

# Directory names to include; only the directories off the ubxlib
# root need be included, AStyle will recurse below each of these
ASTYLE_DIRS = u_settings.ASTYLE_DIRS

# Directory names to exclude (exclusion is done from
# the end of the file path backwards, so "build" excludes
# "blah\build" as well as "build" but not "build\blah")
EXCLUDE_DIRS = u_settings.ASTYLE_EXCLUDE_DIRS

def run(instance, ubxlib_dir, working_dir, printer, reporter):
    '''Run AStyle'''
    return_value = 1
    got_astyle = False
    call_list = []
    instance_text = u_utils.get_instance_text(instance)

    prompt = PROMPT + instance_text + ": "

    # Print out what we've been told to do
    text = "running AStyle from ubxlib directory \"" + ubxlib_dir +  \
           "\" using configuration file \"" + ubxlib_dir + os.sep +  \
           CONFIG_FILE + "\""
    if working_dir:
        text += ", working directory \"" + working_dir + "\""
    printer.string("{}{}.".format(prompt, text))

    reporter.event(u_report.EVENT_TYPE_CHECK,
                   u_report.EVENT_START,
                   "AStyle")
    got_astyle = u_utils.exe_where("astyle", \
                        "ERROR: can't find AStyle, please make"      \
                        " sure that it is installed and on the path.", \
                        printer, prompt)
    if got_astyle:
        # Run AStyle
        printer.string("{}CD to {}...".format(prompt, ubxlib_dir))
        with u_utils.ChangeDir(ubxlib_dir):
            # Assemble the call list
            call_list.append("astyle")
            call_list.append("--options=" + CONFIG_FILE) # Options file
            call_list.append("--dry-run") # Don't make changes
            call_list.append("--formatted") # Only list changed files
            call_list.append("--suffix=none") # Don't leave .orig files everywhere
            call_list.append("--verbose") # Print out stats
            for exclude_dir in EXCLUDE_DIRS:  # Exclude these directories
                call_list.append("--exclude=" + exclude_dir)
            call_list.append("--ignore-exclude-errors-x") # Ignore unfound excludes
            call_list.append("--recursive") # Recurse through...
            for include_dir in ASTYLE_DIRS:  # ...these files
                call_list.append(include_dir + os.sep + ASTYLE_FILE_EXTENSIONS)

            # Print what we're gonna do
            tmp = ""
            for item in call_list:
                tmp += " " + item
            printer.string("{}in directory {} calling{}".         \
                           format(prompt, os.getcwd(), tmp))
            try:
                popen_keywords = {
                    'stderr': subprocess.STDOUT,
                    'shell': True # Stop Jenkins hanging
                }
                text = subprocess.check_output(u_utils.subprocess_osify(call_list),
                                               **popen_keywords)
                formatted = []
                for line in text.splitlines():
                    line = line.decode(encoding="utf-8", errors="ignore")
                    printer.string("{}{}".format(prompt, line))
                    # AStyle doesn't return anything other than 0,
                    # need to look for the word "Formatted" to find
                    # a file it has fiddled with
                    if line.startswith("Formatted"):
                        formatted.append(line)
                if not formatted:
                    reporter.event(u_report.EVENT_TYPE_CHECK,
                                   u_report.EVENT_PASSED)
                else:
                    reporter.event(u_report.EVENT_TYPE_CHECK,
                                   u_report.EVENT_WARNING)
                    for line in formatted:
                        reporter.event_extra_information(line)
                # We don't return any errors about formatting
                return_value = 0
            except subprocess.CalledProcessError as error:
                reporter.event(u_report.EVENT_TYPE_CHECK,
                               u_report.EVENT_FAILED)
                printer.string("{}AStyle returned error {}:".
                               format(prompt, error.returncode))
                for line in error.output.splitlines():
                    line = line.strip()
                    if line:
                        reporter.event_extra_information(line)
                        printer.string("{}{}".format(prompt, line))
    else:
        reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                       u_report.EVENT_FAILED,
                       "there is a problem with the AStyle installation")

    return return_value
