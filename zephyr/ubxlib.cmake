
# Append UBXLIB as a Zephyr module
get_filename_component(UBXLIB_BASE "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
list(APPEND ZEPHYR_EXTRA_MODULES ${UBXLIB_BASE})
set(UBXLIB_USED 1)
if(NOT UBXLIB_NO_DEF_CONF)
  # Add the UBXLIB default configuration variables
  list(APPEND CONF_FILE "${UBXLIB_BASE}/port/platform/zephyr/default.conf")
  # Ensure that possible application config can still override the defaults as
  # West includes that file before the CONF_FILE list. It is ok to read the
  # file twice and the last read will prevail
  set(UBXLIB_APP_CONF "${CMAKE_SOURCE_DIR}/prj.conf")
  if(EXISTS ${UBXLIB_APP_CONF})
      list(APPEND CONF_FILE ${UBXLIB_APP_CONF})
  endif()
endif()
