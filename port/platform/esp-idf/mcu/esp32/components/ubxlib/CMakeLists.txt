message("Processing UBXLIB component starts")

get_filename_component(UBXLIB_BASE ${CMAKE_CURRENT_LIST_DIR}/../../../../../../../ ABSOLUTE)
get_filename_component(PLATFORM_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../.. ABSOLUTE)
set(ENV{UBXLIB_BASE} ${UBXLIB_BASE})

# Set the ubxlib features if the user hasn't
if (NOT DEFINED UBXLIB_FEATURES)
   set(UBXLIB_FEATURES short_range cell gnss)
endif()

if (geodesic IN_LIST UBXLIB_FEATURES)
   # For reasons that I don't undestand, and I spent
   # a whole day banging my head against a brick wall
   # with this, bringing geodesic in from ubxlib.cmake
   # as a CMake library, as we do with Linux and Windows,
   # just doesn't work with ESP-IDF: though it processes
   # the .cpp files it is unable to link against the
   # library that is produced at the end.
   # As a workaround, we have a separate geodesic component
   # for ourselves here in ESP-IDF, hence we need to
   # remove geodesic from UBXLIB_FEATURES before we
   # include ubxlib.cmake.
   set(GEODESIC_COMPONENT geodesic)
   list(REMOVE_ITEM UBXLIB_FEATURES geodesic)
endif()

# From this line we will get back:
# - UBXLIB_SRC
# - UBXLIB_INC
# - UBXLIB_PRIVATE_INC
# - UBXLIB_TEST_SRC
# - UBXLIB_TEST_INC
# and optionally:
# - UBXLIB_EXTRA_LIBS
# - UBXLIB_COMPILE_OPTIONS
include(${UBXLIB_BASE}/port/ubxlib.cmake)

if (GEODESIC_COMPONENT)
    # Put geodesic back again now
    list(APPEND UBXLIB_FEATURES geodesic)
    # Also add it to UBXLIB_EXTRA_LIBS, as
    # ubxlib.cmake would have
    list(APPEND UBXLIB_EXTRA_LIBS geodesic)
endif()


list(APPEND UBXLIB_INC
    ${PLATFORM_DIR}
    ${PLATFORM_DIR}/mcu/esp32/cfg
    ${PLATFORM_DIR}/../../clib
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
    ${PLATFORM_DIR}/src/u_port_spi.c
    ${PLATFORM_DIR}/src/u_port_ppp.c
    ${PLATFORM_DIR}/src/u_port_private.c
    ${PLATFORM_DIR}/../../clib/u_port_clib_mktime64.c
    ${PLATFORM_DIR}/../../u_port_timezone.c
    ${UBXLIB_SRC}
)
set(COMPONENT_PRIV_INCLUDEDIRS
    ${UBXLIB_PRIVATE_INC}
)

# Add the platform-specific tests and examples
list(APPEND UBXLIB_TEST_SRC
    ${PLATFORM_DIR}/test/u_espidf_ppp_test.c
    ${UBXLIB_BASE}/example/sockets/main_ppp_espidf.c
)

# Export these variables to parent so they can be picked up by ubxlib_runner
set(UBXLIB_TEST_SRC ${UBXLIB_TEST_SRC} PARENT_SCOPE)
set(UBXLIB_TEST_INC ${UBXLIB_TEST_INC} PARENT_SCOPE)
set(UBXLIB_INC ${UBXLIB_INC} PARENT_SCOPE)
set(UBXLIB_PRIVATE_INC ${UBXLIB_PRIVATE_INC} PARENT_SCOPE)
set(UBXLIB_BASE ${UBXLIB_BASE} PARENT_SCOPE)

# For crypto functions and, from ESP-IDF v5, for drivers, timers
# and the debug helper in esp_system
set(COMPONENT_REQUIRES "driver" "esp_timer" "esp_system" "esp_netif" ${UBXLIB_EXTRA_LIBS})

message("COMPONENT_REQUIRES for component UBXLIB is ${COMPONENT_REQUIRES}")

register_component()

if (DEFINED ENV{U_FLAGS})
    separate_arguments(U_FLAGS NATIVE_COMMAND "$ENV{U_FLAGS}")
    target_compile_options(${COMPONENT_TARGET} PUBLIC ${U_FLAGS})
    message("ubxlib: added ${U_FLAGS} due to environment variable U_FLAGS.")
endif()

if (GEODESIC_COMPONENT)
    # For reasons I don't understand, the PUBLIC
    # target_compile_definitions() in the geodesic component
    # aren't propagated here, despite the COMPONENT_REQUIRES
    # above, hence they are duplicated here
    target_compile_definitions(${COMPONENT_LIB} PUBLIC GEOGRAPHICLIB_SHARED_LIB=0
                               U_CFG_GEOFENCE_USE_GEODESIC)
endif()

message("Processing UBXLIB component ends")
