#!/usr/bin/env python

'''Stuff used by both u_arduino_lib.py and u_arduino_test.py.'''

import os
import subprocess
import platform # Figure out current OS

# The e-mail address to contact ubxlib
UBXLIB_EMAIL = "ubxlib@u-blox.com"

# The name of the library
LIBRARY_NAME = "ubxlib"

# The name of the library properties file
PROPERTIES_FILE = "library.properties"

# Architectures supported: the first item in each list
# is the ubxlib platform name and the subsequent items
# are the corresponding Arduino architecture(s)
ARCHITECTURES = [["esp-idf", "esp32"]]

# The strings which indicate that an include path is public
PUBLIC_INCLUDE_INDICATOR = ["cfg" + os.sep,
                            os.sep + "api" + os.sep,
                            "_clib_",
                            "platform" + os.sep + "esp-idf"]

def read_list_from_file(file, wanted_platform):
    '''Read a list, line by line, from a file'''
    output_list = []

    # Read list
    temp_list = [line.strip() for line in open(file, 'r', encoding='utf8')]
    for item in temp_list:
        # Throw away comment lines
        item = item.strip()
        if item and not item.startswith("#"):
            platform_type = None
            try:
                platform_type = item.split("port/platform")[1].split("/")[0]
            except:
                pass
            if not platform_type or ("common" or wanted_platform in platform_type):
                output_list.append(item)

    return output_list

# subprocess arguments behaves a little differently on Linux and Windows
# depending if a shell is used or not, which can be read here:
# https://stackoverflow.com/a/15109975
# This function will compensate for these deviations
def subprocess_osify(cmd, shell=True):
    ''' expects an array of strings being [command, param, ...] '''
    if platform.system() != "Windows" and shell:
        line = ''
        for command in cmd:
            # Put everything in a single string and quote args containing spaces
            if ' ' in command:
                line += f'\"{command}\" '
            else:
                line += f'{command} '
        cmd = line
    return cmd

# Copy a file from A to B, full paths included
def copy_file(source, destination, forced):
    '''Copy a file from A to B'''
    return_value = -1

    # If a given file either does not exist in the destination
    # directory (with a .cpp extension if it is a source file)
    # or the one in the ubxlib source directory has a more recent
    # modification date than the one under this directory then
    # copy it from the ubxlib location into here.
    if os.path.isfile(source):
        do_copy = True
        if not forced:
            try:
                source_timestamp = os.path.getmtime(source)
                destination_timestamp = os.path.getmtime(destination)
                if destination_timestamp == source_timestamp:
                    do_copy = False
                    return_value = 0
                    print(f"{destination} is already up to date.")
                else:
                    if destination_timestamp > source_timestamp:
                        do_copy = False
                        return_value = 0
                        print("WARNING: not updating {} as it has been locally modified.". \
                              format(destination))
            except OSError:
                pass
        if do_copy:
            directories = os.path.dirname(destination)
            if not os.path.isdir(directories):
                try:
                    os.makedirs(directories)
                except OSError:
                    print(f"Unable to create directory {directories}.")
                    do_copy = False
            if do_copy:
                call_list = []
                if platform.system() == "Windows":
                    call_list.append("copy")
                    call_list.append("/Y")
                else:
                    call_list.append("cp")
                call_list.append(source)
                call_list.append(destination)
                try:
                    print(f"Copying {source} to {destination}...")
                    subprocess.check_output(subprocess_osify(call_list), shell=True)
                    return_value = 1
                except subprocess.CalledProcessError as error:
                    print("Error when copying {} to {}, {} {}: \"{}\"".
                          format(source, destination, error.cmd,
                                 error.returncode, error.output))
    else:
        print(f"{source} is not a file.")

    return return_value

# Copy the source and header files from the lists into a directory
# structure that Arduino understands
# https://arduino.github.io/arduino-cli/0.19/library-specification/#layout-of-folders-and-files
def copy_files(source_list, include_list, ubxlib_dir, forced, include_files):
    '''Copy files into a form that Arduino can understand'''
    return_value = -1
    file_count = 0
    src_files_copied_count = 0
    path_count = 0
    header_files_copied_count = 0

    # If there is no "src" sub-directory then create it
    if not os.path.isdir("src"):
        os.mkdir("src")

    # First the source files
    for file in source_list:
        file = file.replace("/", os.sep)
        ubxlib_file = os.path.join(ubxlib_dir, file)
        # Copy without the whole path, just dumping
        # the source file into the src directory; we
        # won't have duplicate names (the Segger
        # Embedded Studio toolchain already brings
        # that requirement since it dumps all object
        # files into one directory) and its clearer
        # this way
        destination_file = os.path.join("src", os.path.basename(file))
        copied = copy_file(ubxlib_file, destination_file, forced)
        if copied < 0:
            break
        file_count += 1
        src_files_copied_count += copied

    print(f"{src_files_copied_count} source file(s) copied.")
    if file_count == len(source_list):
        # Must have succeeded, now do the include files
        for file_path in include_list:
            file_path = file_path.replace("/", os.sep)
            ubxlib_file_path = os.path.join(ubxlib_dir, file_path)
            ubxlib_files = []
            destination_files = []
            if os.path.isfile(ubxlib_file_path) and ubxlib_file_path not in ubxlib_files:
                # An explicitly included file: just add it
                ubxlib_files.append(ubxlib_file_path)
                destination_files.append(os.path.join("src", ubxlib_file_path))
                include_files.append(os.path.basename(ubxlib_file_path))
            else:
                for file_name in sorted(os.listdir(ubxlib_file_path)):
                    # A directory, go through the files
                    item = os.path.join(ubxlib_file_path, file_name)
                    if os.path.isfile(item) and item.endswith(".h") and item not in ubxlib_files:
                        ubxlib_files.append(item)
                        destination_files.append(os.path.join("src", file_name))
                        add_to_includes = False
                        if include_files is not None:
                            for public_indicator in PUBLIC_INCLUDE_INDICATOR:
                                if public_indicator in item:
                                    add_to_includes = True
                                    break
                            if add_to_includes:
                                include_files.append(file_name)
            for idx, ubxlib_file in enumerate(ubxlib_files):
                copied = copy_file(ubxlib_file, destination_files[idx], forced)
                if copied < 0:
                    break
                header_files_copied_count += copied
            path_count += 1
        if path_count == len(include_list):
            # Must have succeeded
            return_value = 0
        print(f"{header_files_copied_count} header file(s) copied.")

    if return_value >= 0:
        return_value = src_files_copied_count + header_files_copied_count

    return return_value

# Create a common header file which brings in a set of headers
def create_header_file(filename, comment_text, include_files):
    '''Create Arduino library header file'''
    return_value = -1
    filename_no_ext = filename.split(".")[0].upper()

    with open(os.path.join("src", filename), "w", encoding="utf8") as file:
        file.write(comment_text)
        file.write(f"#ifndef _U_{filename_no_ext}_H_\n")
        file.write(f"#define _U_{filename_no_ext}_H_\n\n")
        if include_files:
            for include_file in include_files:
                file.write(f"#include <{include_file}>\n")
        file.write(f"\n#endif // _U_{filename_no_ext}_H_\n\n")
        file.write("// End of file\n")
        return_value = 0

    return return_value

# Create the Arduino library metadata
# https://arduino.github.io/arduino-cli/0.19/library-specification/#library-metadata
def create_metadata(library_name, version_string, sentence, paragraph, precompiled,
                    platform_type, include_files):
    '''Create Arduino library metadata file'''
    return_value = -1
    architecture_list = ""

    for architecture in ARCHITECTURES:
        for idx, item in enumerate(architecture):
            if idx == 0:
                if item.lower() != platform_type.lower():
                    break
            else:
                if len(architecture_list) > 0:
                    architecture_list += ", "
                architecture_list += item

    if architecture_list:
        with open(PROPERTIES_FILE, "w", encoding="utf8") as file:
            file.write(f"name={library_name}\n")
            if not version_string:
                # version is a mandatory field, have to put in something
                version_string = "0.0.0"
            file.write(f"version={version_string}\n")
            file.write(f"author={UBXLIB_EMAIL}\n")
            file.write(f"maintainer={UBXLIB_EMAIL}\n")
            file.write(f"sentence={sentence}\n")
            file.write(f"paragraph={paragraph}\n")
            file.write("category=Communication\n")
            file.write("url=https://github.com/u-blox/ubxlib\n")
            file.write(f"architectures={architecture_list}\n")
            if precompiled:
                file.write("dot_a_linkage=true\n")
                # precompiled is set to full so that, if a version of the library
                # is compiled, it may be copied from your sketch (kept in the
                # subdirectory libraries/ubxlib/ubxlib.a) to the global library
                # folder with the right architecture name, which in this case
                # would be libraries/ubxlib/esp32/ubxlib.a
                file.write("precompiled=full\n")
            text = ""
            for include_file in include_files:
                if len(text) > 0:
                    text += ", "
                text += include_file
            file.write(f"includes={text}\n")
            return_value = 0

    return return_value
