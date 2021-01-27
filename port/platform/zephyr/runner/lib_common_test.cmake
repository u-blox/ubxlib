# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

set(UBXLIB_BASE_SUB_PROJ ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)

set(lib_common_test_name fibonacci)
set(lib_common_test_flags 4)
set(lib_common_test_version 155)
set(lib_common_test_string Hello_world_from_libfib)
target_compile_definitions(app PUBLIC U_COMMON_LIB_TEST_NAME=${lib_common_test_name})
target_compile_definitions(app PUBLIC U_COMMON_LIB_TEST_FLAGS=${lib_common_test_flags})
target_compile_definitions(app PUBLIC U_COMMON_LIB_TEST_VERSION=${lib_common_test_version})
target_compile_definitions(app PUBLIC U_COMMON_LIB_TEST_STRING=${lib_common_test_string})


# include the custom target for building libfibonacci
include(${UBXLIB_BASE_SUB_PROJ}/common/lib_common/platform/zephyr/libfibonacci_test.cmake)

# include libfibonacci's api
target_include_directories(app PRIVATE ${UBXLIB_BASE_SUB_PROJ}/common/lib_common/test/test_lib/api)
