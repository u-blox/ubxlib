cmake_minimum_required(VERSION 3.4)

project(runner_win32)

# Set some variables containing the compiler options
if (MSVC)
    # C++20 standard (needed for designated initialisers)
    set(CMAKE_CXX_STANDARD 20)
    # Warnings as errors and ignore a few warnings
    add_compile_options(/J /WX /wd4068 /wd4090 /wd4838 /wd4996 /wd4061 /wd4309 /wd5045)
else()
    # GCC-compatible options
    add_compile_options(-Wall -Wextra -Werror)
endif()

# Get the root of ubxlib
get_filename_component(UBXLIB_BASE "${CMAKE_CURRENT_LIST_DIR}/../../../../../../" ABSOLUTE)
set(ENV{UBXLIB_BASE} ${UBXLIB_BASE})
message("UBXLIB_BASE will be \"${UBXLIB_BASE}\"")

# Set the ubxlib platform we are building for
set(UBXLIB_PLATFORM "windows" CACHE PATH "the name of the ubxlib platform to build for")
message("UBXLIB_PLATFORM will be \"${UBXLIB_PLATFORM}\"")

# Set the MCU we are building for
set(UBXLIB_MCU "win32" CACHE PATH "the name of the ubxlib MCU to build for under the given ubxlib platform")
message("UBXLIB_MCU will be \"${UBXLIB_MCU}\"")

if (DEFINED ENV{UNITY_PATH})
    set(UNITY_PATH $ENV{UNITY_PATH} CACHE PATH "the path to the Unity directory")
else()
    set(UNITY_PATH "${UBXLIB_BASE}/../Unity" CACHE PATH "the path to the Unity directory")
endif()
message("UNITY_PATH will be \"${UNITY_PATH}\"")

# Set the ubxlib features to compile (all must be enabled at the moment)
# These will have an effect down in the included ubxlib .cmake file
set(UBXLIB_FEATURES short_range cell gnss)
message("UBXLIB_FEATURES will be \"${UBXLIB_FEATURES}\"")

# Add any #defines specified by the environment variable U_FLAGS
# For example "U_FLAGS=-DU_CFG_CELL_MODULE_TYPE=U_CELL_MODULE_TYPE_SARA_R5 -DU_CFG_CELL_UART=2"
# Note: MSVC uses # as a way of passing = in the value of a define
# passed to the compiler in this way, so the first # that appears in
# the value of a define will come out as = in the code, i.e.
# -DTHING=1234# will appear to the code as #define THING 1234=
# If the value of one of your #defines happens to include a #
# then replace the = with a hash also; then, for example
# -DTHING#1234# will appear to the code as #define THING 1234#
if (DEFINED ENV{U_FLAGS})
    separate_arguments(U_FLAGS NATIVE_COMMAND "$ENV{U_FLAGS}")
    add_compile_options(${U_FLAGS})
    message("Environment variable U_FLAGS added ${U_FLAGS} to the build.")
endif()

# Get the platform-independent ubxlib source and include files
# from the ubxlib common .cmake file, i.e.
# - UBXLIB_SRC
# - UBXLIB_INC
# - UBXLIB_PRIVATE_INC
# - UBXLIB_TEST_SRC
# - UBXLIB_TEST_INC
include(${UBXLIB_BASE}/port/ubxlib.cmake)

# Create variables to hold the platform-dependent ubxlib source
# and include files
if(${UBXLIB_PLATFORM} STREQUAL "windows")
    set(UBXLIB_PUBLIC_INC_PORT
        ${UBXLIB_BASE}/port/platform/${UBXLIB_PLATFORM}
        ${UBXLIB_BASE}/port/platform/${UBXLIB_PLATFORM}/mcu/${UBXLIB_MCU}/cfg
        ${UBXLIB_BASE}/port/clib)
    set(UBXLIB_PRIVATE_INC_PORT
        ${UBXLIB_BASE}/port/platform/${UBXLIB_PLATFORM}/src)
    set(UBXLIB_SRC_PORT
        ${UBXLIB_BASE}/port/platform/${UBXLIB_PLATFORM}/src/u_port.c
        ${UBXLIB_BASE}/port/platform/${UBXLIB_PLATFORM}/src/u_port_debug.c
        ${UBXLIB_BASE}/port/platform/${UBXLIB_PLATFORM}/src/u_port_os.c
        ${UBXLIB_BASE}/port/platform/${UBXLIB_PLATFORM}/src/u_port_gpio.c
        ${UBXLIB_BASE}/port/platform/${UBXLIB_PLATFORM}/src/u_port_uart.c
        ${UBXLIB_BASE}/port/platform/${UBXLIB_PLATFORM}/src/u_port_i2c.c
        ${UBXLIB_BASE}/port/platform/${UBXLIB_PLATFORM}/src/u_port_crypto.c
        ${UBXLIB_BASE}/port/platform/${UBXLIB_PLATFORM}/src/u_port_private.c
        ${UBXLIB_BASE}/port/clib/u_port_clib_mktime64.c
        ${UBXLIB_BASE}/port/clib/u_port_clib_strtok_r.c)
    set(UBXLIB_TEST_SRC_PORT
        ${UBXLIB_BASE}/port/platform/common/runner/u_runner.c)
    set(UBXLIB_PRIVATE_TEST_INC_PORT
        ${UBXLIB_BASE}/port/platform/common/runner)
else()
    message(ERROR "UBXLIB_PLATFORM is not defined")
endif()

# Using the above, create the ubxlib library and add its headers.
add_library(ubxlib ${UBXLIB_SRC} ${UBXLIB_SRC_PORT})
target_include_directories(ubxlib PUBLIC ${UBXLIB_INC} ${UBXLIB_PUBLIC_INC_PORT})
target_include_directories(ubxlib PRIVATE ${UBXLIB_PRIVATE_INC} ${UBXLIB_PRIVATE_INC_PORT})

# Add Unity and its headers
# Unity requires a definition of "noreturn" and this may be
# missing from MSVC, depending on which Windows SDK CMake
# decides to employ.  To work around this, we provide a
# definition here which lets it evaluate to nothing.
add_compile_options("-DUNITY_NORETURN=")
add_subdirectory(${UNITY_PATH} unity)

# Create a library containing the ubxlib tests
# These files must be compiled as C++ so that the "runner" macro
# which creates the actual test functions works
# This is created as an OBJECT library so that the linker doesn't
# throw away the constructors we need
set_source_files_properties(${UBXLIB_TEST_SRC} PROPERTIES LANGUAGE CXX )
set_source_files_properties(${UBXLIB_TEST_SRC_PORT} PROPERTIES LANGUAGE CXX )
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
target_include_directories(ubxlib_test_main PRIVATE ${UBXLIB_PRIVATE_TEST_INC_PORT} ${UBXLIB_PRIVATE_INC})

# Link the ubxlib test target with the ubxlib tests library and Unity
target_link_libraries(ubxlib_test_main PRIVATE ubxlib unity ubxlib_test)