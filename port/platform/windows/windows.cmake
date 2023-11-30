# This is a shared CMake file used for the Windows port of ubxlib.
# It will create a Windows library containing the features specified
# in UBXLIB_FEATURES. Check README.md for details.

set(UBXLIB_PLATFORM windows)
set(UBXLIB_MCU win32)

# Set the root of ubxlib
if (NOT DEFINED UBXLIB_BASE)
  get_filename_component(UBXLIB_BASE "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)
endif()
set(ENV{UBXLIB_BASE} ${UBXLIB_BASE})
message("UBXLIB_BASE will be \"${UBXLIB_BASE}\"")

if (NOT DEFINED UBXLIB_FEATURES)
  # All ubxlib features activated by default
  set(UBXLIB_FEATURES short_range cell gnss)
endif()
message("UBXLIB_FEATURES will be \"${UBXLIB_FEATURES}\"")

# Set some variables containing the compiler options
if (MSVC)
    # C++20 standard (needed for designated initialisers)
    set(CMAKE_CXX_STANDARD 20)
    # Warnings as errors and ignore a few warnings
    add_compile_options(/J /WX /wd4068 /wd4090 /wd4838 /wd4996 /wd4061 /wd4309 /wd5045)
    # These warnings needs to be disable as well for 64-bit Windows for now
    add_compile_options(/wd4312 /wd4267 /wd4244 /wd4311 /wd4477)
    # Switch off warning about duplicate functions since we use WEAK which MSVC doesn't _need_ but then complains about there being two objects...
    # Turn off 4221 to get rid of bogus warnings from 64-bit linker.
    set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /IGNORE:4006,4221" CACHE STRING "Linker flag" FORCE)
else()
    # GCC-compatible options
    add_compile_options(-Wall -Wextra -Werror)
endif()

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
# and optionally:
# - UBXLIB_EXTRA_LIBS
# - UBXLIB_COMPILE_OPTIONS
include(${UBXLIB_BASE}/port/ubxlib.cmake)

# Create variables to hold the platform-dependent ubxlib source
# and include files
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
    ${UBXLIB_BASE}/port/platform/${UBXLIB_PLATFORM}/src/u_port_spi.c
    ${UBXLIB_BASE}/port/platform/${UBXLIB_PLATFORM}/src/u_port_private.c
    ${UBXLIB_BASE}/port/clib/u_port_clib_mktime64.c
    ${UBXLIB_BASE}/port/clib/u_port_clib_strtok_r.c
    ${UBXLIB_BASE}/port/clib/u_port_clib_gmtime_r.c)

# Using the above, create the ubxlib library and add its headers.
add_library(ubxlib ${UBXLIB_SRC} ${UBXLIB_SRC_PORT})
message("UBXLIB_COMPILE_OPTIONS will be \"${UBXLIB_COMPILE_OPTIONS}\"")
target_compile_options(ubxlib PRIVATE ${UBXLIB_COMPILE_OPTIONS})
target_include_directories(ubxlib PUBLIC ${UBXLIB_INC} ${UBXLIB_PUBLIC_INC_PORT})
target_include_directories(ubxlib PRIVATE ${UBXLIB_PRIVATE_INC} ${UBXLIB_PRIVATE_INC_PORT})
