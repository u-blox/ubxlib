message("Processing UBXLIB_RUNNER TEST starts")

# Bring in the test code
if (geodesic IN_LIST UBXLIB_FEATURES)
   set(GEODESIC_COMPONENT geodesic)
endif()

# These variables:
# * UBXLIB_TEST_SRC
# * UBXLIB_TEST_INC
# * UBXLIB_INC
# * UBXLIB_PRIVATE_INC
# * UBXLIB_BASE
# and optionally:
# - UBXLIB_EXTRA_LIBS
# - UBXLIB_COMPILE_OPTIONS
# ... are loaded from: port/ubxlib.cmake
set(COMPONENT_SRCS ${UBXLIB_TEST_SRC})

set(COMPONENT_ADD_INCLUDEDIRS
    ${UBXLIB_INC}
    ${UBXLIB_PRIVATE_INC}
    ${UBXLIB_TEST_INC}
    ${UBXLIB_BASE}/port/platform/common/mutex_debug
)

if (DEFINED ENV{U_UBXLIB_AUTO})
  # If we're running under automation, bring in u_runner
  list(APPEND COMPONENT_ADD_INCLUDEDIRS ${UBXLIB_BASE}/port/platform/common/runner)
  list(APPEND COMPONENT_SRCS            ${UBXLIB_BASE}/port/platform/common/runner/u_runner.c)
endif()

set(COMPONENT_REQUIRES "driver" "esptool_py" "unity" "esp_netif" ${GEODESIC_COMPONENT})

message("COMPONENT_REQUIRES for component UBXLIB_RUNNER TEST is ${COMPONENT_REQUIRES}")

register_component()

if (DEFINED ENV{U_FLAGS})
    separate_arguments(U_FLAGS NATIVE_COMMAND "$ENV{U_FLAGS}")
    # Remove -Wmissing-field-initializers warning because partial initialisation
    # is very useful when setting up huge tables in test code and Lint will find
    # occurrences of it where it has not been specifically disabled for this purpose
    target_compile_options(${COMPONENT_TARGET} PUBLIC ${U_FLAGS} -Wno-missing-field-initializers)
    message("runner: added ${U_FLAGS} due to environment variable U_FLAGS.")
endif()

message("Processing UBXLIB_RUNNER TEST ends")