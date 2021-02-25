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

# C_FLAGS for static sizing
# e.g. "-Os -g0 -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16"
C_FLAGS = u_settings.STATIC_SIZE_C_FLAGS

# LD_FLAGS for static sizing
# e.g. "-Os -g0 -mcpu=cortex-m4 -mfloat-abi=hard
#      -mfpu=fpv4-sp-d16 --specs=nano.specs -lc -lnosys -lm"
LD_FLAGS = u_settings.STATIC_SIZE_LD_FLAGS

# STATIC_SIZE sub-directory (off ubxlib root)
SUB_DIR = u_settings.STATIC_SIZE_LD_FLAGS_SUB_DIR # e.g. "port/platform/static_size"

# Note: all the work is done by the static_size.py
# script down in port/platform/static_size, all we
# do here is configure it as we wish and wrap it
# in order to shoot the output into the usual
# streams for automation
def run(instance, defines, ubxlib_dir, working_dir, printer, reporter):
    '''Build to check static sizes'''
    return_value = -1
    instance_text = u_utils.get_instance_text(instance)
    cflags = C_FLAGS

    prompt = PROMPT + instance_text + ": "

    # Print out what we've been told to do
    text = "running static size check from ubxlib directory \"" + ubxlib_dir + "\""
    if working_dir:
        text += ", working directory \"" + working_dir + "\""
    printer.string("{}{}.".format(prompt, text))

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
        call_list = ["python"]
        call_list.append(ubxlib_dir + os.sep + SUB_DIR + os.sep + "static_size.py")
        call_list.append("-p")
        call_list.append(GNU_INSTALL_ROOT)
        call_list.append("-u")
        call_list.append(ubxlib_dir)
        call_list.append("-c")
        call_list.append(cflags)
        call_list.append("-l")
        call_list.append(LD_FLAGS)
        call_list.append(ubxlib_dir + os.sep + SUB_DIR + os.sep + "source.txt")
        call_list.append(ubxlib_dir + os.sep + SUB_DIR + os.sep + "include.txt")

        # Print what we're gonna do
        tmp = ""
        for item in call_list:
            tmp += " " + item
        printer.string("{}in directory {} calling{}".         \
                       format(prompt, os.getcwd(), tmp))

        # Set shell to keep Jenkins happy
        if u_utils.exe_run(call_list, 0, printer, prompt, shell_cmd=True):
            return_value = 0
            reporter.event(u_report.EVENT_TYPE_BUILD,
                           u_report.EVENT_COMPLETE)
        else:
            reporter.event(u_report.EVENT_TYPE_BUILD,
                           u_report.EVENT_FAILED,
                           "check debug log for details")

    return return_value
