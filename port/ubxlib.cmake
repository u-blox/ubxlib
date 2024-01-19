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
  # Includes are always brought in: costs nothing
  if(EXISTS ${module_dir}/api)
    list(APPEND UBXLIB_INC ${module_dir}/api)
  endif()
  if(EXISTS ${module_dir}/src)
    list(APPEND UBXLIB_PRIVATE_INC ${module_dir}/src)
  endif()
  if(EXISTS ${module_dir}/test)
    list(APPEND UBXLIB_TEST_INC ${module_dir}/test)
  endif()
  set(UBXLIB_INC ${UBXLIB_INC} PARENT_SCOPE)
  set(UBXLIB_PRIVATE_INC ${UBXLIB_PRIVATE_INC} PARENT_SCOPE)
  set(UBXLIB_TEST_INC ${UBXLIB_TEST_INC} PARENT_SCOPE)
  # Source files only brought in if the feature is present
  if (${feature} IN_LIST UBXLIB_FEATURES)
    if(EXISTS ${module_dir}/src)
      u_add_source_dir(${feature} ${module_dir}/src)
    endif()
    if(EXISTS ${module_dir}/test)
      u_add_test_source_dir(${feature} ${module_dir}/test)
    endif()
    set(UBXLIB_SRC ${UBXLIB_SRC} PARENT_SCOPE)
    set(UBXLIB_TEST_SRC ${UBXLIB_TEST_SRC} PARENT_SCOPE)
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
u_add_module_dir(base ${UBXLIB_BASE}/common/dns)
u_add_module_dir(base ${UBXLIB_BASE}/common/geofence)
u_add_module_dir(base ${UBXLIB_BASE}/port/platform/common/debug_utils)

# Additional source directories
u_add_source_dir(base ${UBXLIB_BASE}/port/platform/common/event_queue)
u_add_source_dir(base ${UBXLIB_BASE}/port/platform/common/mutex_debug)
u_add_source_dir(base ${UBXLIB_BASE}/port/platform/common/log_ram)


# Additional include directories
list(APPEND UBXLIB_INC
  ${UBXLIB_BASE}
  ${UBXLIB_BASE}/cfg
  ${UBXLIB_BASE}/common/type/api
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
list(APPEND UBXLIB_SRC ${UBXLIB_BASE}/common/network/src/u_network_private_ble_extmod_stub.c)
list(APPEND UBXLIB_SRC ${UBXLIB_BASE}/common/network/src/u_network_private_cell_stub.c)
list(APPEND UBXLIB_SRC ${UBXLIB_BASE}/common/network/src/u_network_private_gnss_stub.c)
list(APPEND UBXLIB_SRC ${UBXLIB_BASE}/common/network/src/u_network_private_wifi_stub.c)
list(APPEND UBXLIB_INC ${UBXLIB_BASE}/common/network/api)
list(APPEND UBXLIB_PRIVATE_INC ${UBXLIB_BASE}/common/network/src)
list(APPEND UBXLIB_SRC ${UBXLIB_BASE}/common/device/src/u_device.c)
list(APPEND UBXLIB_SRC ${UBXLIB_BASE}/common/device/src/u_device_serial.c)
list(APPEND UBXLIB_SRC ${UBXLIB_BASE}/common/device/src/u_device_shared.c)
list(APPEND UBXLIB_SRC ${UBXLIB_BASE}/common/device/src/u_device_private.c)
list(APPEND UBXLIB_SRC ${UBXLIB_BASE}/common/device/src/u_device_private_cell_stub.c)
list(APPEND UBXLIB_SRC ${UBXLIB_BASE}/common/device/src/u_device_private_gnss_stub.c)
list(APPEND UBXLIB_SRC ${UBXLIB_BASE}/common/device/src/u_device_private_short_range_stub.c)
list(APPEND UBXLIB_INC ${UBXLIB_BASE}/common/device/api)
list(APPEND UBXLIB_PRIVATE_INC ${UBXLIB_BASE}/common/device/src)

# CPP file required for geofencing
list(APPEND UBXLIB_SRC ${UBXLIB_BASE}/common/geofence/src/u_geofence_geodesic.cpp)

# Default malloc()/free() implementation
list(APPEND UBXLIB_SRC ${UBXLIB_BASE}/port/u_port_heap.c)

# Default uPortGetTimezoneOffsetSeconds() implementation
list(APPEND UBXLIB_SRC ${UBXLIB_BASE}/port/u_port_timezone.c)

# Default uPortXxxResource implementation
list(APPEND UBXLIB_SRC ${UBXLIB_BASE}/port/u_port_resource.c)

# Default uPortPppAttach()/uPortPppDetach() implementation
list(APPEND UBXLIB_SRC ${UBXLIB_BASE}/port/u_port_ppp_default.c)

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
u_add_source_file(gnss ${UBXLIB_BASE}/gnss/src/lib_mga/u_lib_mga.c)
u_add_source_file(gnss ${UBXLIB_BASE}/common/network/src/u_network_private_gnss.c)
u_add_source_file(gnss ${UBXLIB_BASE}/common/device/src/u_device_private_gnss.c)
list(APPEND UBXLIB_PRIVATE_INC ${UBXLIB_BASE}/gnss/src/lib_mga)

# Bring in linker workaround files, needed for ESP-IDF (and no harm for others)
if (NOT short_range IN_LIST UBXLIB_FEATURES)
  list(APPEND UBXLIB_SRC ${UBXLIB_BASE}/common/device/src/u_device_private_short_range_link.c)
  list(APPEND UBXLIB_SRC ${UBXLIB_BASE}/common/network/src/u_network_private_ble_extmod_link.c)
  list(APPEND UBXLIB_SRC ${UBXLIB_BASE}/common/network/src/u_network_private_wifi_link.c)
endif()
if (NOT cell IN_LIST UBXLIB_FEATURES)
  list(APPEND UBXLIB_SRC ${UBXLIB_BASE}/common/device/src/u_device_private_cell_link.c)
  list(APPEND UBXLIB_SRC ${UBXLIB_BASE}/common/network/src/u_network_private_cell_link.c)
endif()
if (NOT gnss IN_LIST UBXLIB_FEATURES)
  list(APPEND UBXLIB_SRC ${UBXLIB_BASE}/common/device/src/u_device_private_gnss_link.c)
  list(APPEND UBXLIB_SRC ${UBXLIB_BASE}/common/network/src/u_network_private_gnss_link.c)
endif()

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
  ${UBXLIB_BASE}/port/platform/common/test_util
)
u_add_test_source_dir(base ${UBXLIB_BASE}/port/platform/common/test_util)
u_add_test_source_dir(base ${UBXLIB_BASE}/port/platform/common/test)
u_add_test_source_dir(base ${UBXLIB_BASE}/port/test)
u_add_test_source_dir(base ${UBXLIB_BASE}/common/device/test)
u_add_test_source_dir(base ${UBXLIB_BASE}/common/network/test)
# Examples are compiled as tests
u_add_test_source_dir(base ${UBXLIB_BASE}/example/sockets)
u_add_test_source_dir(base ${UBXLIB_BASE}/example/security)
u_add_test_source_dir(base ${UBXLIB_BASE}/example/mqtt_client)
u_add_test_source_dir(base ${UBXLIB_BASE}/example/http_client)
u_add_test_source_dir(base ${UBXLIB_BASE}/example/location)
u_add_test_source_dir(base ${UBXLIB_BASE}/example/cell/lte_cfg)
u_add_test_source_dir(base ${UBXLIB_BASE}/example/cell/power_saving)
u_add_test_source_dir(base ${UBXLIB_BASE}/example/gnss)
u_add_test_source_dir(base ${UBXLIB_BASE}/example/utilities/c030_module_fw_update)

# If required, bring in the geodesic library and define
# U_CFG_GNSS_FENCE_USE_GEODESIC, needed if
# U_CFG_GEOFENCE is defined and shapes > 1 km
# in size are employed.
# NOTE: this works fine on "native" CMake systems,
# exporting a CMake variable UBXLIB_EXTRA_LIBS which
# can be included in target_link_libraries() to
# cause any extra libraries to be linked and
# UBXLIB_COMPILE_OPTIONS, which can be added to
# target_compile_definitions(). HOWEVER, it doesn't
# work for ESP-IDF, which has a "helful" component
# system of its own stuck on top, hence BE AWARE
# THAT that ESP-IDF doesn't use the bit below...
#
# ...except that, for reasons I don't understand, the
# include path for the ESP-IDF geodesic component simply
# does not propagate to the ubxlib component as it should;
# to compensate, we always add the path to the
# GeographicLib header files to UBXLIB_PRIVATE_INC here.
set(GEODESIC_DIR ${UBXLIB_BASE}/common/geofence/geographiclib)
set(GEODESIC_INC ${GEODESIC_DIR}/include
                 ${GEODESIC_DIR}/include/GeographicLib)
list(APPEND UBXLIB_PRIVATE_INC ${GEODESIC_INC})

if (geodesic IN_LIST UBXLIB_FEATURES)
  file(GLOB SRCS ${GEODESIC_DIR}/src/*.cpp)
  set(GEODESIC_SRC ${SRCS})
  set(GEOGRAPHICLIB_PRECISION 2)
  configure_file (
    ${GEODESIC_DIR}/include/GeographicLib/Config.h.in
    ${GEODESIC_DIR}/include/GeographicLib/Config.h
    @ONLY)
  # List rather than set so that options can be passed
  # into this script
  list(APPEND UBXLIB_COMPILE_OPTIONS -DGEOGRAPHICLIB_SHARED_LIB=0
       -DU_CFG_GEOFENCE_USE_GEODESIC)
  add_library(geodesic STATIC ${GEODESIC_SRC})
  SET_TARGET_PROPERTIES(geodesic PROPERTIES CXX_STANDARD 11)
  target_compile_options(geodesic PRIVATE ${UBXLIB_COMPILE_OPTIONS})
  target_include_directories(geodesic PRIVATE ${GEODESIC_INC})
  # List rather than set so that static libaries could,
  # potentially, be passed into this script
  list(APPEND UBXLIB_EXTRA_LIBS geodesic)
endif()