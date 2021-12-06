get_filename_component(UBXLIB_BASE ${CMAKE_CURRENT_LIST_DIR}/../../../../../../../ ABSOLUTE)
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

set(COMPONENT_ADD_INCLUDEDIRS
    ${UBXLIB_INC}
    "../../cfg"
    "../../../.."
)
set(COMPONENT_SRCS
    "../../../../src/u_port.c"
    "../../../../src/u_port_debug.c"
    "../../../../src/u_port_gpio.c"
    "../../../../src/u_port_os.c"
    "../../../../src/u_port_uart.c"
    "../../../../../../clib/u_port_clib_mktime64.c"
    ${UBXLIB_SRC}
)
set(COMPONENT_PRIV_INCLUDEDIRS
    "../../../../src"
    "../../../../../../clib"
    ${UBXLIB_PRIVATE_INC}
)

# For crypto functions
set(COMPONENT_REQUIRES mbedtls)

register_component()

if (DEFINED ENV{U_FLAGS})
    separate_arguments(U_FLAGS NATIVE_COMMAND "$ENV{U_FLAGS}")
    target_compile_options(${COMPONENT_TARGET} PUBLIC ${U_FLAGS})
    message("ubxlib: added ${U_FLAGS} due to environment variable U_FLAGS.")
endif()
