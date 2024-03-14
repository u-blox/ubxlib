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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Implementation of functions that are private to cellular.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdio.h"     // snprintf()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()
#include "ctype.h"     // isdigit()

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_uart.h"
#include "u_port_gpio.h"
#include "u_port_event_queue.h"
#include "u_port_debug.h"

#include "u_at_client.h"

#include "u_sock.h"

#include "u_security.h"

#include "u_cell_module_type.h"
#include "u_cell_file.h"    // U_CELL_FILE_NAME_MAX_LENGTH
#include "u_cell.h"         // Order is
#include "u_cell_net.h"     // important here
#include "u_cell_private.h" // don't change it
#include "u_cell_pwr.h"
#include "u_cell_cfg.h"
#include "u_cell_http.h"
#include "u_cell_time.h"
#include "u_cell_http_private.h"
#include "u_cell_pwr_private.h"
#include "u_cell_time_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_CELL_PRIVATE_DTR_PIN_HYSTERESIS_INTERVAL_MS
/** When performing hysteresis of the DTR pin, the interval to use for each
 * wait step; value in milliseconds.
 */
# define U_CELL_PRIVATE_DTR_PIN_HYSTERESIS_INTERVAL_MS U_AT_CLIENT_ACTIVITY_PIN_HYSTERESIS_INTERVAL_MS
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES THAT ARE SHARED THROUGHOUT THE CELLULAR IMPLEMENTATION
 * -------------------------------------------------------------- */

/** Root for the linked list of instances.
 */
uCellPrivateInstance_t *gpUCellPrivateInstanceList = NULL;

/** Mutex to protect the linked list.
 */
uPortMutexHandle_t gUCellPrivateMutex = NULL;

/** The characteristics of the modules supported by this driver,
 * compiled into the driver.
 */
const uCellPrivateModule_t gUCellPrivateModuleList[] = {
    {
        U_CELL_MODULE_TYPE_SARA_U201, 1 /* Pwr On pull ms */, 1500 /* Pwr off pull ms */,
        5 /* Boot wait */, 5 /* Min awake */, 5 /* Pwr down wait */, 5 /* Reboot wait */, 10 /* AT timeout */,
        50 /* Cmd wait ms */, 2000 /* Resp max wait ms */, 0 /* radioOffCfun */, 75 /* resetHoldMilliseconds */,
        2 /* Simultaneous RATs */,
        ((1ULL << (int32_t) U_CELL_NET_RAT_GSM_GPRS_EGPRS) |
         (1ULL << (int32_t) U_CELL_NET_RAT_UTRAN)) /* RATs */,
        ((1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_USE_UPSD_CONTEXT_ACTIVATION) |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_CONTEXT_MAPPING_REQUIRED)    |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_AUTO_BAUDING)                |
         // In theory SARA-U201 does support DTR power saving however we do not
         // have this in our regression test farm and hence it is not marked
         // as supported for now
         // (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_DTR_POWER_SAVING)
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_AT_PROFILES)                   |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_CTS_CONTROL)                   |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_SOCK_SET_LOCAL_PORT)           |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_UART_POWER_SAVING)             |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_CMUX)                          |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_CMUX_CHANNEL_CLOSE)            |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_UCGED)                         |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_AUTHENTICATION_MODE_AUTOMATIC) |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_HTTP)                          |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_PPP) /* features */
        ),
        6, /* Default CMUX channel for GNSS */
        15, /* AT+CFUN reboot command */
        -1  /* PPP PDP context ID; we don't support anything other than -1 for
             * the solely UPSD-type context activation used on SARA-U201 */
    },
    {
        U_CELL_MODULE_TYPE_SARA_R410M_02B, 300 /* Pwr On pull ms */, 2000 /* Pwr off pull ms */,
        6 /* Boot wait */, 30 /* Min awake */, 35 /* Pwr down wait */, 5 /* Reboot wait */, 10 /* AT timeout */,
        100 /* Cmd wait ms */, 3000 /* Resp max wait ms */, 4 /* radioOffCfun */, 16500 /* resetHoldMilliseconds */,
        2 /* Simultaneous RATs */,
        ((1ULL << (int32_t) U_CELL_NET_RAT_CATM1)          |
         (1ULL << (int32_t) U_CELL_NET_RAT_NB1)) /* RATs */,
        ((1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MNO_PROFILE)        |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_ASYNC_SOCK_CLOSE)   |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT)               |
         // In theory SARA-R410M does support keep alive but I have been
         // unable to make it work (always returns error) and hence this is
         // not marked as supported for now
         // (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT_KEEP_ALIVE)         |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX) |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_UCGED5)                  |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_FILE_SYSTEM_TAG)         |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_3GPP_POWER_SAVING)       |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_EDRX)                    |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_FOTA)                    |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_UART_POWER_SAVING)       |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_CMUX)                    |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_CMUX_CHANNEL_CLOSE)      |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_LWM2M)                   |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_UCGED)                   |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_HTTP) /* features */
         // PPP is supported by the module but we do not test its integration with
         // ubxlib and hence it is not marked as supported
        ),
        3, /* Default CMUX channel for GNSS */
        15, /* AT+CFUN reboot command */
        -1  /* PPP PDP context ID */
    },
    {
        U_CELL_MODULE_TYPE_SARA_R412M_02B, 300 /* Pwr On pull ms */, 2000 /* Pwr off pull ms */,
        5 /* Boot wait */, 30 /* Min awake */, 35 /* Pwr down wait */, 10 /* Reboot wait */, 10 /* AT timeout */,
        100 /* Cmd wait ms */, 3000 /* Resp max wait ms */, 4 /* radioOffCfun */, 16500 /* resetHoldMilliseconds */,
        3 /* Simultaneous RATs */,
        ((1ULL << (int32_t) U_CELL_NET_RAT_GSM_GPRS_EGPRS) |
         (1ULL << (int32_t) U_CELL_NET_RAT_CATM1)          |
         (1ULL << (int32_t) U_CELL_NET_RAT_NB1)) /* RATs */,
        ((1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MNO_PROFILE)                            |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_CSCON)                                  |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_ASYNC_SOCK_CLOSE)                       |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_SECURITY_TLS_SERVER_NAME_INDICATION)    |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT)                                |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)             |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT_SET_LOCAL_PORT)                 |
         // In theory SARA-R412M does support keep alive but I have been
         // unable to make it work (always returns error) and hence this is
         // not marked as supported for now
         // (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT_KEEP_ALIVE)                     |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT_SESSION_RETAIN)                 |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_UCGED5)                              |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_FILE_SYSTEM_TAG)                     |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_3GPP_POWER_SAVING)                   |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_EDRX)                                |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_FOTA)                                |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_UART_POWER_SAVING)                   |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_CMUX)                                |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_CMUX_CHANNEL_CLOSE)                  |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_LWM2M)                               |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_UCGED)                               |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_HTTP) /* features */
         // PPP is supported by the module but we do not test its integration with
         // ubxlib and hence it is not marked as supported
        ),
        3, /* Default CMUX channel for GNSS */
        15, /* AT+CFUN reboot command */
        -1  /* PPP PDP context ID */
    },
    {
        U_CELL_MODULE_TYPE_SARA_R412M_03B, 300 /* Pwr On pull ms */, 2000 /* Pwr off pull ms */,
        6 /* Boot wait */, 30 /* Min awake */, 35 /* Pwr down wait */, 5 /* Reboot wait */, 10 /* AT timeout */,
        100 /* Cmd wait ms */, 2000 /* Resp max wait ms */, 4 /* radioOffCfun */, 16500 /* resetHoldMilliseconds */,
        3 /* Simultaneous RATs */,
        ((1ULL << (int32_t) U_CELL_NET_RAT_GSM_GPRS_EGPRS) |
         (1ULL << (int32_t) U_CELL_NET_RAT_CATM1)          |
         (1ULL << (int32_t) U_CELL_NET_RAT_NB1)) /* RATs */,
        ((1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MNO_PROFILE)                         |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_CSCON)                               |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_SECURITY_TLS_SERVER_NAME_INDICATION) |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_UCGED5)                              |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_FILE_SYSTEM_TAG)                     |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_DEEP_SLEEP_URC)                      |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_3GPP_POWER_SAVING)                   |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_EDRX)                                |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_UART_POWER_SAVING)                   |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_CMUX)                                |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_CMUX_CHANNEL_CLOSE)                  |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_UCGED)                               |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_HTTP) /* features */
         // PPP is supported by the module but we do not test its integration with
         // ubxlib and hence it is not marked as supported
        ),
        3, /* Default CMUX channel for GNSS */
        15, /* AT+CFUN reboot command */
        -1  /* PPP PDP context ID */
    },
    {
        U_CELL_MODULE_TYPE_SARA_R5, 1500 /* Pwr On pull ms */, 2000 /* Pwr off pull ms */,
        6 /* Boot wait */, 10 /* Min awake */, 20 /* Pwr down wait */, 15 /* Reboot wait */, 10 /* AT timeout */,
        20 /* Cmd wait ms */, 3000 /* Resp max wait ms */, 4 /* radioOffCfun */, 150 /* resetHoldMilliseconds */,
        1 /* Simultaneous RATs */,
#ifdef U_CELL_CFG_SARA_R5_00B
        (1ULL << (int32_t) U_CELL_NET_RAT_CATM1) /* RATs */,
#else
        ((1ULL << (int32_t) U_CELL_NET_RAT_CATM1) |
         (1ULL << (int32_t) U_CELL_NET_RAT_NB1)) /* RATs */,
#endif
        ((1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MNO_PROFILE)                         |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_CSCON)                               |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_ROOT_OF_TRUST)                       |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_DATA_COUNTERS)                       |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_SECURITY_TLS_IANA_NUMBERING)         |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_SECURITY_TLS_CIPHER_LIST)            |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_SECURITY_TLS_SERVER_NAME_INDICATION) |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT)                                |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT_BINARY_PUBLISH)                 |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT_WILL)                           |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT_KEEP_ALIVE)                     |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT_SECURITY)                       |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_CONTEXT_MAPPING_REQUIRED)            |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_AUTO_BAUDING)                        |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_AT_PROFILES)                         |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_SECURITY_ZTP)                        |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_FILE_SYSTEM_TAG)                     |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_DTR_POWER_SAVING)                    |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_DEEP_SLEEP_URC)                      |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_3GPP_POWER_SAVING)                   |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_3GPP_POWER_SAVING_PAGING_WINDOW_SET) |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_EDRX)                                |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTTSN)                              |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTTSN_SECURITY)                     |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_CTS_CONTROL)                         |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_SOCK_SET_LOCAL_PORT)                 |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_FOTA)                                |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_UART_POWER_SAVING)                   |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_CMUX)                                |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_CMUX_CHANNEL_CLOSE)                  |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_SNR_REPORTED)                        |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_AUTHENTICATION_MODE_AUTOMATIC)       |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_LWM2M)                               |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_UCGED)                               |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_HTTP)                                |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_PPP) /* features */
        ),
        4, /* Default CMUX channel for GNSS */
        16, /* AT+CFUN reboot command */
        -1  /* PPP PDP context ID */
    },
    {
        U_CELL_MODULE_TYPE_SARA_R410M_03B, 300 /* Pwr On pull ms */, 2000 /* Pwr off pull ms */,
        6 /* Boot wait */, 30 /* Min awake */, 35 /* Pwr down wait */, 5 /* Reboot wait */, 10 /* AT timeout */,
        100 /* Cmd wait ms */, 2000 /* Resp max wait ms */, 4 /* radioOffCfun */,  16500 /* resetHoldMilliseconds */,
        2 /* Simultaneous RATs */,
        ((1ULL << (int32_t) U_CELL_NET_RAT_CATM1)          |
         (1ULL << (int32_t) U_CELL_NET_RAT_NB1)) /* RATs */,
        ((1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MNO_PROFILE)                         |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_CSCON)                               |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_SECURITY_TLS_SERVER_NAME_INDICATION) |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT)                                |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT_KEEP_ALIVE)                     |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT_SECURITY)                       |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_UCGED5)                              |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_FILE_SYSTEM_TAG)                     |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_DEEP_SLEEP_URC)                      |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_3GPP_POWER_SAVING)                   |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_EDRX)                                |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_UART_POWER_SAVING)                   |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_CMUX)                                |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_CMUX_CHANNEL_CLOSE)                  |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_LWM2M)                               |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_UCGED)                               |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_HTTP) /* features */
         // PPP is supported by the module but we do not test its integration with
         // ubxlib and hence it is not marked as supported
        ),
        3, /* Default CMUX channel for GNSS */
        15, /* AT+CFUN reboot command */
        -1  /* PPP PDP context ID */
    },
    {
        U_CELL_MODULE_TYPE_SARA_R422, 300 /* Pwr On pull ms */, 2000 /* Pwr off pull ms */,
        5 /* Boot wait */, 30 /* Min awake */, 35 /* Pwr down wait */, 10 /* Reboot wait */, 10 /* AT timeout */,
        // Note: "Cmd wait ms" is set to 100 for the other SARA-R4 series modules;
        // testing has shown that 20 works for SARA-R422.
        20 /* Cmd wait ms */, 3000 /* Resp max wait ms */, 4 /* radioOffCfun */,  16500 /* resetHoldMilliseconds */,
        3 /* Simultaneous RATs */,
        ((1ULL << (int32_t) U_CELL_NET_RAT_GSM_GPRS_EGPRS) |
         (1ULL << (int32_t) U_CELL_NET_RAT_CATM1)          |
         (1ULL << (int32_t) U_CELL_NET_RAT_NB1)) /* RATs */,
        ((1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MNO_PROFILE)                         |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_CSCON)                               |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_ROOT_OF_TRUST)                       |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_ASYNC_SOCK_CLOSE)                    |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_SECURITY_TLS_IANA_NUMBERING)         |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_SECURITY_TLS_SERVER_NAME_INDICATION) |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT)                                |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT_BINARY_PUBLISH)                 |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT_WILL)                           |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT_KEEP_ALIVE)                     |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT_SECURITY)                       |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_CONTEXT_MAPPING_REQUIRED)            |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_FILE_SYSTEM_TAG)                     |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_DEEP_SLEEP_URC)                      |
         // SARA-R422 _does_ support 3GPP power saving, however the tests fail at the
         // moment because a second attempt to enter 3GPP power saving, after waking-up
         // from sleep to do something, fails, hence the support is disabled until
         // we determine why that is
         //(1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_3GPP_POWER_SAVING)                   |
         //(1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_3GPP_POWER_SAVING_PAGING_WINDOW_SET) |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_EDRX)                                  |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTTSN)                                |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTTSN_SECURITY)                       |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_FOTA)                                  |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_UART_POWER_SAVING)                     |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_CMUX)                                  |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_CMUX_CHANNEL_CLOSE)                    |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_SNR_REPORTED)                          |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_LWM2M)                                 |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_UCGED)                                 |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_HTTP)                                  |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_PPP) /* features */
        ),
        3, /* Default CMUX channel for GNSS */
        15, /* AT+CFUN reboot command */
        -1  /* PPP PDP context ID */
    },
    {
        U_CELL_MODULE_TYPE_LARA_R6, 300 /* Pwr On pull ms */, 2000 /* Pwr off pull ms */,
        10 /* Boot wait */, 30 /* Min awake */, 35 /* Pwr down wait */, 10 /* Reboot wait */, 10 /* AT timeout */,
        20 /* Cmd wait ms */, 3000 /* Resp max wait ms */, 4 /* radioOffCfun */,  150 /* resetHoldMilliseconds */,
        3 /* Simultaneous RATs */,
        ((1ULL << (int32_t) U_CELL_NET_RAT_GSM_GPRS_EGPRS) |
         (1ULL << (int32_t) U_CELL_NET_RAT_LTE)            |
         (1ULL << (int32_t) U_CELL_NET_RAT_UTRAN)) /* RATs */,
        ((1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MNO_PROFILE)                         |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_CSCON)                               |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_ROOT_OF_TRUST)                       |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_SECURITY_TLS_IANA_NUMBERING)         |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_SECURITY_TLS_SERVER_NAME_INDICATION) |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT)                                |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT_BINARY_PUBLISH)                 |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT_WILL)                           |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT_KEEP_ALIVE)                     |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT_SECURITY)                       |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_FILE_SYSTEM_TAG)                     |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_DTR_POWER_SAVING)                    |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTTSN)                              |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTTSN_SECURITY)                     |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_SOCK_SET_LOCAL_PORT)                 |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_FOTA)                                |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_CMUX)                                |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_CMUX_CHANNEL_CLOSE)                  |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_SNR_REPORTED)                        |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_LWM2M)                               |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_UCGED)                               |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_HTTP) /* features */
         // PPP is supported by the module but we do not test its integration with
         // ubxlib and hence it is not marked as supported
        ),
        3, /* Default CMUX channel for GNSS */
        15, /* AT+CFUN reboot command */
        -1  /* PPP PDP context ID */
    },
    {
        U_CELL_MODULE_TYPE_LENA_R8, 2000 /* Pwr On pull ms */, 3100 /* Pwr off pull ms */,
        5 /* Boot wait */, 30 /* Min awake */, 35 /* Pwr down wait */, 5 /* Reboot wait */, 10 /* AT timeout */,
        20 /* Cmd wait ms */, 3000 /* Resp max wait ms */, 4 /* radioOffCfun */,  50 /* resetHoldMilliseconds */,
        2 /* Simultaneous RATs */,
        ((1ULL << (int32_t) U_CELL_NET_RAT_GSM_GPRS_EGPRS) |
         (1ULL << (int32_t) U_CELL_NET_RAT_LTE)) /* RATs */,
        ((1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_CSCON)                               |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_ASYNC_SOCK_CLOSE)                    |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_SECURITY_TLS_IANA_NUMBERING)         |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_SECURITY_TLS_SERVER_NAME_INDICATION) |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT)                                |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT_BINARY_PUBLISH)                 |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT_WILL)                           |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT_KEEP_ALIVE)                     |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTT_SECURITY)                       |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_FILE_SYSTEM_TAG)                     |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_DTR_POWER_SAVING)                    |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MQTTSN)                              |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_SOCK_SET_LOCAL_PORT)                 |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_UART_POWER_SAVING)                   |
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_CMUX)                                |
         // LENA-R8 never seems to respond to individual CMUX channel close requests
         // LENA-R8 does, in theory, support HTTP, however the implementation is such that,
         // should the HTTP server respond with a non-2xx error code, the module does not
         // save the response to file (in fact it leaves any previous response file there,
         // unchanged) and returns error rather than success.  This makes it impossible to
         // support proper HTTP operation.
         (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_PPP) /* features */
        ),
        -1, /* Default CMUX channel for GNSS */
        16, /* AT+CFUN reboot command */
        U_CELL_PRIVATE_PPP_CONTEXT_ID_LENA_R8 /* PPP PDP context ID */
    },
    // Add new module types here, before the U_CELL_MODULE_TYPE_ANY entry (since
    // the uCellModuleType_t value is used as an index into this array).
    {
        // The module attributes set here are such that they help in identifying
        // the actual module type.
        U_CELL_MODULE_TYPE_ANY, -1, /* Pwr On pull ms, negative is set to enable the retry behaviour*/
        3100 /* Pwr off pull ms */,
        5 /* Boot wait */, 30 /* Min awake */, 35 /* Pwr down wait */, 5 /* Reboot wait */, 10 /* AT timeout */,
        100 /* Cmd wait ms */, 3000 /* Resp max wait ms */, 4 /* radioOffCfun */, 16500 /* resetHoldMilliseconds */,
        1 /* Simultaneous RATs */,
        (1ULL << (int32_t) U_CELL_NET_RAT_CATM1) /* RATs */,
        (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_MNO_PROFILE) |
        (1ULL << (int32_t) U_CELL_PRIVATE_FEATURE_DTR_POWER_SAVING) /* features */,
        3, /* Default CMUX channel for GNSS */
        15, /* AT+CFUN reboot command */
        -1  /* PPP PDP context ID */
    }
};

/** Number of items in the gUCellPrivateModuleList array, has to be
 * done in this file and externed or GCC complains about asking
 * for the size of a partially defined type.
 */
const size_t gUCellPrivateModuleListSize = sizeof(gUCellPrivateModuleList) /
                                           sizeof(gUCellPrivateModuleList[0]);

/** Table to convert the RAT values used in the module to
 * uCellNetRat_t, U201 version.  As well as being used when reading
 * the RAT configuration this is also used when the module has read
 * the active RAT (AT+COPS) and hence has more nuance than the
 * table going in the other direction: for instance the module
 * could determine that it has EDGE coverage but EDGE is not
 * a RAT that can be configured by itself.
 */
static const uCellNetRat_t gModuleRatToCellRatU201[] = {
    U_CELL_NET_RAT_GSM_GPRS_EGPRS,       // 0: 2G
    U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED,  // 1: GSM compact
    U_CELL_NET_RAT_UTRAN,                // 2: UTRAN
    U_CELL_NET_RAT_EGPRS,                // 3: EDGE
    U_CELL_NET_RAT_HSDPA,                // 4: UTRAN with HSDPA
    U_CELL_NET_RAT_HSUPA,                // 5: UTRAN with HSUPA
    U_CELL_NET_RAT_HSDPA_HSUPA,          // 6: UTRAN with HSDPA and HSUPA
    U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED,  // 7: LTE cat-M1
    U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED,  // 8: LTE NB1
    U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED   // 9: 2G again
};

/** Table to convert the RAT values used in the
 * module to uCellNetRat_t, R4/R5 version.
 */
static const uCellNetRat_t gModuleRatToCellRatR4R5[] = {
    U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED, // 0: 2G
    U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED, // 1: GSM compact
    U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED, // 2: UTRAN
    U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED, // 3: EDGE
    U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED, // 4: UTRAN with HSDPA
    U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED, // 5: UTRAN with HSUPA
    U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED, // 6: UTRAN with HSDPA and HSUPA
    U_CELL_NET_RAT_CATM1,               // 7: LTE cat-M1
    U_CELL_NET_RAT_NB1,                 // 8: LTE NB1
    U_CELL_NET_RAT_GSM_GPRS_EGPRS       // 9: 2G again
};

/** Table to convert the RAT values used in the
 * module to uCellNetRat_t, R6 version.
 */
static const uCellNetRat_t gModuleRatToCellRatR6[] = {
    U_CELL_NET_RAT_GSM_GPRS_EGPRS,      // 0: 2G
    U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED, // 1: GSM compact
    U_CELL_NET_RAT_UTRAN,               // 2: UTRAN
    U_CELL_NET_RAT_LTE,                 // 3: LTE
    U_CELL_NET_RAT_HSDPA,               // 4: UTRAN with HSDPA
    U_CELL_NET_RAT_HSUPA,               // 5: UTRAN with HSUPA
    U_CELL_NET_RAT_HSDPA_HSUPA,         // 6: UTRAN with HSDPA and HSUPA
    U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED, // 7: LTE cat-M1
    U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED, // 8: LTE NB1
    U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED  // 9: 2G again
};

/** Table to convert the RAT values used in the
 * module to uCellNetRat_t, R8 version.
 */
static const uCellNetRat_t gModuleRatToCellRatR8[] = {
    U_CELL_NET_RAT_GSM_GPRS_EGPRS,      // 0: 2G
    U_CELL_NET_RAT_GSM_UMTS,            // 1: GSM/UMTS dual mode
    U_CELL_NET_RAT_UTRAN,               // 2: UMTS single mode
    U_CELL_NET_RAT_LTE,                 // 3: LTE single mode
    U_CELL_NET_RAT_GSM_UMTS_LTE,        // 4: GSM/UMTS/LTE tri-mode
    U_CELL_NET_RAT_GSM_LTE,             // 5: GSM/LTE dual mode
    U_CELL_NET_RAT_UMTS_LTE             // 6: UMTS/LTE dual mode
};

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Context activation result, UPSDA style, where the parameter is
// a pointer to an int32_t which will be set to the result code from
// the UPSDA action, should be 0 for success.
static void UUPSDA_urc(uAtClientHandle_t atHandle, void *pParameter)
{
    int32_t result;

    result = uAtClientReadInt(atHandle);

    if (result == 0) {
        // Tidy up by reading and throwing away the IP address
        uAtClientReadString(atHandle, NULL,
                            U_CELL_NET_IP_ADDRESS_SIZE, false);
    }

    *((int32_t *) pParameter) = result;
}

// Do a wake-up from deep sleep.
static int32_t deepSleepWakeUp(uCellPrivateInstance_t *pInstance)
{
    int32_t errorCode;
    uCellNetRat_t rat;

    errorCode = uCellPwrPrivateOn(pInstance, NULL, false);
    if (errorCode == 0) {
        rat = uCellPrivateGetActiveRat(pInstance);
        if (U_CELL_PRIVATE_RAT_IS_EUTRAN(rat)) {
            // If we're on an EUTRAN RAT, so we must have been context-activated,
            // the PDP context will still be there but the internal "profile" used
            // by the on-board IP stack and MQTT stack etc. of the module, needs
            // to be re-attached to the PDP context on return from power saving
            uCellPrivateActivateProfile(pInstance, U_CELL_NET_CONTEXT_ID,
                                        U_CELL_NET_PROFILE_ID, 1, NULL);
        }
    }

    return errorCode;
}

// Add an entry to the end of the linked list
// of files and count how many are in it once added.
static size_t filelListAddCount(uCellPrivateFileListContainer_t **ppFileContainer,
                                uCellPrivateFileListContainer_t *pAdd)
{
    size_t count = 0;
    uCellPrivateFileListContainer_t **ppTmp = ppFileContainer;

    while (*ppTmp != NULL) {
        ppTmp = &((*ppTmp)->pNext);
        count++;
    }

    if (pAdd != NULL) {
        *ppTmp = pAdd;
        pAdd->pNext = NULL;
        count++;
    }

    return count;
}

// Get an entry from the start of the linked list of files
// and remove it from the list, returning the number left
static int32_t fileListGetRemove(uCellPrivateFileListContainer_t **ppFileContainer,
                                 char *pFile)
{
    int32_t errorOrCount = (int32_t) U_ERROR_COMMON_NOT_FOUND;
    uCellPrivateFileListContainer_t *pTmp = *ppFileContainer;
    size_t fileNameLength;

    if (pTmp != NULL) {
        if (pFile != NULL) {
            fileNameLength = pTmp->fileNameLength;
            if (fileNameLength > U_CELL_FILE_NAME_MAX_LENGTH) {
                fileNameLength = U_CELL_FILE_NAME_MAX_LENGTH;
            }
            memcpy(pFile, pTmp->pFileName, fileNameLength);
            // Add a terminator
            *(pFile + fileNameLength) = 0;
        }
        pTmp = (*ppFileContainer)->pNext;
        uPortFree(*ppFileContainer);
        *ppFileContainer = pTmp;
        errorOrCount = 0;
        while (pTmp != NULL) {
            pTmp = pTmp->pNext;
            errorOrCount++;
        }
    }

    return errorOrCount;
}

// Clear the file list
static void fileListClear(uCellPrivateFileListContainer_t **ppFileContainer)
{
    uCellPrivateFileListContainer_t *pTmp;

    while (*ppFileContainer != NULL) {
        pTmp = (*ppFileContainer)->pNext;
        uPortFree(*ppFileContainer);
        *ppFileContainer = pTmp;
    }
}

// [Re]attach a PDP context to an internal module profile with an
// option on whether the AT client is locked/released or not.
int32_t privateActivateProfile(const uCellPrivateInstance_t *pInstance,
                               int32_t contextId, int32_t profileId, size_t tries,
                               bool (*pKeepGoing) (const uCellPrivateInstance_t *),
                               bool noAtLock)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    int32_t uupsdaUrcResult = -1;

    if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                           U_CELL_PRIVATE_FEATURE_CONTEXT_MAPPING_REQUIRED)) {
        errorCode = (int32_t) U_CELL_ERROR_CONTEXT_ACTIVATION_FAILURE;
        for (size_t x = tries; (x > 0) && (errorCode != 0) &&
             ((pKeepGoing == NULL) || pKeepGoing(pInstance)); x--) {
            // Need to map the context to an internal modem profile
            // e.g. AT+UPSD=0,100,1
            if (noAtLock) {
                uAtClientLockExtend(atHandle);
            } else {
                uAtClientLock(atHandle);
            }
            // The IP type used here must be the same as
            // that used by AT+CGDCONT, hence set it to IP
            // to be sure as some versions of SARA-R5 software
            // have the default as IPV4V6.
            uAtClientCommandStart(atHandle, "AT+UPSD=");
            uAtClientWriteInt(atHandle, profileId);
            uAtClientWriteInt(atHandle, 0);
            uAtClientWriteInt(atHandle, 0);
            uAtClientCommandStopReadResponse(atHandle);
            uAtClientCommandStart(atHandle, "AT+UPSD=");
            uAtClientWriteInt(atHandle, profileId);
            uAtClientWriteInt(atHandle, 100);
            uAtClientWriteInt(atHandle, contextId);
            uAtClientCommandStopReadResponse(atHandle);
            if (noAtLock) {
                errorCode = uAtClientErrorGet(atHandle);
            } else {
                errorCode = uAtClientUnlock(atHandle);
            }
            if ((errorCode == 0) &&
                (pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R5)) {
                errorCode = (int32_t) U_CELL_ERROR_CONTEXT_ACTIVATION_FAILURE;
                // SARA-R5 pattern: the context also has to be
                // activated and we're not actually done
                // until the +UUPSDA URC comes back,
                if (noAtLock) {
                    uAtClientLockExtend(atHandle);
                } else {
                    uAtClientLock(atHandle);
                }
                uAtClientCommandStart(atHandle, "AT+UPSDA=");
                uAtClientWriteInt(atHandle, profileId);
                uAtClientWriteInt(atHandle, 3);
                uAtClientCommandStopReadResponse(atHandle);
                // We wait for the URC "in-line" because this
                // function may be called when waking the
                // module up from sleep, at which point URCs
                // handled asynchronously would be held back
                // Should be pretty quick
                uAtClientTimeoutSet(atHandle, 3000);
                uAtClientUrcDirect(atHandle, "+UUPSDA:", UUPSDA_urc,
                                   &uupsdaUrcResult);
                if (uupsdaUrcResult == 0) {
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
                if (!noAtLock) {
                    uAtClientUnlock(atHandle);
                }
            }
        }
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS THAT ARE PRIVATE TO CELLULAR
 * -------------------------------------------------------------- */

// Return true if the given buffer contains only numeric characters
// (i.e. 0 to 9)
bool uCellPrivateIsNumeric(const char *pBuffer, size_t bufferSize)
{
    bool numeric = true;

    for (size_t x = 0; (x < bufferSize) && numeric; x++) {
        numeric = (isdigit((int32_t) * (pBuffer + x)) != 0);
    }

    return numeric;
}

// Find a cellular instance in the list by instance handle.
uCellPrivateInstance_t *pUCellPrivateGetInstance(uDeviceHandle_t cellHandle)
{
    uCellPrivateInstance_t *pInstance = gpUCellPrivateInstanceList;

    while ((pInstance != NULL) && (pInstance->cellHandle != cellHandle)) {
        pInstance = pInstance->pNext;
    }

    return pInstance;
}

// Convert RSRP in 3GPP TS 36.133 format to dBm.
int32_t uCellPrivateRsrpToDbm(int32_t rsrp)
{
    int32_t rsrpDbm = 0;

    if ((rsrp >= 0) && (rsrp <= 97)) {
        rsrpDbm = rsrp - (97 + 44);
        if (rsrpDbm < -141) {
            rsrpDbm = -141;
        }
    }

    return rsrpDbm;
}

// Convert RSRQ in 3GPP TS 36.133 format to dB.
int32_t uCellPrivateRsrqToDb(int32_t rsrq)
{
    int32_t rsrqDb = 0x7FFFFFFF;

    if ((rsrq >= -30) && (rsrq <= 46)) {
        rsrqDb = (rsrq - 39) / 2;
        if (rsrqDb < -34) {
            rsrqDb = -34;
        }
    }

    return rsrqDb;
}

// Set the radio parameters back to defaults.
void uCellPrivateClearRadioParameters(uCellPrivateRadioParameters_t *pParameters,
                                      bool leaveCellIdLogicalAlone)
{
    pParameters->rssiDbm = 0;
    pParameters->rsrpDbm = 0;
    pParameters->rsrqDb = 0x7FFFFFFF;
    pParameters->cellIdPhysical = -1;
    if (!leaveCellIdLogicalAlone) {
        pParameters->cellIdLogical = -1;
    }
    pParameters->earfcn = -1;
    pParameters->snrDb = 0x7FFFFFFF;
}

// Clear the dynamic parameters of an instance,
// so the network status, the active RAT and
// the radio parameters.
void uCellPrivateClearDynamicParameters(uCellPrivateInstance_t *pInstance)
{
    for (size_t x = 0;
         x < sizeof(pInstance->networkStatus) / sizeof(pInstance->networkStatus[0]);
         x++) {
        pInstance->networkStatus[x] = U_CELL_NET_STATUS_UNKNOWN;
    }
    for (size_t x = 0;
         x < sizeof(pInstance->rat) / sizeof(pInstance->rat[0]);
         x++) {
        pInstance->rat[x] = U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;
    }
    uCellPrivateClearRadioParameters(&(pInstance->radioParameters), false);
}

// Get the current CFUN mode.
int32_t uCellPrivateCFunGet(const uCellPrivateInstance_t *pInstance)
{
    int32_t errorCodeOrMode;
    int32_t x;
    uAtClientHandle_t atHandle = pInstance->atHandle;

    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+CFUN?");
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+CFUN:");
    x = uAtClientReadInt(atHandle);
    uAtClientResponseStop(atHandle);
    errorCodeOrMode = uAtClientUnlock(atHandle);
    if ((errorCodeOrMode == 0) && (x >= 0)) {
        errorCodeOrMode = x;
    }

    return errorCodeOrMode;
}

// Ensure that a module is powered up.
int32_t  uCellPrivateCFunOne(uCellPrivateInstance_t *pInstance)
{
    int32_t errorCodeOrMode;
    uAtClientHandle_t atHandle = pInstance->atHandle;

    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+CFUN?");
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+CFUN:");
    errorCodeOrMode = uAtClientReadInt(atHandle);
    uAtClientResponseStop(atHandle);
    uAtClientUnlock(atHandle);
    // Set powered-up mode if it wasn't already
    if (errorCodeOrMode != 1) {
        // Wait for flip time to expire
        while (uPortGetTickTimeMs() - pInstance->lastCfunFlipTimeMs <
               (U_CELL_PRIVATE_AT_CFUN_FLIP_DELAY_SECONDS * 1000)) {
            uPortTaskBlock(1000);
        }
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+CFUN=1");
        uAtClientCommandStopReadResponse(atHandle);
        if (uAtClientUnlock(atHandle) == 0) {
            pInstance->lastCfunFlipTimeMs = uPortGetTickTimeMs();
            // And don't do anything for a second,
            // as the module might not be quite ready yet
            uPortTaskBlock(1000);
        }
    }

    return errorCodeOrMode;
}

// Do the opposite of uCellPrivateCFunOne(), put the mode back.
void uCellPrivateCFunMode(uCellPrivateInstance_t *pInstance,
                          int32_t mode)
{
    uAtClientHandle_t atHandle = pInstance->atHandle;

    // Wait for flip time to expire
    while (uPortGetTickTimeMs() - pInstance->lastCfunFlipTimeMs <
           (U_CELL_PRIVATE_AT_CFUN_FLIP_DELAY_SECONDS * 1000)) {
        uPortTaskBlock(1000);
    }
    uAtClientLock(atHandle);
    if (mode != 1) {
        // If we're doing anything other than powering up,
        // i.e. AT+CFUN=0 or AT+CFUN=4, this can take
        // longer than your average response time
        uAtClientTimeoutSet(atHandle,
                            U_CELL_PRIVATE_AT_CFUN_OFF_RESPONSE_TIME_SECONDS * 1000);
    }
    uAtClientCommandStart(atHandle, "AT+CFUN=");
    uAtClientWriteInt(atHandle, mode);
    uAtClientCommandStopReadResponse(atHandle);
    if (uAtClientUnlock(atHandle) == 0) {
        pInstance->lastCfunFlipTimeMs = uPortGetTickTimeMs();
    }
}

// Get the IMSI of the SIM.
int32_t uCellPrivateGetImsi(const uCellPrivateInstance_t *pInstance,
                            char *pImsi)
{
    int32_t errorCode = (int32_t) U_CELL_ERROR_AT;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    int32_t bytesRead;

    // Try this ten times: unfortunately
    // the module can spit out a URC just when
    // we're expecting the IMSI and, since there
    // is no prefix on the response, we have
    // no way of telling the difference.  Hence
    // check the length and that length being
    // made up entirely of numerals
    for (size_t x = 10; (x > 0) && (errorCode != 0); x--) {
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+CIMI");
        uAtClientCommandStop(atHandle);
        uAtClientResponseStart(atHandle, NULL);
        bytesRead = uAtClientReadBytes(atHandle, pImsi,
                                       15, false);
        uAtClientResponseStop(atHandle);
        if ((uAtClientUnlock(atHandle) == 0) &&
            (bytesRead == 15) &&
            uCellPrivateIsNumeric(pImsi, 15)) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        } else {
            uPortTaskBlock(1000);
        }
    }

    return errorCode;
}

// Get the IMEI of the cellular module.
int32_t uCellPrivateGetImei(const uCellPrivateInstance_t *pInstance,
                            char *pImei)
{
    int32_t errorCode = (int32_t) U_CELL_ERROR_AT;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    int32_t bytesRead;

    // Try this ten times: unfortunately
    // the module can spit out a URC just when
    // we're expecting the IMEI and, since there
    // is no prefix on the response, we have
    // no way of telling the difference.  Hence
    // check the length and that length being
    // made up entirely of numerals
    for (size_t x = 10; (x > 0) && (errorCode != 0); x--) {
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+CGSN");
        uAtClientCommandStop(atHandle);
        uAtClientResponseStart(atHandle, NULL);
        bytesRead = uAtClientReadBytes(atHandle, pImei,
                                       15, false);
        uAtClientResponseStop(atHandle);
        if ((uAtClientUnlock(atHandle) == 0) &&
            (bytesRead == 15) &&
            uCellPrivateIsNumeric(pImei, 15)) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
    }

    return errorCode;
}

// Get whether the given instance is registered with the network.
// Needs to be in the packet switched domain, circuit switched is
// no use for this API.
bool uCellPrivateIsRegistered(const uCellPrivateInstance_t *pInstance)
{
    return U_CELL_NET_STATUS_MEANS_REGISTERED(
               pInstance->networkStatus[U_CELL_PRIVATE_NET_REG_TYPE_CGREG]) ||
           U_CELL_NET_STATUS_MEANS_REGISTERED(pInstance->networkStatus[U_CELL_PRIVATE_NET_REG_TYPE_CEREG]);
}

// Convert module RAT to our RAT.
uCellNetRat_t uCellPrivateModuleRatToCellRat(uCellModuleType_t moduleType,
                                             int32_t moduleRat)
{
    uCellNetRat_t cellRat = U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED;

    if (moduleRat >= 0) {
        switch (moduleType) {
            case U_CELL_MODULE_TYPE_SARA_U201:
                if (moduleRat < (int32_t) (sizeof(gModuleRatToCellRatU201) / sizeof(gModuleRatToCellRatU201[0]))) {
                    cellRat = gModuleRatToCellRatU201[moduleRat];
                }
                break;
            case U_CELL_MODULE_TYPE_LARA_R6:
                if (moduleRat < (int32_t) (sizeof(gModuleRatToCellRatR6) / sizeof(gModuleRatToCellRatR6[0]))) {
                    cellRat = gModuleRatToCellRatR6[moduleRat];
                }
                break;
            case U_CELL_MODULE_TYPE_LENA_R8:
                if (moduleRat < (int32_t) (sizeof(gModuleRatToCellRatR8) / sizeof(gModuleRatToCellRatR8[0]))) {
                    cellRat = gModuleRatToCellRatR8[moduleRat];
                }
                break;
            default:
                if (moduleRat < (int32_t) (sizeof(gModuleRatToCellRatR4R5) / sizeof(gModuleRatToCellRatR4R5[0]))) {
                    cellRat = gModuleRatToCellRatR4R5[moduleRat];
                }
                break;
        }
    }

    return cellRat;
}

// Get the active RAT.
uCellNetRat_t uCellPrivateGetActiveRat(const uCellPrivateInstance_t *pInstance)
{
    uCellNetRat_t rat = pInstance->rat[U_CELL_PRIVATE_NET_REG_TYPE_CREG];

    // Return the RAT for the registration type we have, prioritising the
    // packet-switched domain, LTE first
    if (U_CELL_NET_STATUS_MEANS_REGISTERED(
            pInstance->networkStatus[U_CELL_PRIVATE_NET_REG_TYPE_CEREG])) {
        rat = pInstance->rat[U_CELL_PRIVATE_NET_REG_TYPE_CEREG];
    } else if (U_CELL_NET_STATUS_MEANS_REGISTERED(
                   pInstance->networkStatus[U_CELL_PRIVATE_NET_REG_TYPE_CGREG])) {
        rat = pInstance->rat[U_CELL_PRIVATE_NET_REG_TYPE_CGREG];
    }

    return rat;
}

// Get the operator name.
int32_t uCellPrivateGetOperatorStr(const uCellPrivateInstance_t *pInstance,
                                   char *pStr, size_t size)
{
    int32_t errorCodeOrSize;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    int32_t bytesRead;

    uAtClientLock(atHandle);
    // First set long alphanumeric format
    uAtClientCommandStart(atHandle, "AT+COPS=3,0");
    uAtClientCommandStopReadResponse(atHandle);
    // Then read the operator name
    uAtClientCommandStart(atHandle, "AT+COPS?");
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+COPS:");
    // Skip past <mode> and <format>
    uAtClientSkipParameters(atHandle, 2);
    // Read the operator name
    bytesRead = uAtClientReadString(atHandle, pStr, size, false);
    uAtClientResponseStop(atHandle);
    errorCodeOrSize = uAtClientUnlock(atHandle);
    if ((errorCodeOrSize == 0) && (bytesRead >= 0)) {
        errorCodeOrSize = bytesRead;
    }

    return errorCodeOrSize;
}

// Free network scan results.
void uCellPrivateScanFree(uCellPrivateNet_t **ppScanResults)
{
    uCellPrivateNet_t *pTmp;

    while (*ppScanResults != NULL) {
        pTmp = (*ppScanResults)->pNext;
        uPortFree(*ppScanResults);
        *ppScanResults = pTmp;
    }

    *ppScanResults = NULL;
}

// Get the module characteristics for a given instance.
const uCellPrivateModule_t *pUCellPrivateGetModule(uDeviceHandle_t cellHandle)
{
    uCellPrivateInstance_t *pInstance = gpUCellPrivateInstanceList;
    const uCellPrivateModule_t *pModule = NULL;

    while ((pInstance != NULL) && (pInstance->cellHandle != cellHandle)) {
        pInstance = pInstance->pNext;
    }

    if (pInstance != NULL) {
        pModule = pInstance->pModule;
    }

    return pModule;
}

// Remove a location context.
void uCellPrivateLocRemoveContext(uCellPrivateInstance_t *pInstance)
{
    uCellPrivateLocContext_t *pContext;

    if (pInstance != NULL) {
        // Free all Wifi APs
        pContext = pInstance->pLocContext;
        if (pContext != NULL) {
            uAtClientRemoveUrcHandler(pInstance->atHandle, "+UULOC:");
            uAtClientRemoveUrcHandler(pInstance->atHandle, "+UULOCIND:");
            U_PORT_MUTEX_LOCK(pContext->fixDataStorageMutex);
            U_PORT_MUTEX_UNLOCK(pContext->fixDataStorageMutex);
            uPortMutexDelete(pContext->fixDataStorageMutex);
            pContext->fixDataStorageMutex = NULL;
        }
        // Free the context
        uPortFree(pContext);
        pInstance->pLocContext = NULL;
    }
}

// Remove the sleep context for the given instance.
void uCellPrivateSleepRemoveContext(uCellPrivateInstance_t *pInstance)
{
    if (pInstance != NULL) {
        // Free the context
        uPortFree(pInstance->pSleepContext);
        pInstance->pSleepContext = NULL;
    }
}

// [Re]attach a PDP context to an internal module profile.
int32_t uCellPrivateActivateProfile(const uCellPrivateInstance_t *pInstance,
                                    int32_t contextId, int32_t profileId, size_t tries,
                                    bool (*pKeepGoing) (const uCellPrivateInstance_t *))
{
    return privateActivateProfile(pInstance, contextId, profileId, tries,
                                  pKeepGoing, false);
}

// As uCellPrivateActivateProfile() but without locking/unlocking the AT client.
int32_t uCellPrivateActivateProfileNoAtLock(const uCellPrivateInstance_t *pInstance,
                                            int32_t contextId, int32_t profileId, size_t tries,
                                            bool (*pKeepGoing) (const uCellPrivateInstance_t *))
{
    return privateActivateProfile(pInstance, contextId, profileId, tries,
                                  pKeepGoing, true);
}

// Determine whether deep sleep is active, i.e. the module is asleep.
bool uCellPrivateIsDeepSleepActive(uCellPrivateInstance_t *pInstance)
{
    bool sleepActive = false;
    uCellPrivateSleep_t *pContext = pInstance->pSleepContext;

    if (((pContext != NULL) && pContext->powerSaving3gppAgreed &&
         (pInstance->pinVInt >= 0) &&
         (uPortGpioGet(pInstance->pinVInt) ==
          (int32_t) !U_CELL_PRIVATE_VINT_PIN_ON_STATE(pInstance->pinStates)))) {
        pInstance->deepSleepState = U_CELL_PRIVATE_DEEP_SLEEP_STATE_ASLEEP;
        // If we've configured sleep and VInt has gone to its off state,
        // then we are asleep.
        sleepActive = true;
    }

    return sleepActive;
}

// Callback to wake up the cellular module from power saving.
// IMPORTANT: nothing called from here should rely on callbacks
// sent via the uAtClientCallback() mechanism or URCS; these will
// be held back during the time that the module is being woken from
// deep sleep, which would lead to a lock-up.
int32_t uCellPrivateWakeUpCallback(uAtClientHandle_t atHandle, void *pInstance)
{
    int32_t errorCode = (int32_t) U_CELL_ERROR_AT;
    uCellPrivateInstance_t *_pInstance = (uCellPrivateInstance_t *) pInstance;
    uAtClientDeviceError_t deviceError;
    uAtClientStreamHandle_t stream = U_AT_CLIENT_STREAM_HANDLE_DEFAULTS;

    _pInstance->inWakeUpCallback = true;

    uAtClientStreamGetExt(atHandle, &stream);
    if (stream.type == U_AT_CLIENT_STREAM_TYPE_UART) {
        // Disable CTS, in case it gets in our way
        uPortUartCtsSuspend(stream.handle.int32);
    }

    if (uCellPrivateIsDeepSleepActive(_pInstance)) {
        // We know that the module has gone into 3GPP sleep, wake it up.
        errorCode = deepSleepWakeUp(_pInstance);
    } else {
        // Poke the AT interface a few times at short intervals
        // to either awaken the module or make sure it is awake
        for (size_t x = 0;
             (x < U_CELL_PRIVATE_UART_WAKE_UP_RETRIES + 1) && (errorCode != 0);
             x++) {
            uAtClientLock(atHandle);
            if (x == 0) {
                uAtClientTimeoutSet(atHandle, U_CELL_PRIVATE_UART_WAKE_UP_FIRST_WAIT_MS);
            } else {
                uAtClientTimeoutSet(atHandle, U_CELL_PRIVATE_UART_WAKE_UP_RETRY_INTERVAL_MS);
            }
            uAtClientCommandStart(atHandle, "AT");
            uAtClientCommandStopReadResponse(atHandle);
            uAtClientDeviceErrorGet(atHandle, &deviceError);
            // Doesn't matter what the response is, even an error is OK,
            // provided there is a response we're happy
            if ((uAtClientUnlock(atHandle) == 0) ||
                (deviceError.type != U_AT_CLIENT_DEVICE_ERROR_TYPE_NO_ERROR)) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            }
        }
        // If the AT-poking wake-up didn't work, check again if we've
        // gone to deep sleep and, if so, do the wake-up
        if ((errorCode != 0) && (uCellPrivateIsDeepSleepActive(_pInstance))) {
            errorCode = deepSleepWakeUp(_pInstance);
        }
    }

    if (stream.type == U_AT_CLIENT_STREAM_TYPE_UART) {
        // We can listen to CTS again
        uPortUartCtsResume(stream.handle.int32);
    }

    _pInstance->inWakeUpCallback = false;

    return errorCode;
}

// Determine the deep sleep state.
void uCellPrivateSetDeepSleepState(uCellPrivateInstance_t *pInstance)
{
    uCellPrivateSleep_t *pContext = pInstance->pSleepContext;

    // If the sleep state has already been set to "asleep", or
    // "protocol stack asleep" (which will have occurred because the
    // deep sleep URC was received), then we don't need to do anything.
    if ((pInstance->deepSleepState != U_CELL_PRIVATE_DEEP_SLEEP_STATE_ASLEEP) &&
        (pInstance->deepSleepState != U_CELL_PRIVATE_DEEP_SLEEP_STATE_PROTOCOL_STACK_ASLEEP)) {
        if (!U_CELL_PRIVATE_HAS(pInstance->pModule,
                                U_CELL_PRIVATE_FEATURE_3GPP_POWER_SAVING)) {
            // If 3GPP power saving is not supported then deep sleep is plainly unavailable
            pInstance->deepSleepState = U_CELL_PRIVATE_DEEP_SLEEP_STATE_UNAVAILABLE;
        } else {
            if (pContext == NULL) {
                // If there is no sleep context then we assume sleep is unavailable
                pInstance->deepSleepState = U_CELL_PRIVATE_DEEP_SLEEP_STATE_UNAVAILABLE;
            } else {
                // If 3GPP sleep has not been agreed with the network then sleep is unavailable.
                // Note: must have called uCellPwrPrivateGet3gppPowerSaving() beforehand
                // to set the powerSaving3gppAgreed flags appropriately.
                if (!pContext->powerSaving3gppAgreed) {
                    pInstance->deepSleepState = U_CELL_PRIVATE_DEEP_SLEEP_STATE_UNAVAILABLE;
                } else {
                    // Otherwise sleep can occur
                    pInstance->deepSleepState = U_CELL_PRIVATE_DEEP_SLEEP_STATE_AVAILABLE;
                }
            }
        }
    }
}

// Suspend "32 kHz" or UART/AT+UPSV sleep.
int32_t uCellPrivateSuspendUartPowerSaving(const uCellPrivateInstance_t *pInstance,
                                           int32_t *pMode, int32_t *pTimeout)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uAtClientHandle_t atHandle = pInstance->atHandle;

    if ((pMode != NULL) && (pTimeout != NULL)) {
        // First, read the current AT+UPSV mode
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UPSV?");
        uAtClientCommandStop(atHandle);
        uAtClientResponseStart(atHandle, "+UPSV:");
        *pMode = uAtClientReadInt(atHandle);
        *pTimeout = -1;
        if (!U_CELL_PRIVATE_MODULE_IS_SARA_R4(pInstance->pModule->moduleType) &&
            ((*pMode == 1) || (*pMode == 4))) {
            // Only non-SARA-R4 modules have a timeout value and
            // only for AT+UPSV modes 1 and 4
            *pTimeout = uAtClientReadInt(atHandle);
        }
        uAtClientResponseStop(atHandle);
        errorCode = uAtClientUnlock(atHandle);
        if ((errorCode == 0) && (*pMode > 0)) {
            // If that was successful and the current mode was
            // not already zero then we now disable AT+UPSV
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UPSV=");
            uAtClientWriteInt(atHandle, 0);
            uAtClientCommandStopReadResponse(atHandle);
            errorCode = uAtClientUnlock(atHandle);
        }
    }

    return errorCode;
}

// Resume "32 kHz" or UART/AT+UPSV sleep.
int32_t uCellPrivateResumeUartPowerSaving(const uCellPrivateInstance_t *pInstance,
                                          int32_t mode, int32_t timeout)
{
    uAtClientHandle_t atHandle = pInstance->atHandle;

    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UPSV=");
    uAtClientWriteInt(atHandle, mode);
    if (timeout >= 0) {
        uAtClientWriteInt(atHandle, timeout);
    }
    uAtClientCommandStopReadResponse(atHandle);

    return uAtClientUnlock(atHandle);
}

// Delete file on file system.
int32_t uCellPrivateFileDelete(const uCellPrivateInstance_t *pInstance,
                               const char *pFileName)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uAtClientHandle_t atHandle;

    // Check parameters
    if ((pInstance != NULL) && (pFileName != NULL) &&
        (strlen(pFileName) <= U_CELL_FILE_NAME_MAX_LENGTH)) {
        errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
        atHandle = pInstance->atHandle;
        // Do the UDELFILE thang with the AT interface
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UDELFILE=");
        // Write file name
        uAtClientWriteString(atHandle, pFileName, true);
        if (pInstance->pFileSystemTag != NULL) {
            // Write tag
            uAtClientWriteString(atHandle, pInstance->pFileSystemTag, true);
        }
        uAtClientCommandStop(atHandle);
        // Grab the response
        uAtClientCommandStopReadResponse(atHandle);
        if (uAtClientUnlock(atHandle) == 0) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
    }

    return errorCode;
}

// Get the name of the first file stored on file system.
int32_t uCellPrivateFileListFirst(const uCellPrivateInstance_t *pInstance,
                                  uCellPrivateFileListContainer_t **ppFileListContainer,
                                  char *pFileName)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uAtClientHandle_t atHandle;
    char *pFileNameTmp;
    uCellPrivateFileListContainer_t *pFileContainer = NULL;
    bool keepGoing = true;
    int32_t bytesRead = 0;
    size_t count = 0;

    // Check parameters
    if ((pInstance != NULL) && (ppFileListContainer != NULL) && (pFileName != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
        // Allocate temporary storage for a file name string
        pFileNameTmp = pUPortMalloc(U_CELL_FILE_NAME_MAX_LENGTH + 1);
        if (pFileNameTmp != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
            atHandle = pInstance->atHandle;
            // Do the ULSTFILE thang with the AT interface
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+ULSTFILE=");
            // List files operation
            uAtClientWriteInt(atHandle, 0);
            if (pInstance->pFileSystemTag != NULL) {
                // Write tag
                uAtClientWriteString(atHandle, pInstance->pFileSystemTag, true);
            }
            uAtClientCommandStop(atHandle);
            uAtClientResponseStart(atHandle, "+ULSTFILE:");
            while (keepGoing) {
                // Read file name
                keepGoing = false;
                bytesRead = uAtClientReadString(atHandle, pFileNameTmp,
                                                U_CELL_FILE_NAME_MAX_LENGTH + 1,
                                                false);
                if (bytesRead > 0) {
                    errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                    // Allocate space for the structure plus the actual file name
                    // stored immediately after the structure in the same malloc()ed space
                    pFileContainer = (uCellPrivateFileListContainer_t *) pUPortMalloc(sizeof(*pFileContainer) +
                                                                                      bytesRead);
                    if (pFileContainer != NULL) {
                        keepGoing = true;
                        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                        // Point pFileName just beyond the end of the structure
                        pFileContainer->pFileName = ((char *) pFileContainer) + sizeof(*pFileContainer);
                        // Copy the file name to this location (noting no null terminator
                        /// since it has a separate length indicator)
                        memcpy(pFileContainer->pFileName, pFileNameTmp, bytesRead);
                        pFileContainer->fileNameLength = bytesRead;
                        // Add the container to the end of the list
                        count = filelListAddCount(ppFileListContainer, pFileContainer);
                    }
                }
            }
            uAtClientResponseStop(atHandle);

            // Do the following parts inside the AT lock,
            // providing protection for the linked-list.
            if (errorCode == (int32_t) U_ERROR_COMMON_NO_MEMORY) {
                // If we ran out of memory, clear the whole list,
                // don't want to report partial information
                fileListClear(&pFileContainer);
            } else {
                if (count > 0) {
                    // Set the return value, copy out the first item in the list
                    // and remove it.
                    errorCode = (int32_t) count;
                    fileListGetRemove(ppFileListContainer, pFileName);
                } else {
                    errorCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
                }
            }
            uAtClientUnlock(atHandle);

            // Free temporary storage
            uPortFree(pFileNameTmp);
        }
    }

    return errorCode;
}

// Return subsequent file name in the list.
int32_t uCellPrivateFileListNext(uCellPrivateFileListContainer_t **ppFileListContainer,
                                 char *pFileName)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;

    // Check parameters
    if ((ppFileListContainer != NULL) && (*ppFileListContainer != NULL) &&
        (pFileName != NULL)) {
        errorCode = fileListGetRemove(ppFileListContainer, pFileName);
    }

    return errorCode;
}

// Free memory from list.
void uCellPrivateFileListLast(uCellPrivateFileListContainer_t **ppFileListContainer)
{
    if ((ppFileListContainer != NULL) && (*ppFileListContainer != NULL)) {
        fileListClear(ppFileListContainer);
    }
}

// Remove the HTTP context for the given instance.
void uCellPrivateHttpRemoveContext(uCellPrivateInstance_t *pInstance)
{
    uCellHttpContext_t *pHttpContext;
    uCellHttpInstance_t *pHttpInstance;
    uCellHttpInstance_t *pNextHttpInstance;

    if ((pInstance != NULL) && (pInstance->pHttpContext != NULL)) {

        pHttpContext = pInstance->pHttpContext;

        // Shut-down the event queue
        uPortEventQueueClose(pHttpContext->eventQueueHandle);

        // Free the instances
        pHttpInstance = pHttpContext->pInstanceList;
        while (pHttpInstance != NULL) {
            pNextHttpInstance = pHttpInstance->pNext;
            uPortFree(pHttpInstance);
            pHttpInstance = pNextHttpInstance;
        }

        // Free the mutex
        uPortMutexDelete(pHttpContext->linkedListMutex);

        // Free the context
        uPortFree(pHttpContext);
        pInstance->pHttpContext = NULL;
    }
}

// Set the DTR pin for power saving.
void uCellPrivateSetPinDtr(uCellPrivateInstance_t *pInstance, bool doNotPowerSave)
{
    int32_t targetState = U_CELL_PRIVATE_DTR_POWER_SAVING_PIN_ON_STATE(pInstance->pinStates);

    if (pInstance->pinDtrPowerSaving >= 0) {
        if (!doNotPowerSave) {
            targetState = !targetState;
        }
        while (uPortGetTickTimeMs() - pInstance->lastDtrPinToggleTimeMs <
               U_CELL_PWR_UART_POWER_SAVING_DTR_HYSTERESIS_MS) {
            uPortTaskBlock(U_CELL_PRIVATE_DTR_PIN_HYSTERESIS_INTERVAL_MS);
        }
        if (uPortGpioSet(pInstance->pinDtrPowerSaving, targetState) == 0) {
            pInstance->lastDtrPinToggleTimeMs = uPortGetTickTimeMs();
            uPortTaskBlock(U_CELL_PWR_UART_POWER_SAVING_DTR_READY_MS);
        }
    }
}

// Get the active serial interface configuration
int32_t uCellPrivateGetActiveSerialInterface(const uCellPrivateInstance_t *pInstance)
{
    int32_t errorCodeOrActiveVariant = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uAtClientHandle_t atHandle;
    int32_t activeVariant;

    if (pInstance != NULL) {
        atHandle = pInstance->atHandle;
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+USIO?");
        uAtClientCommandStop(atHandle);
        uAtClientResponseStart(atHandle, "+USIO:");
        uAtClientSkipParameters(atHandle, 1);
        // Skip one byte of '*' coming in the second param. e.g +USIO: 5,*5
        uAtClientSkipBytes(atHandle, 1);
        activeVariant = uAtClientReadInt(atHandle);
        uAtClientResponseStop(atHandle);
        errorCodeOrActiveVariant = uAtClientUnlock(atHandle);
        if ((errorCodeOrActiveVariant == 0) && (activeVariant >= 0)) {
            errorCodeOrActiveVariant = activeVariant;
        }
    }

    return errorCodeOrActiveVariant;
}

// Set "AT+UGPRF".
int32_t uCellPrivateSetGnssProfile(const uCellPrivateInstance_t *pInstance,
                                   int32_t profileBitMap,
                                   const char *pServerName)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uAtClientHandle_t atHandle;
    char *pServerNameTmp = NULL;
    char *pTmp = NULL;
    int32_t port = 0;

    if ((pInstance != NULL) &&
        (((profileBitMap & U_CELL_CFG_GNSS_PROFILE_IP) == 0) || (pServerName != NULL))) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        if (((profileBitMap & U_CELL_CFG_GNSS_PROFILE_IP) > 0) && (pServerName != NULL)) {
            errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
            // Grab some temporary memory to manipulate the server name
            // +1 for terminator
            pServerNameTmp = (char *) pUPortMalloc(U_CELL_CFG_GNSS_SERVER_NAME_MAX_LEN_BYTES);
            if (pServerNameTmp != NULL) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                strncpy(pServerNameTmp, pServerName, U_CELL_CFG_GNSS_SERVER_NAME_MAX_LEN_BYTES);
                // Ensure terminator
                *(pServerNameTmp + U_CELL_CFG_GNSS_SERVER_NAME_MAX_LEN_BYTES - 1) = 0;
                // Grab any port number off the end
                // and then remove it from the string
                port = uSockDomainGetPort(pServerNameTmp);
                pTmp = pUSockDomainRemovePort(pServerNameTmp);
            }
        }
        if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
            atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UGPRF=");
            uAtClientWriteInt(atHandle, profileBitMap);
            if (pTmp != NULL) {
                uAtClientWriteInt(atHandle, port);
                uAtClientWriteString(atHandle, pTmp, true);
            }
            uAtClientCommandStopReadResponse(atHandle);
            errorCode = uAtClientUnlock(atHandle);
        }

        // Free memory
        uPortFree(pServerNameTmp);
    }

    return errorCode;
}

// Get "AT+UGPRF".
int32_t uCellPrivateGetGnssProfile(const uCellPrivateInstance_t *pInstance,
                                   char *pServerName, size_t sizeBytes)
{
    int32_t errorCodeOrBitMap = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uAtClientHandle_t atHandle;
    int32_t bitMap;
    int32_t port = 0;
    int32_t bytesRead = 0;
    int32_t x;

    if (pInstance != NULL) {
        atHandle = pInstance->atHandle;
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UGPRF?");
        uAtClientCommandStop(atHandle);
        uAtClientResponseStart(atHandle, "+UGPRF:");
        bitMap = uAtClientReadInt(atHandle);
        if ((bitMap >= 0) && ((bitMap & U_CELL_CFG_GNSS_PROFILE_IP) > 0)) {
            port = uAtClientReadInt(atHandle);
            bytesRead = uAtClientReadString(atHandle, pServerName, sizeBytes, false);
        }
        uAtClientResponseStop(atHandle);
        errorCodeOrBitMap = uAtClientUnlock(atHandle);
        if (errorCodeOrBitMap == 0) {
            errorCodeOrBitMap = bitMap;
            if ((bytesRead > 0) && (pServerName != NULL)) {
                x = strlen(pServerName);
                snprintf(pServerName + x, sizeBytes - x, ":%d", (int) port);
            }
        }
    }

    return errorCodeOrBitMap;
}

// Check whether there is a GNSS chip on-board the cellular module.
bool uCellPrivateGnssInsideCell(const uCellPrivateInstance_t *pInstance)
{
    bool isInside = false;
    uAtClientHandle_t atHandle;
    int32_t bytesRead;
    char buffer[64]; // Enough for the ATI response

    if (pInstance != NULL) {
        atHandle = pInstance->atHandle;
        // Simplest way to check is to send ATI and see if
        // it includes an "M8" or an "M10"
        bytesRead = uAtClientGetAti(atHandle, buffer, sizeof(buffer));
        if (bytesRead > 0) {
            if (strstr(buffer, "M8") != NULL) {
                isInside = true;
            } else if (strstr(buffer, "M10") != NULL) {
                isInside = true;
            }
        }
    }

    return isInside;
}

// Remove the CellTime context for the given instance.
void uCellPrivateCellTimeRemoveContext(uCellPrivateInstance_t *pInstance)
{
    uCellTimePrivateContext_t *pContext;

    if (pInstance != NULL) {
        pContext = (uCellTimePrivateContext_t *) pInstance->pCellTimeContext;
        if (pContext != NULL) {
            if (pContext->pCallbackEvent != NULL) {
                uAtClientRemoveUrcHandler(pInstance->atHandle, "+UUTIMEIND:");
            }
            if (pContext->pCallbackTime != NULL) {
                uAtClientRemoveUrcHandler(pInstance->atHandle, "+UTIME:");
            }
            uPortFree(pInstance->pCellTimeContext);
            pInstance->pCellTimeContext = NULL;
        }
        uPortFree(pInstance->pCellTimeCellSyncContext);
        pInstance->pCellTimeCellSyncContext = NULL;
    }
}

// Perform an abort of an AT command.
void uCellPrivateAbortAtCommand(const uCellPrivateInstance_t *pInstance)
{
    uAtClientHandle_t atHandle = pInstance->atHandle;
    uAtClientDeviceError_t deviceError;
    bool success = false;

    // Abort is done by sending anything, we use
    // here just a space, after an AT command has
    // been sent and before the response comes
    // back.  It is, however, possible for
    // an abort to be ignored so we test for that
    // and try a few times
    for (size_t x = 3; (x > 0) && !success; x--) {
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, " ");
        uAtClientCommandStopReadResponse(atHandle);
        uAtClientDeviceErrorGet(atHandle, &deviceError);
        // Check that a device error has been signalled,
        // we've not simply timed out
        success = (deviceError.type != U_AT_CLIENT_DEVICE_ERROR_TYPE_NO_ERROR);
        uAtClientUnlock(atHandle);
    }
}

// Get an ID string from the cellular module.
int32_t uCellPrivateGetIdStr(uAtClientHandle_t  atHandle,
                             const char *pCmd, char *pBuffer,
                             size_t bufferSize)
{
    int32_t errorCodeOrSize;
    int32_t bytesRead;
    char delimiter;

    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "ATE0");
    uAtClientCommandStopReadResponse(atHandle);
    uAtClientCommandStart(atHandle, pCmd);
    uAtClientCommandStop(atHandle);
    // Don't want characters in the string being interpreted
    // as delimiters
    delimiter = uAtClientDelimiterGet(atHandle);
    uAtClientDelimiterSet(atHandle, '\x00');
    uAtClientResponseStart(atHandle, NULL);
    bytesRead = uAtClientReadString(atHandle, pBuffer,
                                    bufferSize, false);
    uAtClientResponseStop(atHandle);
    // Restore the delimiter
    uAtClientDelimiterSet(atHandle, delimiter);
    errorCodeOrSize = uAtClientUnlock(atHandle);
    if ((bytesRead >= 0) && (errorCodeOrSize == 0)) {
        uPortLog("U_CELL_INFO: ID string, length %d character(s),"
                 " returned by %s is \"%s\".\n",
                 bytesRead, pCmd, pBuffer);
        errorCodeOrSize = bytesRead;
    } else {
        errorCodeOrSize = (int32_t) U_CELL_ERROR_AT;
        uPortLog("U_CELL_INFO: unable to read ID string using"
                 " %s.\n", pCmd);
    }

    return errorCodeOrSize;
}

// Updates the module related settings for the given instance.
void uCellPrivateModuleSpecificSetting(uCellPrivateInstance_t *pInstance)
{
    if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                           U_CELL_PRIVATE_FEATURE_AUTHENTICATION_MODE_AUTOMATIC)) {
        // Set automatic authentication mode where supported
        pInstance->authenticationMode = U_CELL_NET_AUTHENTICATION_MODE_AUTOMATIC;
    }
    uAtClientTimeoutSet(pInstance->atHandle,
                        pInstance->pModule->atTimeoutSeconds * 1000);
    uAtClientDelaySet(pInstance->atHandle,
                      pInstance->pModule->commandDelayDefaultMs);
}

// End of file
