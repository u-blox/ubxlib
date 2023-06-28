cmake_minimum_required(VERSION 3.4)
project(runner_linux)

# Get the Linux ubxlib library
include(../../../linux.cmake)

# Unity is needed
if (DEFINED ENV{UNITY_PATH})
    set(UNITY_PATH $ENV{UNITY_PATH} CACHE PATH "the path to the Unity directory" FORCE)
else()
    set(UNITY_PATH "${UBXLIB_BASE}/../Unity" CACHE PATH "the path to the Unity directory" FORCE)
endif()
message("UNITY_PATH will be \"${UNITY_PATH}\"")

# Add Unity and its headers
add_subdirectory(${UNITY_PATH} unity)

# Create a library containing the ubxlib tests
set(UBXLIB_PRIVATE_TEST_INC_PORT
    ${UBXLIB_BASE}/port/platform/common/runner)
set(UBXLIB_TEST_SRC_PORT
    ${UBXLIB_BASE}/port/platform/common/runner/u_runner.c)
add_library(ubxlib_test OBJECT ${UBXLIB_TEST_SRC} ${UBXLIB_TEST_SRC_PORT})
target_include_directories(ubxlib_test PRIVATE
                           ${UBXLIB_TEST_INC}
                           ${UBXLIB_PRIVATE_TEST_INC_PORT}
                           ${UBXLIB_INC}
                           ${UBXLIB_PRIVATE_INC}
                           ${UBXLIB_PUBLIC_INC_PORT}
                           ${UBXLIB_PRIVATE_INC_PORT}
                           ${UNITY_PATH}/src)


# Create the test target for ubxlib, including in it u_main.c
add_executable(ubxlib_test_main ${UBXLIB_BASE}/port/platform/${UBXLIB_PLATFORM}/app/u_main.c)
target_include_directories(ubxlib_test_main PUBLIC ${UBXLIB_INC} ${UBXLIB_PUBLIC_INC_PORT})
target_include_directories(ubxlib_test_main PRIVATE ${UBXLIB_PRIVATE_TEST_INC_PORT} ${UBXLIB_PRIVATE_INC})
# Link the ubxlib test target with the ubxlib tests library and Unity
target_link_libraries(ubxlib_test_main PRIVATE ubxlib unity ubxlib_test ${UBXLIB_REQUIRED_LINK_LIBS})