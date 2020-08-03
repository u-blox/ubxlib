# This is the project CMakeLists.txt file for the test subproject
cmake_minimum_required(VERSION 3.5)

# Include the body of stuff in the directory
# above and the tests component from below;
# main gets in automagically
set(EXTRA_COMPONENT_DIRS ".."
                         "ubxlib_runner")

# Empty the component list, main will add just what it needs
set(COMPONENTS "")

# Set the components to include the "tests" for.
# This is intended to be overriden in the CMake cache
# by invoking idf.py with a line of the following form:
# idf.py -D TEST_COMPONENTS="ubxlib_runner" all
set(TEST_COMPONENTS "replace-me"  CACHE STRING "Component to test")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ubxlib)
