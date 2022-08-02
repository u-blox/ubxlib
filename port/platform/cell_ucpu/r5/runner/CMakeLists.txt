cmake_minimum_required(VERSION 3.13.1)

set( APPLICATION_NAME ubxlib_test_app )
project(${APPLICATION_NAME})
    
enable_language(CXX C ASM)
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} --apcs=/ropi/rwpi --lower_ropi")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} --show_cmdline --apcs=/ropi/rwpi")

add_definitions(-DU_CFG_APP_FILTER=${U_CFG_APP_FILTER} -DU_AT_CLIENT_URC_TASK_PRIORITY -DU_CFG_TEST_CELL_MODULE_TYPE=U_CELL_MODULE_TYPE_SARA_R5 -DUNITY_INCLUDE_CONFIG_H)

# Tell everyone where ubxlib is
set(UBXLIB_BASE "${CMAKE_CURRENT_LIST_DIR}/../../../../.." CACHE PATH "the path to the ubxlib root directory")

# Set the ubxlib platform we are building for
set(UBXLIB_PLATFORM "cell_ucpu/r5" CACHE PATH "the name of the ubxlib platform to build for")

##################################
# Third Party stuff
##################################

message("Adding \"txmlib\"...")

# Tell everyone where threadx is
set(THREADX_API "${UBXLIB_BASE}/../cell_ucpu_sdk/r5/ThreadX5.8/api" CACHE PATH "the path to threadx api directory")
message("CMake variable THREADX_API will be \"${THREADX_API}\"")
set(THREADX_LIB "${UBXLIB_BASE}/../cell_ucpu_sdk/r5/ThreadX5.8/lib" CACHE PATH "the path to threadx library")
message("CMake variable THREADX_LIB will be \"${THREADX_LIB}\"")

set(THREADX_INC ${THREADX_API})

add_library(txmlib STATIC IMPORTED)
set_target_properties(txmlib PROPERTIES
    IMPORTED_LOCATION "${THREADX_LIB}/txm_arm_cortexm7.lib"
)
target_include_directories(txmlib INTERFACE ${THREADX_INC} )

##################################
# UCPU stuff
##################################

message("Adding \"ucpulib\"...")

set(UCPU_API "${UBXLIB_BASE}/../cell_ucpu_sdk/r5/api" CACHE PATH "the path to the UCPU api directory")
message("CMake variable UCPULIB will be \"${UCPU_API}\"")
set(UCPU_LIB "${UBXLIB_BASE}/../cell_ucpu_sdk/r5/lib" CACHE PATH "the path to the UCPU library")
message("CMake variable UCPULIB will be \"${UCPU_LIB}\"")

set(UCPULIB_INCLUDES ${UCPU_API})

add_library(ucpulib STATIC IMPORTED)
set_target_properties(ucpulib PROPERTIES
    IMPORTED_LOCATION "${UCPU_LIB}/ucpuapi_arm_cortexm7.lib"
)
target_include_directories(ucpulib INTERFACE ${UCPULIB_INCLUDES} )

##################################
# UBXLIB stuff
##################################

message("Adding \"ubxlib\"...")

# Tell everyone where mbedtls is
set(MBEDTLS_SUBDIR "${UBXLIB_BASE}/../mbedtls" CACHE PATH "the path to the mbedtls directory")
message("CMake variable MBEDTLS_SUBDIR will be \"${MBEDTLS_SUBDIR}\"")

# Tell everyone where the directory Unity is (used in testing)
set(UNITY_SUBDIR "${UBXLIB_BASE}/../Unity" CACHE PATH "the path to the Unity directory")
message("CMake variable UNITY_SUBDIR will be \"${UNITY_SUBDIR}\"")

# Set the ubxlib features to compile (all must be enabled at the moment)
# These will have an effect down in the included ubxlib .cmake file
set(UBXLIB_FEATURES short_range cell gnss)
message("CMake variable UBXLIB_FEATURES will be \"${UBXLIB_FEATURES}\"")

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

set(UBXLIB_PUBLIC_INC_PORT
    ${UBXLIB_BASE}/port/platform/${UBXLIB_PLATFORM}
    ${UBXLIB_BASE}/port/platform/${UBXLIB_PLATFORM}/cfg
    ${UBXLIB_BASE}/port/clib
    ${MBEDTLS_SUBDIR}/include
    ${MBEDTLS_SUBDIR}/library)
set(UBXLIB_PRIVATE_INC_PORT
    ${UBXLIB_BASE}/port/platform/${UBXLIB_PLATFORM}/src)
set(UBXLIB_SRC_PORT
    ${UBXLIB_BASE}/port/platform/${UBXLIB_PLATFORM}/src/u_port.c
    ${UBXLIB_BASE}/port/platform/${UBXLIB_PLATFORM}/src/u_port_debug.c
    ${UBXLIB_BASE}/port/platform/${UBXLIB_PLATFORM}/src/u_port_os.c
    ${UBXLIB_BASE}/port/platform/${UBXLIB_PLATFORM}/src/u_port_gpio.c
    ${UBXLIB_BASE}/port/platform/${UBXLIB_PLATFORM}/src/u_port_uart.c
    ${UBXLIB_BASE}/port/platform/${UBXLIB_PLATFORM}/src/u_port_i2c.c
    ${UBXLIB_BASE}/port/platform/${UBXLIB_PLATFORM}/src/u_port_clib.c
    ${UBXLIB_BASE}/port/platform/${UBXLIB_PLATFORM}/src/u_port_private.c
    ${UBXLIB_BASE}/port/platform/common/mbedtls/u_port_crypto.c
    ${UBXLIB_BASE}/port/clib/u_port_clib_mktime64.c
    ${UBXLIB_BASE}/port/clib/u_port_clib_strtok_r.c
    ${MBEDTLS_SUBDIR}/library/sha256.c
    ${MBEDTLS_SUBDIR}/library/md.c
    ${MBEDTLS_SUBDIR}/library/md5.c
    ${MBEDTLS_SUBDIR}/library/aes.c  
    ${MBEDTLS_SUBDIR}/library/sha1.c
    ${MBEDTLS_SUBDIR}/library/platform_util.c
    ${MBEDTLS_SUBDIR}/library/ripemd160.c
    ${MBEDTLS_SUBDIR}/library/sha512.c)
set(UBXLIB_TEST_SRC_PORT
    ${UBXLIB_BASE}/port/platform/common/runner/u_runner.c)
set(UBXLIB_PRIVATE_TEST_INC_PORT
    ${UBXLIB_BASE}/port/platform/common/runner)

# Using the above, create the ubxlib library and its headers.
add_library(ubxlib ${UBXLIB_SRC} ${UBXLIB_SRC_PORT})
target_include_directories(ubxlib PUBLIC ${UBXLIB_INC} ${UBXLIB_PUBLIC_INC_PORT})
target_include_directories(ubxlib PRIVATE ${UBXLIB_PRIVATE_INC}
                                          ${UBXLIB_PRIVATE_INC_PORT}
                                          ${THREADX_INC}
                                          ${UCPULIB_INCLUDES}
                                          ${UNITY_SUBDIR}/src)

##################################
# The final target
##################################

# This function will take a test directory and:
# - Add <test_dir>/*.c to UBXLIB_TEST_SRC_FOR_R5
function(add_ubxlib_tests src_dir)
    if (NOT EXISTS ${src_dir})
        message(FATAL_ERROR "Directory does not exist: ${src_dir}")
    endif()
    file(GLOB SRCS ${src_dir}/*.c)
    if (NOT SRCS)
        message(FATAL_ERROR "No source files found in directory: ${src_dir}")
    endif()
    list(APPEND UBXLIB_TEST_SRC_FOR_R5 ${SRCS})
    set(UBXLIB_TEST_SRC_FOR_R5 ${UBXLIB_TEST_SRC_FOR_R5} PARENT_SCOPE)
endfunction()

# Add test source files to UBXLIB_TEST_SRC_FOR_R5
# according to the U_CFG_TEST_FILTER provided.

if ((U_CFG_TEST_FILTER STREQUAL port) OR
    (U_CFG_TEST_FILTER STREQUAL cell)
    )
    add_ubxlib_tests(${UBXLIB_BASE}/${U_CFG_TEST_FILTER}/test)
elseif ((U_CFG_TEST_FILTER STREQUAL mqttClient) OR
        (U_CFG_TEST_FILTER STREQUAL network) OR
        (U_CFG_TEST_FILTER STREQUAL security) OR
        (U_CFG_TEST_FILTER STREQUAL sock)
        )
    add_ubxlib_tests(${UBXLIB_BASE}/common/mqtt_client/test)
    add_ubxlib_tests(${UBXLIB_BASE}/common/network/test)
    add_ubxlib_tests(${UBXLIB_BASE}/common/location/test)
    add_ubxlib_tests(${UBXLIB_BASE}/common/security/test)
    add_ubxlib_tests(${UBXLIB_BASE}/common/sock/test)
elseif (U_CFG_TEST_FILTER STREQUAL ubxProtocol)
    add_ubxlib_tests(${UBXLIB_BASE}/common/ubx_protocol/test)
elseif (U_CFG_TEST_FILTER STREQUAL exampleCell)
    add_ubxlib_tests(${UBXLIB_BASE}/example/cell/lte_cfg)
elseif (U_CFG_TEST_FILTER STREQUAL exampleMqtt)
    add_ubxlib_tests(${UBXLIB_BASE}/example/mqtt_client)
elseif (U_CFG_TEST_FILTER STREQUAL exampleSec)
    add_ubxlib_tests(${UBXLIB_BASE}/example/security/c2c)
    add_ubxlib_tests(${UBXLIB_BASE}/example/security/e2e)
    add_ubxlib_tests(${UBXLIB_BASE}/example/security/psk)
elseif (U_CFG_TEST_FILTER STREQUAL exampleSockets)
    add_ubxlib_tests(${UBXLIB_BASE}/example/sockets)
else ()
    message("No test or example provided.")
endif()

# Create a test target for ubxlib which can be used to run
# all of its tests, including in it u_main.c
set(TARGET_NAME ${APPLICATION_NAME}_main)
message("Adding ubxlib test target \"${TARGET_NAME}\"")
add_executable(${TARGET_NAME} ${UBXLIB_BASE}/port/platform/${UBXLIB_PLATFORM}/app/u_main.c
               ${UBXLIB_BASE}/port/platform/${UBXLIB_PLATFORM}/runner/txm_preamble.s
               ${UBXLIB_TEST_SRC_FOR_R5}
               ${UBXLIB_TEST_SRC_PORT}
               ${UNITY_SUBDIR}/src/unity.c)
target_include_directories(${TARGET_NAME} PRIVATE
                           ${UBXLIB_TEST_INC}
                           ${UBXLIB_PRIVATE_TEST_INC_PORT}
                           ${UBXLIB_INC}
                           ${UBXLIB_PRIVATE_INC}
                           ${UBXLIB_PUBLIC_INC_PORT}
                           ${UBXLIB_PRIVATE_INC_PORT}
                           ${UNITY_SUBDIR}/src
                           ${THREADX_INC})

# Link the ubxlib test target with the required libraries
target_link_libraries(${TARGET_NAME} PRIVATE txmlib ucpulib ubxlib)

set ( TXM_LINKER_FLAGS "--elf --first=txm_preamble.s.o --entry=_txm_module_thread_shell_entry --ropi --rwpi --info=sizes,totals,summarysizes,summarystack --remove --map --symbols --list ${TARGET_NAME}.map")

# Unset already set linker options
unset(CMAKE_EXE_LINKER_FLAGS CACHE)

set_target_properties(${TARGET_NAME} PROPERTIES
                      LINK_FLAGS ${TXM_LINKER_FLAGS}
                      )

add_custom_command( TARGET ${TARGET_NAME}
                    POST_BUILD
                    COMMAND  fromelf --bin --output ${TARGET_NAME}.bin ${TARGET_NAME}.axf
                    )

