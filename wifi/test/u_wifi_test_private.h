/*
 * Copyright 2019-2022 u-blox
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

#ifndef _U_BLE_TEST_PRIVATE_H_
#define _U_BLE_TEST_PRIVATE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file defines types, functions and inclusions that
 * are common and private to wifi API testing.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Struct to contain all the stuff needed by the common functions.
 */
typedef struct {
    int32_t uartHandle; /**< The handle returned by uShortRangeGetUartHandle(). */
    int32_t edmStreamHandle; /**< The handle returned by uShortRangeGetEdmStreamHandle(). */
    uAtClientHandle_t atClientHandle; /**< The handle returned by uShortRangeAtClientHandleGet(). */
    uDeviceHandle_t devHandle;  /**< The u-blox device handle returned by uShortRangeOpenUart(). */
} uWifiTestPrivate_t;


//lint -esym(769, uWifiTestError_t::U_WIFI_TEST_ERROR_NONE)
//lint -esym(769, uWifiTestError_t::U_WIFI_TEST_ERROR_PREAMBLE)
//lint -esym(769, uWifiTestError_t::U_WIFI_TEST_ERROR_CONNECT)
//lint -esym(769, uWifiTestError_t::U_WIFI_TEST_ERROR_CONNECTED)
//lint -esym(769, uWifiTestError_t::U_WIFI_TEST_ERROR_IPRECV)
//lint -esym(769, uWifiTestError_t::U_WIFI_TEST_ERROR_DISCONNECT)
//lint -esym(756, uWifiTestError_t)
typedef enum {
    U_WIFI_TEST_ERROR_NONE = 0,
    U_WIFI_TEST_ERROR_PREAMBLE,
    U_WIFI_TEST_ERROR_CONNECT,
    U_WIFI_TEST_ERROR_CONNECTED,
    U_WIFI_TEST_ERROR_IPRECV,
    U_WIFI_TEST_ERROR_DISCONNECT
} uWifiTestError_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** The standard preamble for a Wifi test.  Creates all the necessary
 * instances, powers the module on if requested and, if the module
 * has been powered on, ensures that it is operating on the correct
 * RAT and bands for testing.
 *
 * @param moduleType  the module type.
 * @param pUartConfig the uart config.
 * @param pParameters the place to put the parameters.
 * @return            zero on success else negative error code.
 */
//lint -esym(759, uWifiTestPrivatePreamble) Suppress the "can be
//lint -esym(765, uWifiTestPrivatePreamble) made static" etc. which
//lint -esym(714, uWifiTestPrivatePreamble) will occur if
//                                          U_CFG_TEST_SHORT_RANGE_MODULE_TYPE
//                                          is not defined
int32_t uWifiTestPrivatePreamble(uWifiModuleType_t moduleType,
                                 const uShortRangeUartConfig_t *pUartConfig,
                                 uWifiTestPrivate_t *pParameters);

/** The standard postamble for a Wifi test.
 *
 * @param pParameters a pointer to the parameters struct.
 */
//lint -esym(759, uWifiTestPrivatePostamble) Suppress the "can be
//lint -esym(765, uWifiTestPrivatePostamble) made static" etc. which
//lint -esym(714, uWifiTestPrivatePostamble) will occur if
//                                           U_CFG_TEST_SHORT_RANGE_MODULE_TYPE
//                                           is not defined
void uWifiTestPrivatePostamble(uWifiTestPrivate_t *pParameters);

/** The standard clean-up for a wifi test.
 *
 * @param pParameters a pointer to the parameters struct.
 */
//lint -esym(759, uWifiTestPrivateCleanup) Suppress the "can be
//lint -esym(765, uWifiTestPrivateCleanup) made static" etc. which
//lint -esym(714, uWifiTestPrivateCleanup) will occur if
//                                         U_CFG_TEST_SHORT_RANGE_MODULE_TYPE
//                                         is not defined
void uWifiTestPrivateCleanup(uWifiTestPrivate_t *pParameters);

#ifdef __cplusplus
}
#endif

#endif // _U_BLE_TEST_PRIVATE_H_

// End of file
