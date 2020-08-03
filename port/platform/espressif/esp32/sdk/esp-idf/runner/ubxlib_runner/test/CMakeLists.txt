# For this platform we can simply bring in the generic test
# source code and map it to the unity framework used by
# Espressif through the macros in u_port_test.h
set(COMPONENT_SRCS            "../../../../../../../../test/u_port_test.c")
set(COMPONENT_ADD_INCLUDEDIRS "../../../../../app"
                              "../../../../../../../../../cfg"
                              "../../../../../../../../../common/error/api"
                              "../../../../../../../../../port/api"
                              "../../../../../../../../api"
                              "../../../../../cfg"
                              "../../../../../src")
set(COMPONENT_REQUIRES "esp32" "esptool_py" "unity")

register_component()

if (DEFINED ENV{U_FLAGS})
    separate_arguments(U_FLAGS NATIVE_COMMAND "$ENV{U_FLAGS}")
    target_compile_options(${COMPONENT_TARGET} PUBLIC ${U_FLAGS})
    message("runner: added ${U_FLAGS} due to environment variable U_FLAGS.")
endif()
