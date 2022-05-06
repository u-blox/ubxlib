#!/usr/bin/env python

'''Build the platform independent, non-test, non-example of ubxlib to establish static sizes.'''

import os           # For sep(), getcwd()
from logging import Logger
from scripts import u_report, u_utils, u_settings
from scripts.u_logging import ULog

# Prefix to put at the start of all prints
PROMPT = "u_run_static_size"

# The logger
U_LOG: Logger = None

# Expected bin directory of GCC ARM compiler
# e.g. "C:/Program Files (x86)/GNU Tools ARM Embedded/9 2019-q4-major/bin/"
GNU_INSTALL_ROOT = u_settings.STATIC_SIZE_ARM_GNU_INSTALL_ROOT

# Expected name for compiler
GNU_COMPILER = "arm-none-eabi-gcc"

# Expected name for size
GNU_SIZE = "arm-none-eabi-size"

# STATIC_SIZE directory (off ubxlib root)
MAKEFILE_DIR = "port/platform/static_size"

# Sub-directory used by static_size.py when building
BUILD_SUBDIR = "build"

# Note: all the work is done by the static_size.py
# script down in port/platform/static_size, all we
# do here is configure it as we wish and wrap it
# in order to shoot the output into the usual
# streams for automation
def run(defines, ubxlib_dir, reporter):
    '''Build to check static sizes'''
    return_value = -1
    cflags = ""

    # "global" should be avoided, but we make an exception for the logger
    global U_LOG # pylint: disable=global-statement
    U_LOG = ULog.get_logger(PROMPT)

    # Print out what we've been told to do
    text = "running static size check from ubxlib directory \"" + ubxlib_dir + "\""
    U_LOG.info(text)

    build_dir = os.getcwd() + os.sep + BUILD_SUBDIR

    reporter.event(u_report.EVENT_TYPE_BUILD,
                   u_report.EVENT_START,
                   "StaticSize")

    # Add the #defines to C_FLAGS
    if defines:
        for define in defines:
            cflags +=" -D" + define

    # Assemble the call list
    # Call size on the result
    call_list = [
        "make",
        "-C", ubxlib_dir + os.sep + MAKEFILE_DIR,
        "CC=" + GNU_INSTALL_ROOT + os.sep + GNU_COMPILER,
        "SIZE=" + GNU_INSTALL_ROOT + os.sep + GNU_SIZE,
        "OUTDIR=" + build_dir,
        "CFLAGS=" + cflags,
        "-j8",
        "float_size"
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
