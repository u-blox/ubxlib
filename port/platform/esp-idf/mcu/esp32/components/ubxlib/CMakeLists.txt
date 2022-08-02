get_filename_component(UBXLIB_BASE ${CMAKE_CURRENT_LIST_DIR}/../../../../../../../ ABSOLUTE)
get_filename_component(PLATFORM_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../.. ABSOLUTE)
set(ENV{UBXLIB_BASE} ${UBXLIB_BASE})

# Set the ubxlib features to compile (all needs to be enabled at the moment)
set(UBXLIB_FEATURES short_range cell gnss)

# From this line we will get back:
# - UBXLIB_SRC
# - UBXLIB_INC
# - UBXLIB_PRIVATE_INC
# - UBXLIB_TEST_SRC
# - UBXLIB_TEST_INC
include(${UBXLIB_BASE}/port/ubxlib.cmake)

list(APPEND UBXLIB_INC
    ${PLATFORM_DIR}
    ${PLATFORM_DIR}/mcu/esp32/cfg
    ${PLATFORM_DIR}/../../clib
    ${PLATFORM_DIR}/../common/mbedtls
)

list(APPEND UBXLIB_PRIVATE_INC
    ${PLATFORM_DIR}/src
    ${UBXLIB_PRIVATE_INC}
)

set(COMPONENT_ADD_INCLUDEDIRS
    ${UBXLIB_INC}
)
set(COMPONENT_SRCS
    ${PLATFORM_DIR}/src/u_port.c
    ${PLATFORM_DIR}/src/u_port_debug.c
    ${PLATFORM_DIR}/src/u_port_gpio.c
    ${PLATFORM_DIR}/src/u_port_os.c
    ${PLATFORM_DIR}/src/u_port_uart.c
    ${PLATFORM_DIR}/src/u_port_i2c.c
    ${PLATFORM_DIR}/src/u_port_private.c
    ${PLATFORM_DIR}/../../clib/u_port_clib_mktime64.c
    ${PLATFORM_DIR}/../common/mbedtls/u_port_crypto.c
    ${UBXLIB_SRC}
)
set(COMPONENT_PRIV_INCLUDEDIRS
    ${UBXLIB_PRIVATE_INC}
)

# Export these variables to parent so they can be picked up by ubxlib_runner
set(UBXLIB_TEST_SRC ${UBXLIB_TEST_SRC} PARENT_SCOPE)
set(UBXLIB_TEST_INC ${UBXLIB_TEST_INC} PARENT_SCOPE)
set(UBXLIB_INC ${UBXLIB_INC} PARENT_SCOPE)
set(UBXLIB_PRIVATE_INC ${UBXLIB_PRIVATE_INC} PARENT_SCOPE)
set(UBXLIB_BASE ${UBXLIB_BASE} PARENT_SCOPE)


# For crypto functions
set(COMPONENT_REQUIRES mbedtls)

register_component()

if (DEFINED ENV{U_FLAGS})
    separate_arguments(U_FLAGS NATIVE_COMMAND "$ENV{U_FLAGS}")
    target_compile_options(${COMPONENT_TARGET} PUBLIC ${U_FLAGS})
    message("ubxlib: added ${U_FLAGS} due to environment variable U_FLAGS.")
endif()
