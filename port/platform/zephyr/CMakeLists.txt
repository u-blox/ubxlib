# This CMake file will select the ubxlib source and include directories
# to include in the Zephyr application based on Kconfig. Use menuconfig
# to view the available options.
if(CONFIG_UBXLIB)

zephyr_library_named(ubxlib)

# we need to be able to include generated header files
zephyr_include_directories()

get_filename_component(UBXLIB_BASE ../../../ ABSOLUTE)
set(ENV{UBXLIB_BASE} ${UBXLIB_BASE})

# Add environment variables passed-in via U_FLAGS
if (DEFINED ENV{U_FLAGS})
    separate_arguments(U_FLAGS NATIVE_COMMAND "$ENV{U_FLAGS}")
    target_compile_options(ubxlib PRIVATE ${U_FLAGS})
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
target_sources(ubxlib PRIVATE
    ${UBXLIB_SRC}
    src/u_port.c
    src/u_port_debug.c
    src/u_port_os.c
    src/u_port_gpio.c
    src/u_port_uart.c
    src/u_port_i2c.c
    src/u_port_spi.c
    src/u_port_private.c
    ${UBXLIB_BASE}/port/clib/u_port_clib_mktime64.c
    ${UBXLIB_BASE}/port/platform/common/mbedtls/u_port_crypto.c
)

if (CONFIG_UBXLIB_OPEN_CPU_BLE)
    target_sources(ubxlib PRIVATE src/u_port_gatt.c)
endif()

# Set the include directories
zephyr_include_directories(
    ${UBXLIB_INC}
    ${UBXLIB_PRIVATE_INC}
    cfg
    src
    ${UBXLIB_BASE}/port/clib
    ${ZEPHYR_BASE}/include/zephyr
)

# Add test source & include dirs if selected
if (CONFIG_UBXLIB_TEST)
    target_compile_definitions(ubxlib PRIVATE UNITY_INCLUDE_CONFIG_H)

    target_sources(ubxlib PRIVATE
        ${UBXLIB_TEST_SRC}
    )

    zephyr_include_directories(
        ${UBXLIB_PRIVATE_INC}
        ${UBXLIB_TEST_INC}
    )

    # we need to build https://github.com/ThrowTheSwitch/unity
    # Path resolution order:
    # 1. environment variable UNITY_PATH
    # 2. if ubxlib is imported by zephyr west, it will be at
    #    ${ZEPHYR_BASE}/../modules/test/cmock/vendor/unity
    # 3. if the nRF Connect SDK is inside our workspace, it will be at
    #    ${ZEPHYR_BASE}/../test/cmock/vendor/unity
    # 4. otherwise the platform itself must build and include it
    if (DEFINED $ENV{UNITY_PATH})
        set(UNITY_PATH $ENV{UNITY_PATH})
    elseif (EXISTS "${ZEPHYR_BASE}/../modules/test/unity")
        # this is the location if west fetched unity for us
        set(UNITY_PATH "${ZEPHYR_BASE}/../modules/test/unity")
    elseif (EXISTS "${ZEPHYR_BASE}/../test/cmock/vendor/unity")
        #this is where the nRF Connect SDK puts unity
        set(UNITY_PATH "${ZEPHYR_BASE}/../test/cmock/vendor/unity")
    endif()

    if (DEFINED UNITY_PATH)
        message("Unity from ${UNITY_PATH} will be used")
        target_sources(ubxlib PRIVATE
            ${UNITY_PATH}/src/unity.c
        )
        zephyr_include_directories(
            ${UNITY_PATH}/src
        )
    else()
        message("Assuming that the platform will bring in Unity")
    endif()
endif()

if (CONFIG_MINIMAL_LIBC)
    target_sources(ubxlib PRIVATE
        ${UBXLIB_BASE}/port/clib/u_port_clib_isblank.c
        ${UBXLIB_BASE}/port/clib/u_port_clib_mktime.c
        ${UBXLIB_BASE}/port/clib/u_port_setjmp.S
        src/u_port_clib.c
    )
endif()

if (CONFIG_UBXLIB_EDM_STREAM_DEBUG)
    target_compile_definitions(ubxlib PRIVATE U_CFG_SHORT_RANGE_EDM_STREAM_DEBUG)
endif()

if (CONFIG_UBXLIB_EDM_STREAM_DEBUG_COLOR)
    target_compile_definitions(ubxlib PRIVATE U_CFG_SHORT_RANGE_EDM_STREAM_DEBUG_COLOR)
endif()

if (CONFIG_UBXLIB_EDM_STREAM_DEBUG_DUMP_DATA)
    target_compile_definitions(ubxlib PRIVATE U_CFG_SHORT_RANGE_EDM_STREAM_DEBUG_DUMP_DATA)
endif()

endif() #CONFIG_UBXLIB
