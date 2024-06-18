/*
 * Copyright 2019-2024 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/** @file
 * @brief Implementation of the uPortBoardCfgXxx() functions for Zephyr.
 *
 * The code here works in concert with the .yaml files over in the
 * dts/bindings directory to allow the Zephyr device tree to specify
 * all of the device and network configuration parameters to be used
 * with ubxlib.
 *
 * See /port/platform/zephyr/README.md for a description of how it works.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

/* ----------------------------------------------------------------
 * INCLUDE FILES
 * -------------------------------------------------------------- */

#include "stdlib.h"   // strtol()
#include "stddef.h"   // NULL, size_t etc.
#include "stdint.h"   // int32_t etc.
#include "stdbool.h"
#include "string.h"   // strcmp(), strstr(), memset()
#include "ctype.h"    // isdigit()

#include "u_cfg_sw.h"
#include "u_error_common.h"

#include "u_common_spi.h"

#include "u_device.h"
#include "u_device_shared.h"
#include "u_network_type.h"
#include "u_network_config_ble.h"
#include "u_network_config_cell.h"
#include "u_network_config_gnss.h"
#include "u_network_config_wifi.h"

#include "u_cell_module_type.h"
#include "u_gnss_module_type.h"
#include "u_short_range_module_type.h"

#include "u_ble_cfg.h" // For uBleCfgRole_t

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_board_cfg.h"

#include <version.h>

#if KERNEL_VERSION_NUMBER >= ZEPHYR_VERSION(3,1,0)
# include <zephyr/kernel.h>
# include <zephyr/device.h>
#else
# include <kernel.h>
# include <device.h>
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Return true if the given flag is present, else false
 * (the comma at the end is significant).
 */
#define U_PORT_BOARD_CFG_GET_BOOLEAN(i, booleanName) DT_PROP_OR(i, booleanName, false),

/** Get the value of an integer or -1 if the integer is not present
 * (the comma at the end is significant).
 */
#define U_PORT_BOARD_CFG_GET_INT(i, integerName) DT_PROP_OR(i, integerName, -1),

/** Get the value of an integer or the given default if not present
 * (the comma at the end is significant).
 */
#define U_PORT_BOARD_CFG_GET_INT_OR_DEFAULT(i, integerName, default) DT_PROP_OR(i, integerName, default),

/** Get the value of a string or NULL (the comma at the end is
 * significant).
 */
#define U_PORT_BOARD_CFG_GET_STRING(i, stringName) DT_PROP_OR(i, stringName, NULL),

/** Get the name of a node (the comma at the end is significant).
 */
#define U_PORT_BOARD_CFG_GET_NAME(i) DT_NODE_FULL_NAME(i),

/** Get the value of the "module-type" property of a ubxlib-device as
 * a C token i.e. without quotes (the comma at the end is significant).
 */
#define U_PORT_BOARD_CFG_GET_MODULE_TYPE(i, defaultModuleType) DT_STRING_TOKEN_OR(i, module_type, defaultModuleType),

/** Get whether the given Boolean property of a node referenced by
 * "network" is set, for up to two "network" phandles in a ubxlib-device,
 * as an array (the comma at the end is significant).
 */
#define U_PORT_BOARD_CFG_GET_NETWORK_LIST_BOOLEAN(i, booleanName) {DT_PROP_BY_PHANDLE_IDX_OR(i, network, 0, booleanName, false),  \
                                                                   DT_PROP_BY_PHANDLE_IDX_OR(i, network, 1, booleanName, false)},

/** Get the given integer property of a node referenced by "network",
 * for up to two "network" phandles in a ubxlib-device, as an array,
 * applying -1 where there are none (the comma at the end is significant).
 */
#define U_PORT_BOARD_CFG_GET_NETWORK_LIST_INT(i, integerName) {DT_PROP_BY_PHANDLE_IDX_OR(i, network, 0, integerName, -1),  \
                                                               DT_PROP_BY_PHANDLE_IDX_OR(i, network, 1, integerName, -1)},

/** Get the given integer property of a node referenced by "network",
 * for up to two "network" phandles in a ubxlib-device, as an array,
 * applying the given default where there are none (the comma at the
 * end is significant).
 */
#define U_PORT_BOARD_CFG_GET_NETWORK_LIST_INT_OR_DEFAULT(i, integerName, default) {DT_PROP_BY_PHANDLE_IDX_OR(i, network, 0, integerName, default),  \
                                                                                   DT_PROP_BY_PHANDLE_IDX_OR(i, network, 1, integerName, default)},

/** Get the given string property of a node referenced by "network",
 * for up to two "network" phandles in a ubxlib-device, as an array,
 * applying NULL where there are none (the comma at the end is significant).
 */
#define U_PORT_BOARD_CFG_GET_NETWORK_LIST_STRING(i, stringName) {DT_PROP_BY_PHANDLE_IDX_OR(i, network, 0, stringName, NULL),  \
                                                                 DT_PROP_BY_PHANDLE_IDX_OR(i, network, 1, stringName, NULL)},

/** Get the named property of the phandle "uart_ppp" or the default,
 * note that the extra bracketing is _required_.
 */
#define U_PORT_BOARD_CFG_GET_UART_PPP_OR_DEFAULT(i, name, default) \
  COND_CODE_1(DT_NODE_HAS_PROP(i, uart_ppp), (DT_PROP_BY_PHANDLE(i, uart_ppp, name)), (default))

/** Get the given string property of phandle node "uart_ppp"
 * referenced by "network", for up to two "network" phandles in a
 * ubxlib-device, as an array, applying NULL where there are none
 * (the comma at the end is significant and the extra bracketing
 * is _required_).
 */
#define U_PORT_BOARD_CFG_GET_NETWORK_LIST_UART_PPP_STRING(i, stringName) {COND_CODE_1(DT_PROP_HAS_IDX(i, network, 0),                                             \
                                                                                      (U_PORT_BOARD_CFG_GET_UART_PPP_OR_DEFAULT(DT_PHANDLE_BY_IDX(i, network, 0), \
                                                                                                                                stringName, NULL)),               \
                                                                                      (NULL)),                                                                    \
                                                                          COND_CODE_1(DT_PROP_HAS_IDX(i, network, 1),                                             \
                                                                                      (U_PORT_BOARD_CFG_GET_UART_PPP_OR_DEFAULT(DT_PHANDLE_BY_IDX(i, network, 1), \
                                                                                                                                stringName, NULL)),                \
                                                                                      (NULL))},

/** Get the given integer property of a phandle node "uart_ppp"
 * referenced by "network", for up to two "network" phandles in a
 * ubxlib-device, as an array, applying -1 where there are none
 * (the comma at the end is significant and the extra bracketing
 * is _required_).
 */
#define U_PORT_BOARD_CFG_GET_NETWORK_LIST_UART_PPP_INT(i, integerName) {COND_CODE_1(DT_PROP_HAS_IDX(i, network, 0),                                             \
                                                                                    (U_PORT_BOARD_CFG_GET_UART_PPP_OR_DEFAULT(DT_PHANDLE_BY_IDX(i, network, 0), \
                                                                                                                             integerName, -1)),                 \
                                                                                    (-1)),                                                                      \
                                                                        COND_CODE_1(DT_PROP_HAS_IDX(i, network, 1),                                             \
                                                                                    (U_PORT_BOARD_CFG_GET_UART_PPP_OR_DEFAULT(DT_PHANDLE_BY_IDX(i, network, 1), \
                                                                                                                             integerName, -1)),                 \
                                                                                    (-1))},

#ifndef U_PORT_BOARD_CFG_SPI_PREFIX
/** The prefix to expect on the node name of an SPI port in the
 * device tree.
  */
# define U_PORT_BOARD_CFG_SPI_PREFIX "spi"
#endif

#ifndef U_PORT_BOARD_CFG_I2C_PREFIX
/** The prefix to expect on the node name of an I2C port in the
 * device tree.
 */
# define U_PORT_BOARD_CFG_I2C_PREFIX "i2c"
#endif

#ifndef U_PORT_BOARD_CFG_DEFAULT_CELL_MODULE_TYPE
/** The default module type to apply for cellular if not specified;
 * should match that used as the default in
 * u-blox,ubxlib-device-cellular.yaml.
 */
# define U_PORT_BOARD_CFG_DEFAULT_CELL_MODULE_TYPE U_CELL_MODULE_TYPE_ANY
#endif

#ifndef U_PORT_BOARD_CFG_DEFAULT_GNSS_MODULE_TYPE
/** The default module type to apply for GNSS if not specified;
 * should match that used as the default in
 * u-blox,ubxlib-device-gnss.yaml and u-blox,ubxlib-network-gnss.yaml.
 */
# define U_PORT_BOARD_CFG_DEFAULT_GNSS_MODULE_TYPE U_GNSS_MODULE_TYPE_ANY
#endif

#ifndef U_PORT_BOARD_CFG_DEFAULT_SHORT_RANGE_MODULE_TYPE
/** The default module type to apply for short-range if not specified;
 * should match that used as the default in
 * u-blox,ubxlib-device-short-range.yaml.
 */
# define U_PORT_BOARD_CFG_DEFAULT_SHORT_RANGE_MODULE_TYPE U_SHORT_RANGE_MODULE_TYPE_ANY
#endif

#ifndef U_PORT_BOARD_CFG_DEFAULT_BLE_ROLE
/** The default BLE role to apply if not specified, i.e. not present
 * at all (otherwise the default from u-blox,ubxlib-network-ble.yaml
 * would be applied).
 */
# define U_PORT_BOARD_CFG_DEFAULT_BLE_ROLE U_BLE_CFG_ROLE_DISABLED
#endif

#ifndef U_PORT_BOARD_CFG_DEFAULT_WIFI_MODE
/** The default Wi-Fi mode to apply if not specified, i.e. not present
 * at all (otherwise the default from u-blox,ubxlib-network-wifi.yaml
 * would be applied).
 */
# define U_PORT_BOARD_CFG_DEFAULT_WIFI_MODE U_WIFI_MODE_NONE
#endif

#ifndef U_PORT_BOARD_CFG_SECRET_STRING
/** The string to put in a debug print if the string value should
 * not be printed.
 */
# define U_PORT_BOARD_CFG_SECRET_STRING "***"
#endif

// THERE ARE ADDITIONAL COMPILE-TIME MACROS AT THE END OF THIS FILE

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Type to use in a string-to-enum look-up table, required because
 * there is no way in the Zephyr DT macros to get a string as a
 * C token if it is behind a phandle reference.
 */
typedef struct {
    const char *pString;
    int32_t value;
} uPortBoardCfgStringToEnum_t;

/* ----------------------------------------------------------------
 * VARIABLES: CELLULAR DEVICE TREE CONFIGURATION PARAMETERS
 * -------------------------------------------------------------- */

/** Since the Zephyr device tree is a compile-time macro thing, here
 * we use it to build up static tables of all of the configuration
 * values it contains for the three device types.
 */
#if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_cellular)

/* First the properties of everything ubxlib-device-cellular compatible.
 */

/** The node name of each of the ubxlib-device-cellular compatible
 * devices in the device tree.
 */
static const char *const gpCfgCellDeviceName[] = {
    DT_FOREACH_STATUS_OKAY(u_blox_ubxlib_device_cellular, U_PORT_BOARD_CFG_GET_NAME)
};

/** Get the "transport-type" property (which will be something like "uart0")
 * of each of the ubxlib-device-cellular compatible devices, in the
 * order they appear in the device tree.
 */
static const char *const gpDeviceCfgCellTransportType[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_cellular,
                                 U_PORT_BOARD_CFG_GET_STRING,
                                 transport_type)
};

/** The "module-type" property of each of the ubxlib-device-cellular
 * compatible devices as an array of C tokens, in the order they appear
 * in the device tree.
 */
static const uCellModuleType_t gDeviceCfgCellModuleType[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_cellular,
                                 U_PORT_BOARD_CFG_GET_MODULE_TYPE,
                                 U_PORT_BOARD_CFG_DEFAULT_CELL_MODULE_TYPE)
};

/** The "uart-baud-rate" property of each of the ubxlib-device-cellular
 * compatible devices, or -1 where not present, in the order they
 * appear in the device tree.
 */
static const int32_t gDeviceCfgCellUartBaudRate[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_cellular,
                                 U_PORT_BOARD_CFG_GET_INT,
                                 uart_baud_rate)
};

/** The "pin-enable-power" property of each of the
 * ubxlib-device-cellular compatible devices, or -1 where not present,
 * in the order they appear in the device tree.
 */
static const int32_t gDeviceCfgCellPinEnablePower[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_cellular,
                                 U_PORT_BOARD_CFG_GET_INT,
                                 pin_enable_power)
};

/** The "pin-pwr-on" property of each of the ubxlib-device-cellular
 * compatible devices, or -1 where not present, in the order they
 * appear in the device tree.
 */
static const int32_t gDeviceCfgCellPinPwrOn[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_cellular,
                                 U_PORT_BOARD_CFG_GET_INT,
                                 pin_pwr_on)
};

/** The "pin-vint" property of each of the ubxlib-device-cellular
 * compatible devices, or -1 where not present, in the order they
 * appear in the device tree.
 */
static const int32_t gDeviceCfgCellPinVInt[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_cellular,
                                 U_PORT_BOARD_CFG_GET_INT,
                                 pin_vint)
};

/** The "pin-dtr-power-saving" property of each of the
 * ubxlib-device-cellular compatible devices, or -1 where not
 * present, in the order they appear in the device tree.
 */
static const int32_t gDeviceCfgCellPinDtrPowerSaving[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_cellular,
                                 U_PORT_BOARD_CFG_GET_INT,
                                 pin_dtr_power_saving)
};

/* Then the ubxlib-network-xxx compatible properties that
 * the "network" property of a ubxlib-device-xxx might refer
 * to, which is the cellular and GNSS network properties.
 */

/** The hidden "do-not-set-network-type" properties pointed-to
 * by the first two "network" phandles of each of the
 * ubxlib-device-cellular compatible devices, or -1 where
 * not present, in the order they appear in the device tree.
 */
static const int32_t gpDeviceCfgCellNetworkType[][2] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_cellular,
                                 U_PORT_BOARD_CFG_GET_NETWORK_LIST_INT,
                                 do_not_set_network_type)
};

/** The "apn" properties pointed-to by the first two
 * "network" phandles of each of the ubxlib-device-cellular
 * compatible devices, or NULL where not present, in the order
 * they appear in the device tree.
 */
static const char *const gpDeviceCfgCellNetworkListApn[][2] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_cellular,
                                 U_PORT_BOARD_CFG_GET_NETWORK_LIST_STRING,
                                 apn)
};

/** The "timeout-seconds" properties pointed-to by the first
 * two "network" phandles of each of the ubxlib-device-cellular
 * compatible devices, or -1 where not present, in the order
 * they appear in the device tree.
 */
static const int32_t gDeviceCfgCellNetworkListTimeoutSeconds[][2] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_cellular,
                                 U_PORT_BOARD_CFG_GET_NETWORK_LIST_INT,
                                 timeout_seconds)
};

/** The "username" properties pointed-to by the first two
 * "network" phandles of each of the ubxlib-device-cellular
 * compatible devices, or NULL where not present, in the order
 * they appear in the device tree.
 */
static const char *const gpDeviceCfgCellNetworkListUsername[][2] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_cellular,
                                 U_PORT_BOARD_CFG_GET_NETWORK_LIST_STRING,
                                 username)
};

/** The "password" properties pointed-to by the first two
 * "network" phandles of each of the ubxlib-device-cellular
 * compatible devices, or NULL where not present, in the order
 * they appear in the device tree.
 */
static const char *const gpDeviceCfgCellNetworkListPassword[][2] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_cellular,
                                 U_PORT_BOARD_CFG_GET_NETWORK_LIST_STRING,
                                 password)
};

/** The "authentication-mode" properties pointed-to by the first two
 * "network" phandles of each of the ubxlib-device-cellular
 * compatible devices, or 0 (automatic) where not present, in the
 * order they appear in the device tree.
 */
static const int32_t gDeviceCfgCellNetworkListAuthenticationMode[][2] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_cellular,
                                 U_PORT_BOARD_CFG_GET_NETWORK_LIST_INT_OR_DEFAULT,
                                 authentication_mode, 0)
};

/** The "mccmnc" properties pointed-to by the first two
 * "network" phandles of each of the ubxlib-device-cellular
 * compatible devices, or NULL where not present, in the order
 * they appear in the device tree.
 */
static const char *const gpDeviceCfgCellNetworkListMccMnc[][2] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_cellular,
                                 U_PORT_BOARD_CFG_GET_NETWORK_LIST_STRING,
                                 mccmnc)
};

/* Then the ubxlib-network-cellular-uart-ppp compatible properties that
 * the "uart-ppp" property of a ubxlib-network-cellular might refer to.
 */

/** The "transport-type" property of the "uart-ppp" phandle
 * pointed-to by the first two "network" phandles of each of
 * the ubxlib-device-cellular compatible devices, or NULL
 * where not present, in the order they appear in the device tree.
 */
static const char *const gpDeviceCfgCellUartPppListTransportType[][2] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_cellular,
                                 U_PORT_BOARD_CFG_GET_NETWORK_LIST_UART_PPP_STRING,
                                 transport_type)
};

/** The "uart-baud-rate" property pointed-to by an optional
 * "uart-ppp" phandle of each of the ubxlib-network-cellular
 * compatible devices, or NULL where not present, in the order
 * they appear in the device tree.
 */
static const int32_t gDeviceCfgCellUartPppListUartBaudRate[][2] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_cellular,
                                 U_PORT_BOARD_CFG_GET_NETWORK_LIST_UART_PPP_INT,
                                 uart_baud_rate)
};

/* Back to the non-phandle ubxlib-network-xxx compatible properties
 * that the "network" property of a ubxlib-device-xxx might refer to.
 */

/** The "async-connect" properties pointed-to by the first two
 * "network" phandles of each of the ubxlib-device-cellular
 * compatible devices, or NULL where not present, in the order
 * they appear in the device tree.
 */
static const bool gDeviceCfgCellNetworkListAsyncConnect[][2] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_cellular,
                                 U_PORT_BOARD_CFG_GET_NETWORK_LIST_BOOLEAN,
                                 async_connect)
};

# if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_gnss)

/** The "module-type" properties pointed-to by the first two
 * "network" phandles of each of the ubxlib-device-cellular
 * compatible devices, or NULL where not present, in the order
 * they appear in the device tree.  We would ideally get the
 * value in token form rather than string form but unfortunately
 * there is no macro in the Zephyr device tree macro set which
 * will get a C token from a phandle-referenced node.
 */
static const char *const gpDeviceCfgCellNetworkListModuleType[][2] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_cellular,
                                 U_PORT_BOARD_CFG_GET_NETWORK_LIST_STRING,
                                 module_type)
};

/** The "device-pin-pwr" properties pointed-to by the first two
 * "network" phandles of each of the ubxlib-device-cellular
 * compatible devices (which will be present if the network is a
 * GNSS one), or -1 where not present, in the order they appear
 * in the device tree.
 */
static const int32_t gDeviceCfgCellNetworkListDevicePinPower[][2] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_cellular,
                                 U_PORT_BOARD_CFG_GET_NETWORK_LIST_INT,
                                 device_pin_pwr)
};

/** The "device-pin-data-ready" properties pointed-to by the first
 * two "network" phandles of each of the ubxlib-device-cellular
 * compatible devices (which will be present if the network is
 * a GNSS one) , or -1 where not present, in the order they appear
 * in the device tree.
 */
static const int32_t gDeviceCfgCellNetworkListDevicePinDataReady[][2] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_cellular,
                                 U_PORT_BOARD_CFG_GET_NETWORK_LIST_INT,
                                 device_pin_data_ready)
};

# endif // # if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_gnss)
#endif // #if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_cellular)

/* ----------------------------------------------------------------
 * VARIABLES: GNSS DEVICE TREE CONFIGURATION PARAMETERS
 * -------------------------------------------------------------- */

#if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_gnss)

/* Get the properties of everything ubxlib-device-gnss compatible,
 * noting that there is no "network" property in this case, so
 * nothing to worry about there.
 */

/** The node name of each of the ubxlib-device-gnss compatible
 * devices in the device tree.
 */
static const char *const gpCfgGnssDeviceName[] = {
    DT_FOREACH_STATUS_OKAY(u_blox_ubxlib_device_gnss, U_PORT_BOARD_CFG_GET_NAME)
};

/** Get the "transport-type" (which will be something like "i2c0") of each
 * of the ubxlib-device-gnss compatible devices, in the order
 * they appear in the device tree.
 */
static const char *const gpDeviceCfgGnssTransportType[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_gnss,
                                 U_PORT_BOARD_CFG_GET_STRING,
                                 transport_type)
};

/** The "uart-baud-rate" property of each of the ubxlib-device-gnss
 * compatible devices, or -1 where not present, in the order they
 * appear in the device tree.
 */
static const int32_t gDeviceCfgGnssUartBaudRate[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_gnss,
                                 U_PORT_BOARD_CFG_GET_INT,
                                 uart_baud_rate)
};

/** Get whether the "gnss-uart2" property is set for each
 * ubxlib-device-gnss compatible device, in the order they
 * appear in the device tree.
 */
static const bool gDeviceCfgGnssUart2[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_gnss,
                                 U_PORT_BOARD_CFG_GET_BOOLEAN,
                                 gnss_uart2)
};

/** The "i2c-address" property of each of the ubxlib-device-gnss
 * compatible devices, or -1 where not present, in the order they
 * appear in the device tree.
 */
static const int32_t gDeviceCfgGnssI2cAddress[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_gnss,
                                 U_PORT_BOARD_CFG_GET_INT,
                                 i2c_address)
};

/** The "i2c-clock-hertz" property of each of the
 * ubxlib-device-gnss compatible devices, or -1 where not present,
 * in the order they appear in the device tree.
 */
static const int32_t gDeviceCfgGnssI2cClockHertz[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_gnss,
                                 U_PORT_BOARD_CFG_GET_INT,
                                 i2c_clock_hertz)
};

/** Get whether the "i2c-already-open" property is set for each
 * ubxlib-device-gnss compatible device, in the order they
 * appear in the device tree.
 */
static const bool gDeviceCfgGnssI2cAlreadyOpen[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_gnss,
                                 U_PORT_BOARD_CFG_GET_BOOLEAN,
                                 i2c_already_open)
};

/** The "i2c-max-segment-size" property of each of the
 * ubxlib-device-gnss compatible devices, or -1 where not present,
 * in the order they appear in the device tree.
 */
static const int32_t gDeviceCfgGnssI2cMaxSegmentSize[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_gnss,
                                 U_PORT_BOARD_CFG_GET_INT,
                                 i2c_max_segment_size)
};

/** The "spi-max-segment-size" property of each of the
 * ubxlib-device-gnss compatible devices, or -1 where not present,
 * in the order they appear in the device tree.
 */
static const int32_t gDeviceCfgGnssSpiMaxSegmentSize[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_gnss,
                                 U_PORT_BOARD_CFG_GET_INT,
                                 spi_max_segment_size)
};

/** The "spi-pin-select" of each of the ubxlib-device-gnss
 * compatible devices, or -1 where not present, in the order they
 * appear in the device tree.
 */
static const int32_t gDeviceCfgGnssSpiPinSelect[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_gnss,
                                 U_PORT_BOARD_CFG_GET_INT,
                                 spi_pin_select)
};

/** The "spi-frequency-hertz" property of each of the
 * ubxlib-device-gnss compatible devices, or -1 where not present,
 * in the order they appear in the device tree.
 */
static const int32_t gDeviceCfgGnssSpiFrequencyHertz[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_gnss,
                                 U_PORT_BOARD_CFG_GET_INT,
                                 spi_frequency_hertz)
};

/** The "spi-index-select" of each of the ubxlib-device-gnss
 * compatible devices, or -1 where not present, in the order they
 * appear in the device tree.
 */
static const int32_t gDeviceCfgGnssSpiIndexSelect[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_gnss,
                                 U_PORT_BOARD_CFG_GET_INT,
                                 spi_index_select)
};

/** The "spi-mode" of each of the ubxlib-device-gnss
 * compatible devices, or -1 where not present, in the order they
 * appear in the device tree.
 */
static const int32_t gDeviceCfgGnssSpiMode[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_gnss,
                                 U_PORT_BOARD_CFG_GET_INT,
                                 spi_mode)
};

/** The "spi-word-size-bytes" of each of the ubxlib-device-gnss
 * compatible devices, or -1 where not present, in the order they
 * appear in the device tree.
 */
static const int32_t gDeviceCfgGnssSpiWordSizeBytes[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_gnss,
                                 U_PORT_BOARD_CFG_GET_INT,
                                 spi_word_size_bytes)
};

/** Get whether the "spi-lsb-first" property is set for each
 * ubxlib-device-gnss compatible device, in the order they
 * appear in the device tree.
 */
static const bool gDeviceCfgGnssSpiLsbFirst[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_gnss,
                                 U_PORT_BOARD_CFG_GET_BOOLEAN,
                                 spi_lsb_first)
};

/** The "spi-start-offset-nanoseconds" of each of the ubxlib-device-gnss
 * compatible devices, or -1 where not present, in the order they
 * appear in the device tree.
 */
static const int32_t gDeviceCfgGnssSpiStartOffsetNanoseconds[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_gnss,
                                 U_PORT_BOARD_CFG_GET_INT,
                                 spi_start_offset_nanoseconds)
};

/** The "spi-stop-offset-nanoseconds" of each of the ubxlib-device-gnss
 * compatible devices, or -1 where not present, in the order they
 * appear in the device tree.
 */
static const int32_t gDeviceCfgGnssSpiStopOffsetNanoseconds[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_gnss,
                                 U_PORT_BOARD_CFG_GET_INT,
                                 spi_stop_offset_nanoseconds)
};

/** The "module-type" of each of the ubxlib-device-gnss compatible
 * devices as an array of C tokens, in the order they appear in the
 * device tree.
 */
static const uGnssModuleType_t gDeviceCfgGnssModuleType[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_gnss,
                                 U_PORT_BOARD_CFG_GET_MODULE_TYPE,
                                 U_PORT_BOARD_CFG_DEFAULT_GNSS_MODULE_TYPE)
};

/** The "pin-enable-power" of each of the ubxlib-device-gnss
 * compatible devices, or -1 where not present, in the order they
 * appear in the device tree.
 */
static const int32_t gDeviceCfgGnssPinEnablePower[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_gnss,
                                 U_PORT_BOARD_CFG_GET_INT,
                                 pin_enable_power)
};

/** The "pin-data-ready" of each of the ubxlib-device-gnss
 * compatible devices, or -1 where not present, in the order they
 * appear in the device tree.
 */
static const int32_t gDeviceCfgGnssPinDataReady[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_gnss,
                                 U_PORT_BOARD_CFG_GET_INT,
                                 pin_data_ready)
};

/** Get whether the "power-off-to-backup" property is set
 * for each ubxlib-device-gnss compatible device, in the
 * order they appear in the device tree.
 */
static const bool gDeviceCfgGnssPowerOffToBackup[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_gnss,
                                 U_PORT_BOARD_CFG_GET_BOOLEAN,
                                 power_off_to_backup)
};

#endif // #if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_gnss)

/* ----------------------------------------------------------------
 * VARIABLES: SHORT-RANGE DEVICE TREE CONFIGURATION PARAMETERS
 * -------------------------------------------------------------- */

#if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_short_range)

/* First get the properties of everything ubxlib-device-short-range
 * compatible.
 */

/** The node name of each of the ubxlib-device-short-range compatible
 * devices in the device tree.
 */
static const char *const gpCfgShortRangeDeviceName[] = {
    DT_FOREACH_STATUS_OKAY(u_blox_ubxlib_device_short_range, U_PORT_BOARD_CFG_GET_NAME)
};

/** Get the "transport-type" (which will be something like "uart0") of
 * each of the ubxlib-device-short-range compatible devices, in the
 * order they appear in the device tree.
 */
static const char *const gpDeviceCfgShortRangelTransportType[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_short_range,
                                 U_PORT_BOARD_CFG_GET_STRING,
                                 transport_type)
};

/** The "uart-baud-rate" property of each of the ubxlib-device-short-range
 * compatible devices, or -1 where not present, in the order they
 * appear in the device tree.
 */
static const int32_t gDeviceCfgShortRangeUartBaudRate[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_short_range,
                                 U_PORT_BOARD_CFG_GET_INT,
                                 uart_baud_rate)
};

/** The "module-type" of each of the ubxlib-device-short-range
 * compatible devices as an array of C tokens, in the order they
 * appear in the device tree.
 */
static const uShortRangeModuleType_t gDeviceCfgShortRangeModuleType[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_short_range,
                                 U_PORT_BOARD_CFG_GET_MODULE_TYPE,
                                 U_PORT_BOARD_CFG_DEFAULT_SHORT_RANGE_MODULE_TYPE)
};

/** Get whether the "open-cpu" property is set for each
 * ubxlib-device-short-range compatible device, in the order they
 * appear in the device tree.
 */
static const bool gDeviceCfgCellShortRangeOpenCpu[] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_short_range,
                                 U_PORT_BOARD_CFG_GET_BOOLEAN,
                                 open_cpu)
};

/** The hidden "do-not-set-network-type" properties pointed-to by
 * the first two "network" phandles of each of the
 * ubxlib-device-short-range compatible devices, or -1 where not
 * present, in the order they appear in the device tree.
 */
static const int32_t gpDeviceCfgShortRangeNetworkType[][2] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_short_range,
                                 U_PORT_BOARD_CFG_GET_NETWORK_LIST_INT,
                                 do_not_set_network_type)
};

#endif // #if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_short_range)

/* ----------------------------------------------------------------
 * VARIABLES: BLE NETWORK CONFIGURATION PARAMETERS FROM THE DEVICE TREE
 * -------------------------------------------------------------- */

#if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_ble)

/** Table to convert a BLE "role" string into an enum, required because
 * there is no way to get the string from the DT as a C token when
 * it is on the other end of a phandle.
 */
static uPortBoardCfgStringToEnum_t gNetworkBleRoleStringToEnum[] = {
    {"U_BLE_CFG_ROLE_DISABLED", U_BLE_CFG_ROLE_DISABLED},
    {"U_BLE_CFG_ROLE_CENTRAL", U_BLE_CFG_ROLE_CENTRAL},
    {"U_BLE_CFG_ROLE_PERIPHERAL", U_BLE_CFG_ROLE_PERIPHERAL},
    {"U_BLE_CFG_ROLE_CENTRAL_AND_PERIPHERAL", U_BLE_CFG_ROLE_CENTRAL_AND_PERIPHERAL}
};

/** The "role" properties pointed-to by the first two
 * "network" phandles of each of the ubxlib-device-cellular
 * compatible devices, or NULL where not present, in the order
 * they appear in the device tree.  We would ideally get the
 * value in token form rather than string form but unfortunately
 * there is no macro in the Zephyr device tree macro set which
 * will get a C token from a phandle-referenced node.
 */
static const char *const gpDeviceCfgBleNetworkListRole[][2] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_short_range,
                                 U_PORT_BOARD_CFG_GET_NETWORK_LIST_STRING,
                                 role)
};

/** Get whether "enable-sps-server" is set in the nodes pointed-to
 * by the first two "network" phandles of each of the
 * ubxlib-device-short-range compatible devices, in the order they
 * appear in the device tree.
 */
static const bool gDeviceCfgBleNetworkListEnableSpsServer[][2] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_short_range,
                                 U_PORT_BOARD_CFG_GET_NETWORK_LIST_BOOLEAN,
                                 enable_sps_server)
};

#endif // #if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_ble)

/* ----------------------------------------------------------------
 * VARIABLES: GNSS NETWORK CONFIGURATION PARAMETERS FROM THE DEVICE TREE
 * -------------------------------------------------------------- */

#if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_gnss)

/** Table to convert a GNSS "module-type" string into an enum,
 * required because there is no way to get the string from the DT
 * as a C token when it is on the other end of a phandle.
 */
static uPortBoardCfgStringToEnum_t gNetworkGnssModuleTypeStringToEnum[] = {
    {"U_GNSS_MODULE_TYPE_M8", U_GNSS_MODULE_TYPE_M8},
    {"U_GNSS_MODULE_TYPE_M9", U_GNSS_MODULE_TYPE_M9},
    {"U_GNSS_MODULE_TYPE_M10", U_GNSS_MODULE_TYPE_M10}
};

#endif // #if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_gnss)

/* ----------------------------------------------------------------
 * VARIABLES: Wi-FI NETWORK CONFIGURATION PARAMETERS FROM THE DEVICE TREE
 * -------------------------------------------------------------- */

#if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_wifi)

/** Table to convert a Wi-Fi "mode" string into an enum,
 * required because there is no way to get the string from the DT
 * as a C token when it is on the other end of a phandle.
 */
static uPortBoardCfgStringToEnum_t gNetworkWifiModeStringToEnum[] = {
    {"U_WIFI_MODE_STA", U_WIFI_MODE_STA},
    {"U_WIFI_MODE_AP", U_WIFI_MODE_AP},
    {"U_WIFI_MODE_STA_AP", U_WIFI_MODE_STA_AP},
    {"U_WIFI_MODE_NONE", U_WIFI_MODE_NONE}
};

/** The "ssid" properties pointed-to by the first two
 * "network" phandles of each of the ubxlib-device-cellular
 * compatible devices, or NULL where not present, in the order
 * they appear in the device tree.
 */
static const char *const gpDeviceCfgWifiNetworkListSsid[][2] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_short_range,
                                 U_PORT_BOARD_CFG_GET_NETWORK_LIST_STRING,
                                 ssid)
};

/** The "authentication" properties pointed-to by the first two
 * "network" phandles of each of the ubxlib-device-cellular
 * compatible devices, or 1 (open) where not present, in the order
 * they appear in the device tree.
 */
static const int32_t gDeviceCfgWifiNetworkListAuthentication[][2] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_short_range,
                                 U_PORT_BOARD_CFG_GET_NETWORK_LIST_INT_OR_DEFAULT,
                                 authentication, 1)
};

/** The "pass-phrase" properties pointed-to by the first two
 * "network" phandles of each of the ubxlib-device-cellular
 * compatible devices, or NULL where not present, in the order
 * they appear in the device tree.
 */
static const char *const gpDeviceCfgWifiNetworkListPassPhrase[][2] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_short_range,
                                 U_PORT_BOARD_CFG_GET_NETWORK_LIST_STRING,
                                 pass_phrase)
};

/** The "host-name" properties pointed-to by the first two
 * "network" phandles of each of the ubxlib-device-cellular
 * compatible devices, or NULL where not present, in the order
 * they appear in the device tree.
 */
static const char *const gpDeviceCfgWifiNetworkListHostName[][2] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_short_range,
                                 U_PORT_BOARD_CFG_GET_NETWORK_LIST_STRING,
                                 host_name)
};

/** The "mode" properties pointed-to by the first two
 * "network" phandles of each of the ubxlib-device-cellular
 * compatible devices, or NULL where not present, in the order
 * they appear in the device tree.  We would ideally get the
 * value in token form rather than string form but unfortunately
 * there is no macro in the Zephyr device tree macro set which
 * will get a C token from a phandle-referenced node.
 */
static const char *const gpDeviceCfgWifiNetworkListMode[][2] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_short_range,
                                 U_PORT_BOARD_CFG_GET_NETWORK_LIST_STRING,
                                 mode)
};

/** The "ap-ssid" properties pointed-to by the first two
 * "network" phandles of each of the ubxlib-device-cellular
 * compatible devices, or NULL where not present, in the order
 * they appear in the device tree.
 */
static const char *const gpDeviceCfgWifiNetworkListApSsid[][2] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_short_range,
                                 U_PORT_BOARD_CFG_GET_NETWORK_LIST_STRING,
                                 ap_ssid)
};

/** The "ap-authentication" properties pointed-to by the first two
 * "network" phandles of each of the ubxlib-device-cellular
 * compatible devices, or 1 (open) where not present, in the order
 * they appear in the device tree.
 */
static const int32_t gDeviceCfgWifiNetworkListApAuthentication[][2] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_short_range,
                                 U_PORT_BOARD_CFG_GET_NETWORK_LIST_INT_OR_DEFAULT,
                                 ap_authentication, 1)
};

/** The "ap-pass-phrase" properties pointed-to by the first two
 * "network" phandles of each of the ubxlib-device-cellular
 * compatible devices, or NULL where not present, in the order
 * they appear in the device tree.
 */
static const char *const gpDeviceCfgWifiNetworkListApPassPhrase[][2] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_short_range,
                                 U_PORT_BOARD_CFG_GET_NETWORK_LIST_STRING,
                                 ap_pass_phrase)
};

/** The "ap-ip-address" properties pointed-to by the first two
 * "network" phandles of each of the ubxlib-device-cellular
 * compatible devices, or NULL where not present, in the order
 * they appear in the device tree.
 */
static const char *const gpDeviceCfgWifiNetworkListApIpAddress[][2] = {
    DT_FOREACH_STATUS_OKAY_VARGS(u_blox_ubxlib_device_short_range,
                                 U_PORT_BOARD_CFG_GET_NETWORK_LIST_STRING,
                                 ap_ip_address)
};

#endif // #if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_wifi)

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: DEVICE CONFIGURATION RELATED
 * -------------------------------------------------------------- */

#if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_cellular) ||    \
    DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_gnss) ||        \
    DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_short_range) || \
    DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_ble) ||        \
    DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_cellular) ||   \
    DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_gnss) ||       \
    DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_wifi)

// Find the index into the arrays above for the given device
// types and, optionally, name.
// Note: two device types are allowed since a short range
// device can be either U_DEVICE_TYPE_SHORT_RANGE or
// U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU; if the second device
// type is not required it should be set to U_DEVICE_TYPE_NONE.
// Note: this function should only be called if either (a)
// pThisCfgName is non-NULL or (b) there is only a single ubxlib
// device configuration entry in the device tree or (c)
// there is only a single ubxlib device configuration entry
// of the given type(s) in the device tree.
static int32_t findCfg(uDeviceType_t wantedType1, uDeviceType_t wantedType2,
                       uDeviceType_t thisType, const char *pThisCfgName,
                       const char *const *ppDeviceNameList,
                       size_t deviceNameListNumEntries,
                       bool printIt)
{
    int32_t index = -1;
    bool found = false;

    if ((thisType == U_DEVICE_TYPE_NONE) ||
        ((thisType == wantedType1) || (thisType == wantedType2))) {
        // Either a single entry in the device tree or only
        // a single entry of this type in the device tree
        index = 0;
    }
    if (pThisCfgName != NULL) {
        // A name was specified so need to find just that
        for (index = 0; index < deviceNameListNumEntries; index++) {
            if (strcmp(pThisCfgName, *(ppDeviceNameList + index)) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            index = -1;
            if (printIt &&
                ((thisType == wantedType1) || (thisType == wantedType2))) {
                // This is not an error: it just means the user has
                // chosen not to override this cellular device
                // configuration using the device tree
                uPortLog("U_PORT_BOARD_CFG: device \"%s\" not found in the device tree.\n",
                         pThisCfgName);
            }
        }
    }

    return index;
}

#endif // #if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_cellular) ||
//     DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_gnss) ||
//     DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_short_range) ||
//     DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_ble) ||
//     DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_cellular) ||
//     DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_gnss) ||
//     DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_wifi)

#if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_cellular) ||    \
    DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_gnss) ||        \
    DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_short_range)

// Get the UART number from a UART string (e.g. "uart0").
static int32_t getUart(const char *pUartString)
{
    int32_t x = -1;

    if (pUartString != NULL) {
        // In order that this works for randomly named UARTs,
        // e.g. "serial0", don't check anything other than
        // that there is a number on the end and return
        // that number
        while ((*pUartString != 0) && !isdigit((int32_t) *pUartString)) {
            pUartString++;
        }
        if (pUartString != 0) {
            x = strtol(pUartString, NULL, 10);
        }
    }

    return x;
}

// Get the port from a string, e.g. "i2c0", "spi0", "uart0",
// returning the type in the last parameter.
static int32_t getPort(const char *pPortString,
                       uDeviceTransportType_t *pTransportType)
{
    int32_t x = -1;

    if (pPortString != NULL) {
        *pTransportType = U_DEVICE_TRANSPORT_TYPE_NONE;
        // Check if we have an I2C or SPI port, which
        // are always conventionally named, but allow for there
        // to be an ampersand at the start
        if (*pPortString == '&') {
            pPortString++;
        }
        if (strstr(pPortString, U_PORT_BOARD_CFG_SPI_PREFIX) == pPortString) {
            *pTransportType = U_DEVICE_TRANSPORT_TYPE_SPI;
            x = strtol(pPortString + strlen(U_PORT_BOARD_CFG_SPI_PREFIX), NULL, 10);
        } else if (strstr(pPortString, U_PORT_BOARD_CFG_I2C_PREFIX) == pPortString) {
            *pTransportType = U_DEVICE_TRANSPORT_TYPE_I2C;
            x = strtol(pPortString + strlen(U_PORT_BOARD_CFG_I2C_PREFIX), NULL, 10);
        } else {
            // If it is not SPI or I2C, let the UART function have at it
            x = getUart(pPortString);
            if (x >= 0) {
                *pTransportType = U_DEVICE_TRANSPORT_TYPE_UART;
            }
        }
    }

    return x;
}

#endif // #if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_cellular) ||
//     DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_gnss) ||
//     DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_short_range)

#if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_cellular)

// Configure a uDeviceCfg_t struct for cellular.
static void cfgCellular(uDeviceCfg_t *pCfg, int32_t index)
{
    uDeviceCfgCell_t *pCfgCell = &(pCfg->deviceCfg.cfgCell);
    uDeviceCfgUart_t *pCfgUart = &(pCfg->transportCfg.cfgUart);
    int32_t x;

    // To prevent warnings about unused static functions while not
    // ripping this code to shreds with conditional compilation,
    // do a dummy call to getPort()
    getPort(NULL, NULL);

    pCfg->version = 0;
    pCfg->deviceType = U_DEVICE_TYPE_CELL;
    pCfg->pCfgName = gpCfgCellDeviceName[index];
    memset(pCfgCell, 0, sizeof(*pCfgCell));
    pCfgCell->moduleType = gDeviceCfgCellModuleType[index];
    pCfgCell->pinEnablePower = gDeviceCfgCellPinEnablePower[index];
    pCfgCell->pinPwrOn = gDeviceCfgCellPinPwrOn[index];
    pCfgCell->pinVInt = gDeviceCfgCellPinVInt[index];
    pCfgCell->pinDtrPowerSaving = gDeviceCfgCellPinDtrPowerSaving[index];
    // Only UART transport for cellular
    pCfg->transportType = U_DEVICE_TRANSPORT_TYPE_NONE;
    memset(pCfgUart, 0, sizeof(*pCfgUart));
    // Don't need to check for NULL here as transport is a required field for cellular
    x = getUart(gpDeviceCfgCellTransportType[index]);
    if (x >= 0) {
        pCfg->transportType = U_DEVICE_TRANSPORT_TYPE_UART;
        pCfgUart->uart = x;
        pCfgUart->baudRate = gDeviceCfgCellUartBaudRate[index];
        pCfgUart->pinTxd = -1;
        pCfgUart->pinRxd = -1;
        pCfgUart->pinCts = -1;
        pCfgUart->pinRts = -1;
    }
    uPortLog("U_PORT_BOARD_CFG: using CELLULAR device \"%s\" from the device tree,"
             " module-type %d on UART %d, uart-baud-rate %d with pin-enable-power %d (0x%02x),"
             " pin-pwr-on %d (0x%02x), pin-vint %d  (0x%02x), pin-dtr-power-saving %d  (0x%02x).\n",
             gpCfgCellDeviceName[index],
             pCfgCell->moduleType, pCfgUart->uart, pCfgUart->baudRate,
             pCfgCell->pinEnablePower, pCfgCell->pinEnablePower,
             pCfgCell->pinPwrOn, pCfgCell->pinPwrOn,
             pCfgCell->pinVInt, pCfgCell->pinVInt,
             pCfgCell->pinDtrPowerSaving, pCfgCell->pinDtrPowerSaving);
}

#endif // #if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_cellular)

#if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_gnss)

// Configure a uDeviceCfg_t struct for GNSS.
static void cfgGnss(uDeviceCfg_t *pCfg, int32_t index)
{
    uDeviceCfgGnss_t *pCfgGnss = &(pCfg->deviceCfg.cfgGnss);
    int32_t x;

    pCfg->version = 0;
    pCfg->deviceType = U_DEVICE_TYPE_GNSS;
    pCfg->pCfgName = gpCfgGnssDeviceName[index];
    memset(pCfgGnss, 0, sizeof(*pCfgGnss));
    pCfgGnss->moduleType = gDeviceCfgGnssModuleType[index];
    pCfgGnss->pinEnablePower = gDeviceCfgGnssPinEnablePower[index];
    pCfgGnss->pinDataReady = gDeviceCfgGnssPinDataReady[index];
    pCfgGnss->i2cAddress = -1;
    pCfgGnss->powerOffToBackup = gDeviceCfgGnssPowerOffToBackup[index];
    uPortLog("U_PORT_BOARD_CFG: using GNSS device \"%s\" from the device tree,"
             " module-type %d with pin-enable-power %d (0x%02x),"
             " pin-data-ready %d (0x%02x)%s...\n",
             gpCfgGnssDeviceName[index], pCfgGnss->moduleType,
             pCfgGnss->pinEnablePower, pCfgGnss->pinEnablePower,
             pCfgGnss->pinDataReady, pCfgGnss->pinDataReady,
             pCfgGnss->powerOffToBackup ? ", power-off-to-backup" : "");
    // Don't need to check for NULL here as transport is a required field for GNSS
    x = getPort(gpDeviceCfgGnssTransportType[index], &(pCfg->transportType));
    switch (pCfg->transportType) {
        case U_DEVICE_TRANSPORT_TYPE_UART: {
            uDeviceCfgUart_t *pCfgUart = &(pCfg->transportCfg.cfgUart);
            memset(pCfgUart, 0, sizeof(*pCfgUart));
            if (gDeviceCfgGnssUart2[index]) {
                pCfg->transportType = U_DEVICE_TRANSPORT_TYPE_UART_2;
            }
            pCfgUart->uart = x;
            pCfgUart->baudRate = gDeviceCfgGnssUartBaudRate[index];
            pCfgUart->pinTxd = -1;
            pCfgUart->pinRxd = -1;
            pCfgUart->pinCts = -1;
            pCfgUart->pinRts = -1;
            uPortLog("U_PORT_BOARD_CFG: ...GNSS on UART %d, uart-baud-rate %d%s.\n",
                     pCfgUart->uart, pCfgUart->baudRate,
                     gDeviceCfgGnssUart2[index] ? ", gnss-uart2" : "");
        }
        break;
        case U_DEVICE_TRANSPORT_TYPE_I2C: {
            uDeviceCfgI2c_t *pCfgI2c = &(pCfg->transportCfg.cfgI2c);
            memset(pCfgI2c, 0, sizeof(*pCfgI2c));
            pCfgGnss->i2cAddress = (uint16_t) gDeviceCfgGnssI2cAddress[index];
            pCfgI2c->i2c = x;
            pCfgI2c->clockHertz = gDeviceCfgGnssI2cClockHertz[index];
            pCfgI2c->alreadyOpen = gDeviceCfgGnssI2cAlreadyOpen[index];
            pCfgI2c->pinSda = -1;
            pCfgI2c->pinScl = -1;
            pCfgI2c->maxSegmentSize = gDeviceCfgGnssI2cMaxSegmentSize[index];
            uPortLog("U_PORT_BOARD_CFG: ...GNSS on I2C %d, i2c-address 0x%02x, i2c-clock-hertz %d, i2c-max-segment-size %d%s.\n",
                     pCfgI2c->i2c, pCfgGnss->i2cAddress,
                     pCfgI2c->clockHertz,
                     pCfgI2c->maxSegmentSize,
                     pCfgI2c->alreadyOpen ? ", i2c-already-open" : "");
        }
        break;
        case U_DEVICE_TRANSPORT_TYPE_SPI: {
            uDeviceCfgSpi_t *pCfgSpi = &(pCfg->transportCfg.cfgSpi);
            uCommonSpiControllerDevice_t *pSpiDevice = &(pCfgSpi->device);
            memset(pCfgSpi, 0, sizeof(*pCfgSpi));
            memset(pSpiDevice, 0, sizeof(*pSpiDevice));
            pCfgSpi->spi = x;
            pCfgSpi->pinMosi = -1;
            pCfgSpi->pinMiso = -1;
            pCfgSpi->pinClk = -1;
            pCfgSpi->maxSegmentSize = gDeviceCfgGnssSpiMaxSegmentSize[index];
            pSpiDevice->pinSelect = gDeviceCfgGnssSpiPinSelect[index];
            pSpiDevice->indexSelect = gDeviceCfgGnssSpiIndexSelect[index];
            pSpiDevice->frequencyHertz = gDeviceCfgGnssSpiFrequencyHertz[index];
            pSpiDevice->mode = gDeviceCfgGnssSpiMode[index];
            pSpiDevice->wordSizeBytes = gDeviceCfgGnssSpiWordSizeBytes[index];
            pSpiDevice->lsbFirst = gDeviceCfgGnssSpiLsbFirst[index];
            pSpiDevice->startOffsetNanoseconds = gDeviceCfgGnssSpiStartOffsetNanoseconds[index];
            pSpiDevice->stopOffsetNanoseconds = gDeviceCfgGnssSpiStopOffsetNanoseconds[index];
            // Can't set these last two in Zephyr
            pSpiDevice->sampleDelayNanoseconds = U_COMMON_SPI_SAMPLE_DELAY_NANOSECONDS;
            pSpiDevice->fillWord = U_COMMON_SPI_FILL_WORD;
            uPortLog("U_PORT_BOARD_CFG: ...GNSS on SPI %d, spi-max-segment-size %d,"
                     " spi-pin-select %d (0x%02x), spi-index-select %d,"
                     " spi-frequency-hertz %d, spi-mode %d, spi-word-size-bytes %d%s,"
                     " spi-start-offset-nanoseconds %d, spi-stop-offset-nanoseconds %d"
                     " [sample delay %d nanoseconds, fill word 0x%08x].\n",
                     pCfgSpi->spi, pCfgSpi->maxSegmentSize, pSpiDevice->pinSelect,
                     pSpiDevice->pinSelect, pSpiDevice->indexSelect,
                     pSpiDevice->frequencyHertz, pSpiDevice->mode,
                     pSpiDevice->wordSizeBytes,
                     pSpiDevice->lsbFirst ? ", spi-lsb-first" : "",
                     pSpiDevice->startOffsetNanoseconds,
                     pSpiDevice->stopOffsetNanoseconds,
                     pSpiDevice->sampleDelayNanoseconds,
                     pSpiDevice->fillWord);
        }
        break;
        default:
            break;
    }
}

#endif // #if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_gnss)

#if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_short_range)

// Configure a uDeviceCfg_t struct for short-range.
static void cfgShortRange(uDeviceCfg_t *pCfg, int32_t index)
{
    uDeviceCfgShortRange_t *pCfgShortRange = &(pCfg->deviceCfg.cfgSho);
    uDeviceCfgUart_t *pCfgUart = &(pCfg->transportCfg.cfgUart);
    int32_t x;

    // To prevent warnings about unused static functions while not
    // ripping this code to shreds with conditional compilation,
    // do a dummy call to getPort()
    getPort(NULL, NULL);

    pCfg->version = 0;
    pCfg->deviceType = U_DEVICE_TYPE_SHORT_RANGE;
    if (gDeviceCfgCellShortRangeOpenCpu[index]) {
        pCfg->deviceType = U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU;
    }
    pCfg->pCfgName = gpCfgShortRangeDeviceName[index];
    memset(pCfgShortRange, 0, sizeof(*pCfgShortRange));
    pCfgShortRange->moduleType = gDeviceCfgShortRangeModuleType[index];
    // Only UART transport for short range, but the transport
    // type can be missing (the open CPU case)
    pCfg->transportType = U_DEVICE_TRANSPORT_TYPE_NONE;
    memset(pCfgUart, 0, sizeof(*pCfgUart));
    if (gpDeviceCfgShortRangelTransportType[index] != NULL) {
        x = getUart(gpDeviceCfgShortRangelTransportType[index]);
        if (x >= 0) {
            pCfg->transportType = U_DEVICE_TRANSPORT_TYPE_UART;
            pCfgUart->uart = x;
            pCfgUart->baudRate = gDeviceCfgShortRangeUartBaudRate[index];
            pCfgUart->pinTxd = -1;
            pCfgUart->pinRxd = -1;
            pCfgUart->pinCts = -1;
            pCfgUart->pinRts = -1;
        }
    }
    uPortLog("U_PORT_BOARD_CFG: using SHORT-RANGE device \"%s\" from the device tree,"
             " module-type %d%s", gpCfgShortRangeDeviceName[index],
             pCfgShortRange->moduleType,
             (pCfg->deviceType == U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU) ? ", open-cpu" : "");
    if (pCfg->transportType == U_DEVICE_TRANSPORT_TYPE_UART) {
        uPortLog(", on UART %d, uart-baud-rate %d", pCfgUart->uart, pCfgUart->baudRate);
    }
    uPortLog(".\n");
}

#endif // #if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_short_range)

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: NETWORK CONFIGURATION RELATED
 * -------------------------------------------------------------- */

#if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_ble) ||       \
    DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_cellular) ||  \
    DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_gnss) ||      \
    DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_wifi) ||      \
    DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_cellular) ||   \
    DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_gnss) ||       \
    DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_short_range)

// For the given device instance, return the index of it
// in the various arrays above.
static int32_t getDeviceIndex(uDeviceInstance_t *pDeviceInstance)
{
    int32_t index = -1;

    switch (pDeviceInstance->deviceType) {
        case U_DEVICE_TYPE_CELL:
# if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_cellular)
            index = findCfg(U_DEVICE_TYPE_CELL, U_DEVICE_TYPE_NONE,
                            pDeviceInstance->deviceType, pDeviceInstance->pCfgName,
                            gpCfgCellDeviceName,
                            sizeof(gpCfgCellDeviceName) / sizeof(gpCfgCellDeviceName[0]),
                            false);
# endif
            break;
        case U_DEVICE_TYPE_GNSS:
# if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_gnss)
            index = findCfg(U_DEVICE_TYPE_GNSS, U_DEVICE_TYPE_NONE,
                            pDeviceInstance->deviceType, pDeviceInstance->pCfgName,
                            gpCfgGnssDeviceName,
                            sizeof(gpCfgGnssDeviceName) / sizeof(gpCfgGnssDeviceName[0]),
                            false);
# endif
            break;
        case U_DEVICE_TYPE_SHORT_RANGE:
        case U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU:
# if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_short_range)
            index = findCfg(U_DEVICE_TYPE_SHORT_RANGE, U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU,
                            pDeviceInstance->deviceType, pDeviceInstance->pCfgName,
                            gpCfgShortRangeDeviceName,
                            sizeof(gpCfgShortRangeDeviceName) / sizeof(gpCfgShortRangeDeviceName[0]),
                            false);
# endif
            break;
        default:
            break;
    }

    return index;
}

// For the given device configuration type, return the index of the
// network array that is the correct one for the given network type.
static int32_t getNetworkIndex(int32_t deviceIndex,
                               uDeviceType_t deviceType,
                               uNetworkType_t networkType)
{
    int32_t index = -1;
    const int32_t *pNetworkTypeList = NULL;
    size_t networkTypeListSize = 0;
    bool found = false;

    // Pick up the "do-not-set-network-type" array for this device type
    switch (deviceType) {
        case U_DEVICE_TYPE_CELL:
#if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_cellular)
            pNetworkTypeList = gpDeviceCfgCellNetworkType[deviceIndex];
            networkTypeListSize = sizeof(gpDeviceCfgCellNetworkType[deviceIndex]) /
                                  sizeof(gpDeviceCfgCellNetworkType[deviceIndex][0]);
#endif
            break;
        case U_DEVICE_TYPE_GNSS:
            // There is no "do-not-set-network-type" array for a GNSS device
            break;
        case U_DEVICE_TYPE_SHORT_RANGE:
        case U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU:
#if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_short_range)
            pNetworkTypeList = gpDeviceCfgShortRangeNetworkType[deviceIndex];
            networkTypeListSize = sizeof(gpDeviceCfgShortRangeNetworkType[deviceIndex]) /
                                  sizeof(gpDeviceCfgShortRangeNetworkType[deviceIndex][0]);
#endif
            break;
        default:
            break;
    }

    if (pNetworkTypeList != NULL) {
        // Search the network type array for the type we want
        for (index = 0; index < networkTypeListSize; index++) {
            if (*(pNetworkTypeList + index) == (int32_t) networkType) {
                found = true;
                break;
            }
        }
        if (!found) {
            // If we haven't been able to find a network
            // configuration of the right type, return
            // the index of one which is definitely NOT
            // the correct type - this will provide
            // "not present" default values which we can
            // apply
            for (index = 0; index < networkTypeListSize; index++) {
                if (*(pNetworkTypeList + index) != (int32_t) networkType) {
                    found = true;
                    break;
                }
            }
        }
    }

    if (!found) {
        index = -1;
    }

    return index;
}

#endif // #if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_ble) ||
//            DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_cellular) ||
//            DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_gnss) ||
//            DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_wifi)
//            DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_cellular) ||
//            DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_gnss) ||
//            DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_short_range)

#if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_ble) ||     \
    DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_cellular) || \
    DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_gnss) ||    \
    DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_wifi)

// Get the int32_t enum value for a string.
static bool getEnum(const char *pString,
                    uPortBoardCfgStringToEnum_t *pTableStringToEnum,
                    size_t tableNumEntries,
                    int32_t *pValue)
{
    bool found = false;

    if (pString != NULL) {
        for (size_t x = 0; x < tableNumEntries; x++) {
            if (strcmp(pString, pTableStringToEnum->pString) == 0) {
                found = true;
                if (pValue != NULL) {
                    *pValue = pTableStringToEnum->value;
                }
                break;
            }
            pTableStringToEnum++;
        }
    }

    return found;
}

#endif // #if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_ble) ||
//            DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_cellular) ||
//            DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_gnss) ||
//            DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_wifi)

#if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_ble)

// Populate a BLE network configuration structure.
static void cfgNetworkBle(int32_t deviceIndex, int32_t networkIndex,
                          uNetworkCfgBle_t *pNetworkCfg)
{
    memset(pNetworkCfg, 0, sizeof(*pNetworkCfg));
    pNetworkCfg->type = U_NETWORK_TYPE_BLE;

    // Populate the "role" field through a table look-up
    if (!getEnum(gpDeviceCfgBleNetworkListRole[deviceIndex][networkIndex],
                 gNetworkBleRoleStringToEnum,
                 sizeof(gNetworkBleRoleStringToEnum) / sizeof(gNetworkBleRoleStringToEnum[0]),
                 &(pNetworkCfg->role))) {
        pNetworkCfg->role = U_PORT_BOARD_CFG_DEFAULT_BLE_ROLE;
    }
    pNetworkCfg->spsServer = gDeviceCfgBleNetworkListEnableSpsServer[deviceIndex][networkIndex];
    uPortLog("U_PORT_BOARD_CFG: using BLE network configuration"
             " associated with SHORT-RANGE device \"%s\" from the"
             " device tree, role %d%s.\n",
             gpCfgShortRangeDeviceName[deviceIndex],
             pNetworkCfg->role,
             pNetworkCfg->spsServer ? ", sps-server" : "");
}

#endif // #if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_ble)

#if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_cellular)
// Note: this is under u_blox_ubxlib_device_cellular and not
// u_blox_ubxlib_network_cellular as we need the default
// settings to be populated in all cases.

// Populate a cellular network configuration structure.
static void cfgNetworkCellular(int32_t deviceIndex, int32_t networkIndex,
                               uNetworkCfgCell_t *pNetworkCfg)
{
    bool (*pKeepGoingCallback) (uDeviceHandle_t) = pNetworkCfg->pKeepGoingCallback;
    uDeviceCfgUart_t *pUartPpp = (uDeviceCfgUart_t *) pNetworkCfg->pUartPpp;
    int32_t x;

    // Free any memory that the application may have allocated for a PPP UART
    uPortFree(pUartPpp);
    memset(pNetworkCfg, 0, sizeof(*pNetworkCfg));
    pNetworkCfg->type = U_NETWORK_TYPE_CELL;
    // Special case: we would like the application to still be able to use
    // a pKeepGoingCallback since it is rather more flexible, however there
    // is no way for one to be provided through the device tree, hence we
    // just keep any that the  application may have set in C code
    pNetworkCfg->pKeepGoingCallback = pKeepGoingCallback;

    // To prevent warnings about unused static functions while not
    // ripping this code to shreds with conditional compilation,
    // do a dummy call to getEnum()
    getEnum(NULL, NULL, 0, NULL);

    pNetworkCfg->pApn = gpDeviceCfgCellNetworkListApn[deviceIndex][networkIndex];
    pNetworkCfg->timeoutSeconds = gDeviceCfgCellNetworkListTimeoutSeconds[deviceIndex][networkIndex];
    pNetworkCfg->pUsername = gpDeviceCfgCellNetworkListUsername[deviceIndex][networkIndex];
    pNetworkCfg->pPassword = gpDeviceCfgCellNetworkListPassword[deviceIndex][networkIndex];
    pNetworkCfg->authenticationMode =
        gDeviceCfgCellNetworkListAuthenticationMode[deviceIndex][networkIndex];
    pNetworkCfg->pMccMnc = gpDeviceCfgCellNetworkListMccMnc[deviceIndex][networkIndex];
    if (gpDeviceCfgCellUartPppListTransportType[deviceIndex][networkIndex] != NULL) {
        pUartPpp = (uDeviceCfgUart_t *) pUPortMalloc(sizeof(*pUartPpp));
        if (pUartPpp != NULL) {
            memset(pUartPpp, 0, sizeof(*pUartPpp));
            pUartPpp->uart = -1;
            x = getUart(gpDeviceCfgCellUartPppListTransportType[deviceIndex][networkIndex]);
            if (x >= 0) {
                pUartPpp->uart = x;
                pUartPpp->baudRate = gDeviceCfgCellUartPppListUartBaudRate[deviceIndex][networkIndex];
                pUartPpp->pinTxd = -1;
                pUartPpp->pinRxd = -1;
                pUartPpp->pinCts = -1;
                pUartPpp->pinRts = -1;
            }
            pNetworkCfg->pUartPpp = pUartPpp;
        }
    }
    pNetworkCfg->asyncConnect = gDeviceCfgCellNetworkListAsyncConnect[deviceIndex][networkIndex];
    uPortLog("U_PORT_BOARD_CFG: using CELLULAR network configuration"
             " associated with device \"%s\" from the device tree,"
             " timeout-seconds ", gpCfgCellDeviceName[deviceIndex]);
    if (pNetworkCfg->pKeepGoingCallback) {
        uPortLog("from pKeepGoingCallback,");
    } else {
        uPortLog("%d,", pNetworkCfg->timeoutSeconds);
    }
    // Since whether the APN is "" or NULL is significant, be
    // explicit about that
    if (pNetworkCfg->pApn) {
        uPortLog(" APN \"%s\",", pNetworkCfg->pApn);
    } else {
        uPortLog(" APN NULL,");
    }
    uPortLog(" username \"%s\", password \"%s\","
             " authentication-mode %d, MCC/MNC %s, async-connect %s",
             pNetworkCfg->pUsername ? pNetworkCfg->pUsername : "",
             pNetworkCfg->pPassword ? U_PORT_BOARD_CFG_SECRET_STRING : "",
             pNetworkCfg->authenticationMode,
             pNetworkCfg->pMccMnc ? pNetworkCfg->pMccMnc : "NULL",
             pNetworkCfg->asyncConnect ? "true" : "false");
    if (pNetworkCfg->pUartPpp) {
        uPortLog(", uart-ppp: uart %d, uart-baud-rate %d.\n",
                 pNetworkCfg->pUartPpp->uart,
                 pNetworkCfg->pUartPpp->baudRate);
    } else {
        uPortLog(", uart-ppp: NULL.\n");
    }
}

#endif // #if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_cellular)

#if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_gnss)

// Populate a GNSS network configuration structure.
static void cfgNetworkGnss(int32_t deviceIndex, int32_t networkIndex,
                           uNetworkCfgGnss_t *pNetworkCfg)
{
    memset(pNetworkCfg, 0, sizeof(*pNetworkCfg));
    pNetworkCfg->type = U_NETWORK_TYPE_GNSS;

    // The naming here is a bit confusing: the names starts with
    // DeviceCfgCell since this network configuration is, and can
    // only be, pointed-to by a cellular device

    // Populate the "module-type" field through a table look-up
    if (!getEnum(gpDeviceCfgCellNetworkListModuleType[deviceIndex][networkIndex],
                 gNetworkGnssModuleTypeStringToEnum,
                 sizeof(gNetworkGnssModuleTypeStringToEnum) / sizeof(gNetworkGnssModuleTypeStringToEnum[0]),
                 &(pNetworkCfg->moduleType))) {
        pNetworkCfg->moduleType = U_PORT_BOARD_CFG_DEFAULT_GNSS_MODULE_TYPE;
    }
    pNetworkCfg->devicePinPwr = gDeviceCfgCellNetworkListDevicePinPower[deviceIndex][networkIndex];
    pNetworkCfg->devicePinDataReady =
        gDeviceCfgCellNetworkListDevicePinDataReady[deviceIndex][networkIndex];
    uPortLog("U_PORT_BOARD_CFG: using GNSS network configuration"
             " associated with CELLULAR device \"%s\" from the"
             " device tree, GNSS module-type %d, device-pin-pwr %d,"
             " device-pin-data-ready %d.\n",
             gpCfgCellDeviceName[deviceIndex],
             pNetworkCfg->moduleType,
             pNetworkCfg->devicePinPwr,
             pNetworkCfg->devicePinDataReady);
}

#endif // #if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_gnss)

#if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_wifi)

// Populate a Wi-Fi network configuration structure.
static void cfgNetworkWifi(int32_t deviceIndex, int32_t networkIndex,
                           uNetworkCfgWifi_t *pNetworkCfg)
{
    memset(pNetworkCfg, 0, sizeof(*pNetworkCfg));
    pNetworkCfg->type = U_NETWORK_TYPE_WIFI;

    // Populate the "mode" field through a table look-up
    if (!getEnum(gpDeviceCfgWifiNetworkListMode[deviceIndex][networkIndex],
                 gNetworkWifiModeStringToEnum,
                 sizeof(gNetworkWifiModeStringToEnum) / sizeof(gNetworkWifiModeStringToEnum[0]),
                 (int32_t *) &(pNetworkCfg->mode))) { // *NOPAD*
        pNetworkCfg->mode = U_PORT_BOARD_CFG_DEFAULT_WIFI_MODE;
    }
    pNetworkCfg->pSsid = gpDeviceCfgWifiNetworkListSsid[deviceIndex][networkIndex];
    pNetworkCfg->authentication = gDeviceCfgWifiNetworkListAuthentication[deviceIndex][networkIndex];
    pNetworkCfg->pPassPhrase = gpDeviceCfgWifiNetworkListPassPhrase[deviceIndex][networkIndex];
    pNetworkCfg->pHostName = gpDeviceCfgWifiNetworkListHostName[deviceIndex][networkIndex];
    pNetworkCfg->pApSssid = gpDeviceCfgWifiNetworkListApSsid[deviceIndex][networkIndex];
    pNetworkCfg->apAuthentication =
        gDeviceCfgWifiNetworkListApAuthentication[deviceIndex][networkIndex];
    pNetworkCfg->pApPassPhrase = gpDeviceCfgWifiNetworkListApPassPhrase[deviceIndex][networkIndex];
    pNetworkCfg->pApIpAddress = gpDeviceCfgWifiNetworkListApIpAddress[deviceIndex][networkIndex];
    uPortLog("U_PORT_BOARD_CFG: using WI-FI network configuration"
             " associated with SHORT-RANGE device \"%s\" from the"
             " device tree, mode %d, ssid \"%s\", authentication %d,"
             " pass-phrase \"%s\", host-name \"%s\", ap-ssid \"%s\","
             " ap-authentication %d, ap-pass-phrase \"%s\","
             " ap-ip-address \"%s\".\n",
             gpCfgShortRangeDeviceName[deviceIndex],
             pNetworkCfg->mode,
             pNetworkCfg->pSsid ? pNetworkCfg->pSsid : "",
             pNetworkCfg->authentication,
             pNetworkCfg->pPassPhrase ? U_PORT_BOARD_CFG_SECRET_STRING : "",
             pNetworkCfg->pHostName ? pNetworkCfg->pHostName : "",
             pNetworkCfg->pApSssid ? pNetworkCfg->pApSssid : "",
             pNetworkCfg->apAuthentication,
             pNetworkCfg->pApPassPhrase ? U_PORT_BOARD_CFG_SECRET_STRING : "",
             pNetworkCfg->pApIpAddress ? pNetworkCfg->pApIpAddress : "");
}

#endif // #if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_wifi)

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: MISC
 * -------------------------------------------------------------- */

#if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_cellular) ||    \
    DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_gnss) ||        \
    DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_short_range)

// Dummy initialisation function for the ubxlib device.
static int init(const struct device *pDev)
{
    (void) pDev;
    return 0; // Return success
}

#endif // #if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_cellular) ||
//            DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_gnss) ||
//            DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_short_range)

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Override pDeviceCfg with any items that are in the device tree.
int32_t uPortBoardCfgDevice(void *pDeviceCfg)
{
    int32_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
#if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_cellular) || \
    DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_gnss)     || \
    DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_short_range)
    uDeviceCfg_t *pLocalDeviceCfg = (uDeviceCfg_t *) pDeviceCfg;
    int32_t index = -1;

    if (pDeviceCfg != NULL) {
        errorCode = U_ERROR_COMMON_SUCCESS;

        // This is all compile-time stuff, hence the laborious code here

        // First, check for configuration errors.
        // If there is more than one type of ubxlib device in
        // the device tree then the deviceType field must be set
# if ((DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_cellular) &&       \
       (DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_gnss) ||          \
        DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_short_range))) || \
      (DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_gnss) &&           \
       (DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_cellular) ||      \
        DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_short_range))) || \
      (DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_short_range) &&    \
       (DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_gnss) ||      \
        DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_cellular))))
        if (pLocalDeviceCfg->deviceType == U_DEVICE_TYPE_NONE) {
            errorCode = U_ERROR_COMMON_CONFIGURATION;
            uPortLog("U_PORT_BOARD_CFG: ERROR - %d ubxlib devices in"
                     " the device tree, deviceType must be populated"
                     " in the device configuration structure so that we"
                     " can find the right one.\n",
                     DT_NUM_INST_STATUS_OKAY(u_blox_ubxlib_device_cellular) +
                     DT_NUM_INST_STATUS_OKAY(u_blox_ubxlib_device_gnss) +
                     DT_NUM_INST_STATUS_OKAY(u_blox_ubxlib_device_short_range));
        }
# endif
        if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
            // Now deal with cellular
# if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_cellular)
            // There is a cellular device in the device tree
#  if DT_NUM_INST_STATUS_OKAY(u_blox_ubxlib_device_cellular) > 1
            // There is more than one cellular device in the device
            // tree, so a name must have been specified in the
            // configuration structure passed-in so that we know
            // which configuration to look for
            if ((pLocalDeviceCfg->deviceType == U_DEVICE_TYPE_CELL) &&
                (pLocalDeviceCfg->pCfgName == NULL)) {
                errorCode = U_ERROR_COMMON_CONFIGURATION;
                uPortLog("U_PORT_BOARD_CFG: ERROR - %d ubxlib"
                         " cellular devices in the device tree, pCfgName"
                         " must be populated in the device configuration"
                         " structure so that we can find the right one.\n",
                         DT_NUM_INST_STATUS_OKAY(u_blox_ubxlib_device_cellular));
            }
#  endif
            if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                index = findCfg(U_DEVICE_TYPE_CELL, U_DEVICE_TYPE_NONE,
                                pLocalDeviceCfg->deviceType, pLocalDeviceCfg->pCfgName,
                                gpCfgCellDeviceName,
                                sizeof(gpCfgCellDeviceName) / sizeof(gpCfgCellDeviceName[0]),
                                true);
                if (index >= 0) {
                    cfgCellular(pLocalDeviceCfg, index);
                }
            }
# endif // #if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_cellular)
        }

        if ((index < 0) && (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS)) {
            // If we didn't find anything in cellular, try GNSS
# if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_gnss)
            // There is a GNSS device in the device tree
#  if DT_NUM_INST_STATUS_OKAY(u_blox_ubxlib_device_gnss) > 1
            // There is more than one GNSS device in the device
            // tree, so a name must have been specified in the
            // configuration structure passed-in so that we know
            // which configuration to look for
            if ((pLocalDeviceCfg->deviceType == U_DEVICE_TYPE_GNSS) &&
                (pLocalDeviceCfg->pCfgName == NULL)) {
                errorCode = U_ERROR_COMMON_CONFIGURATION;
                uPortLog("U_PORT_BOARD_CFG: ERROR - %d ubxlib"
                         " GNSS devices in the device tree, pCfgName"
                         " must be populated in the device configuration"
                         " structure so that we can find the right one.\n",
                         DT_NUM_INST_STATUS_OKAY(u_blox_ubxlib_device_gnss));
            }
#  endif
            if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                index = findCfg(U_DEVICE_TYPE_GNSS, U_DEVICE_TYPE_NONE,
                                pLocalDeviceCfg->deviceType, pLocalDeviceCfg->pCfgName,
                                gpCfgGnssDeviceName,
                                sizeof(gpCfgGnssDeviceName) / sizeof(gpCfgGnssDeviceName[0]),
                                true);
                if (index >= 0) {
                    cfgGnss(pLocalDeviceCfg, index);
                }
            }
# endif // #if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_gnss)
        }

        if ((index < 0) && (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS)) {
            // And finally, if we didn't find anything in GNSS,
            // try short-range
# if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_short_range)
            // There is a short-range device in the device tree
#  if DT_NUM_INST_STATUS_OKAY(u_blox_ubxlib_device_short_range) > 1
            // There is more than one short-range device in the device
            // tree, so a name must have been specified in the
            // configuration structure passed-in so that we know
            // which configuration to look for
            if (((pLocalDeviceCfg->deviceType == U_DEVICE_TYPE_SHORT_RANGE) ||
                 (pLocalDeviceCfg->deviceType == U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU)) &&
                (pLocalDeviceCfg->pCfgName == NULL)) {
                errorCode = U_ERROR_COMMON_CONFIGURATION;
                uPortLog("U_PORT_BOARD_CFG: ERROR - %d ubxlib"
                         " short-range devices in the device tree, pCfgName"
                         " must be populated in the device configuration"
                         " structure so that we can find the right one.\n",
                         DT_NUM_INST_STATUS_OKAY(u_blox_ubxlib_device_short_range));
            }
#  endif
            if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                index = findCfg(U_DEVICE_TYPE_SHORT_RANGE, U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU,
                                pLocalDeviceCfg->deviceType, pLocalDeviceCfg->pCfgName,
                                gpCfgShortRangeDeviceName,
                                sizeof(gpCfgShortRangeDeviceName) / sizeof(gpCfgShortRangeDeviceName[0]),
                                true);
                if (index >= 0) {
                    cfgShortRange(pLocalDeviceCfg, index);
                }
            }
# endif // #if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_short_range)
        }
    }
#else
    errorCode = U_ERROR_COMMON_SUCCESS;
    (void) pDeviceCfg;
#endif

    return errorCode;
}

// Override pNetworkCfg with any items that are in the device tree.
int32_t uPortBoardCfgNetwork(uDeviceHandle_t devHandle,
                             uNetworkType_t networkType,
                             void *pNetworkCfg)
{
    int32_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
#if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_cellular) || \
    DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_gnss)     || \
    DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_short_range)
    uDeviceInstance_t *pDeviceInstance = NULL;
    int32_t deviceIndex;
    int32_t networkIndex;

    if ((devHandle != NULL) && (pNetworkCfg != NULL) &&
        (uDeviceGetInstance(devHandle, &pDeviceInstance) == 0)) {
        errorCode = U_ERROR_COMMON_SUCCESS;
        deviceIndex = getDeviceIndex(pDeviceInstance);
        if (deviceIndex >= 0) {
            // Determine which of the two "network" array
            // entries is the correct one for this network type
            networkIndex = getNetworkIndex(deviceIndex,
                                           pDeviceInstance->deviceType,
                                           networkType);
            if (networkIndex >= 0) {
                // We now have the device index and we know which of the
                // up-to-two network arrays is the one we're after:
                // populate the network configuration based on this
                switch (networkType) {
                    case U_NETWORK_TYPE_BLE:
#if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_ble)
                        cfgNetworkBle(deviceIndex, networkIndex,
                                      (uNetworkCfgBle_t *) pNetworkCfg);
#endif
                        break;
                    case U_NETWORK_TYPE_CELL:
#if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_device_cellular)
                        // Note: this is under u_blox_ubxlib_device_cellular and not
                        // u_blox_ubxlib_network_cellular as we need the default
                        // settings to be populated in all cases.
                        cfgNetworkCellular(deviceIndex, networkIndex,
                                           (uNetworkCfgCell_t *) pNetworkCfg);
#endif
                        break;
#if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_gnss)
                    case U_NETWORK_TYPE_GNSS:
                        cfgNetworkGnss(deviceIndex, networkIndex,
                                       (uNetworkCfgGnss_t *) pNetworkCfg);
#endif
                        break;
#if DT_HAS_COMPAT_STATUS_OKAY(u_blox_ubxlib_network_wifi)
                    case U_NETWORK_TYPE_WIFI:
                        cfgNetworkWifi(deviceIndex, networkIndex,
                                       (uNetworkCfgWifi_t *) pNetworkCfg);
#endif
                        break;
                    default:
                        break;
                }
            }
        }
    }
#else
    errorCode = U_ERROR_COMMON_SUCCESS;
    (void) devHandle;
    (void) networkType;
    (void) pNetworkCfg;
#endif

    return errorCode;
}

/* ----------------------------------------------------------------
 * MORE COMPILE-TIME MACROS
 * These are conventionally placed at the end of a Zephyr driver file.
 * -------------------------------------------------------------- */

/** A DEVICE_DT_INST_DEFINE() macro needs to exist for each of the
 * "compatible" types above, which is done with the boiler-plate
 * below.  The name we give to the ubxlib device in the binding file
 * over in the "dts" directory, following Zephyr convention, is
 * "u-blox,ubxlib-xxx-yyy", but the device tree macros need any
 * punctuation to be replaced with an underscore, hence the name
 * becomes "u_blox_ubxlib_xxx_yyy".
 */
#define DT_DRV_COMPAT u_blox_ubxlib_device_cellular

#define U_PORT_BOARD_CFG_DEVICE_DEFINE(i)                                           \
DEVICE_DT_INST_DEFINE(i,                                                            \
                      init, /* Initialisation callback function; not used but must exist */ \
                      NULL, /* pm_device */                                         \
                      NULL, /* Context: this would be pDev->data if we needed it */ \
                      NULL, /* Constant configuration data */                       \
                      POST_KERNEL, /* Device initialisation level */                \
                      99,   /* Initialisation priority (99 is the lowest, used since there is nothing to initialise) */ \
                      NULL); /* API jump-table */

DT_INST_FOREACH_STATUS_OKAY(U_PORT_BOARD_CFG_DEVICE_DEFINE)

#undef U_PORT_BOARD_CFG_DEVICE_DEFINE
#undef DT_DRV_COMPAT

#define DT_DRV_COMPAT u_blox_ubxlib_device_gnss

#define U_PORT_BOARD_CFG_DEVICE_DEFINE(i)                                           \
DEVICE_DT_INST_DEFINE(i,                                                            \
                      init, /* Initialisation callback function; not used but must exist */ \
                      NULL, /* pm_device */                                         \
                      NULL, /* Context: this would be pDev->data if we needed it */ \
                      NULL, /* Constant configuration data */                       \
                      POST_KERNEL, /* Device initialisation level */                \
                      99,   /* Initialisation priority (99 is the lowest, used since there is nothing to initialise) */ \
                      NULL); /* API jump-table */

DT_INST_FOREACH_STATUS_OKAY(U_PORT_BOARD_CFG_DEVICE_DEFINE)

#undef U_PORT_BOARD_CFG_DEVICE_DEFINE
#undef DT_DRV_COMPAT

#define DT_DRV_COMPAT u_blox_ubxlib_device_short_range

#define U_PORT_BOARD_CFG_DEVICE_DEFINE(i)                                           \
DEVICE_DT_INST_DEFINE(i,                                                            \
                      init, /* Initialisation callback function; not used but must exist */ \
                      NULL, /* pm_device */                                         \
                      NULL, /* Context: this would be pDev->data if we needed it */ \
                      NULL, /* Constant configuration data */                       \
                      POST_KERNEL, /* Device initialisation level */                \
                      99,   /* Initialisation priority (99 is the lowest, used since there is nothing to initialise) */ \
                      NULL); /* API jump-table */

DT_INST_FOREACH_STATUS_OKAY(U_PORT_BOARD_CFG_DEVICE_DEFINE)

// End of file
