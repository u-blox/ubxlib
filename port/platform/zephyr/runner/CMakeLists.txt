# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

get_filename_component(UBXLIB_BASE ../../../.. ABSOLUTE)

# Add custom u-blox boards
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../boards/short_range/zephyr/)
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../boards/cellular/)

# Add ubxlib as a Zephyr module
list(APPEND ZEPHYR_EXTRA_MODULES ${UBXLIB_BASE})

find_package(Zephyr HINTS $ENV{ZEPHYR_BASE})
project(ubxlib)
set(ZEPHYR_PORT_BASE ..)

# lib-common
target_include_directories(app PRIVATE ${UBXLIB_BASE}/common/lib_common/test)
include(./lib_common_test.cmake)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/lib_common/test/u_lib_common_test.c)

# test runner application
target_sources(app PRIVATE
    ${UBXLIB_BASE}/port/platform/common/runner/u_runner.c
    ${ZEPHYR_PORT_BASE}/app/u_main.c
)
target_include_directories(app PRIVATE
    ${UBXLIB_BASE}/port/platform/common/runner
)