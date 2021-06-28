# This is the project CMakeLists.txt file for the test subproject
cmake_minimum_required(VERSION 3.5)

# Include the body of stuff in the directory
# above, main and the tests component from below
set(EXTRA_COMPONENT_DIRS "../components"
                         "main"
                         "ubxlib_runner")

set(SDKCONFIG "sdkconfig" CACHE STRING "IDF sdkconfig file path")

# Reduce the component list to a minimum
set(COMPONENTS "esp32" "esptool_py" "unity" "ubxlib" "main" "ubxlib_runner" "heap")

# Set the components to include the "tests" for.
# This is intended to be overriden in the CMake cache
# by invoking idf.py with a line of the following form:
# idf.py -D TEST_COMPONENTS="ubxlib_runner" all
set(TEST_COMPONENTS "replace-me"  CACHE STRING "Component to test")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ubxlib)
