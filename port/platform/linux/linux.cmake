# This is a shared CMake file used for the Linux port of ubxlib.
# It will create a Linux library containing the features specified
# in UBXLIB_FEATURES. Check README.md for details.

set(UBXLIB_PLATFORM linux)
set(UBXLIB_MCU posix)

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

# Build for debug by default
set(CMAKE_BUILD_TYPE Debug)
# The Posix implementations require these libraries
set(UBXLIB_REQUIRED_LINK_LIBS -lm -lssl -lpthread -lrt -lgpiod)
# Warnings are errors
add_compile_options(-Wall -Werror -Wno-format-truncation -Wno-stringop-truncation -funsigned-char)

# Add any #defines specified by the environment variable U_FLAGS
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

# Linux port specific files
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
    ${UBXLIB_BASE}/port/platform/${UBXLIB_PLATFORM}/src/u_port_ppp.c
    ${UBXLIB_BASE}/port/clib/u_port_clib_mktime64.c)

# Add the platform-specific tests and examples
list(APPEND UBXLIB_TEST_SRC
    ${UBXLIB_BASE}/port/platform/${UBXLIB_PLATFORM}/test/u_linux_ppp_test.c
    ${UBXLIB_BASE}/example/sockets/main_ppp_linux.c
)

# Generate a library of ubxlib
add_library(ubxlib OBJECT ${UBXLIB_SRC} ${UBXLIB_SRC_PORT})
message("UBXLIB_COMPILE_OPTIONS will be \"${UBXLIB_COMPILE_OPTIONS}\"")
target_compile_options(ubxlib PUBLIC ${UBXLIB_COMPILE_OPTIONS})
target_include_directories(ubxlib PUBLIC ${UBXLIB_INC} ${UBXLIB_PUBLIC_INC_PORT})
target_include_directories(ubxlib PRIVATE ${UBXLIB_PRIVATE_INC} ${UBXLIB_PRIVATE_INC_PORT})


