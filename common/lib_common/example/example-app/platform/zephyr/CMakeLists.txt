# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr HINTS $ENV{ZEPHYR_BASE})
project(ubxlib_lib_example_app)
set(UBXLIB_BASE ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../..)
set(UBXLIB_PF_COMMON ${UBXLIB_BASE}/port/platform/common)
set(ZEPHYR_PORT_BASE ${UBXLIB_BASE}/port/platform/zephyr)

# include the custom target for building libfibonacci
include(${UBXLIB_BASE}/common/lib_common/example/platform/zephyr/libfibonacci.cmake)

# include libfibonacci's api
target_include_directories(app PRIVATE ${UBXLIB_BASE}/common/lib_common/example/api)

# lib fibonacci example application
target_include_directories(app PRIVATE ${ZEPHYR_PORT_BASE}/src/ ${ZEPHYR_PORT_BASE}/cfg )
target_sources(app PRIVATE ${UBXLIB_BASE}/common/lib_common/example/example-app/platform/zephyr/u_libexample_example_app.c)
target_sources(app PRIVATE ${ZEPHYR_PORT_BASE}/src/u_port.c)
target_sources(app PRIVATE ${ZEPHYR_PORT_BASE}/src/u_port_debug.c)
target_sources(app PRIVATE ${ZEPHYR_PORT_BASE}/src/u_port_os.c)
target_sources(app PRIVATE ${ZEPHYR_PORT_BASE}/src/u_port_gpio.c)
target_sources(app PRIVATE ${ZEPHYR_PORT_BASE}/src/u_port_uart.c)

# ubxlib common
target_include_directories(app PRIVATE ${UBXLIB_BASE}/cfg ${UBXLIB_BASE}/common/error/api ${UBXLIB_BASE}/port/api ${UBXLIB_PF_COMMON}/event_queue)
target_include_directories(app PRIVATE ${UBXLIB_BASE}/common/lib_common/api)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/lib_common/src/u_lib_handler.c)
target_sources(app PRIVATE ${UBXLIB_PF_COMMON}/event_queue/u_port_event_queue.c)

# Add environment variables passed-in via U_FLAGS
if (DEFINED ENV{U_FLAGS})
    separate_arguments(U_FLAGS NATIVE_COMMAND "$ENV{U_FLAGS}")
    target_compile_options(app PRIVATE ${U_FLAGS})
    message("ubxlib: added ${U_FLAGS} due to environment variable U_FLAGS.")
endif()
