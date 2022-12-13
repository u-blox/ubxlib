#!/usr/bin/env python

'''Check if any public API files have been left out of ubxlib.h.'''

import os          # For join
from logging import Logger
from scripts import u_report
from scripts.u_logging import ULog

# Prefix to put at the start of all prints
PROMPT = "u_run_check_ubxlib_h"

# The logger
U_LOG: Logger = None

# The directory containing the excludes file, relative to the ubxlib root
EXCLUDES_FILE_DIR = "port/platform/common/automation"

# The name of the excludes file
EXCLUDES_FILE = "ubxlib_h_excludes.txt"

def run(ubxlib_dir, reporter):
    '''Run the ubxlib.h checker'''
    return_value = 1
    excludes_file_path = os.path.join(ubxlib_dir, EXCLUDES_FILE_DIR, EXCLUDES_FILE)
    ubxlib_h_file_path = os.path.join(ubxlib_dir, "ubxlib.h")
    excludes = []
    ubxlib_headers = []
    missing_headers = []

    # "global" should be avoided, but we make an exception for the logger
    global U_LOG # pylint: disable=global-statement
    U_LOG = ULog.get_logger(PROMPT)

    # Print out what we've been told to do
    text = "running ubxlib.h checker with ubxlib directory \"" + ubxlib_dir +  \
           "\" using excludes file \"" + excludes_file_path + "\""
    U_LOG.info(text)

    reporter.event(u_report.EVENT_TYPE_CHECK,
                   u_report.EVENT_START,
                   "ubxlib.h")

    # Read in ubxlib.h and assemble the list of header files in it
    U_LOG.info("%s contains headers:", ubxlib_h_file_path)
    temp_list = [line.strip() for line in open(ubxlib_h_file_path, 'r', encoding='utf8')]
    for item in temp_list:
        header = None
        if item and item.startswith("#") and "include" in item:
            # Allow <> or ""
            bits = item.split("\"")
            if len(bits) == 3:
                header = bits[1]
            else:
                bits = item.split("<")
                if len(bits) == 2:
                    bits = bits[1].split(">")
                    if len(bits) == 2:
                        header = bits[0]
        if header:
            ubxlib_headers.append(header)
            U_LOG.info("%s", header)
    U_LOG.info("%d header(s) found in ubxlib.h.", len(ubxlib_headers))


    # Read in the excludes
    U_LOG.info("%s contains excludes:", excludes_file_path)
    temp_list = [line.strip() for line in open(excludes_file_path, 'r', encoding='utf8')]
    for item in temp_list:
        # Throw away comment lines
        if item and not item.startswith("#"):
            excludes.append(item)
            U_LOG.info("%s",  item)
    U_LOG.info("%d exclude(s) found in %s.", len(excludes), EXCLUDES_FILE)

    # Find the paths of all .h files in API directories and check
    # for missing ones
    U_LOG.info("recursing %s looking for headers in api directories:", ubxlib_dir)
    for root, _, files in os.walk(ubxlib_dir):
        for file_name in files:
            if file_name.endswith(".h") and "api" in root.lower():
                item = os.path.join(root, file_name)
                # Found a header in an API directory, check if it is
                # already in ubxlib.h
                if file_name in ubxlib_headers:
                    U_LOG.info("%s: already in ubxlib.h.", item)
                else:
                    # Check if it is excluded
                    if file_name in excludes:
                        U_LOG.info("%s: excluded by %s.", item, EXCLUDES_FILE)
                    else:
                        # Flag an error
                        missing_headers.append(item)
                        U_LOG.error("%s: NOT in ubxlib.h and NOT excluded; please" \
                                    " add it to ubxlib.h or %s.", item, EXCLUDES_FILE)

    if not missing_headers:
        reporter.event(u_report.EVENT_TYPE_CHECK,
                       u_report.EVENT_PASSED)
        return_value = 0
    else:
        return_value = len(missing_headers)
        reporter.event(u_report.EVENT_TYPE_CHECK,
                       u_report.EVENT_FAILED)
        U_LOG.error("%s API header(s) found that are not in \"ubxlib.h\" or excluded.", return_value)

    return return_value
