#!/usr/bin/env python

'''Create a version of ubxlib that can be built as an Arduino library.'''

from multiprocessing import Process, freeze_support # Needed to make Windows behave
                                                    # when run under multiprocessing,
from signal import signal, SIGINT   # For CTRL-C handling
import os
import sys # For exit() and stdout
import argparse
import u_arduino_common

# The post-fix to add to all the usual ubxlib things to
# indicate the library (as opposed to test) part
UBXLIB_LIB_POSTFIX = ""

# Source file listing file
SOURCE_FILES = "source" + UBXLIB_LIB_POSTFIX + ".txt"

# Include file listing file
INCLUDE_FILES = "include" + UBXLIB_LIB_POSTFIX + ".txt"

# A sentence describing the library
SENTENCE = "Portable C library add-on for MCU and RTOS SDKs to run data and control APIs" \
           " for embedded applications with u-blox products and services."

# A paragraph describing the library, to go after SENTENCE
PARAGRAPH = ""

# The sub-directory of ubxlib in which the examples can be found
EXAMPLE_UBXLIB_SUBDIR = "example"

# The name to use for the ubxlib header file
UBXLIB_HEADER_FILE = u_arduino_common.LIBRARY_NAME + ".h"

def signal_handler(sig, frame):
    '''CTRL-C Handler'''
    del sig
    del frame
    sys.stdout.write('\n')
    print("CTRL-C received, EXITING.")
    sys.exit(-1)

# Add examples to the library
def add_examples(example_dir_root, forced):
    '''Add examples to the library'''
    return_value = 0
    file_list = []
    count = 0

    for root, _directories, files in os.walk(example_dir_root):
        for file in files:
            if file.endswith(".ino"):
                if file not in file_list:
                    file_list.append(file)
                    source = os.path.join(root, file)
                    destination = os.path.join("examples", os.path.basename(os.path.splitext(file)[0]), file)
                    return_value = u_arduino_common.copy_file(source, destination, forced)
                    if return_value < 0:
                        break
                    count += return_value
                else:
                    print(f"Duplicate example sketch file name \"{file}\"")
                    return_value = -2
                    break

    if return_value >= 0:
        return_value = count

    print(f"{count} example(s) copied.")

    return return_value

def main(source_files, include_paths, platform_type, ubxlib_dir,
         forced, output_dir, version_string):
    '''Main as a function'''
    saved_path = None
    include_files = []

    signal(SIGINT, signal_handler)

    # Make the ubxlib directory absolute, clearer that way
    ubxlib_dir = os.path.abspath(ubxlib_dir)

    # Print out what we've been told to do
    text = "Creating an Arduino library version of ubxlib for \"" + \
           platform_type + "\" in \"" + output_dir + "\" with the"  \
           " list of files from \"" + source_files + "\", the list" \
           " of include paths from \"" + include_paths + "\" and"   \
           " assuming the ubxlib directory is \"" + ubxlib_dir +    \
           "\" and any examples are under the \"" + EXAMPLE_UBXLIB_SUBDIR + \
           "\" sub-directory of the ubxlib directory."
    if forced:
        text += ", forcing overwrite of locally modified files"
    text += "."
    print(text)

    # Read the source files and include paths, filtered by platform
    source_list = u_arduino_common.read_list_from_file(source_files, platform_type)
    include_list = u_arduino_common.read_list_from_file(include_paths, platform_type)

    saved_path = os.getcwd()
    if not os.path.isdir(output_dir):
        os.makedirs(output_dir)
    os.chdir(output_dir)

    # First, copy the files
    print("Copying files...")
    return_value = u_arduino_common.copy_files(source_list, include_list, ubxlib_dir,
                                               forced, include_files)

    if return_value >= 0:
        # Add the examples
        print("Adding examples...")
        return_value = add_examples(os.path.join(ubxlib_dir, EXAMPLE_UBXLIB_SUBDIR), forced)

    if return_value >= 0:
        # Now create the library metadata
        print("Writing metadata file...")
        return_value = u_arduino_common.create_metadata(u_arduino_common.LIBRARY_NAME,
                                                        version_string, SENTENCE, PARAGRAPH,
                                                        False, platform_type, [UBXLIB_HEADER_FILE])

    if return_value == 0:
        print("Done.")
    else:
        print("Done, with errors.")

    if saved_path:
        os.chdir(saved_path)

    return return_value

if __name__ == "__main__":
    PARSER = argparse.ArgumentParser(description="A script to"     \
                                     " create a ubxlib Arduino"    \
                                     " library.\n")
    PARSER.add_argument("-p", default="esp-idf", help="the ubxlib" \
                        " platform to use with Arduino, i.e. a"    \
                        " directory name under the ubxlib platform"\
                        " directory; only \"esp-idf\" is supported.")
    PARSER.add_argument("-u", default="../../..", help="the root"  \
                        " directory of ubxlib; if this is not"     \
                        " provided it is assumed we are running"   \
                        " in the \"port/platform/arduino\" directory"  \
                        " and hence the ubxlib directory is three" \
                        " levels above.")
    PARSER.add_argument("-f", action='store_true', help="if a file" \
                        " has been locally modified then force"     \
                        " the changes to be overwritten.")
    PARSER.add_argument("-o", default=u_arduino_common.LIBRARY_NAME,\
                        help="the output directory for the Arduino" \
                        " library, default \"" +                    \
                        u_arduino_common.LIBRARY_NAME + "\".")
    PARSER.add_argument("-v", help="include the given version string.")
    PARSER.add_argument("source", nargs="?", default=SOURCE_FILES, \
                        help="a file containing the list of source"\
                        " files to include, each on a single line,"\
                        " default \"" + SOURCE_FILES + "\".")
    PARSER.add_argument("include", nargs="?", default=INCLUDE_FILES,\
                        help="a file containing the list of include"\
                        " paths required to compile the source,"    \
                        " each on a single line, default \"" +      \
                        INCLUDE_FILES + "\".")
    ARGS = PARSER.parse_args()

    # Call main()
    RETURN_VALUE = main(ARGS.source, ARGS.include, ARGS.p, ARGS.u, ARGS.f, ARGS.o, ARGS.v)

    sys.exit(RETURN_VALUE)

# A main is required because Windows needs it in order to
# behave when this module is called during multiprocessing
# see https://docs.python.org/2/library/multiprocessing.html#windows
if __name__ == '__main__':
    freeze_support()
    PROCESS = Process(target=main)
    PROCESS.start()
