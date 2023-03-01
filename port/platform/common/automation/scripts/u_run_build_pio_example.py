#!/usr/bin/env python

'''Build the examples specific to PlatformIO'''

import os           # For sep(), getcwd()
from logging import Logger
from scripts import u_report, u_utils, u_settings
from scripts.u_logging import ULog

# Prefix to put at the start of all prints
PROMPT = "u_run_build_pio_example"

# The logger
U_LOG: Logger = None

# The path to build.py directory (off ubxlib root)
BUILD_SCRIPT_PATH = "port/platform/platformio/build/build.py"

# The path to the PIO executable
PIO_PATH = os.path.join(os.path.expanduser("~"), ".platformio", "penv")
if u_utils.is_linux():
    PIO_PATH = os.path.join(PIO_PATH, "bin", "pio")
else:
    PIO_PATH = os.path.join(PIO_PATH, "Scripts", "pio")

# Note: all the work is done by the build.py
# script down in port/platform/platformio/build,
# all we do here is wrap it in order to shoot the
# output into the usual streams for automation
def run(ubxlib_dir, reporter):
    '''Build the PlatformIO examples'''
    return_value = -1

    # "global" should be avoided, but we make an exception for the logger
    global U_LOG # pylint: disable=global-statement
    U_LOG = ULog.get_logger(PROMPT)

    # Print out what we've been told to do
    text = f"build the PlatformIO examples from ubxlib directory {ubxlib_dir}"
    U_LOG.info(text)

    reporter.event(u_report.EVENT_TYPE_BUILD,
                   u_report.EVENT_START,
                   "BuildPioExample")

    # Assemble the call list
    call_list = [
        "python",
        ubxlib_dir + os.sep + BUILD_SCRIPT_PATH,
        "-p " + PIO_PATH,
        "-u " + ubxlib_dir
    ]

    # Set shell to keep Jenkins happy
    if u_utils.exe_run(call_list, 0, logger=U_LOG, shell_cmd=True):
        return_value = 0
        reporter.event(u_report.EVENT_TYPE_BUILD,
                       u_report.EVENT_COMPLETE)
    else:
        reporter.event(u_report.EVENT_TYPE_BUILD,
                       u_report.EVENT_FAILED,
                       "check debug log for details")

    return return_value
