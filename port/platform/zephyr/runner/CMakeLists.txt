# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr HINTS $ENV{ZEPHYR_BASE})
project(ubxlib)
set(UBXLIB_BASE ../../../..)
set(UBXLIB_PF_COMMON ${UBXLIB_BASE}/port/platform/common)
set(ZEPHYR_PORT_BASE ..)

#nrf5340 test runner application
target_include_directories(app PRIVATE ${ZEPHYR_PORT_BASE}/src/ ../cfg ${UBXLIB_PF_COMMON}/runner)
target_sources(app PRIVATE ${UBXLIB_PF_COMMON}/runner/u_runner.c)
target_sources(app PRIVATE ${ZEPHYR_PORT_BASE}/app/u_main.c)

#ubxlib common
target_include_directories(app PRIVATE ${UBXLIB_BASE}/cfg ${UBXLIB_BASE}/common/error/api)

#ubxlib port
target_include_directories(app PRIVATE ${UBXLIB_BASE}/port/api ${UBXLIB_BASE}/port/clib ${UBXLIB_PF_COMMON}/event_queue)
target_sources(app PRIVATE ${ZEPHYR_PORT_BASE}/src/u_port.c)
target_sources(app PRIVATE ${ZEPHYR_PORT_BASE}/src/u_port_debug.c)
target_sources(app PRIVATE ${ZEPHYR_PORT_BASE}/src/u_port_os.c)
target_sources(app PRIVATE ${ZEPHYR_PORT_BASE}/src/u_port_gpio.c)
target_sources(app PRIVATE ${ZEPHYR_PORT_BASE}/src/u_port_uart.c)
target_sources(app PRIVATE ${ZEPHYR_PORT_BASE}/src/u_port_gatt.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/port/clib/u_port_clib_mktime64.c)

#ubxlib common
target_include_directories(app PRIVATE ${UBXLIB_BASE}/cfg ${UBXLIB_BASE}/common/error/api ${UBXLIB_BASE}/port/api ${UBXLIB_PF_COMMON}/runner ${UBXLIB_PF_COMMON}/event_queue)
target_sources(app PRIVATE ${UBXLIB_PF_COMMON}/mbedtls/u_port_crypto.c)
target_sources(app PRIVATE ${UBXLIB_PF_COMMON}/event_queue/u_port_event_queue.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/port/test/u_port_test.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/port/test/u_port_gatt_test.c)
target_sources(app PRIVATE ${UBXLIB_PF_COMMON}/test/u_preamble_test.c)
target_sources(app PRIVATE ${UBXLIB_PF_COMMON}/runner/u_runner.c)

#at-client
target_include_directories(app PRIVATE ${UBXLIB_BASE}/common/at_client/api ${UBXLIB_BASE}/common/at_client/src ${UBXLIB_BASE}/common/at_client/test)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/at_client/src/u_at_client.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/at_client/test/u_at_client_test.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/at_client/test/u_at_client_test_data.c)

#lib-common
target_include_directories(app PRIVATE ${UBXLIB_BASE}/common/lib_common/api ${UBXLIB_BASE}/common/lib_common/test)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/lib_common/src/u_lib_handler.c)
include(./lib_common_test.cmake)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/lib_common/test/u_lib_common_test.c)

#network
target_include_directories(app PRIVATE ${UBXLIB_BASE}/common/network/api ${UBXLIB_BASE}/common/network/src  ${UBXLIB_BASE}/common/network/test)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/network/src/u_network.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/network/src/u_network_private_ble_extmod.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/network/src/u_network_private_ble_intmod.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/network/src/u_network_private_cell.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/network/src/u_network_private_wifi.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/network/test/u_network_test.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/network/test/u_network_test_shared_cfg.c)

#sock
target_include_directories(app PRIVATE ${UBXLIB_BASE}/common/sock/api ${UBXLIB_BASE}/common/sock/test)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/sock/src/u_sock.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/sock/test/u_sock_test.c)

#security
target_include_directories(app PRIVATE ${UBXLIB_BASE}/common/security/api ${UBXLIB_BASE}/common/security/test)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/security/src/u_security.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/security/src/u_security_tls.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/security/src/u_security_credential.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/security/test/u_security_test.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/security/test/u_security_tls_test.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/security/test/u_security_credential_test_data.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/security/test/u_security_credential_test.c)

#mqtt client
target_include_directories(app PRIVATE ${UBXLIB_BASE}/common/mqtt_client/api)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/mqtt_client/src/u_mqtt_client.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/mqtt_client/test/u_mqtt_client_test.c)

#cell API
target_include_directories(app PRIVATE ${UBXLIB_BASE}/cell/api ${UBXLIB_BASE}/cell/src ${UBXLIB_BASE}/cell/test)
target_sources(app PRIVATE ${UBXLIB_BASE}/cell/src/u_cell.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/cell/src/u_cell_pwr.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/cell/src/u_cell_cfg.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/cell/src/u_cell_info.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/cell/src/u_cell_net.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/cell/src/u_cell_sock.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/cell/src/u_cell_sec.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/cell/src/u_cell_sec_c2c.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/cell/src/u_cell_sec_tls.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/cell/src/u_cell_mqtt.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/cell/src/u_cell_private.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/cell/test/u_cell_test.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/cell/test/u_cell_pwr_test.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/cell/test/u_cell_cfg_test.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/cell/test/u_cell_info_test.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/cell/test/u_cell_net_test.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/cell/test/u_cell_sock_test.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/cell/test/u_cell_sec_c2c_test.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/cell/test/u_cell_sec_tls_test.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/cell/test/u_cell_mqtt_test.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/cell/test/u_cell_test_preamble.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/cell/test/u_cell_test_private.c)

#ble API
target_include_directories(app PRIVATE ${UBXLIB_BASE}/ble/api ${UBXLIB_BASE}/ble/src ${UBXLIB_BASE}/ble/test)
target_sources(app PRIVATE ${UBXLIB_BASE}/ble/src/u_ble_extmod.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/ble/src/u_ble_intmod.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/ble/src/u_ble_cfg_extmod.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/ble/src/u_ble_cfg_intmod.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/ble/src/u_ble_data_extmod.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/ble/src/u_ble_data_intmod.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/ble/src/u_ble_private.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/ble/test/u_ble_test.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/ble/test/u_ble_cfg_test.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/ble/test/u_ble_data_test.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/ble/test/u_ble_test_private.c)

#short range
target_include_directories(app PRIVATE ${UBXLIB_BASE}/common/short_range/api ${UBXLIB_BASE}/common/short_range/src ${UBXLIB_BASE}/common/short_range/test)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/short_range/src/u_short_range.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/short_range/src/u_short_range_edm.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/short_range/src/u_short_range_edm_stream.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/short_range/src/u_short_range_sec_tls.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/short_range/src/u_short_range_private.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/short_range/test/u_short_range_test.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/common/short_range/test/u_short_range_test_private.c)

# examples
target_sources(app PRIVATE ${UBXLIB_BASE}/example/sockets/main.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/example/sockets/main_tls.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/example/sockets/credentials_tls.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/example/security/e2e/e2e_main.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/example/security/psk/psk_main.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/example/security/c2c/c2c_main.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/example/mqtt_client/mqtt_main.c)
target_sources(app PRIVATE ${UBXLIB_BASE}/example/cell/lte_cfg/lte_cfg_main.c)

# unity
target_compile_definitions(app PRIVATE UNITY_INCLUDE_CONFIG_H)
target_include_directories(app PRIVATE $ENV{ZEPHYR_BASE}/../test/cmock/vendor/unity/src)
target_sources(app PRIVATE $ENV{ZEPHYR_BASE}/../test/cmock/vendor/unity/src/unity.c)

# Add environment variables passed-in via U_FLAGS
if (DEFINED ENV{U_FLAGS})
    separate_arguments(U_FLAGS NATIVE_COMMAND "$ENV{U_FLAGS}")
    target_compile_options(app PRIVATE ${U_FLAGS})
    message("ubxlib: added ${U_FLAGS} due to environment variable U_FLAGS.")
endif()

# Add ubxlib port files only required if newlib is not in use
get_target_property(tmp app COMPILE_OPTIONS)
if ("-DU_CFG_ZEPHYR_USE_NEWLIB" IN_LIST tmp)
    # Nothing to do, the user has chosen to use newlib
    message("ubxlib: using newlib")
else()
    target_sources(app PRIVATE ${UBXLIB_BASE}/port/clib/u_port_clib_isblank.c)
    target_sources(app PRIVATE ${UBXLIB_BASE}/port/clib/u_port_clib_mktime.c)
    target_sources(app PRIVATE ${UBXLIB_BASE}/port/clib/u_port_setjmp.S)
    target_sources(app PRIVATE ${ZEPHYR_PORT_BASE}/src/u_port_clib.c)
endif()
