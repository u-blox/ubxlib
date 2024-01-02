#!/usr/bin/env python3

# Copyright 2024 u-blox
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

# build.py
#
# This script builds all the ubxlib examples for PlatformIO.
# Requires an available installation of PlatformIO.

import os
from sys import exit
from os import mkdir, chdir
from glob import glob
from tempfile import TemporaryDirectory
from os.path import realpath, abspath,  dirname, basename, splitext
from subprocess import run
import argparse

def do_run(command):
    p = run(command, shell = True)
    if p.returncode != 0:
        exit(p.returncode)

def main(pio_path, ubxlib_dir):
    '''Main as a function'''

    if not pio_path:
        pio_path = "pio"
    this_dir = dirname(realpath(__file__))
    if not ubxlib_dir:
        ubxlib_dir = abspath(this_dir + "/../../../..")

    # Use temporary which will be cleaned automatically on exit
    with TemporaryDirectory(prefix="pio_build") as build_dir:
        # Setup the build directory.
        print(f"Build directory: {build_dir}")
        chdir(build_dir)
        do_run(f"cp {this_dir}/platformio.ini .")
        mkdir("src")
        # Set variable used in platformio.ini
        os.environ["UBXLIB_DIR"] = ubxlib_dir
        first = True
        # The examples are sorted and named so that the Arduino ones comes last.
        # This is to avoid unnecessary rebuilds.
        for path in sorted(glob(abspath(this_dir + "/../example/*.c*"))):
            print("Building: ", basename(path))
            ext = splitext(path)[1]
            # Use same name for the main source file, avoids rebuild
            # of the whole ubxlib library.
            do_run(f"cp {path} ./src/main{ext}")
            # Use different environments based on source file extension.
            env = "zephyr_test" if ext == ".c" else "arduino_test"
            com = f"{pio_path} run -e {env}"
            if first:
                # On the first run some PlatformIO trickery is needed in order
                # to get the build directory setup properly.
                first = False
                do_run(f"{com} --list-targets")
                do_run(f"{com} --target cleanall")
            do_run(f"{com}")
            do_run("rm ./src/*")

if __name__ == "__main__":
    PARSER = argparse.ArgumentParser(description="A script to"     \
                                     " build the PIO examples.\n")
    PARSER.add_argument("-p", help="the path of the pio tool.")
    PARSER.add_argument("-u", help="the root directory of ubxlib;"  \
                        " if this is not provided it is assumed we" \
                        " are running in the"                       \
                        " \"port/platform/platformio/build\""       \
                        " directory.")
    ARGS = PARSER.parse_args()

    # Call main()
    RETURN_VALUE = main(ARGS.p, ARGS.u)

    exit(RETURN_VALUE)