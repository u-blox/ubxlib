#!/usr/bin/env python

'''Run Lint on the platform independent code of ubxlib and report results.'''

import os          # For sep
import subprocess
import u_report
import u_utils
import u_settings

# Prefix to put at the start of all prints
PROMPT = "u_run_lint_"

# The path to the Lint directory off the ubxlib root
LINT_PLATFORM_PATH = os.path.join("port","platform","lint")

# The .lnt files to be included from the Lint platform
# directory
LINT_PLATFORM_CONFIG_FILES = u_settings.LINT_PLATFORM_CONFIG_FILES #["co-gcc.lnt", "ubxlib.lnt"]

# Directory names to include.  The COMPLETE PATHs of ALL
# sub-directories from ubxlib downwards containing
# platform-independent ubxlib .c files must be included;
# Lint does not recurse and, in fact, we don't want it
# to as it cannot handle the platform-based code under
# \port\platform.
# Note that examples are DELIBERATELY not added
# here as we need more lee-way and don't want Lint
# suppressions cluttering them up.
LINT_DIRS = [os.path.join("common","network","src"),
             os.path.join("common","network","test"),
             os.path.join("common","sock","src"),
             os.path.join("common","sock","test"),
             os.path.join("common","security","src"),
             os.path.join("common","security","test"),
             os.path.join("common","mqtt_client","src"),
             os.path.join("common","mqtt_client","test"),
             os.path.join("common","at_client","src"),
             os.path.join("common","at_client","test"),
             os.path.join("ble","src"),
             os.path.join("ble","test"),
             os.path.join("cell","src"),
             os.path.join("cell","test"),
             os.path.join("common","short_range","src"),
             os.path.join("common","short_range","test"),
             os.path.join("port","clib"),
             os.path.join("port","platform","common","event_queue"),
             os.path.join("port","test"),
             os.path.join("port","platform","common","test"),
             os.path.join("port","platform","common","runner")]

# Include directories for ubxlib, off the ubxlib root.
UBXLIB_INCLUDE_DIRS = [LINT_PLATFORM_PATH,
                       os.path.join("ble","api"),
                       os.path.join("ble","src"),
                       os.path.join("ble","test"),
                       os.path.join("cfg"),
                       os.path.join("common","network","api"),
                       os.path.join("common","network","src"),
                       os.path.join("common","network","test"),
                       os.path.join("common","sock","api"),
                       os.path.join("common","sock","test"),
                       os.path.join("common","security","api"),
                       os.path.join("common","mqtt_client","api"),
                       os.path.join("common","error","api"),
                       os.path.join("common","at_client","api"),
                       os.path.join("common","short_range","api"),
                       os.path.join("common","short_range","src"),
                       os.path.join("common","short_range","test"),
                       os.path.join("cell","api"),
                       os.path.join("cell","src"),
                       os.path.join("cell","test"),
                       os.path.join("port","api"),
                       os.path.join("port","clib"),
                       os.path.join("port","platform","common","event_queue"),
                       os.path.join("port","platform","common","runner"),
                       os.path.join("port","platform","lint","stubs")]

# Include directories for the C compiler and its C library.
COMPILER_INCLUDE_DIRS = u_settings.LINT_COMPILER_INCLUDE_DIRS

# Table of "where.exe" search paths for tools required
# plus hints as to how to install the tools
TOOLS_LIST = [{"which_string": "flexelint",
               "hint": "can't find flexelint, please install it and ensure"  \
                       " that it is on the path."},                          \
              {"which_string": "make",
               "hint": "can't find make, please install it"                  \
                       " (e.g. from here:"                                   \
                       " http://gnuwin32.sourceforge.net/packages/make.htm)" \
                       " and ensure it is on the path."},
              {"which_string": "gcc",
               "hint": "can't find a GCC compiler, please either install one" \
                       " or add one to the path and make sure that the"       \
                       " COMPILER_INCLUDE_DIRS variable in this script"       \
                       " contains the necessary include paths."},
              {"which_string": "g++",
               "hint": "can't find G++, please either install it"            \
                       " or add it to the path and make sure that the"       \
                       " COMPILER_INCLUDE_DIRS variable in this script"      \
                       " contains the necessary include paths."},
              {"which_string": "rm",
               "hint": "can't find rm, please either install a version"       \
                       " e.g. by installing https://sourceforge.net/projects/"\
                       "unxutils or or add it to the path."},                \
              {"which_string": "touch",
               "hint": "can't find touch, please either install a version"    \
                       " e.g. by installing https://sourceforge.net/projects/"\
                       "unxutils or or add it to the path."},
              {"which_string": "gawk",
               "hint": "can't find gawk, please either install a version"    \
                       " e.g. by installing https://sourceforge.net/projects/"\
                       "unxutils or or add it to the path."}]

def check_installation(tools_list, compiler_dirs_list, printer, prompt):
    '''Check that everything required has been installed'''
    success = True

    # Check for the tools on the path
    printer.string("{}checking tools...".format(prompt))
    for item in tools_list:
        if not u_utils.exe_where(item["which_string"], item["hint"],
                                 printer, prompt):
            success = False

    # Check for existence of the given directories
    printer.string("{}checking directories...".format(prompt))
    for item in compiler_dirs_list:
        if not os.path.exists(item):
            printer.string("{}compiler include directory \"{}\""
                           " does not exist.".format(prompt, item))
            success = False

    return success

def create_lint_config(lint_platform_path, defines, printer, prompt):
    '''Create the Lint configuration files'''
    call_list = []

    # Get the defines
    if defines:
        # Create the CFLAGS string
        cflags = ""
        for idx, define in enumerate(defines):
            if idx == 0:
                cflags = "-D" + define
            else:
                cflags += " -D" + define

    # Run make to create the configuration files
    call_list.append("make")
    call_list.append("--debug=all")
    if defines:
        call_list.append("CFLAGS=" + cflags)
    call_list.append("-f")
    call_list.append(lint_platform_path + os.sep + "co-gcc.mak")

    # Print what we're gonna do
    tmp = ""
    for item in call_list:
        tmp += " " + item
    printer.string("{}in directory {} calling{}".         \
                   format(prompt, os.getcwd(), tmp))

    # Call it
    return u_utils.exe_run(call_list, None, printer, prompt)

def get_file_list(ubxlib_dir, lint_dirs, use_stubs):
    '''Get the list of files to be Linted'''
    file_list = []
    stub_file_list = []

    # Lint is really bad at recursing, you can't give it
    # a directory, a list of file extensions and have it
    # do the work.  Hence we do the heavy lifting here.
    for directory in lint_dirs:
        files = os.listdir(ubxlib_dir + os.sep + directory)
        for item in files:
            if item.endswith(".c") or item.endswith(".cpp"):
                if item.endswith("_stub.c") or item.endswith("_stub.cpp"):
                    stub_file_list.append(ubxlib_dir + os.sep +
                                          directory + os.sep + item)
                else:
                    file_list.append(ubxlib_dir + os.sep +
                                     directory + os.sep + item)

    if use_stubs:
        # If we're using stubs then remove all the "non-stubs"
        # file versions from the list
        for stub_file in stub_file_list:
            if stub_file.endswith("_stub.c"):
                non_stub_file = stub_file.replace("_stub.c", ".c")
            else:
                if stub_file.endswith("_stub.cpp"):
                    non_stub_file = stub_file.replace("_stub.cpp", ".cpp")
            for idx, item in enumerate(file_list):
                if item == non_stub_file:
                    file_list.pop(idx)
                    break
        # ...and add the stub versions instead
        file_list.extend(stub_file_list)

    return file_list

def run(instance, defines, ubxlib_dir, working_dir, printer, reporter,
        keep_going_flag=None, unity_dir=None):
    '''Run Lint'''
    return_value = 1
    call_list = []
    instance_text = u_utils.get_instance_text(instance)

    prompt = PROMPT + instance_text + ": "

    # Print out what we've been told to do
    text = "running Lint from ubxlib directory \"" + ubxlib_dir + "\""
    if working_dir:
        text += ", working directory \"" + working_dir + "\""
    if unity_dir:
        text += ", using Unity from \"" + unity_dir + "\""
    printer.string("{}{}.".format(prompt, text))

    reporter.event(u_report.EVENT_TYPE_CHECK,
                   u_report.EVENT_START,
                   "Lint")
    # Switch to the working directory
    with u_utils.ChangeDir(working_dir):
        # Check that everything we need is installed
        if u_utils.keep_going(keep_going_flag, printer, prompt) and \
           check_installation(TOOLS_LIST, COMPILER_INCLUDE_DIRS,
                              printer, prompt):
            # Fetch Unity, if necessary
            if u_utils.keep_going(keep_going_flag, printer, prompt) and \
                not unity_dir:
                if u_utils.fetch_repo(u_utils.UNITY_URL,
                                      u_utils.UNITY_SUBDIR,
                                      None, printer, prompt,
                                      submodule_init=False):
                    unity_dir = os.getcwd() + os.sep + u_utils.UNITY_SUBDIR
            if unity_dir:
                # Create the local Lint configuration files
                if u_utils.keep_going(keep_going_flag, printer, prompt) and \
                   create_lint_config(ubxlib_dir + os.sep +
                                      LINT_PLATFORM_PATH,
                                      defines, printer, prompt):
                    # Determine if "U_CFG_LINT_USE_STUBS" is in the list of
                    # compiler options
                    use_stubs = False
                    for item in defines:
                        if item == "U_CFG_LINT_USE_STUBS":
                            use_stubs = True
                            break
                    # Get the file list
                    file_list = get_file_list(ubxlib_dir, LINT_DIRS,
                                              use_stubs)

                    # Assemble the call list
                    call_list.append("flexelint")
                    if defines:
                        for item in defines:
                            call_list.append("-d" + item)
                    for item in COMPILER_INCLUDE_DIRS:
                        call_list.append("-i\"" + item + "\"")
                    call_list.append("-i\"" + unity_dir + os.sep + "src\"")
                    for item in UBXLIB_INCLUDE_DIRS:
                        call_list.append("-i\"" + ubxlib_dir + os.sep + item + "\"")
                    for item in LINT_PLATFORM_CONFIG_FILES:
                        call_list.append(ubxlib_dir + os.sep + LINT_PLATFORM_PATH +
                                         os.sep + item)
                    call_list.extend(file_list)

                    # Print what we're gonna do
                    tmp = ""
                    for item in call_list:
                        tmp += " " + item
                    printer.string("{}in directory {} calling{}".         \
                                   format(prompt, os.getcwd(), tmp))
                    try:
                        text = subprocess.check_output(call_list,
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
                        printer.string("{}Lint returned error {}:".
                                       format(prompt, error.returncode))
                        for line in error.output.splitlines():
                            line = line.strip().decode()
                            if line:
                                reporter.event_extra_information(line)
                                printer.string("{}{}".format(prompt, line))
                else:
                    reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                                   u_report.EVENT_FAILED,
                                   "could not create Lint config")
            else:
                reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                               u_report.EVENT_FAILED,
                               "unable to fetch Unity")
        else:
            reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                           u_report.EVENT_FAILED,
                           "there is a problem with the Lint installation")

    return return_value
