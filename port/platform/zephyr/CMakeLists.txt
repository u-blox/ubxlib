# This CMake file will select the ubxlib source and include directories
# to include in the Zephyr application based on Kconfig. Use menuconfig
# to view the available options.
#
# TODO: Instead of compiling the files directly with the app this cmake
#       file should create an ubxlib library instead.
if(CONFIG_UBXLIB)

get_filename_component(UBXLIB_BASE ../../../ ABSOLUTE)
set(ENV{UBXLIB_BASE} ${UBXLIB_BASE})

# Add environment variables passed-in via U_FLAGS
if (DEFINED ENV{U_FLAGS})
    separate_arguments(U_FLAGS NATIVE_COMMAND "$ENV{U_FLAGS}")
    target_compile_options(app PRIVATE ${U_FLAGS})
    message("ubxlib: added ${U_FLAGS} due to environment variable U_FLAGS.")
endif()

# Set the features for ubxlib.cmake
list(APPEND UBXLIB_FEATURES u_lib)
if (CONFIG_UBXLIB_SHORTRANGE)
    list(APPEND UBXLIB_FEATURES short_range)
endif()
if (CONFIG_UBXLIB_CELL)
    list(APPEND UBXLIB_FEATURES cell)
endif()
if (CONFIG_UBXLIB_GNSS)
    list(APPEND UBXLIB_FEATURES gnss)
endif()

# From this line we will get back:
# - UBXLIB_SRC
# - UBXLIB_INC
# - UBXLIB_PRIVATE_INC
# - UBXLIB_TEST_SRC
# - UBXLIB_TEST_INC
include(${UBXLIB_BASE}/port/ubxlib.cmake)

# Set ubxlib source files
target_sources(app PRIVATE
    ${UBXLIB_SRC}
    src/u_port.c
    src/u_port_debug.c
    src/u_port_os.c
    src/u_port_gpio.c
    src/u_port_uart.c
    src/u_port_i2c.c
    src/u_port_private.c
    ${UBXLIB_BASE}/port/clib/u_port_clib_mktime64.c
    ${UBXLIB_BASE}/port/platform/common/mbedtls/u_port_crypto.c
)

if (CONFIG_UBXLIB_OPEN_CPU_BLE)
    target_sources(app PRIVATE src/u_port_gatt.c)
endif()

# Set the include directories
zephyr_include_directories(
    ${UBXLIB_INC}
    ${UBXLIB_PRIVATE_INC}
    cfg
    src
    ${UBXLIB_BASE}/port/clib
)

# Add test source & include dirs if selected
if (CONFIG_UBXLIB_TEST)
    target_compile_definitions(app PRIVATE UNITY_INCLUDE_CONFIG_H)

    target_sources(app PRIVATE
        ${UBXLIB_TEST_SRC}
        ${ZEPHYR_BASE}/../test/cmock/vendor/unity/src/unity.c
    )

    zephyr_include_directories(
        ${UBXLIB_PRIVATE_INC}
        ${UBXLIB_TEST_INC}
        ${ZEPHYR_BASE}/../test/cmock/vendor/unity/src
    )
endif()

if (CONFIG_MINIMAL_LIBC)
    target_sources(app PRIVATE
        ${UBXLIB_BASE}/port/clib/u_port_clib_isblank.c
        ${UBXLIB_BASE}/port/clib/u_port_clib_mktime.c
        ${UBXLIB_BASE}/port/clib/u_port_setjmp.S
        src/u_port_clib.c
    )
endif()

if (CONFIG_UBXLIB_EDM_STREAM_DEBUG)
    target_compile_definitions(app PRIVATE U_CFG_SHORT_RANGE_EDM_STREAM_DEBUG)
endif()

if (CONFIG_UBXLIB_EDM_STREAM_DEBUG_COLOR)
    target_compile_definitions(app PRIVATE U_CFG_SHORT_RANGE_EDM_STREAM_DEBUG_COLOR)
endif()

if (CONFIG_UBXLIB_EDM_STREAM_DEBUG_DUMP_DATA)
    target_compile_definitions(app PRIVATE U_CFG_SHORT_RANGE_EDM_STREAM_DEBUG_DUMP_DATA)
endif()

endif() #CONFIG_UBXLIB