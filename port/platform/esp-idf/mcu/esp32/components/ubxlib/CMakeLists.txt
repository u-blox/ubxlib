# The APIs
set(COMPONENT_ADD_INCLUDEDIRS 
# The ble API
                   "../../../../../../../ble/api"
                   "../../../../../../../ble/src"
# The generic configuration files
                   "../../../../../../../cfg"
# The cell API
                   "../../../../../../../cell/api"
                   "../../../../../../../cell/src"
# Common component APIs
                   "../../../../../../../common/error/api"
                   "../../../../../../../common/sock/api"
                   "../../../../../../../common/network/api"
                   "../../../../../../../common/security/api"
                   "../../../../../../../common/at_client/api"
                   "../../../../../../../common/short_range/api"
                   "../../../../../../../common/short_range/src"
# The API for the porting layer
                   "../../../../../../api"
# The private API for the event queue
                   "../../../../../common/event_queue"
# The platform specific configuration files
                   "../../../.."
                   "../../cfg")

# The implementations of the APIs
set(COMPONENT_SRCS 
# The ble API
                   "../../../../../../../ble/src/u_ble.c"
                   "../../../../../../../ble/src/u_ble_cfg.c"
                   "../../../../../../../ble/src/u_ble_data.c"
# The cell API
                   "../../../../../../../cell/src/u_cell.c"
                   "../../../../../../../cell/src/u_cell_pwr.c"
                   "../../../../../../../cell/src/u_cell_cfg.c"
                   "../../../../../../../cell/src/u_cell_info.c"
                   "../../../../../../../cell/src/u_cell_net.c"
                   "../../../../../../../cell/src/u_cell_sock.c"
                   "../../../../../../../cell/src/u_cell_sec.c"
                   "../../../../../../../cell/src/u_cell_sec_c2c.c"
                   "../../../../../../../cell/src/u_cell_private.c"
# The common network layer
                   "../../../../../../../common/network/src/u_network.c"
                   "../../../../../../../common/network/src/u_network_private_ble.c"
                   "../../../../../../../common/network/src/u_network_private_cell.c"
                   "../../../../../../../common/network/src/u_network_private_wifi.c"
# The common sock layer
                   "../../../../../../../common/sock/src/u_sock.c"
# The common security layer
                   "../../../../../../../common/security/src/u_security.c"
# The common AT client
                   "../../../../../../../common/at_client/src/u_at_client.c"
# The common short range layer
                   "../../../../../../../common/short_range/src/u_short_range.c"
                   "../../../../../../../common/short_range/src/u_short_range_edm.c"
                   "../../../../../../../common/short_range/src/u_short_range_edm_stream.c"
                   "../../../../../../../common/short_range/src/u_short_range_private.c"
# The porting layer
                   "../../../../../common/mbedtls/u_port_crypto.c"
                   "../../../../../common/event_queue/u_port_event_queue.c"
                   "../../../../src/u_port.c"
                   "../../../../src/u_port_debug.c"
                   "../../../../src/u_port_gpio.c"
                   "../../../../src/u_port_os.c"
                   "../../../../src/u_port_uart.c")

# The private include directories for the above
set(COMPONENT_PRIV_INCLUDEDIRS
# The common AT client
                   "../../../../../../../common/at_client/src"
# The common network layer
                   "../../../../../../../common/network/src"
# The common security layer
                   "../../../../../../../common/security/src"
# The porting layer
                   "../../../../src")

# For crypto functions
set(COMPONENT_REQUIRES mbedtls)

register_component()

if (DEFINED ENV{U_FLAGS})
    separate_arguments(U_FLAGS NATIVE_COMMAND "$ENV{U_FLAGS}")
    target_compile_options(${COMPONENT_TARGET} PUBLIC ${U_FLAGS})
    message("ubxlib: added ${U_FLAGS} due to environment variable U_FLAGS.")
endif()