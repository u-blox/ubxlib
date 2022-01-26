# SPDX-License-Identifier: Apache-2.0

#
#

cmake_minimum_required(VERSION 3.13.1)

zephyr_get_include_directories_for_lang_as_string(       C includes)
zephyr_get_system_include_directories_for_lang_as_string(C system_includes)
zephyr_get_compile_definitions_for_lang_as_string(       C definitions)
zephyr_get_compile_options_for_lang_as_string(           C options)

set(name fibonacci)
set(lib_name lib${name})
set(lib_test_string_def "-DU_COMMON_LIB_TEST_STRING=${lib_common_test_string}")

set(external_project_cflags "${definitions} ${options} ${system_includes} ${lib_test_string_def}")

set(library_src_dir   ${UBXLIB_BASE_SUB_PROJ}/common/lib_common/test/test_lib)
set(library_build_dir ${CMAKE_CURRENT_BINARY_DIR}/${lib_name})

include(ExternalProject)

find_program(CMAKE_OBJCOPY objcopy)
find_program(CMAKE_OBJDUMP objdump)
set(lib_flags 4)

if (CMAKE_HOST_WIN32)
set(CMAKE_SH $ENV{ZEPHYR_BASE}/../toolchain/bin/sh.exe)
file(TO_NATIVE_PATH  ${CMAKE_SH} CMAKE_SH_PATH)
set(CMAKE_SHELL_COMMAND "${CMAKE_SH_PATH} -c")
set (mkdir_delimeter "'")
set(echo_delimeter "'")
endif (CMAKE_HOST_WIN32)

if(CMAKE_GENERATOR STREQUAL "Unix Makefiles")
# https://www.gnu.org/software/make/manual/html_node/MAKE-Variable.html
set(submake "$(MAKE)")
else() # Obviously no MAKEFLAGS. Let's hope a "make" can be found somewhere.
set(submake "make")
endif()

ExternalProject_Add(
  lib_${name}                   # Name for custom target
  PREFIX     ${library_build_dir}
  SOURCE_DIR ${library_src_dir}
  BINARY_DIR ${library_src_dir}
  CONFIGURE_COMMAND ""    # Skip configuring the project, e.g. with autoconf
  BUILD_COMMAND
  ${submake}
  PREFIX=${library_build_dir}
  CC=${CMAKE_C_COMPILER}
  AR=${CMAKE_AR}
  CFLAGS=${external_project_cflags}
  OBJCOPY=${CMAKE_OBJCOPY}
  OBJDUMP=${CMAKE_OBJDUMP}
  SHELL_COMMAND=${CMAKE_SHELL_COMMAND}
  SHELL_DELIMETER=${mkdir_delimeter}
  ECHO_DELIMETER=${echo_delimeter}
  LIB_VERSION=${lib_common_test_version}
  LIB_FLAGS=${lib_flags}
  NAME=${name}
  INSTALL_COMMAND ""      # This particular build system has no install command
  BUILD_BYPRODUCTS ${library_build_dir}/${lib_name}_blob.c
  )

file(GLOB lib_fib_srcs
	   "${library_src_dir}/src/*.c"
	   "${library_src_dir}/api/*.h"
	   )

ExternalProject_Add_StepDependencies(lib_fibonacci build  ${lib_fib_srcs})


set_target_properties(lib_fibonacci PROPERTIES IMPORTED_LOCATION ${library_build_dir}/${lib_name}_blob.c)

add_dependencies(app lib_fibonacci)

# add the binary blob array to the application
set_source_files_properties( ${library_build_dir}/${lib_name}_blob.c PROPERTIES GENERATED TRUE)
target_sources(app PRIVATE ${library_build_dir}/${lib_name}_blob.c)
