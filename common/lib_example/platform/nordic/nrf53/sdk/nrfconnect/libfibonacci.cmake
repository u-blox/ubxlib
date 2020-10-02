# SPDX-License-Identifier: Apache-2.0

#
#

cmake_minimum_required(VERSION 3.13.1)

zephyr_get_include_directories_for_lang_as_string(       C includes)
zephyr_get_system_include_directories_for_lang_as_string(C system_includes)
zephyr_get_compile_definitions_for_lang_as_string(       C definitions)
zephyr_get_compile_options_for_lang_as_string(           C options)

set(external_project_cflags
  "${includes} ${definitions} ${options} ${system_includes}"
  )

include(ExternalProject)

set(library_src_dir   ${UBXLIB_BASE}/common/lib_example )
set(library_build_dir ${CMAKE_CURRENT_BINARY_DIR}/libfibonacci)

find_program(CMAKE_OBJCOPY objcopy)
find_program(CMAKE_OBJDUMP objdump)

if(CMAKE_GENERATOR STREQUAL "Unix Makefiles")
# https://www.gnu.org/software/make/manual/html_node/MAKE-Variable.html
set(submake "$(MAKE)")
else() # Obviously no MAKEFLAGS. Let's hope a "make" can be found somewhere.
set(submake "make")
endif()

ExternalProject_Add(
  lib_fibonacci                   # Name for custom target
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
  INSTALL_COMMAND ""      # This particular build system has no install command
  BUILD_BYPRODUCTS ${library_build_dir}/libfibonacci_blob.c
  )

set_target_properties(lib_fibonacci PROPERTIES IMPORTED_LOCATION ${library_build_dir}/libfibonacci_blob.c)

# add the binary blob array to the application
target_sources(app PRIVATE ${library_build_dir}/libfibonacci_blob.c)
