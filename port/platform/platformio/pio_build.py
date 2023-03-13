#!/usr/bin/env python3

# Copyright 2019-2023 u-blox
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#  http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and

# pio_build.py
#
# This is a pre-build script for building with uxlib as a library in PlatformIO
# It will supply PlatformIO with lists of all the required source files and
# include directories from ubxlib.

import os
from os.path import realpath
from re import sub, match, search
from pathlib import Path
import sys
from glob import glob

Import('env')

# Set up the platform
framework = env['PIOFRAMEWORK'][0]
if "arduino" in framework:
    if not "espressif32" in env['PIOPLATFORM']:
        print("*** Error: ubxlib currently only supports esp32 boards in the Arduino framework")
        sys.exit(1)
    framework = "esp-idf"
elif "espidf" in framework:
    framework = "esp-idf"
this_dir = realpath(os.getcwd())
ubxlib_dir = realpath(this_dir + "/../../..")

# The files to read lists of source/include files from
file_list = ["inc_src.txt"]

# Set up the ubxlib features
ubxlib_features = "cell gnss short_range"
if "UBXLIB_FEATURES" in os.environ:
    ubxlib_features = os.environ["UBXLIB_FEATURES"]

module_test = False
if "U_UBXLIB_AUTO" in os.environ and int(os.environ["U_UBXLIB_AUTO"]) >= 10:
    # If we are running under automation and are going to run a test
    # (on the test automation system tests are instance numbers 10
    # or higher), then we need to bring in the test code also
    file_list.append("inc_src_test.txt")
    module_test = True

def section_parameter_true(section_parameter):
    ''' Check if a section parameter is included or not, handling negation '''
    return_value = False
    section_parameter_neg = False
    section_string = section_parameter

    if section_string and section_string.startswith("!"):
        section_string = section_string[1:]
        section_parameter_neg = True
    if section_parameter_neg:
        if section_string not in ubxlib_features:
            return_value = True
    else:
        if not section_string or section_string in ubxlib_features:
            return_value = True
    return return_value

def add_include_path(line, exclude):
    ''' Add line to the include paths if not in exclude '''
    for path in glob(ubxlib_dir + "/" + line, recursive=True):
        if not search(exclude, path.replace("\\", "/")):
            env.Append(CPPPATH=[path])

def add_source_path(line, src_filter, exclude, section_parameter):
    ''' Add line to src_filter if not in exclude, obeying section filtering '''
    if section_parameter_true(section_parameter):
        for path in glob(ubxlib_dir + "/" + line, recursive=True):
            if not search(exclude, path.replace("\\", "/")):
                src_filter.append(f"+<{path}>")

src_filter = ["-<*>"]
for file_name in file_list:
    section = ""
    exclude = ""
    for line in open(file_name, "r"):
        line = line.strip()
        if not line or line.startswith("#"):
            # Ignore comments and empty lines
            continue
        line = sub("\$FRAMEWORK", framework, line)
        m = match(r"^\[(\w+)( +!{0,1}\w+)*\]", line)
        if m:
            # New section
            section = m.group(1)
            section_parameter = None
            section_parameter_neg = False
            if m.lastindex > 1:
                # New section with a parameter (e.g. [INCLUDE gnss] or [SOURCE !gnss])
                section_parameter = m.group(2).strip()
        elif section == "EXCLUDE":
            if section_parameter_true(section_parameter):
                # Reg exp for paths to exclude from wild card searches
                exclude += ("|" if exclude else "") + line
        elif section == "MODULE":
            add_include_path(line + "/api/", exclude)
            add_source_path(line + "/src/*.c", src_filter, exclude, section_parameter)
            add_include_path(line + "/src/", exclude) # Ideally this would be private
            if module_test:
                add_source_path(line + "/test/*.c", src_filter, exclude, section_parameter)
                add_include_path(line + "/test/", exclude) # Ideally this would be private
        elif section == "INCLUDE":
            add_include_path(line, exclude)
        else:
            if section == "SOURCE" or section == framework:
                add_source_path(line, src_filter, exclude, section_parameter)

env.Append(SRC_FILTER=src_filter)