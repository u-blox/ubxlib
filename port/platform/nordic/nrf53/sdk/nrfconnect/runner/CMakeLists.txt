# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

set(DTC_OVERLAY_FILE uart.overlay)
find_package(Zephyr HINTS $ENV{ZEPHYR_BASE})
project(ubxlib)
set(UBXLIB_BASE ../../../../../../..)
set(UBXLIB_PF_COMMON ${UBXLIB_BASE}/port/platform/common)
set(NRF5340_BASE ../../..)


#nrf5340 test runner application
target_include_directories(app PRIVATE ${NRF5340_BASE}/src/ ${NRF5340_BASE}/cfg )
target_sources(app PRIVATE ${NRF5340_BASE}/app/u_main.c)
target_sources(app PRIVATE ${NRF5340_BASE}/src/u_port.c)
target_sources(app PRIVATE ${NRF5340_BASE}/src/u_port_debug.c)
target_sources(app PRIVATE ${NRF5340_BASE}/src/u_port_os.c)
target_sources(app PRIVATE ${NRF5340_BASE}/src/u_port_gpio.c)
target_sources(app PRIVATE ${NRF5340_BASE}/src/u_port_uart.c)

#ubxlib common
target_include_directories(app PRIVATE ${UBXLIB_BASE}/cfg ${UBXLIB_BASE}/common/error/api ${UBXLIB_BASE}/port/api ${UBXLIB_PF_COMMON}/runner ${UBXLIB_PF_COMMON}/event_queue)
target_sources(app PRIVATE ${UBXLIB_PF_COMMON}/runner/u_runner.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/port/test/u_port_test.c)
target_sources(app PRIVATE ${UBXLIB_PF_COMMON}/event_queue/u_port_event_queue.c)

# unity
target_compile_definitions(app PRIVATE UNITY_INCLUDE_CONFIG_H)
target_include_directories(app PRIVATE $ENV{ZEPHYR_BASE}/../test/cmock/vendor/unity/src)
target_sources(app PRIVATE $ENV{ZEPHYR_BASE}/../test/cmock/vendor/unity/src/unity.c)

# Add environment variables passed-in via U_FLAGS
if (DEFINED ENV{U_FLAGS})
    separate_arguments(U_FLAGS NATIVE_COMMAND "$ENV{U_FLAGS}")
    target_compile_options(app PRIVATE ${U_FLAGS})
    message("ubxlib: added ${U_FLAGS} due to environment variable U_FLAGS.")
endif()
