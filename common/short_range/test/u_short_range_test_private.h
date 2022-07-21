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

#ifndef _U_SHORT_RANGE_TEST_PRIVATE_H_
#define _U_SHORT_RANGE_TEST_PRIVATE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file defines types, functions and inclusions that
 * are common and private to short range API testing.
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
    uDeviceHandle_t devHandle;  /**< The handle returned by uShortRangeOpenUart(). */
} uShortRangeTestPrivate_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** The standard preamble for a short range test.  Creates all the necessary
 * instances
 *
 * @param moduleType       the module type.
 * @param[in] pUartConfig  the uart config.
 * @param[out] pParameters the place to put the parameters.
 * @return                 zero on success else negative error code.
 */
//lint -esym(759, uShortRangeTestPrivatePreamble) Suppress the "can be
//lint -esym(765, uShortRangeTestPrivatePreamble) made static" etc. which
//lint -esym(714, uShortRangeTestPrivatePreamble) will occur if
//                                          U_CFG_TEST_SHORT_RANGE_MODULE_TYPE
//                                          is not defined
int32_t uShortRangeTestPrivatePreamble(uShortRangeModuleType_t moduleType,
                                       const uShortRangeUartConfig_t *pUartConfig,
                                       uShortRangeTestPrivate_t *pParameters);

/** The standard postamble for a short range test.
 *
 * @param[in,out] pParameters a pointer to the parameters struct.
 */
//lint -esym(759, uShortRangeTestPrivatePostamble) Suppress the "can be
//lint -esym(765, uShortRangeTestPrivatePostamble) made static" etc. which
//lint -esym(714, uShortRangeTestPrivatePostamble) will occur if
//                                           U_CFG_TEST_SHORT_RANGE_MODULE_TYPE
//                                           is not defined
void uShortRangeTestPrivatePostamble(uShortRangeTestPrivate_t *pParameters);

/** The standard clean-up for a short range test.
 *
 * @param[in,out] pParameters a pointer to the parameters struct.
 */
//lint -esym(759, uShortRangeTestPrivateCleanup) Suppress the "can be
//lint -esym(765, uShortRangeTestPrivateCleanup) made static" etc. which
//lint -esym(714, uShortRangeTestPrivateCleanup) will occur if
//                                         U_CFG_TEST_SHORT_RANGE_MODULE_TYPE
//                                         is not defined
void uShortRangeTestPrivateCleanup(uShortRangeTestPrivate_t *pParameters);

#ifdef __cplusplus
}
#endif

#endif // _U_SHORT_RANGE_TEST_PRIVATE_H_

// End of file
