# The APIs
set(COMPONENT_ADD_INCLUDEDIRS 
# The generic configuration files
                   "../../../../../../cfg"
# Common component APIs
                   "../../../../../../common/at_client/api"
                   "../../../../../../common/error/api"
# The API for the porting layer
                   "../../../../../api"
# The private API for the event queue
                   "../../../../common/event_queue"
# The platform specific configuration files
                   "../../cfg")

# The implementations of the APIs
set(COMPONENT_SRCS 
# The common AT client
                   "../../../../../../common/at_client/src/u_at_client.c"
# The porting layer
                   "../../../../common/event_queue/u_port_event_queue.c"
                   "../../src/u_port.c"
                   "../../src/u_port_debug.c"
                   "../../src/u_port_gpio.c"
                   "../../src/u_port_os.c"
                   "../../src/u_port_uart.c")

# The private include directories for the above
set(COMPONENT_PRIV_INCLUDEDIRS
# The common AT client
                   "../../../../../../common/at_client/src"
# The porting layer
                   "../../src")
register_component()

if (DEFINED ENV{U_FLAGS})
    separate_arguments(U_FLAGS NATIVE_COMMAND "$ENV{U_FLAGS}")
    target_compile_options(${COMPONENT_TARGET} PUBLIC ${U_FLAGS})
    message("ubxlib: added ${U_FLAGS} due to environment variable U_FLAGS.")
endif()
