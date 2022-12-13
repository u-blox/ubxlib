#!/usr/bin/env python

'''Check if any core part of ubxlib calls malloc() or free().'''

import os          # For join
import re
from logging import Logger
from scripts import u_report
from scripts.u_logging import ULog
from scripts import u_utils

# Prefix to put at the start of all prints
PROMPT = "u_run_check_malloc"

# The logger
U_LOG: Logger = None

# The directory containing the excludes file, relative to the ubxlib root
EXCLUDES_FILE_DIR = "port/platform/common/automation"

# The name of the excludes file
EXCLUDES_FILE = "malloc_excludes.txt"

def run(ubxlib_dir, reporter):
    '''Run the malloc checker'''
    return_value = 1
    excludes_file_path = os.path.join(ubxlib_dir, EXCLUDES_FILE_DIR, EXCLUDES_FILE)
    excludes = []
    file_paths = []
    naughty_files = []

    # "global" should be avoided, but we make an exception for the logger
    global U_LOG # pylint: disable=global-statement
    U_LOG = ULog.get_logger(PROMPT)

    # Print out what we've been told to do
    text = "running malloc checker with ubxlib directory \"" + ubxlib_dir +  \
           "\" using excludes file \"" + excludes_file_path + "\""
    U_LOG.info(text)

    reporter.event(u_report.EVENT_TYPE_CHECK,
                   u_report.EVENT_START,
                   "malloc")

    # Read in the excludes
    U_LOG.info("%s contains exclude paths:", excludes_file_path)
    temp_list = [line.strip() for line in open(excludes_file_path, 'r', encoding='utf8')]
    for item in temp_list:
        # Throw away comment lines
        if item and not item.startswith("#"):
            excludes.append(item)
            U_LOG.info("%s",  item)
    U_LOG.info("%d exclude path(s) found in %s", len(excludes), EXCLUDES_FILE)

    # Find the paths of all .c/.cpp/.h//hpp files in ubxlib except
    # those that are excluded
    U_LOG.info("recursing %s looking for .c/.cpp/.h/.hpp:", ubxlib_dir)
    with u_utils.ChangeDir(ubxlib_dir):
        for root, _, files in os.walk("."):
            for file_name in files:
                if file_name.endswith(".c") or file_name.endswith(".cpp") or \
                   file_name.endswith(".h")  or file_name.endswith(".hpp"):
                    item = os.path.join(root, file_name)
                    for exclude in excludes:
                        if item.startswith(os.path.join(".", exclude.replace("/", os.sep))):
                            U_LOG.info("%s: excluded by %s in %s", item, exclude, EXCLUDES_FILE)
                            item = None
                            break
                    if item:
                        file_paths.append(item)

        # Parse all of the files we now have checking for malloc()/free() calls
        for target_file in file_paths:
            with open(target_file, "r", encoding="utf8") as file:
                # Read the lot in
                U_LOG.info("checking file %s...", target_file)
                line_list = file.readlines()
            if line_list:
                for idx, line in enumerate(line_list):
                    # Regex tested using https://regex101.com/
                    if re.match(r".*[^\w]malloc\s*\(.+\)|.*[^\w]free\s*\(.+\)", line):
                        naughty_files.append(target_file)
                        U_LOG.error("%s:%d appears to contain a call to malloc() or free();"     \
                                    " please use pUPortMalloc() or uPortFree() instead or, if"    \
                                    " this is in a comment remove the (); or add the file path"   \
                                    " to %s", target_file, idx + 1, EXCLUDES_FILE)

    if not naughty_files:
        reporter.event(u_report.EVENT_TYPE_CHECK,
                       u_report.EVENT_PASSED)
        return_value = 0
    else:
        return_value = len(naughty_files)
        reporter.event(u_report.EVENT_TYPE_CHECK,
                       u_report.EVENT_FAILED)
        U_LOG.error("%s file(s) found that appear to call malloc() or free() and" \
                    " are not excluded.", return_value)

    return return_value
