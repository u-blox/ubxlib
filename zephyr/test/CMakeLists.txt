# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

get_filename_component(UBXLIB_BASE ../.. ABSOLUTE)

# Add ubxlib as a module
list(APPEND ZEPHYR_EXTRA_MODULES ${UBXLIB_BASE})

find_package(Zephyr HINTS $ENV{ZEPHYR_BASE})
project(zephyr_module_test)

# test application
target_sources(app PRIVATE u_main.c)