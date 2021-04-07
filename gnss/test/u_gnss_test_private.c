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

#include "u_gnss_types.h"
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

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// The standard preamble for a GNSS test.
int32_t uGnssTestPrivatePreamble(uGnssModuleType_t moduleType,
                                 uGnssTestPrivate_t *pParameters,
                                 bool powerOn)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssTransportHandle_t transportHandle;

    // Set some defaults
    pParameters->uartHandle = -1;
    pParameters->gnssHandle = -1;

    uPortLog("U_GNSS_TEST_PRIVATE: test preamble start.\n");

    // Initialise the porting layer
    if (uPortInit() == 0) {
        uPortLog("U_GNSS_TEST_PRIVATE: opening UART %d...\n",
                 U_CFG_APP_GNSS_UART);
        // Open a UART with the standard parameters
        pParameters->uartHandle = uPortUartOpen(U_CFG_APP_GNSS_UART,
                                                115200, NULL,
                                                U_GNSS_UART_BUFFER_LENGTH_BYTES,
                                                U_CFG_APP_PIN_GNSS_TXD,
                                                U_CFG_APP_PIN_GNSS_RXD,
                                                U_CFG_APP_PIN_GNSS_CTS,
                                                U_CFG_APP_PIN_GNSS_RTS);
        if (pParameters->uartHandle >= 0) {
            if (uGnssInit() == 0) {
                uPortLog("U_GNSS_TEST_PRIVATE: adding a GNSS instance...\n");
                transportHandle.uart = pParameters->uartHandle;
                pParameters->gnssHandle = uGnssAdd(moduleType,
                                                   U_GNSS_TRANSPORT_NMEA_UART,
                                                   transportHandle,
                                                   U_CFG_APP_PIN_GNSS_EN, false);
                if (pParameters->gnssHandle >= 0) {
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

    uPortUartClose(pParameters->uartHandle);
    pParameters->uartHandle = -1;

    uPortDeinit();
}

// The standard clean-up for a GNSS test.
void uGnssTestPrivateCleanup(uGnssTestPrivate_t *pParameters)
{
    uGnssDeinit();
    pParameters->gnssHandle = -1;

    if (pParameters->uartHandle >= 0) {
        uPortUartClose(pParameters->uartHandle);
    }
    pParameters->uartHandle = -1;
}

// End of file
