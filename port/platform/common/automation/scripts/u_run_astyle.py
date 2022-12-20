#!/usr/bin/env python

'''Check-only  run of AStyle and on ubxlib and report results.'''

import os          # For sep
import subprocess
from logging import Logger
from scripts import u_report, u_utils, u_settings
from scripts.u_logging import ULog

# Prefix to put at the start of all prints
PROMPT = "u_run_astyle"

# The logger
U_LOG: Logger = None

# The name of the AStyle configuration file to look for
# in the root of the ubxlib directory
CONFIG_FILE = u_settings.ASTYLE_CONFIG_FILE

# File extensions to include
ASTYLE_FILE_EXTENSIONS = u_settings.ASTYLE_FILE_EXTENSIONS

# Directory names to include; only the directories off the ubxlib
# root need be included, AStyle will recurse below each of these
ASTYLE_DIRS = u_settings.ASTYLE_DIRS

def run(ubxlib_dir, reporter):
    '''Run AStyle'''
    return_value = 1
    got_astyle = False
    call_list = []

    # "global" should be avoided, but we make an exception for the logger
    global U_LOG # pylint: disable=global-statement
    U_LOG = ULog.get_logger(PROMPT)

    # Print out what we've been told to do
    text = "running AStyle from ubxlib directory \"" + ubxlib_dir +  \
           "\" using configuration file \"" + ubxlib_dir + os.sep +  \
           CONFIG_FILE + "\""
    U_LOG.info(text)

    reporter.event(u_report.EVENT_TYPE_CHECK,
                   u_report.EVENT_START,
                   "AStyle")
    got_astyle = u_utils.exe_where("astyle", \
                        "ERROR: can't find AStyle, please make"      \
                        " sure that it is installed and on the path.", \
                        logger=U_LOG)
    if got_astyle:
        # Run AStyle
        U_LOG.info(f"CD to {ubxlib_dir}...")
        with u_utils.ChangeDir(ubxlib_dir):
            # Assemble the call list
            if u_utils.is_linux():
                call_list.append("astyle")
            else:
                call_list.append("astyle.exe")
            call_list.append("--options=" + CONFIG_FILE) # Options file
            call_list.append("--dry-run") # Don't make changes
            call_list.append("--formatted") # Only list changed files
            call_list.append("--suffix=none") # Don't leave .orig files everywhere
            call_list.append("--verbose") # Print out stats
            call_list.append("--recursive") # Recurse through...
            for include_dir in ASTYLE_DIRS:  # ...these files
                call_list.append(include_dir + os.sep + ASTYLE_FILE_EXTENSIONS)

            # Print what we're gonna do
            tmp = ""
            for item in call_list:
                tmp += " " + item
            U_LOG.info(f"in directory {os.getcwd()} calling{tmp}")
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
                    U_LOG.info(line)
                    # AStyle doesn't return anything other than 0,
                    # need to look for the word "Formatted" to find
                    # a file it has fiddled with
                    if line.startswith("Formatted"):
                        formatted.append(line)
                if not formatted:
                    reporter.event(u_report.EVENT_TYPE_CHECK,
                                   u_report.EVENT_PASSED)
                    return_value = 0
                else:
                    reporter.event(u_report.EVENT_TYPE_CHECK,
                                   u_report.EVENT_WARNING)
                    for line in formatted:
                        reporter.event_extra_information(line)
            except subprocess.CalledProcessError as error:
                reporter.event(u_report.EVENT_TYPE_CHECK,
                               u_report.EVENT_FAILED)
                U_LOG.error(f"AStyle returned error {error.returncode}:")
                for line in error.output.splitlines():
                    line = line.strip()
                    if line:
                        reporter.event_extra_information(line)
                        U_LOG.error(line)
    else:
        reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                       u_report.EVENT_FAILED,
                       "there is a problem with the AStyle installation")

    return return_value
