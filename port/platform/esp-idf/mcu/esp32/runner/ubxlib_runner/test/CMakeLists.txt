# Bring in the test code
set(COMPONENT_SRCS            "../../../../../../../../example/sockets/main.c"
                              "../../../../../../../../example/sockets/main_tls.c"
                              "../../../../../../../../example/sockets/credentials_tls.c"
                              "../../../../../../../../example/security/e2e/e2e_main.c"
                              "../../../../../../../../example/security/psk/psk_main.c"
                              "../../../../../../../../example/mqtt_client/mqtt_main.c"
                              "../../../../../../../../example/location/main_loc_gnss.c"
                              "../../../../../../../../example/location/main_loc_gnss_cell.c"
                              "../../../../../../../../example/location/main_loc_cell_locate.c"
                              "../../../../../../../../example/cell/lte_cfg/lte_cfg_main.c"
                              "../../../../../../../../ble/test/u_ble_test.c"
                              "../../../../../../../../ble/test/u_ble_cfg_test.c"
                              "../../../../../../../../ble/test/u_ble_data_test.c"
                              "../../../../../../../../ble/test/u_ble_test_private.c"
                              "../../../../../../../../cell/test/u_cell_test.c"
                              "../../../../../../../../cell/test/u_cell_pwr_test.c"
                              "../../../../../../../../cell/test/u_cell_cfg_test.c"
                              "../../../../../../../../cell/test/u_cell_info_test.c"
                              "../../../../../../../../cell/test/u_cell_net_test.c"
                              "../../../../../../../../cell/test/u_cell_sock_test.c"
                              "../../../../../../../../cell/test/u_cell_sec_c2c_test.c"
                              "../../../../../../../../cell/test/u_cell_sec_tls_test.c"
                              "../../../../../../../../cell/test/u_cell_mqtt_test.c"
                              "../../../../../../../../cell/test/u_cell_file_test.c"
                              "../../../../../../../../cell/test/u_cell_loc_test.c"
                              "../../../../../../../../cell/test/u_cell_gpio_test.c"
                              "../../../../../../../../cell/test/u_cell_test_preamble.c"
                              "../../../../../../../../cell/test/u_cell_test_private.c"
                              "../../../../../../../../gnss/test/u_gnss_test.c"
                              "../../../../../../../../gnss/test/u_gnss_pwr_test.c"
                              "../../../../../../../../gnss/test/u_gnss_cfg_test.c"
                              "../../../../../../../../gnss/test/u_gnss_info_test.c"
                              "../../../../../../../../gnss/test/u_gnss_pos_test.c"
                              "../../../../../../../../gnss/test/u_gnss_util_test.c"
                              "../../../../../../../../gnss/test/u_gnss_test_private.c"
                              "../../../../../../../../common/network/test/u_network_test.c"
                              "../../../../../../../../common/network/test/u_network_test_shared_cfg.c"
                              "../../../../../../../../common/sock/test/u_sock_test.c"
                              "../../../../../../../../common/security/test/u_security_test.c"
                              "../../../../../../../../common/security/test/u_security_tls_test.c"
                              "../../../../../../../../common/security/test/u_security_credential_test.c"
                              "../../../../../../../../common/security/test/u_security_credential_test_data.c"
                              "../../../../../../../../common/location/test/u_location_test.c"
                              "../../../../../../../../common/location/test/u_location_test_shared_cfg.c"
                              "../../../../../../../../common/at_client/test/u_at_client_test.c"
                              "../../../../../../../../common/at_client/test/u_at_client_test_data.c"
                              "../../../../../../../../common/ubx_protocol/test/u_ubx_protocol_test.c"
                              "../../../../../../../../common/short_range/test/u_short_range_test.c"
                              "../../../../../../../../common/short_range/test/u_short_range_test_private.c"
                              "../../../../../../../../common/mqtt_client/test/u_mqtt_client_test.c"
                              "../../../../../../../../wifi/test/u_wifi_test.c"
                              "../../../../../../../../wifi/test/u_wifi_cfg_test.c"
                              "../../../../../../../../wifi/test/u_wifi_net_test.c"
                              "../../../../../../../../wifi/test/u_wifi_test_private.c"
                              "../../../../../../../test/u_port_test.c"
                              "../../../../../../common/test/u_preamble_test.c")

if (DEFINED ENV{U_UBXLIB_AUTO})
# If we're running under automation, bring in u_runner
set(COMPONENT_SRCS            ${COMPONENT_SRCS} "../../../../../../common/runner/u_runner.c")
endif()

set(COMPONENT_ADD_INCLUDEDIRS "../../../../../app"
                              "../../../../../../../../ble/api"
                              "../../../../../../../../ble/src"
                              "../../../../../../../../ble/test"
                              "../../../../../../../../cfg"
                              "../../../../../../../../cell/api"
                              "../../../../../../../../cell/src"
                              "../../../../../../../../cell/test"
                              "../../../../../../../../gnss/api"
                              "../../../../../../../../gnss/src"
                              "../../../../../../../../gnss/test"
                              "../../../../../../../../common/error/api"
                              "../../../../../../../../common/sock/api"
                              "../../../../../../../../common/sock/test"
                              "../../../../../../../../common/network/api"
                              "../../../../../../../../common/network/src"
                              "../../../../../../../../common/network/test"
                              "../../../../../../../../common/security/api"
                              "../../../../../../../../common/security/test"
                              "../../../../../../../../common/location/api"
                              "../../../../../../../../common/location/src"
                              "../../../../../../../../common/location/test"
                              "../../../../../../../../common/at_client/api"
                              "../../../../../../../../common/at_client/src"
                              "../../../../../../../../common/at_client/test"
                              "../../../../../../../../common/ubx_protocol/api"
                              "../../../../../../../../common/short_range/api"
                              "../../../../../../../../common/short_range/src"
                              "../../../../../../../../common/short_range/test"
                              "../../../../../../../../common/mqtt_client/api"
                              "../../../../../../../../port/api"
                              "../../../../../../../../wifi/api"
                              "../../../../../../../../wifi/test"
                              "../../../../../../../api"
                              "../../../../.."
                              "../../../cfg"
                              "../../../../../src"
                              "../../../../../../../clib")

if (DEFINED ENV{U_UBXLIB_AUTO})
# If we're running under automation, bring in u_runner
set(COMPONENT_ADD_INCLUDEDIRS ${COMPONENT_ADD_INCLUDEDIRS} "../../../../../../common/runner")
endif()

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
