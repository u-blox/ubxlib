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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Common stuff used in testing of the GNSS API.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdlib.h"    // strtol()
#include "string.h"    // memset(), strtol()

#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_os_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_uart.h"
#include "u_port_i2c.h"

#include "u_at_client.h"

#include "u_cell_module_type.h"
#include "u_cell_file.h"    // Required by u_cell_private.h
#include "u_cell_net.h"     // Required by u_cell_private.h
#include "u_cell_private.h" // Required by u_cell_test_private.h
#include "u_cell_test_private.h"
#ifdef U_CFG_TEST_CELL_MODULE_TYPE
#include "u_cell.h"
#include "u_cell_pwr.h"
#endif
#include "u_cell_loc.h"  // For uCellLocGnssInsideCell()

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss_private.h"
#include "u_gnss.h"
#include "u_gnss_pwr.h"
#include "u_gnss_util.h"

#include "u_gnss_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_GNSS_TEST_PRIVATE: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Possible states for tracking NMEA messages, see
 * uGnssTestPrivateNmeaComprehender().
 */
typedef enum {
    U_GNSS_TEST_PRIVATE_NMEA_STATE_NULL,
    U_GNSS_TEST_PRIVATE_NMEA_STATE_GOT_GNRMC_1_START,
    U_GNSS_TEST_PRIVATE_NMEA_STATE_GOT_GNVTG_2,
    U_GNSS_TEST_PRIVATE_NMEA_STATE_GOT_GNGGA_3,
    U_GNSS_TEST_PRIVATE_NMEA_STATE_GOT_GNGSA_4,
    U_GNSS_TEST_PRIVATE_NMEA_STATE_GOT_GXGSV_5
} uGnssTestPrivateNmeaState_t;

/** Context data structure for uGnssTestPrivateNmeaComprehender().
 */
typedef struct {
    uGnssTestPrivateNmeaState_t state;
    size_t lastGngsa;
    char xInGxgsv;
    size_t totalInGxgsv;
    size_t lastInGxgsv;
} uGnssTestPrivateNmeaContext_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** The names of the transport types.
 */
static const char *const gpTransportTypeString[] = {"none", "UART",
                                                    "AT", "I2C",
                                                    "UBX UART", "UBX I2C"
                                                   };

/** The names of the protocol types.
 */
static const char *const gpProtocolString[] = {"UBX", "NMEA"};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Get the number just before the * in an NMEA string, which could
// be the sequence number in a GNGSA, used by
// uGnssTestPrivateNmeaComprehender().
static size_t getGngsa(const char *pBuffer, size_t size)
{
    size_t number = 0;
    const char *pLastComma = NULL;

    // Find the last comma in the string
    for (const char *pTmp = pBuffer;  // *NOPAD*
         (pTmp != NULL) && (pTmp - pBuffer < (int32_t) size);
         pTmp = strchr(pTmp + 1, (int32_t) ',')) {
        pLastComma = pTmp;
    }
    // Make sure there was a command and there is a * after it
    if ((pLastComma != NULL) && (pLastComma - pBuffer + 1 < (int32_t) size) &&
        (strchr(pLastComma, (int32_t) '*') != NULL)) {
        number = strtol(pLastComma + 1, NULL, 10);
    }

    return number;
}

// Get the ? character and the values of the numbers Y and Z from
// a string of the form "$G?GSV,Y,Z", used by
// uGnssTestPrivateNmeaComprehender().
static char getGxgsv(const char *pBuffer, size_t size,
                     size_t *pTotalInGxgsv, size_t *pThisInGxgsv)
{
    char x = 0;
    char *pEnd = NULL;

    *pTotalInGxgsv = 0;
    *pThisInGxgsv = 0;
    if ((size >= 10) && (*pBuffer == '$') && (*(pBuffer + 1) == 'G') &&
        (*(pBuffer + 3) == 'G') && (*(pBuffer + 4) == 'S') &&
        (*(pBuffer + 5) == 'V') && (*(pBuffer + 6) == ',')) {
        x = *(pBuffer + 2);
        *pTotalInGxgsv = strtol(pBuffer + 7, &pEnd, 10);
        if ((pEnd != NULL) && (pEnd - pBuffer < (int32_t) size - 1)) {
            *pThisInGxgsv = strtol(pEnd + 1, NULL, 10);
        }
    }

    return x;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

#ifdef U_CFG_TEST_CELL_MODULE_TYPE
// Make sure that the cellular module is off.
int32_t uGnssTestPrivateCellularOff()
{
    int32_t errorCode;
    int32_t uartHandle = -1;
    uAtClientHandle_t atClientHandle = NULL;
    uDeviceHandle_t cellHandle = NULL;

    U_TEST_PRINT_LINE("making sure cellular is off...");

    U_TEST_PRINT_LINE("opening UART %d...", U_CFG_APP_CELL_UART);
    // Open a UART with the standard parameters
    errorCode = uPortUartOpen(U_CFG_APP_CELL_UART,
                              115200, NULL,
                              U_CELL_UART_BUFFER_LENGTH_BYTES,
                              U_CFG_APP_PIN_CELL_TXD,
                              U_CFG_APP_PIN_CELL_RXD,
                              U_CFG_APP_PIN_CELL_CTS,
                              U_CFG_APP_PIN_CELL_RTS);

    if (errorCode >= 0) {
        uartHandle = errorCode;
        errorCode = uAtClientInit();
        if (errorCode == 0) {
            errorCode = (int32_t) U_ERROR_COMMON_UNKNOWN;
            U_TEST_PRINT_LINE("adding an AT client on UART %d...",
                              U_CFG_APP_CELL_UART);
            atClientHandle = uAtClientAdd(uartHandle,
                                          U_AT_CLIENT_STREAM_TYPE_UART,
                                          NULL,
                                          U_CELL_AT_BUFFER_LENGTH_BYTES);
        }
    }

    if (atClientHandle != NULL) {
        errorCode = uCellInit();
        if (errorCode == 0) {
            U_TEST_PRINT_LINE("adding a cellular instance on the AT client...");
            errorCode = uCellAdd(U_CFG_TEST_CELL_MODULE_TYPE,
                                 atClientHandle,
                                 U_CFG_APP_PIN_CELL_ENABLE_POWER,
                                 U_CFG_APP_PIN_CELL_PWR_ON,
                                 U_CFG_APP_PIN_CELL_VINT, false,
                                 &cellHandle);
#if defined(U_CFG_APP_PIN_CELL_DTR) && (U_CFG_APP_PIN_CELL_DTR >= 0)
            if (errorCode == 0) {
                errorCode = uCellPwrSetDtrPowerSavingPin(cellHandle, U_CFG_APP_PIN_CELL_DTR);
            }
#endif
        }
    }

    if (errorCode >= 0) {
        if (uCellPwrIsPowered(cellHandle) && uCellPwrIsAlive(cellHandle)) {
            // Finally, power it off
# if U_CFG_APP_PIN_CELL_PWR_ON >= 0
            U_TEST_PRINT_LINE("now we can power cellular off...");
            errorCode = uCellPwrOff(cellHandle, NULL);
# endif
        } else {
            U_TEST_PRINT_LINE("cellular is already off.");
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
    }

    // Tidy up
    uCellDeinit();
    uAtClientDeinit();
    if (uartHandle >= 0) {
        uPortUartClose(uartHandle);
    }

    return errorCode;
}
#endif // #ifdef U_CFG_TEST_CELL_MODULE_TYPE

// Return a string representing the name of the given transport type.
const char *pGnssTestPrivateTransportTypeName(uGnssTransportType_t transportType)
{
    const char *pString = NULL;

    if ((size_t) transportType < sizeof(gpTransportTypeString) / sizeof(gpTransportTypeString[0])) {
        pString = gpTransportTypeString[(size_t) transportType];
    }

    return pString;
}

// Set the transport types to be tested.
size_t uGnssTestPrivateTransportTypesSet(uGnssTransportType_t *pTransportTypes,
                                         int32_t uart, int32_t i2c)
{
    size_t numEntries = 0;

    if (pTransportTypes != NULL) {
        if (uart >= 0) {
            *pTransportTypes = U_GNSS_TRANSPORT_UART;
            pTransportTypes++;
            *pTransportTypes = U_GNSS_TRANSPORT_UBX_UART;
            pTransportTypes++;
            numEntries += 2;
        }
        if (i2c >= 0) {
            *pTransportTypes = U_GNSS_TRANSPORT_I2C;
            pTransportTypes++;
            *pTransportTypes = U_GNSS_TRANSPORT_UBX_I2C;
            pTransportTypes++;
            numEntries += 2;
        }
        if (numEntries == 0) {
            *pTransportTypes = U_GNSS_TRANSPORT_AT;
            numEntries++;
        }
    }

    return numEntries;
}

// Return a string representing the protocol.
const char *pGnssTestPrivateProtocolName(uGnssProtocol_t protocol)
{
    const char *pString = NULL;

    if (((size_t) protocol >= 0) &&
        ((size_t) protocol < sizeof(gpProtocolString) / sizeof(gpProtocolString[0]))) {
        pString = gpProtocolString[(size_t) protocol];
    }

    return pString;
}

// The standard preamble for a GNSS test.
int32_t uGnssTestPrivatePreamble(uGnssModuleType_t moduleType,
                                 uGnssTransportType_t transportType,
                                 uGnssTestPrivate_t *pParameters,
                                 bool powerOn,
                                 int32_t atModulePinPwr,
                                 int32_t atModulePinDataReady)
{
    int32_t errorCode;
    uGnssTransportHandle_t transportHandle;

    // Set some defaults
    pParameters->transportType = transportType;
    pParameters->streamHandle = -1;
    pParameters->pAtClientHandle = NULL;
    pParameters->cellHandle = NULL;
    pParameters->gnssHandle = NULL;

    U_TEST_PRINT_LINE("test preamble start.");

    // Initialise the porting layer
    errorCode = uPortInit();
    if (errorCode == 0) {
        // Set up the transport stuff
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        switch (transportType) {
            case U_GNSS_TRANSPORT_UART:
            //lint -fallthrough
            case U_GNSS_TRANSPORT_UBX_UART:
                U_TEST_PRINT_LINE("opening GNSS UART %d...", U_CFG_APP_GNSS_UART);
                // Open a UART with the standard parameters
                errorCode = uPortUartOpen(U_CFG_APP_GNSS_UART,
                                          U_GNSS_UART_BAUD_RATE, NULL,
                                          U_GNSS_UART_BUFFER_LENGTH_BYTES,
                                          U_CFG_APP_PIN_GNSS_TXD,
                                          U_CFG_APP_PIN_GNSS_RXD,
                                          U_CFG_APP_PIN_GNSS_CTS,
                                          U_CFG_APP_PIN_GNSS_RTS);
                if (errorCode >= 0) {
                    pParameters->streamHandle = errorCode;
                    transportHandle.uart = pParameters->streamHandle;
                }
                break;
            case U_GNSS_TRANSPORT_I2C:
            //lint -fallthrough
            case U_GNSS_TRANSPORT_UBX_I2C:
                U_TEST_PRINT_LINE("opening GNSS I2C %d...", U_CFG_APP_GNSS_I2C);
                errorCode = uPortI2cInit();
                if (errorCode == 0) {
                    // Open the I2C bus with the standard parameters
                    errorCode = uPortI2cOpen(U_CFG_APP_GNSS_I2C,
                                             U_CFG_APP_PIN_GNSS_SDA,
                                             U_CFG_APP_PIN_GNSS_SCL,
                                             true);
                    if (errorCode >= 0) {
                        pParameters->streamHandle = errorCode;
                        transportHandle.i2c = pParameters->streamHandle;
                    }
                }
                break;
            case U_GNSS_TRANSPORT_AT:
#ifdef U_CFG_TEST_CELL_MODULE_TYPE
            {
                uCellTestPrivate_t parameters = U_CELL_TEST_PRIVATE_DEFAULTS;
                // Re-use the cellular test preamble function for the AT transport,
                // making sure to always power cellular on so that we can get at
                // the GNSS chip
                errorCode = uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                     &parameters, true);
                pParameters->streamHandle = parameters.uartHandle;
                pParameters->pAtClientHandle = (void *) parameters.atClientHandle;
                pParameters->cellHandle = parameters.cellHandle;
                transportHandle.pAt = pParameters->pAtClientHandle;
            }
#else
            U_TEST_PRINT_LINE("U_CFG_TEST_CELL_MODULE_TYPE is not defined, can't use AT.");
#endif
            break;
            default:
                break;
        }

        if (errorCode >= 0) {
            // Now add GNSS on the transport
            if (uGnssInit() == 0) {
                U_TEST_PRINT_LINE("adding a GNSS instance...");
                errorCode = uGnssAdd(moduleType,
                                     transportType,
                                     //lint -e(644) Suppress transportHandle might not be
                                     // initialised: it is checked through errorCode
                                     transportHandle,
                                     U_CFG_APP_PIN_GNSS_ENABLE_POWER, false,
                                     &pParameters->gnssHandle);
                if (errorCode >= 0) {
                    if ((pParameters->cellHandle != NULL) &&
                        !uCellLocGnssInsideCell(pParameters->cellHandle)) {
                        // If we're talking via cellular and the GNSS chip
                        // isn't inside the cellular module, need to configure the
                        // module pins that control the GNSS chip
                        if (atModulePinPwr >= 0) {
                            uGnssSetAtPinPwr(pParameters->gnssHandle, atModulePinPwr);
                        }
                        if (atModulePinDataReady >= 0) {
                            uGnssSetAtPinDataReady(pParameters->gnssHandle, atModulePinDataReady);
                        }
                    }
                    if (powerOn) {
                        errorCode = uGnssPwrOn(pParameters->gnssHandle);
                    } else {
                        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    }
                }
            }
        }
    }

    return errorCode;
}

// The standard postamble for a GNSS test.
void uGnssTestPrivatePostamble(uGnssTestPrivate_t *pParameters,
                               bool powerOff)
{
    if (powerOff && (pParameters->gnssHandle != NULL)) {
        uGnssPwrOff(pParameters->gnssHandle);
    }

    U_TEST_PRINT_LINE("deinitialising GNSS API...");
    // Let uGnssDeinit() remove the GNSS handle
    uGnssDeinit();
    pParameters->gnssHandle = NULL;

    if (pParameters->cellHandle != NULL) {
        // Cellular was in use, call the cellular test postamble
        uCellTestPrivate_t parameters = U_CELL_TEST_PRIVATE_DEFAULTS;
        parameters.uartHandle = pParameters->streamHandle;
        parameters.atClientHandle = (uAtClientHandle_t) pParameters->pAtClientHandle;
        parameters.cellHandle = pParameters->cellHandle;
        uCellTestPrivatePostamble(&parameters, powerOff);
        pParameters->cellHandle = NULL;
    } else {
        if (pParameters->streamHandle >= 0) {
            switch (pParameters->transportType) {
                case U_GNSS_TRANSPORT_UART:
                //lint -fallthrough
                case U_GNSS_TRANSPORT_UBX_UART:
                    uPortUartClose(pParameters->streamHandle);
                    break;
                case U_GNSS_TRANSPORT_I2C:
                //lint -fallthrough
                case U_GNSS_TRANSPORT_UBX_I2C:
                    uPortI2cClose(pParameters->streamHandle);
                    uPortI2cDeinit();
                default:
                    break;
            }
        }
    }
    pParameters->streamHandle = -1;

    uPortDeinit();
}

// The standard clean-up for a GNSS test.
void uGnssTestPrivateCleanup(uGnssTestPrivate_t *pParameters)
{
    uGnssDeinit();
    pParameters->gnssHandle = NULL;

    if (pParameters->cellHandle != NULL) {
        // Cellular was in use, call the cellular test clean-up
        uCellTestPrivate_t parameters = U_CELL_TEST_PRIVATE_DEFAULTS;
        parameters.uartHandle = pParameters->streamHandle;
        parameters.atClientHandle = (uAtClientHandle_t) pParameters->pAtClientHandle;
        parameters.cellHandle = pParameters->cellHandle;
        uCellTestPrivateCleanup(&parameters);
        pParameters->cellHandle = NULL;
    } else {
        if (pParameters->streamHandle >= 0) {
            switch (pParameters->transportType) {
                case U_GNSS_TRANSPORT_UART:
                //lint -fallthrough
                case U_GNSS_TRANSPORT_UBX_UART:
                    uPortUartClose(pParameters->streamHandle);
                    break;
                case U_GNSS_TRANSPORT_I2C:
                //lint -fallthrough
                case U_GNSS_TRANSPORT_UBX_I2C:
                    uPortI2cClose(pParameters->streamHandle);
                    uPortI2cDeinit();
                default:
                    break;
            }
        }
    }
    pParameters->streamHandle = -1;
}

// Track a sequence of NMEA message to spot errors,
//
// Here is an example of a good sequence of NMEA messages, taken
// from a ZED-F9P:
//
// $GNRMC,143858.00,A,4710.5737891,N,00825.4665003,E,0.009,,180922,2.83,E,D,V*40\r\n
// $GNVTG,,T,,M,0.009,N,0.016,K,D*36\r\n
// $GNGGA,143858.00,4710.5737891,N,00825.4665003,E,2,12,0.58,459.860,M,47.319,M,,0123*4B\r\n
// $GNGSA,A,3,02,05,06,09,11,20,07,30,,,,,1.24,0.58,1.10,1*05\r\n
// $GNGSA,A,3,76,67,82,81,75,65,66,,,,,,1.24,0.58,1.10,2*0A\r\n
// $GNGSA,A,3,30,33,12,26,19,07,,,,,,,1.24,0.58,1.10,3*02\r\n
// $GNGSA,A,3,20,32,37,46,19,,,,,,,,1.24,0.58,1.10,4*03\r\n
// $GNGSA,A,3,,,,,,,,,,,,,1.24,0.58,1.10,5*0F\r\n
// $GPGSV,3,1,11,02,26,307,37,05,16,309,45,06,30,212,43,07,73,126,48,1*60\r\n
// $GPGSV,3,2,11,09,48,072,41,11,44,251,46,13,07,259,31,20,54,298,43,1*6A\r\n
// $GPGSV,3,3,11,30,50,195,46,36,31,150,45,49,36,185,44,1*5B\r\n
// $GPGSV,2,1,08,04,12,077,23,05,16,309,46,06,30,212,41,07,73,126,43,6*64\r\n
// $GPGSV,2,2,08,09,48,072,40,11,44,251,38,29,03,323,26,30,50,195,47,6*64\r\n
// $GLGSV,3,1,09,65,37,088,48,66,66,346,42,67,21,297,50,75,44,053,32,1*71\r\n
// $GLGSV,3,2,09,76,45,141,48,77,08,177,35,81,20,246,47,82,29,299,50,1*79\r\n
// $GLGSV,3,3,09,83,13,343,16,1*4B\r\n
// $GLGSV,3,1,09,65,37,088,41,66,66,346,39,67,21,297,42,75,44,053,37,3*70\r\n
// $GLGSV,3,2,09,76,45,141,44,77,08,177,28,81,20,246,46,82,29,299,40,3*7B\r\n
// $GLGSV,3,3,09,83,13,343,23,3*4F\r\n
// $GLGSV,1,1,01,74,02,018,,0*40\r\n
// $GAGSV,2,1,08,07,54,073,40,10,08,335,37,12,23,316,47,19,22,272,36,2*7C\r\n
// $GAGSV,2,2,08,26,19,204,43,27,21,142,30,30,32,083,47,33,39,256,46,2*72\r\n
// $GAGSV,3,1,09,07,54,073,34,10,08,335,38,12,23,316,38,19,22,272,41,7*7D\r\n
// $GAGSV,3,2,09,20,,,34,26,19,204,42,27,21,142,18,30,32,083,44,7*43\r\n
// $GAGSV,3,3,09,33,39,256,44,7*41\r\n
// $GAGSV,1,1,01,08,04,085,,0*44\r\n
// $GBGSV,2,1,07,19,36,297,41,20,84,015,43,32,32,108,50,37,55,118,46,1*79\r\n
// $GBGSV,2,2,07,46,22,183,43,56,,,38,57,,,44,1*44\r\n
// $GQGSV,1,1,00,0*64\r\n
// $GNGLL,4710.5737891,N,00825.4665003,E,143858.00,A,D*78\r\n
//
// Hence the expected pattern is:
//
// - start with a $GNRMC message, followed by a $GNVTG message, followed by
//   a $GNGGA message,
// - one or more $GNGSA message will follow, where the digit before the *
//   at the end starts at 1 and increments by one for each message,
// - sets of $G?GSV,y,z messages will follow where y is the number of each
//   type and z the count of the messages within that type,
// - end with a $GNGLL message.
int32_t uGnssTestPrivateNmeaComprehender(const char *pNmeaMessage, size_t size,
                                         void **ppContext, bool printErrors)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
    uGnssTestPrivateNmeaContext_t *pContext = (uGnssTestPrivateNmeaContext_t *) *ppContext;
    char x ;
    size_t thisGngsa = 0;
    size_t totalInGxgsv = 0;
    size_t thisInGxgsv = 0;

    if (pContext == NULL) {
        // No context, so we are looking for the start of a sequence, $GNRMC.
        if ((size >= 6) && (strstr(pNmeaMessage, "$GNRMC") == pNmeaMessage)) {
            // Got the start of a sequence, allocate memory to track it
            errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
            pContext = (uGnssTestPrivateNmeaContext_t *) pUPortMalloc(sizeof(uGnssTestPrivateNmeaContext_t));
            if (pContext != NULL) {
                memset(pContext, 0, sizeof(*pContext));
                pContext->state = U_GNSS_TEST_PRIVATE_NMEA_STATE_GOT_GNRMC_1_START;
                errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
            }
        }
    } else if (size >= 6) {
        // Have a context and enough of a message to contain
        // a talker/sentence, we must be in a sequence; track it
        switch (pContext->state) {
            case U_GNSS_TEST_PRIVATE_NMEA_STATE_GOT_GNRMC_1_START:
                // Next must be $GNVTG
                if (strstr(pNmeaMessage, "$GNVTG") == pNmeaMessage) {
                    pContext->state = U_GNSS_TEST_PRIVATE_NMEA_STATE_GOT_GNVTG_2;
                    errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
                } else if (printErrors) {
                    U_TEST_PRINT_LINE("NMEA sequence error: had $GNRMC,"
                                      " expecting $GNVTG but got \"%.6s\".",
                                      pNmeaMessage);
                }
                break;
            case U_GNSS_TEST_PRIVATE_NMEA_STATE_GOT_GNVTG_2:
                // Next must be $GNGGA
                if (strstr(pNmeaMessage, "$GNGGA") == pNmeaMessage) {
                    pContext->state = U_GNSS_TEST_PRIVATE_NMEA_STATE_GOT_GNGGA_3;
                    errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
                } else if (printErrors) {
                    U_TEST_PRINT_LINE("NMEA sequence error: had $GNVTG,"
                                      " expecting $GNGGA but got \"%.6s\".",
                                      pNmeaMessage);
                }
                break;
            case U_GNSS_TEST_PRIVATE_NMEA_STATE_GOT_GNGGA_3:
                // Next must be $GNGSA
                thisGngsa = getGngsa(pNmeaMessage, size);
                if (strstr(pNmeaMessage, "$GNGSA") == pNmeaMessage) {
                    pContext->state = U_GNSS_TEST_PRIVATE_NMEA_STATE_GOT_GNGSA_4;
                    pContext->lastGngsa = thisGngsa;
                    errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
                } else if (printErrors) {
                    U_TEST_PRINT_LINE("NMEA sequence error: had $GNGGA,"
                                      " expecting $GNGSA ... %d* but got \"%.6s ... %d*\".",
                                      pContext->lastGngsa + 1, pNmeaMessage, thisGngsa);
                }
                break;
            case U_GNSS_TEST_PRIVATE_NMEA_STATE_GOT_GNGSA_4:
            //lint -fallthrough
            case U_GNSS_TEST_PRIVATE_NMEA_STATE_GOT_GXGSV_5:
                // Next either we have another $GNGSA or we're in a $G?GSV or
                // we might even be expecting a $GNGLL
                thisGngsa = getGngsa(pNmeaMessage, size);
                x = getGxgsv(pNmeaMessage, size, &totalInGxgsv, &thisInGxgsv);
                if ((pContext->state == U_GNSS_TEST_PRIVATE_NMEA_STATE_GOT_GNGSA_4) &&
                    (strstr(pNmeaMessage, "$GNGSA") == pNmeaMessage)) {
                    if (thisGngsa == pContext->lastGngsa + 1) {
                        // $GNGSA continues
                        pContext->lastGngsa = thisGngsa;
                        errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
                    } else if (printErrors) {
                        U_TEST_PRINT_LINE("NMEA sequence error: expecting"
                                          " $GNGSA ... %d* but got $GNGSA ... %d*.",
                                          pContext->lastGngsa + 1, thisGngsa);
                    }
                } else if (x != 0) {
                    // We're in, or maybe starting a sequence of $G?GSV
                    pContext->lastGngsa = 0;
                    if (pContext->xInGxgsv == x) {
                        // We've had this $G?GSV before, check the numbers
                        if ((pContext->totalInGxgsv == totalInGxgsv) &&
                            (thisInGxgsv == pContext->lastInGxgsv + 1)) {
                            // We're in sequence, all is good
                            pContext->state = U_GNSS_TEST_PRIVATE_NMEA_STATE_GOT_GXGSV_5;
                            pContext->lastInGxgsv = thisInGxgsv;
                            errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
                            if (thisInGxgsv == pContext->totalInGxgsv) {
                                // That must be the last of this $G?GSV, reset
                                pContext->xInGxgsv = 0;
                            }
                        } else if (printErrors) {
                            U_TEST_PRINT_LINE("NMEA sequence error: expecting $G%cGSV %d,%d"
                                              " but got $G%cGSV %d,%d.",
                                              pContext->xInGxgsv, pContext->totalInGxgsv,
                                              pContext->lastInGxgsv + 1,
                                              pContext->xInGxgsv, thisInGxgsv, totalInGxgsv);
                        }
                    } else {
                        // Not seen this $G?GSV before, check that we're
                        // at the start of one
                        if (pContext->xInGxgsv == 0) {
                            pContext->state = U_GNSS_TEST_PRIVATE_NMEA_STATE_GOT_GXGSV_5;
                            if (thisInGxgsv < totalInGxgsv) {
                                // We are at the start of one that has
                                // more than a single message, remember it
                                pContext->xInGxgsv = x;
                                pContext->totalInGxgsv = totalInGxgsv;
                                pContext->lastInGxgsv = thisInGxgsv;
                            }
                            errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
                        } else if (printErrors) {
                            U_TEST_PRINT_LINE("NMEA sequence error: a new $G?GSV has"
                                              " started but we haven't finished the last"
                                              " one yet, was expecting $G%cGSV %d,%d.",
                                              pContext->xInGxgsv, pContext->totalInGxgsv,
                                              pContext->lastInGxgsv);
                        }
                    }
                } else if ((pContext->state == U_GNSS_TEST_PRIVATE_NMEA_STATE_GOT_GXGSV_5) &&
                           (pContext->xInGxgsv == 0) &&
                           (strstr(pNmeaMessage, "$GNGLL") == pNmeaMessage)) {
                    // We're not currently in a $G?GSV but we have been in one and we've now
                    // hit a $GNGLL: we're done
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                } else if (printErrors) {
                    if (pContext->xInGxgsv > 0) {
                        if (size >= 11) {
                            U_TEST_PRINT_LINE("NMEA sequence error: expecting $G%cGSV %d"
                                              " of %d or $GNGLL but got \"%.11s...\".",
                                              pContext->xInGxgsv, pContext->lastInGxgsv + 1,
                                              pContext->totalInGxgsv, pNmeaMessage);
                        } else {
                            U_TEST_PRINT_LINE("NMEA sequence error: expecting $G%cGSV %d"
                                              " of %d or $GNGLL but got \"%.6s\".",
                                              pContext->xInGxgsv, pContext->lastInGxgsv + 1,
                                              pContext->totalInGxgsv, pNmeaMessage);
                        }
                    } else {
                        U_TEST_PRINT_LINE("NMEA sequence error: expecting a new $G?GSV"
                                          " or $GNGLL but got \"%.6s\".", pNmeaMessage);
                    }
                }
                break;
            default:
                if (printErrors) {
                    U_TEST_PRINT_LINE("NMEA sequence error: unknown state (%d).", pContext->state);
                }
                break;
        }
    } else if (printErrors) {
        U_TEST_PRINT_LINE("NMEA sequence error: message too short (%d character(s): \"%.*s\").",
                          size, size, pNmeaMessage);
    }

    if ((errorCode == (int32_t) U_ERROR_COMMON_NOT_FOUND) ||
        (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS)) {
        // In either case we can free the context
        uPortFree(pContext);
        pContext = NULL;
    }

    // Always store the context back before exiting
    *ppContext = pContext;

    return errorCode;
}

// End of file
