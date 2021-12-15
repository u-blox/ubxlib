#!/usr/bin/env python

'''Build the platform independent, non-test, non-example of ubxlib to establish static sizes.'''

import os           # For sep(), getcwd()
import u_report
import u_utils
import u_settings

# Prefix to put at the start of all prints
PROMPT = "u_run_static_size_"

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
def run(instance, defines, ubxlib_dir, working_dir, printer, reporter, keep_going_flag=None):
    '''Build to check static sizes'''
    return_value = -1
    instance_text = u_utils.get_instance_text(instance)
    cflags = ""

    prompt = PROMPT + instance_text + ": "

    # Print out what we've been told to do
    text = "running static size check from ubxlib directory \"" + ubxlib_dir + "\""
    if working_dir:
        text += ", working directory \"" + working_dir + "\""
    else:
        working_dir = os.getcwd()
    printer.string("{}{}.".format(prompt, text))

    build_dir = working_dir + os.sep + BUILD_SUBDIR

    reporter.event(u_report.EVENT_TYPE_BUILD,
                   u_report.EVENT_START,
                   "StaticSize")

    # Switch to the working directory
    with u_utils.ChangeDir(working_dir):
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
            "-j8",
            "float_size"
        ]

        # Print what we're gonna do
        tmp = ""
        for item in call_list:
            tmp += " " + item
        printer.string("{}in directory {} calling{}".         \
                       format(prompt, os.getcwd(), tmp))

        # Set shell to keep Jenkins happy
        if u_utils.exe_run(call_list, 0, printer, prompt, shell_cmd=True,
                           keep_going_flag=keep_going_flag):
            return_value = 0
            reporter.event(u_report.EVENT_TYPE_BUILD,
                           u_report.EVENT_COMPLETE)
        else:
            reporter.event(u_report.EVENT_TYPE_BUILD,
                           u_report.EVENT_FAILED,
                           "check debug log for details")

    return return_value
