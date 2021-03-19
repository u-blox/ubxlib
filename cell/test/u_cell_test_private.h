/*
 * Copyright 2020 u-blox Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _U_CELL_TEST_PRIVATE_H_
#define _U_CELL_TEST_PRIVATE_H_

/* No #includes allowed here */

/** @file
 * @brief This header file defines types, functions and inclusions that
 * are common and private to cellular API testing.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Default values for uCellTestPrivate_t.
 */
//lint -esym(755, U_CELL_TEST_PRIVATE_DEFAULTS) Suppress not referenced,
// which it might not be if U_CFG_TEST_CELL_MODULE_TYPE is not defined.
#define U_CELL_TEST_PRIVATE_DEFAULTS {-1, NULL, -1}

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Struct to contain all the stuff needed by the common functions.
 */
typedef struct {
    int32_t uartHandle; /**< The handle returned by uPortUartOpen(). */
    uAtClientHandle_t atClientHandle; /**< The handle returned by uAtClientAdd(). */
    int32_t cellHandle;  /**< The handle returned by uCellAdd(). */
} uCellTestPrivate_t;

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
 * @param pParameters the place to put the parameters.
 * @param powerOn     set to true if the module should also be
 *                    powered on.
 * @return            zero on success else negative error code.
 */
//lint -esym(759, uCellTestPrivatePreamble) Suppress the "can be
//lint -esym(765, uCellTestPrivatePreamble) made static" etc. which
//lint -esym(714, uCellTestPrivatePreamble) will occur if
//                                          U_CFG_TEST_CELL_MODULE_TYPE
//                                          is not defined
int32_t uCellTestPrivatePreamble(uCellModuleType_t moduleType,
                                 uCellTestPrivate_t *pParameters,
                                 bool powerOn);

/** The standard postamble for a cell test.
 *
 * @param pParameters a pointer to the parameters struct.
 * @param powerOff    set to true if the module should also be
 *                    powered off.
 */
//lint -esym(759, uCellTestPrivatePostamble) Suppress the "can be
//lint -esym(765, uCellTestPrivatePostamble) made static" etc. which
//lint -esym(714, uCellTestPrivatePostamble) will occur if
//                                           U_CFG_TEST_CELL_MODULE_TYPE
//                                           is not defined
void uCellTestPrivatePostamble(uCellTestPrivate_t *pParameters,
                               bool powerOff);

/** The standard clean-up for a cell test.
 *
 * @param pParameters a pointer to the parameters struct.
 */
//lint -esym(759, uCellTestPrivateCleanup) Suppress the "can be
//lint -esym(765, uCellTestPrivateCleanup) made static" etc. which
//lint -esym(714, uCellTestPrivateCleanup) will occur if
//                                         U_CFG_TEST_CELL_MODULE_TYPE
//                                         is not defined
void uCellTestPrivateCleanup(uCellTestPrivate_t *pParameters);

/** Return a string describing the given RAT.
 *
 * @param rat    the RAT.
 * @return       a string representing the RAT.
 */
//lint -esym(759, pUCellTestPrivateRatStr) Suppress the "can be
//lint -esym(765, pUCellTestPrivateRatStr) made static" etc. which
//lint -esym(714, pUCellTestPrivateRatStr) will occur if
//                                         U_CFG_TEST_CELL_MODULE_TYPE
//                                         is not defined
const char *pUCellTestPrivateRatStr(uCellNetRat_t rat);

/** Return the sole RAT that the uCellTestPrivatePreamble() ensures
 * will be set before a test begins.
 *
 * @param supportedRatsBitmap  the support RATS bitmap for the module.
 * @return                     the RAT.
 */
//lint -esym(759, uCellTestPrivateInitRatGet) Suppress the "can be
//lint -esym(765, uCellTestPrivateInitRatGet) made static" etc. which
//lint -esym(714, uCellTestPrivateInitRatGet) will occur if
//                                            U_CFG_TEST_CELL_MODULE_TYPE
//                                            is not defined
uCellNetRat_t uCellTestPrivateInitRatGet(uint32_t supportedRatsBitmap);

#ifdef __cplusplus
}
#endif

#endif // _U_CELL_TEST_PRIVATE_H_

// End of file
