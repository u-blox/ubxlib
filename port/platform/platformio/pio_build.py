#!/usr/bin/env python3

# Copyright 2022 u-blox
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

if "U_UBXLIB_AUTO" in os.environ and int(os.environ["U_UBXLIB_AUTO"]) >= 10:
    # If we are running under automation and are going to run a test
    # (on the test automation system tests are instance numbers 10
    # or higher), then we need to bring in the test code also
    file_list.append("inc_src_test.txt")

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
        m = match(r"^\[(\w+)\]", line)
        if m:
            # New section
            section = m.group(1)
        elif section == "EXCLUDE":
            # Reg exp for paths to exclude from wild card searches
            exclude += ("|" if exclude else "") + line
        else:
            # File or directory
            for path in glob(ubxlib_dir + "/" + line, recursive=True):
                if not search(exclude, path.replace("\\", "/")):
                    if section == "INCLUDE":
                        env.Append(CPPPATH=[path])
                    else:
                        if section == "SOURCE" or section == framework:
                            src_filter.append(f"+<{path}>")

env.Append(SRC_FILTER=src_filter)
