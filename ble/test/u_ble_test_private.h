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
 * are common and private to ble API testing.
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
    uDeviceHandle_t devHandle;  /**< The device handle returned by uShortRangeOpenUart(). */
} uBleTestPrivate_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** The standard preamble for a cell test.  Creates all the necessary
 * instances, powers the module on if requested and, if the module
 * has been powered on, ensures that it is operating on the correct
 * RAT and bands for testing.
 *
 * @param moduleType  the module type.
 * @param pUartConfig the uart config.
 * @param pParameters the place to put the parameters.
 * @return            zero on success else negative error code.
 */
//lint -esym(759, uBleTestPrivatePreamble) Suppress the "can be
//lint -esym(765, uBleTestPrivatePreamble) made static" etc. which
//lint -esym(714, uBleTestPrivatePreamble) will occur if
//                                          U_CFG_TEST_SHORT_RANGE_MODULE_TYPE
//                                          is not defined
int32_t uBleTestPrivatePreamble(uBleModuleType_t moduleType,
                                const uShortRangeUartConfig_t *pUartConfig,
                                uBleTestPrivate_t *pParameters);

/** The standard postamble for a cell test.
 *
 * @param pParameters a pointer to the parameters struct.
 */
//lint -esym(759, uBleTestPrivatePostamble) Suppress the "can be
//lint -esym(765, uBleTestPrivatePostamble) made static" etc. which
//lint -esym(714, uBleTestPrivatePostamble) will occur if
//                                           U_CFG_TEST_SHORT_RANGE_MODULE_TYPE
//                                           is not defined
void uBleTestPrivatePostamble(uBleTestPrivate_t *pParameters);

/** The standard clean-up for a cell test.
 *
 * @param pParameters a pointer to the parameters struct.
 */
//lint -esym(759, uBleTestPrivateCleanup) Suppress the "can be
//lint -esym(765, uBleTestPrivateCleanup) made static" etc. which
//lint -esym(714, uBleTestPrivateCleanup) will occur if
//                                         U_CFG_TEST_SHORT_RANGE_MODULE_TYPE
//                                         is not defined
void uBleTestPrivateCleanup(uBleTestPrivate_t *pParameters);

#ifdef __cplusplus
}
#endif

#endif // _U_BLE_TEST_PRIVATE_H_

// End of file
