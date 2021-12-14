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

#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_os_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_uart.h"

#include "u_at_client.h"

#include "u_cell_module_type.h"
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

#include "u_gnss_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** The names of the transport types.
 */
static const char *const gpTransportTypeString[] = {"none", "ubx UART",
                                                    "ubx AT", "NMEA UART"
                                                   };

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

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
    int32_t cellHandle = -1;

    uPortLog("U_GNSS_TEST_PRIVATE: making sure cellular is off...\n");

    uPortLog("U_GNSS_TEST_PRIVATE: opening UART %d...\n",
             U_CFG_APP_CELL_UART);
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
            uPortLog("U_GNSS_TEST_PRIVATE: adding an AT client on UART %d...\n",
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
            uPortLog("U_GNSS_TEST_PRIVATE: adding a cellular instance on"
                     " the AT client...\n");
            errorCode = uCellAdd(U_CFG_TEST_CELL_MODULE_TYPE,
                                 atClientHandle,
                                 U_CFG_APP_PIN_CELL_ENABLE_POWER,
                                 U_CFG_APP_PIN_CELL_PWR_ON,
                                 U_CFG_APP_PIN_CELL_VINT, false);
        }
    }

    if (errorCode >= 0) {
        cellHandle = errorCode;
        if (uCellPwrIsPowered(cellHandle) && uCellPwrIsAlive(cellHandle)) {
            // Finally, power it off
# if U_CFG_APP_PIN_CELL_PWR_ON >= 0
            uPortLog("U_GNSS_TEST_PRIVATE: now we can power cellular off...\n");
            errorCode = uCellPwrOff(cellHandle, NULL);
# endif
        } else {
            uPortLog("U_GNSS_TEST_PRIVATE: cellular is already off.\n");
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
                                         int32_t uart)
{
    size_t numEntries = 0;

    if (pTransportTypes != NULL) {
        if (uart >= 0) {
            *pTransportTypes = U_GNSS_TRANSPORT_NMEA_UART;
            pTransportTypes++;
            *pTransportTypes = U_GNSS_TRANSPORT_UBX_UART;
            numEntries += 2;
        } else {
            *pTransportTypes = U_GNSS_TRANSPORT_UBX_AT;
            numEntries++;
        }
    }

    return numEntries;
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
    pParameters->uartHandle = -1;
    pParameters->pAtClientHandle = NULL;
    pParameters->cellHandle = -1;
    pParameters->gnssHandle = -1;

    uPortLog("U_GNSS_TEST_PRIVATE: test preamble start.\n");

    // Initialise the porting layer
    errorCode = uPortInit();
    if (errorCode == 0) {
        // Set up the transport stuff
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        switch (transportType) {
            case U_GNSS_TRANSPORT_UBX_UART:
            //lint -fallthrough
            case U_GNSS_TRANSPORT_NMEA_UART:
                uPortLog("U_GNSS_TEST_PRIVATE: opening GNSS UART %d...\n",
                         U_CFG_APP_GNSS_UART);
                // Open a UART with the standard parameters
                errorCode = uPortUartOpen(U_CFG_APP_GNSS_UART,
                                          U_GNSS_UART_BAUD_RATE, NULL,
                                          U_GNSS_UART_BUFFER_LENGTH_BYTES,
                                          U_CFG_APP_PIN_GNSS_TXD,
                                          U_CFG_APP_PIN_GNSS_RXD,
                                          U_CFG_APP_PIN_GNSS_CTS,
                                          U_CFG_APP_PIN_GNSS_RTS);
                if (errorCode >= 0) {
                    pParameters->uartHandle = errorCode;
                    transportHandle.uart = pParameters->uartHandle;
                }
                break;
            case U_GNSS_TRANSPORT_UBX_AT:
#ifdef U_CFG_TEST_CELL_MODULE_TYPE
            {
                uCellTestPrivate_t parameters = U_CELL_TEST_PRIVATE_DEFAULTS;
                // Re-use the cellular test preamble function for the AT transport,
                // making sure to always power cellular on so that we can get at
                // the GNSS chip
                errorCode = uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                     &parameters, true);
                pParameters->uartHandle = parameters.uartHandle;
                pParameters->pAtClientHandle = (void *) parameters.atClientHandle;
                pParameters->cellHandle = parameters.cellHandle;
                transportHandle.pAt = pParameters->pAtClientHandle;
            }
#else
            uPortLog("U_GNSS_TEST_PRIVATE: U_CFG_TEST_CELL_MODULE_TYPE is"
                     " not defined, can't use AT.\n");
#endif
            break;
            default:
                break;
        }

        if (errorCode >= 0) {
            // Now add GNSS on the transport
            if (uGnssInit() == 0) {
                uPortLog("U_GNSS_TEST_PRIVATE: adding a GNSS instance...\n");
                pParameters->gnssHandle = uGnssAdd(moduleType,
                                                   transportType,
                                                   //lint -e(644) Suppress transportHandle might not be
                                                   // initialised: it is checked through errorCode
                                                   transportHandle,
                                                   U_CFG_APP_PIN_GNSS_ENABLE_POWER, false);
                if (pParameters->gnssHandle >= 0) {
                    if ((pParameters->cellHandle >= 0) &&
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
    if (powerOff && (pParameters->gnssHandle >= 0)) {
        uGnssPwrOff(pParameters->gnssHandle);
    }

    uPortLog("U_GNSS_TEST_PRIVATE: deinitialising GNSS API...\n");
    // Let uGnssDeinit() remove the GNSS handle
    uGnssDeinit();
    pParameters->gnssHandle = -1;

    if (pParameters->cellHandle >= 0) {
        // Cellular was in use, call the cellular test postamble
        uCellTestPrivate_t parameters = U_CELL_TEST_PRIVATE_DEFAULTS;
        parameters.uartHandle = pParameters->uartHandle;
        parameters.atClientHandle = (uAtClientHandle_t) pParameters->pAtClientHandle;
        parameters.cellHandle = pParameters->cellHandle;
        uCellTestPrivatePostamble(&parameters, powerOff);
        pParameters->cellHandle = -1;
    } else {
        uPortUartClose(pParameters->uartHandle);
    }
    pParameters->uartHandle = -1;

    uPortDeinit();
}

// The standard clean-up for a GNSS test.
void uGnssTestPrivateCleanup(uGnssTestPrivate_t *pParameters)
{
    uGnssDeinit();
    pParameters->gnssHandle = -1;

    if (pParameters->cellHandle >= 0) {
        // Cellular was in use, call the cellular test clean-up
        uCellTestPrivate_t parameters = U_CELL_TEST_PRIVATE_DEFAULTS;
        parameters.uartHandle = pParameters->uartHandle;
        parameters.atClientHandle = (uAtClientHandle_t) pParameters->pAtClientHandle;
        parameters.cellHandle = pParameters->cellHandle;
        uCellTestPrivateCleanup(&parameters);
        pParameters->cellHandle = -1;
    } else {
        if (pParameters->uartHandle >= 0) {
            uPortUartClose(pParameters->uartHandle);
        }
    }
    pParameters->uartHandle = -1;
}

// End of file
