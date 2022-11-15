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
from os.path import exists, realpath
from re import sub, match
from pathlib import Path

Import('env')

framework = env['PIOFRAMEWORK'][0]
framework = sub("espidf", "esp-idf", framework)
this_dir = realpath(os.getcwd())
ubxlib_dir = realpath(this_dir + "/../../..")

def parse_file(file_name):
    list = []
    for line in open(this_dir + "/" + file_name, "r"):
        line = line.strip()
        # Ignore comments and empty lines
        if line and not line.startswith("#"):
            line = sub("\$FRAMEWORK", framework, line)
            path = ubxlib_dir + "/" + line
            if (exists(path)):
                # Ignore paths for other platforms (frameworks) than current
                if not match(f"^port/platform/(?!common|{framework})", line):
                    list.append(path)
            else:
                print(f"* Error: Invalid path: {path}")
    return list


# Include directories
for inc in parse_file("include.txt"):
    env.Append(CPPPATH=[inc])

# Source files
src_filter = ["-<*>"]
for file in parse_file("source.txt"):
    src_filter.append(f"+<{file}>")
env.Append(SRC_FILTER=src_filter)
