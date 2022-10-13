# This is a shared CMake file used for the ports using CMake build system.
# It is used for collecting source code files and include directories
# that are selected based on UBXLIB_FEATURES. Check README.md for details.
cmake_minimum_required(VERSION 3.13.1)

# Always include base feature
list(APPEND UBXLIB_FEATURES base)

# Conditionally add one .c file to UBXLIB_SRC
function(u_add_source_file feature file)
  if (NOT EXISTS ${file})
    message(FATAL_ERROR "File does not exist: ${file}")
  endif()
  if (${feature} IN_LIST UBXLIB_FEATURES)
    list(APPEND UBXLIB_SRC ${file})
    set(UBXLIB_SRC ${UBXLIB_SRC} PARENT_SCOPE)
  endif()
endfunction()

# Conditionally add all .c files in a directory to UBXLIB_SRC
function(u_add_source_dir feature src_dir)
  if (NOT EXISTS ${src_dir})
    message(FATAL_ERROR "Directory does not exist: ${src_dir}")
  endif()
  if (${feature} IN_LIST UBXLIB_FEATURES)
    file(GLOB SRCS ${src_dir}/*.c)
    list(APPEND UBXLIB_SRC ${SRCS})
    set(UBXLIB_SRC ${UBXLIB_SRC} PARENT_SCOPE)
  endif()
endfunction()

# Conditionally add all .c files in a directory to UBXLIB_TEST_SRC
function(u_add_test_source_dir feature src_dir)
  if (NOT EXISTS ${src_dir})
    message(FATAL_ERROR "Directory does not exist: ${src_dir}")
  endif()
  if (${feature} IN_LIST UBXLIB_FEATURES)
    file(GLOB SRCS ${src_dir}/*.c)
    if (NOT SRCS)
      message(FATAL_ERROR "No source files found in directory: ${src_dir}")
    endif()
    list(APPEND UBXLIB_TEST_SRC ${SRCS})
    set(UBXLIB_TEST_SRC ${UBXLIB_TEST_SRC} PARENT_SCOPE)
  endif()
endfunction()

# This function will take a module directory and:
# - Add <module_dir>/src/*.c to UBXLIB_SRC
# - Add <module_dir>/src to UBXLIB_PRIVATE_INC
# - Add <module_dir>/api to UBXLIB_INC
# - Add <module_dir>/test/*.c to UBXLIB_TEST_SRC
# - Add <module_dir>/test to UBXLIB_TEST_INC
# but only if the feature is enabled
function(u_add_module_dir feature module_dir)
  if (NOT EXISTS ${module_dir})
    message(FATAL_ERROR "Directory does not exist: ${module_dir}")
  endif()
  if (${feature} IN_LIST UBXLIB_FEATURES)
    if(EXISTS ${module_dir}/src)
      u_add_source_dir(${feature} ${module_dir}/src)
    endif()
    if(EXISTS ${module_dir}/test)
      u_add_test_source_dir(${feature} ${module_dir}/test)
    endif()
    if(EXISTS ${module_dir}/api)
      list(APPEND UBXLIB_INC ${module_dir}/api)
    endif()
    if(EXISTS ${module_dir}/src)
      list(APPEND UBXLIB_PRIVATE_INC ${module_dir}/src)
    endif()
    if(EXISTS ${module_dir}/test)
      list(APPEND UBXLIB_TEST_INC ${module_dir}/test)
    endif()
    set(UBXLIB_SRC ${UBXLIB_SRC} PARENT_SCOPE)
    set(UBXLIB_INC ${UBXLIB_INC} PARENT_SCOPE)
    set(UBXLIB_PRIVATE_INC ${UBXLIB_PRIVATE_INC} PARENT_SCOPE)
    set(UBXLIB_TEST_SRC ${UBXLIB_TEST_SRC} PARENT_SCOPE)
    set(UBXLIB_TEST_INC ${UBXLIB_TEST_INC} PARENT_SCOPE)
  endif()
endfunction()


# ubxlib base source and includes

# Add /api, /src and /test sub folders for these:
u_add_module_dir(base ${UBXLIB_BASE}/common/at_client)
u_add_module_dir(base ${UBXLIB_BASE}/common/error)
u_add_module_dir(base ${UBXLIB_BASE}/common/assert)
u_add_module_dir(base ${UBXLIB_BASE}/common/location)
u_add_module_dir(base ${UBXLIB_BASE}/common/mqtt_client)
u_add_module_dir(base ${UBXLIB_BASE}/common/http_client)
u_add_module_dir(base ${UBXLIB_BASE}/common/security)
u_add_module_dir(base ${UBXLIB_BASE}/common/sock)
u_add_module_dir(base ${UBXLIB_BASE}/common/ubx_protocol)
u_add_module_dir(base ${UBXLIB_BASE}/common/spartn)
u_add_module_dir(base ${UBXLIB_BASE}/common/utils)
u_add_module_dir(base ${UBXLIB_BASE}/port/platform/common/debug_utils)

# Additional source directories
u_add_source_dir(base ${UBXLIB_BASE}/port/platform/common/event_queue)
u_add_source_dir(base ${UBXLIB_BASE}/port/platform/common/mutex_debug)
u_add_source_dir(base ${UBXLIB_BASE}/port/platform/common/log_ram)

# Additional include directories
list(APPEND UBXLIB_INC
  ${UBXLIB_BASE}
  ${UBXLIB_BASE}/cfg
  ${UBXLIB_BASE}/port/api
)

list(APPEND UBXLIB_PRIVATE_INC
  ${UBXLIB_BASE}/port/platform/common/event_queue
  ${UBXLIB_BASE}/port/platform/common/mutex_debug
  ${UBXLIB_BASE}/port/platform/common/log_ram
)

# Device and network require special care since they contains stub & optional files
list(APPEND UBXLIB_SRC ${UBXLIB_BASE}/common/network/src/u_network.c)
list(APPEND UBXLIB_SRC ${UBXLIB_BASE}/common/network/src/u_network_shared.c)
list(APPEND UBXLIB_INC ${UBXLIB_BASE}/common/network/api)
list(APPEND UBXLIB_PRIVATE_INC ${UBXLIB_BASE}/common/network/src)
list(APPEND UBXLIB_SRC ${UBXLIB_BASE}/common/device/src/u_device.c)
list(APPEND UBXLIB_SRC ${UBXLIB_BASE}/common/device/src/u_device_shared.c)
list(APPEND UBXLIB_SRC ${UBXLIB_BASE}/common/device/src/u_device_private.c)
list(APPEND UBXLIB_INC ${UBXLIB_BASE}/common/device/api)
list(APPEND UBXLIB_PRIVATE_INC ${UBXLIB_BASE}/common/device/src)

# Optional features

# short range
u_add_module_dir(short_range ${UBXLIB_BASE}/common/short_range)
u_add_module_dir(short_range ${UBXLIB_BASE}/ble)
u_add_module_dir(short_range ${UBXLIB_BASE}/wifi)
u_add_source_file(short_range ${UBXLIB_BASE}/common/network/src/u_network_private_ble_extmod.c)
u_add_source_file(short_range ${UBXLIB_BASE}/common/network/src/u_network_private_ble_intmod.c)
u_add_source_file(short_range ${UBXLIB_BASE}/common/network/src/u_network_private_wifi.c)
u_add_source_file(short_range ${UBXLIB_BASE}/common/device/src/u_device_private_short_range.c)
# cell
u_add_module_dir(cell ${UBXLIB_BASE}/cell)
u_add_source_file(cell ${UBXLIB_BASE}/common/network/src/u_network_private_cell.c)
u_add_source_file(cell ${UBXLIB_BASE}/common/device/src/u_device_private_cell.c)
# gnss
u_add_module_dir(gnss ${UBXLIB_BASE}/gnss)
u_add_source_file(gnss ${UBXLIB_BASE}/common/network/src/u_network_private_gnss.c)
u_add_source_file(gnss ${UBXLIB_BASE}/common/device/src/u_device_private_gnss.c)
# lib_common
# We have a dependency issue with libfibonacci so lib_common/test needs to manually
# included by the runner app instead at the moment. For this reason we just add the
# source and include dir here.
if (u_lib IN_LIST UBXLIB_FEATURES)
  list(APPEND UBXLIB_INC ${UBXLIB_BASE}/common/lib_common/api)
  u_add_source_dir(u_lib ${UBXLIB_BASE}/common/lib_common/src)
endif()

# Test related files and directories
list(APPEND UBXLIB_TEST_INC
  ${UBXLIB_BASE}/common/network/test
)
u_add_test_source_dir(base ${UBXLIB_BASE}/port/platform/common/test)
u_add_test_source_dir(base ${UBXLIB_BASE}/port/test)
u_add_test_source_dir(base ${UBXLIB_BASE}/common/network/test)
# Examples are compiled as tests
u_add_test_source_dir(base ${UBXLIB_BASE}/example/sockets)
u_add_test_source_dir(base ${UBXLIB_BASE}/example/security/e2e)
u_add_test_source_dir(base ${UBXLIB_BASE}/example/security/psk)
u_add_test_source_dir(base ${UBXLIB_BASE}/example/security/c2c)
u_add_test_source_dir(base ${UBXLIB_BASE}/example/mqtt_client)
u_add_test_source_dir(base ${UBXLIB_BASE}/example/http_client)
u_add_test_source_dir(base ${UBXLIB_BASE}/example/location)
u_add_test_source_dir(base ${UBXLIB_BASE}/example/cell/lte_cfg)
u_add_test_source_dir(base ${UBXLIB_BASE}/example/cell/power_saving)
u_add_test_source_dir(base ${UBXLIB_BASE}/example/gnss)
u_add_test_source_dir(base ${UBXLIB_BASE}/example/utilities/c030_module_fw_update)
