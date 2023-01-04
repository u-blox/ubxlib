#!/usr/bin/env python

'''Check that no float is supported.'''

import os           # For sep(), getcwd()
from logging import Logger
from scripts import u_report, u_utils, u_settings
from scripts.u_logging import ULog

# Prefix to put at the start of all prints
PROMPT = "u_run_no_float"

# The logger
U_LOG: Logger = None

# Expected bin directory of GCC ARM compiler
# e.g. "C:/Program Files (x86)/GNU Tools ARM Embedded/9 2019-q4-major/bin/"
GNU_INSTALL_ROOT = u_settings.STATIC_SIZE_ARM_GNU_INSTALL_ROOT

# Expected name for compiler
GNU_COMPILER = "arm-none-eabi-gcc"

# Expected name for size
GNU_SIZE = "arm-none-eabi-size"

# The name of the map file from the build
MAP_FILE_NAME = "no_float/static_size_no_float.map"

# STATIC_SIZE directory (off ubxlib root)
MAKEFILE_DIR = "port/platform/static_size"

# Sub-directory used by static_size.py when building
BUILD_SUBDIR = "build"

# A list of functions that indicate floating point is in use
FLOAT_FUNCTIONS = ["__adddf3",
                   "__aeabi_cdc",
                   "__aeabi_cfc",
                   "__aeabi_cfr",
                   "__aeabi_d2",
                   "__aeabi_dadd",
                   "__aeabi_dcmp",
                   "__aeabi_ddiv",
                   "__aeabi_dmul",
                   "__aeabi_drsub",
                   "__aeabi_dsub",
                   "__aeabi_f2d",
                   "__aeabi_fcmp",
                   "__aeabi_i2d",
                   "__aeabi_l2d",
                   "__aeabi_ui2d",
                   "__aeabi_ul2d",
                   "__any_on",
                   "__cmpdf2",
                   "__cmpsf2",
                   "__divdf3",
                   "__eqdf2",
                   "__eqsf2",
                   "__extendsfdf2",
                   "__fixdfdi",
                   "__fixdfsi",
                   "__fixunsdfdi",
                   "__fixunsdfsi",
                   "__float",
                   "__gedf2",
                   "__gesf2",
                   "__gtdf2",
                   "__gtsf2",
                   "__ledf2",
                   "__lesf2",
                   "__ltdf2",
                   "__ltsf2",
                   "__muldf3",
                   "__multadd",
                   "__multiply",
                   "__nedf2",
                   "__nesf2",
                   "__pow5mult"]

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
    map_file_path = build_dir + os.sep + MAP_FILE_NAME

    reporter.event(u_report.EVENT_TYPE_BUILD,
                   u_report.EVENT_START,
                   "NoFloat")

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
        "no_float_size"
    ]
    # Set shell to keep Jenkins happy
    if u_utils.exe_run(call_list, 0, logger=U_LOG, shell_cmd=True):
        reporter.event(u_report.EVENT_TYPE_BUILD,
                       u_report.EVENT_COMPLETE)
        reporter.event(u_report.EVENT_TYPE_TEST,
                       u_report.EVENT_START)
        # Having performed the build, open the .map file
        U_LOG.info(f"opening map file {map_file_path}...")
        if os.path.exists(map_file_path):
            map_file = open(map_file_path, "r", encoding="utf8")
            if map_file:
                # Parse the cross-reference section to seek
                # if any of the functions that indicate the
                # floating point has been introduced turn up
                got_xref = False
                got_fp = False
                for line in map_file.read().splitlines():
                    if got_xref:
                        for function in FLOAT_FUNCTIONS:
                            if line.startswith(function):
                                U_LOG.info(f"found {function} in map file which" \
                                           " indicates floating point is"  \
                                           f" in use: {line}")
                                got_fp = True
                    else:
                        if line.startswith("Cross Reference Table"):
                            got_xref = True
                if not got_xref:
                    reporter.event(u_report.EVENT_TYPE_TEST,
                                   u_report.EVENT_FAILED,
                                   "map file has no cross-reference section")
                else:
                    if got_fp:
                        reporter.event(u_report.EVENT_TYPE_TEST,
                                       u_report.EVENT_FAILED,
                                       "floating point seems to be in use")
                    else:
                        return_value = 0
                        reporter.event(u_report.EVENT_TYPE_TEST,
                                       u_report.EVENT_COMPLETE)
                map_file.close()
        else:
            reporter.event(u_report.EVENT_TYPE_TEST,
                           u_report.EVENT_FAILED,
                           "unable to open map file")
    else:
        reporter.event(u_report.EVENT_TYPE_BUILD,
                       u_report.EVENT_FAILED,
                       "check debug log for details")

    return return_value
