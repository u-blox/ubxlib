#!/usr/bin/env python

'''Build the platform independent, non-test related code of ubxlib to establish static sizes.'''

from multiprocessing import Process, freeze_support # Needed to make Windows behave
                                                    # when run under multiprocessing,
from signal import signal, SIGINT   # For CTRL-C handling
from time import time
import os
import sys # For exit() and stdout
import argparse
import subprocess
import psutil                   # For killing things (make sure to do pip install psutil)

# Expected name for compiler
GNU_COMPILER = "arm-none-eabi-gcc"

# Expected name for linker
GNU_LINKER = "arm-none-eabi-gcc"

# Expected name for size
GNU_SIZE = "arm-none-eabi-size"

# The guard time in seconds for each compilation
# This may seem quite large: reason is that this script
# can sometimes be run on a *very* heavily loaded test
# machine which can take some considerable time finding
# CPU memory to launch a process
GUARD_TIME_SECONDS = 120

# Sub-directory to use when building
BUILD_SUBDIR = "build"

def signal_handler(sig, frame):
    '''CTRL-C Handler'''
    del sig
    del frame
    sys.stdout.write('\n')
    print("CTRL-C received, EXITING.")
    sys.exit(-1)

def get_flags(string, name):
    '''Get CFLAGS or LDFLAGS as a list from str or the environment'''
    answer_list = []

    if not string and name in os.environ:
        string = os.environ[name]
    if string:
        answer_list = string.split(" ")

    return answer_list

def read_list_from_file(file):
    '''Read a list, line by line, from a file'''
    output_list = []

    # Read list
    temp_list = [line.strip() for line in open(file, 'r')]
    for item in temp_list:
        # Throw away comment lines
        item = item.strip()
        if item and not item.startswith("#"):
            output_list.append(item)

    return output_list

def exe_terminate(process_pid):
    '''Jonathan's killer'''
    process = psutil.Process(process_pid)
    for proc in process.children(recursive=True):
        proc.terminate()
    process.terminate()

def exe_run(call_list, guard_time_seconds, shell_cmd=False):
    '''Call an executable, printing out what it does'''
    success = False
    start_time = time()
    kill_time = None

    try:
        process = subprocess.Popen(call_list,
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT,
                                   shell=shell_cmd)
        while process.poll() is None:
            string = process.stdout.readline()
            if string:
                print("{}".format(string.decode()), end="")
            if guard_time_seconds and (kill_time is None) and   \
               (time() - start_time > guard_time_seconds):
                kill_time = time()
                print("guard time of {} second(s)." \
                      " expired, stopping {}...".
                      format(guard_time_seconds, call_list[0]))
                exe_terminate(process.pid)
        if (process.poll() == 0) and kill_time is None:
            success = True
    except ValueError as ex:
        print("failed: {} while trying to execute {}.". \
              format(type(ex).__name__, str(ex)))

    return success

# Note: we don't bother with make here as there are few files,
# this is usually run as part of automated testing where a
# clean build is required anyway and make can be a but funny
# about platform differences for if/when we want to run this
# on Linux
def build(source_list, include_list, cflag_list, ldflag_list, gcc_bin_dir):
    '''Build source_list with include_list and flags under GCC'''
    return_value = 0
    obj_list = []

    # Make the include list
    for idx, item in enumerate(include_list):
        include_list[idx] = "-I" + item

    # Compile all the source files
    for item in source_list:
        call_list = [gcc_bin_dir + os.sep + GNU_COMPILER]
        call_list.extend(include_list)
        call_list.extend(cflag_list)
        call_list.append("-c")
        call_list.append(item)
        # Print what we're gonna do
        tmp = ""
        for another_item in call_list:
            tmp += " " + another_item
        print("{}".format(tmp))
        if not exe_run(call_list, GUARD_TIME_SECONDS, True):
            return_value = -1

    if return_value == 0:
        # Now link them
        for file in source_list:
            parts = file.split("/")
            file = parts[len(parts) - 1]
            file = file.replace(".cpp", ".o")
            file = file.replace(".c", ".o")
            obj_list.append(file)
        call_list = [gcc_bin_dir + os.sep + GNU_LINKER]
        call_list.extend(obj_list)
        # Order is important: has to be after the object
        # list or libraries (e.g. -lm) might not be resolved
        call_list.extend(ldflag_list)
        call_list.append("-o")
        call_list.append("total_with_clib.elf")
        # Print what we're gonna do
        tmp = ""
        for item in call_list:
            tmp += " " + item
        print("{}".format(tmp))
        if not exe_run(call_list, GUARD_TIME_SECONDS, True):
            return_value = -1

    if return_value == 0:
        # Call size on the result
        call_list = [gcc_bin_dir + os.sep + GNU_SIZE]
        call_list.append("-G")
        call_list.extend(obj_list)
        call_list.append("total_with_clib.elf")
        # Print what we're gonna do
        tmp = ""
        for item in call_list:
            tmp += " " + item
        print("{}".format(tmp))
        if not exe_run(call_list, GUARD_TIME_SECONDS, True):
            return_value = -1

    return return_value

def main(source_files, include_paths, cflags, ldflags, gcc_bin_dir,
         ubxlib_dir, working_dir):
    '''Main as a function'''
    return_value = 1
    saved_path = None
    cflag_list = []
    ldflag_list = []
    test_call = []

    signal(SIGINT, signal_handler)

    # Print out what we've been told to do
    text = "compiling list of files from \"" + source_files + "\"" \
           " with list of include paths from \"" + include_paths + "\""
    if cflags:
        text += ", with CFLAGS \"" + cflags + "\""
    else:
        text +=", with CFLAGS from the environment"
    if ldflags:
        text += ", with LDFLAGS \"" + ldflags + "\""
    else:
        text +=", with LDLAGS from the environment"
    if gcc_bin_dir:
        text += ", with GCC from \"" + gcc_bin_dir + "\""
    else:
        text +=", with GCC on the PATH"
    if ubxlib_dir:
        text += ", ubxlib directory \"" + ubxlib_dir + "\""
    if working_dir:
        text += ", working directory \"" + working_dir + "\""
    print("{}.".format(text))

    # Read the source files and include paths
    source_list = read_list_from_file(source_files)
    include_list = read_list_from_file(include_paths)

    if ubxlib_dir:
        # Prepend ubxlib to them
        for idx, item in enumerate(source_list):
            source_list[idx] = ubxlib_dir + os.sep + item
        for idx, item in enumerate(include_list):
            include_list[idx] = ubxlib_dir + os.sep + item

    cflag_list = get_flags(cflags, "CFLAGS")
    ldflag_list = get_flags(ldflags, "LDFLAGS")

    saved_path = os.getcwd()
    if working_dir:
        os.chdir(working_dir)
    else:
        if not os.path.isdir(BUILD_SUBDIR):
            os.mkdir(BUILD_SUBDIR)
        os.chdir(BUILD_SUBDIR)

    # Check that the compiler can be found
    print("checking that GCC is installed...")
    if gcc_bin_dir:
        test_call.append(gcc_bin_dir + os.sep + GNU_COMPILER)
    test_call.append("--version")
    if exe_run(test_call, GUARD_TIME_SECONDS, True):
        # Do the build
        return_value = build(source_list, include_list, cflag_list,
                             ldflag_list, gcc_bin_dir)
    else:
        print("unable to run GCC.\n")

    if saved_path:
        os.chdir(saved_path)

    return return_value

if __name__ == "__main__":
    PARSER = argparse.ArgumentParser(description="A script to"     \
                                     " build a list of files"      \
                                     " with a given GCC compiler;" \
                                     " if the compiler is not on"  \
                                     " the path it can be"         \
                                     " supplied as a command-line" \
                                     " parameter. The following"   \
                                     " environment variables"      \
                                     " affect operation:\n"        \
                                     " CFLAGS will be passed to"   \
                                     " the compiler unless the"    \
                                     " command-line parameter -c"  \
                                     " is provided,\n"             \
                                     " LDFLAGS will be passed to"  \
                                     " the linker unless the"      \
                                     " command-line parameter -l"  \
                                     " is provided.\n")
    PARSER.add_argument("-p", help="path to the bin directory of"  \
                        " GCC, e.g. \"C:/Program  Files (x86)/GNU" \
                        " Arm Embedded Toolchain/10 2020-q4-major/bin\"")
    PARSER.add_argument("-c", help="flags to be passed to the"     \
                        " compiler, e.g. \"-Os -g0 -mcpu=cortex-m4"\
                        " -mfloat-abi=hard -mfpu=fpv4-sp-d16"      \
                        " -DU_CFG_APP_PIN_CELL_ENABLE_POWER=-1"    \
                        " -DMY_FLAG -DMY_STRING=thisisastring\".")
    PARSER.add_argument("-l", help="flags to be passed to the"     \
                        " linker, e.g. \"-Os -g0 -mcpu=cortex-m4"  \
                        " -mfloat-abi=hard -mfpu=fpv4-sp-d16"      \
                        " --specs=nano.specs -lc -lnosys -lm\".")
    PARSER.add_argument("-u", help="the root directory of ubxlib.")
    PARSER.add_argument("-w", help="an empty working directory to" \
                        " use; if none is given \"" + BUILD_SUBDIR + \
                        "\" will be created and used.")
    PARSER.add_argument("source", nargs="?", default="source.txt", \
                        help="a file containing the list of source"\
                        " files to compile, each on a single line.")
    PARSER.add_argument("include", nargs="?", default="include.txt",\
                        help="a file containing the list of include"\
                        " paths required to compile the source,"    \
                        " each on a single line.")
    ARGS = PARSER.parse_args()

    # Call main()
    RETURN_VALUE = main(ARGS.source, ARGS.include, ARGS.c, ARGS.l,
                        ARGS.p, ARGS.u, ARGS.w)

    sys.exit(RETURN_VALUE)

# A main is required because Windows needs it in order to
# behave when this module is called during multiprocessing
# see https://docs.python.org/2/library/multiprocessing.html#windows
if __name__ == '__main__':
    freeze_support()
    PROCESS = Process(target=main)
    PROCESS.start()
