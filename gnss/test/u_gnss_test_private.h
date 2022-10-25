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
#define U_GNSS_TEST_PRIVATE_DEFAULTS {U_GNSS_TRANSPORT_NONE, -1, NULL, NULL, NULL}

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Struct to contain all the stuff needed by the common functions.
 */
typedef struct {
    uGnssTransportType_t transportType;
    int32_t streamHandle; /**< The handle returned by uPortUartOpen()/uPortI2cOpen(). */
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
 *                         U_GNSS_TRANSPORT_MAX_NUM_WITH_UBX transport types.
 * @param uart             the value of U_CFG_APP_GNSS_UART, the UART
 *                         on which the GNSS chip is connected or -1
 *                         if there is no direct UART connection to the
 *                         GNSS chip (i.e. if the GNSS chip is connected
 *                         via a cellular module or via I2C).
 * @param i2c              the value of U_CFG_APP_GNSS_I2C, the I2C bus
 *                         on which the GNSS chip is connected or -1
 *                         if there is no direct I2C connection to the
 *                         GNSS chip (i.e. if the GNSS chip is connected
 *                         via a cellular module or via UART).
 * @return                 the number of entries that have been populated in
 *                         pTransportTypes by this function.
 */
size_t uGnssTestPrivateTransportTypesSet(uGnssTransportType_t *pTransportTypes,
                                         int32_t uart, int32_t i2c);

/** Return a string representing the name of the given protocol.
 *
 * @param  protocol  the protocol.
 * @return           a string giving the name of the protocol.
 */
const char *pGnssTestPrivateProtocolName(uGnssProtocol_t protocol);

/** The standard preamble for a GNSS test.  Creates all the necessary
 * instances and powers the module on if requested.
 *
 * @param moduleType           the module type.
 * @param transportType        the transport type to use.
 * @param pParameters          the place to put the parameters.
 * @param powerOn              set to true if the module should also be
 *                             powered on.
 * @param atModulePinPwr       the pin (not GPIO number, the pin number) of
 *                             the intermediate (for example cellular) module
 *                             which controls power to the cellular module,
 *                             only relevant if the transport type is
 *                             #U_GNSS_TRANSPORT_AT; use -1 if there is
 *                             no such connection.
 * @param atModulePinDataReady the pin (not GPIO number, the pin number) of
 *                             the intermediat (for example cellular) module
 *                             which is connected to the Data Ready output of
 *                             the GNSS, chip only relevant if the transport
 *                             type is #U_GNSS_TRANSPORT_AT; use -1 if
 *                             there is no such connection.
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

/** The sequence of NMEA messages emitted by a GNSS receiver
 * follows a known pattern (see the implementation of this function
 * for details).  This function may be called for a sequence of
 * NMEA messages, one at a time, and will return an error code
 * based on whether the sequence is as expected or whether NMEA
 * messages are missing.  The CRC is NOT checked: messages are
 * assumed to be correct and fully-formed, it is only the _sequence_
 * which is checked.
 *
 * Note: this has only been tested on M9 modules.
 *
 * A good pattern for using this function is as follows:
 *
 * - call it for every NMEA message with ppContext initially
 *   pointing to a NULL pointer,
 * - wait for #U_ERROR_COMMON_TIMEOUT to be returned: this
 *   indicates that an expected sequence has begun,
 * - if the function ever returns #U_ERROR_COMMON_NOT_FOUND *after*
 *   that then there is an error in the sequence.
 * - if it returns #U_ERROR_COMMON_SUCCESS then an expected
 *   NMEA sequence has ended.
 * - to stop using this function without a memory leak you must
 *   uPortFree(*ppContext).
 *
 * @param pNmeaMessage a pointer to a complete NMEA message.
 * @param size         the number of bytes at pNmeaMessage.
 * @param ppContext    a pointer to a pointer where this function
 *                     can store context data for the next time
 *                     it is called.  ppContext must point to
 *                     NULL when this function is first called
 *                     (i.e. *ppContext must be NULL).  Cannot be
 *                     NULL.
 * @param printErrors  if true erorr messages will be printed when
 *                     errors are spotted.
 * @return             if *ppContext is NULL and pNmeaMessage contains
 *                     the start of an NMEA sequence then context data
 *                     will be allocated and stored at *ppContext for
 *                     the next time the function is called and
 *                     #U_ERROR_COMMON_TIMEOUT will be returned.  If
 *                     *ppContext is non-NULL then the data stored
 *                     there and the new message will be used to
 *                     determine if the sequence is as expected; if
 *                     it is, and the sequence is not yet complete,
 *                     #U_ERROR_COMMON_TIMEOUT will be returned.  If
 *                     there is an error in the sequence, or the start
 *                     of a sequence has not yet been found,
 *                     #U_ERROR_COMMON_NOT_FOUND will be returned and
 *                     *ppContext will be free'ed (or not populated),
 *                     resetting things for the next sequence.  If a
 *                     sequence is completed #U_ERROR_COMMON_SUCCESS
 *                     will be returned and, as for the error case,
 *                     the allocated context will be free'ed.
 */
//lint -esym(759, uGnssTestPrivateNmeaComprehender) Suppress the "can be
//lint -esym(765, uGnssTestPrivateNmeaComprehender) made static" etc. which
//lint -esym(714, uGnssTestPrivateNmeaComprehender) will occur if
//                                                  U_CFG_TEST_GNSS_MODULE_TYPE
//                                                  is not defined
int32_t uGnssTestPrivateNmeaComprehender(const char *pNmeaMessage, size_t size,
                                         void **ppContext, bool printErrors);

#ifdef __cplusplus
}
#endif

#endif // _U_GNSS_TEST_PRIVATE_H_

// End of file
