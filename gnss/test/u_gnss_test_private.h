/*
 * Copyright 2020 u-blox
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

#ifndef _U_GNSS_TEST_PRIVATE_H_
#define _U_GNSS_TEST_PRIVATE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file defines types, functions and inclusions that
 * are common and private to GNSS API testing.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Default values for uGnssTestPrivate_t.
 */
//lint -esym(755, U_GNSS_TEST_PRIVATE_DEFAULTS) Suppress not referenced,
// which it might not be if U_CFG_TEST_GNSS_MODULE_TYPE is not defined.
#define U_GNSS_TEST_PRIVATE_DEFAULTS {-1, NULL, NULL, NULL}

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Struct to contain all the stuff needed by the common functions.
 */
typedef struct {
    int32_t uartHandle; /**< The handle returned by uPortUartOpen(). */
    void *pAtClientHandle; /**< The handle returned by uAtClientAdd(). */
    uDeviceHandle_t cellHandle;  /**< The handle returned by uCellAdd(). */
    uDeviceHandle_t gnssHandle;  /**< The handle returned by uGnssAdd(). */
} uGnssTestPrivate_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

#ifdef U_CFG_TEST_CELL_MODULE_TYPE
/** Make sure that the cellular module is off, so that it doesn't
 * interfere with control of the GNSS chip over UART.
 *
 * @return  zero on success else negative error code.
 */
int32_t uGnssTestPrivateCellularOff();
#endif

/** Return a string representing the name of the given transport type.
 *
 * @param  transportType  the transport type.
 * @return                a string representing the name of the
 *                        transport type.
 */
const char *pGnssTestPrivateTransportTypeName(uGnssTransportType_t transportType);

/** Set the transport types to be tested.
 *
 * @param pTransportTypes  a pointer to the first entry in an array of
 *                         U_GNSS_TRANSPORT_MAX_NUM transport types.
 * @param uart             the value of U_CFG_APP_GNSS_UART, i.e. the UART
 *                         on which the GNSS chip is connected or -1
 *                         if there is no direct UART connection to the
 *                         GNSS chip (i.e. if the GNSS chip is connected
 *                         via a cellular module).
 * @return                 the number of entries that have been populated in
 *                         pTransportTypes by this function.
 */
size_t uGnssTestPrivateTransportTypesSet(uGnssTransportType_t *pTransportTypes,
                                         int32_t uart);

/** The standard preamble for a GNSS test.  Creates all the necessary
 * instances and powers the module on if requested.
 *
 * @param moduleType           the module type.
 * @param transportType        the transport type to use.
 * @param pParameters          the place to put the parameters.
 * @param powerOn              set to true if the module should also be
 *                             powered on.
 * @param atModulePinPwr       the pin (not GPIO number, the pin number) of
 *                             the intermediate (e.g. cellular) module which
 *                             controls power to the cellular module, only
 *                             relevant if the transport type is
 *                             U_GNSS_TRANSPORT_UBX_AT; use -1 if there is
 *                             no such connection.
 * @param atModulePinDataReady the pin (not GPIO number, the pin number) of
 *                             the intermediat (e.g. cellular) module which is
 *                             connected to the Data Ready output of the GNSS,
 *                             chip only relevant if the transport type is
 *                             U_GNSS_TRANSPORT_UBX_AT; use -1 if there is no
 *                             such connection.
 * @return                     zero on success else negative error code.
 */
//lint -esym(759, uGnssTestPrivatePreamble) Suppress the "can be
//lint -esym(765, uGnssTestPrivatePreamble) made static" etc. which
//lint -esym(714, uGnssTestPrivatePreamble) will occur if
//lint -esym(757, uGnssTestPrivatePreamble) U_CFG_TEST_GNSS_MODULE_TYPE
//                                          is not defined
int32_t uGnssTestPrivatePreamble(uGnssModuleType_t moduleType,
                                 uGnssTransportType_t transportType,
                                 uGnssTestPrivate_t *pParameters,
                                 bool powerOn,
                                 int32_t atModulePinPwr,
                                 int32_t atModulePinDataReady);

/** The standard postamble for a GNSS test.
 *
 * @param pParameters a pointer to the parameters struct.
 * @param powerOff    set to true if the module should also be
 *                    powered off.
 */
//lint -esym(759, uGnssTestPrivatePostamble) Suppress the "can be
//lint -esym(765, uGnssTestPrivatePostamble) made static" etc. which
//lint -esym(714, uGnssTestPrivatePostamble) will occur if
//                                           U_CFG_TEST_GNSS_MODULE_TYPE
//                                           is not defined
void uGnssTestPrivatePostamble(uGnssTestPrivate_t *pParameters,
                               bool powerOff);

/** The standard clean-up for a GNSS test.
 *
 * @param pParameters a pointer to the parameters struct.
 */
//lint -esym(759, uGnssTestPrivateCleanup) Suppress the "can be
//lint -esym(765, uGnssTestPrivateCleanup) made static" etc. which
//lint -esym(714, uGnssTestPrivateCleanup) will occur if
//                                         U_CFG_TEST_GNSS_MODULE_TYPE
//                                         is not defined
void uGnssTestPrivateCleanup(uGnssTestPrivate_t *pParameters);

#ifdef __cplusplus
}
#endif

#endif // _U_GNSS_TEST_PRIVATE_H_

// End of file
