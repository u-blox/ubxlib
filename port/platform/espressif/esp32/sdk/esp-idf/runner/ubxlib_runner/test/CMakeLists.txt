# For this platform we can simply bring in the generic test
# source code and map it to the unity framework used by
# Espressif through the macros in u_port_test.h
set(COMPONENT_SRCS            "../../../../../../../../../common/at_client/test/u_at_client_test.c"
                              "../../../../../../../../../common/at_client/test/u_at_client_test_data.c"
                              "../../../../../../../../test/u_port_test.c")
set(COMPONENT_ADD_INCLUDEDIRS "../../../../../app"
                              "../../../../../../../../../cfg"
                              "../../../../../../../../../common/at_client/api"
                              "../../../../../../../../../common/at_client/src"
                              "../../../../../../../../../common/at_client/test"
                              "../../../../../../../../../common/error/api"
                              "../../../../../../../../../port/api"
                              "../../../../../../../../api"
                              "../../../../../cfg"
                              "../../../../../src")
set(COMPONENT_REQUIRES "esp32" "esptool_py" "unity")

register_component()

if (DEFINED ENV{U_FLAGS})
    separate_arguments(U_FLAGS NATIVE_COMMAND "$ENV{U_FLAGS}")
# Remove -Wmissing-field-initializers warning because partial initialisation
# is very useful when setting up huge tables in test code and Lint will find
# occurrences of it where it has not been specifically disabled for this purpose
    target_compile_options(${COMPONENT_TARGET} PUBLIC ${U_FLAGS} -Wno-missing-field-initializers)
    message("runner: added ${U_FLAGS} due to environment variable U_FLAGS.")
endif()
