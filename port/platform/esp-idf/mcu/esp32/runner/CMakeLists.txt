# This is the project CMakeLists.txt file for the test subproject
cmake_minimum_required(VERSION 3.5)

message("Processing RUNNER build starts")

# Set the ubxlib features if the user hasn't
if (NOT DEFINED UBXLIB_FEATURES)
   set(UBXLIB_FEATURES short_range cell gnss)
endif()
if (geodesic IN_LIST UBXLIB_FEATURES)
   set(GEODESIC_COMPONENT geodesic)
endif()

# Include the body of stuff in the directory
# above, main and the tests component from below
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components"
                         "${CMAKE_CURRENT_LIST_DIR}/main"
                         "${CMAKE_CURRENT_LIST_DIR}/ubxlib_runner")

set(SDKCONFIG "sdkconfig" CACHE STRING "IDF sdkconfig file path")

# Reduce the component list to a minimum
set(COMPONENTS "driver" "esptool_py" "unity" "ubxlib" "main" "ubxlib_runner" "heap" ${GEODESIC_COMPONENT})

message("COMPONENTS for RUNNER build will be ${COMPONENTS}")

# Set the components to include the "tests" for.
# This is intended to be overriden in the CMake cache
# by invoking idf.py with a line of the following form:
# idf.py -D TEST_COMPONENTS="ubxlib_runner" all
set(TEST_COMPONENTS "replace-me"  CACHE STRING "Component to test")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ubxlib)

# Warnings as errors, while we're testing
idf_build_set_property(COMPILE_OPTIONS "-Werror" APPEND)

message("Processing RUNNER build ends")