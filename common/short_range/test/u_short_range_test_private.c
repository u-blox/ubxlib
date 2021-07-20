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
 * @brief Common stuff used in testing of the short range API.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()

#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_uart.h"

#include "u_at_client.h"

//lint -efile(766, u_ble_data.h)
#include "u_ble_data.h"

#include "u_short_range_module_type.h"
#include "u_short_range.h"
#include "u_short_range_private.h"
#include "u_short_range_edm_stream.h"
#include "u_short_range_test_private.h"

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

/** The standard preamble for a short range test.
 */
int32_t uShortRangeTestPrivatePreamble(uShortRangeModuleType_t moduleType,
                                       uAtClientStream_t streamType,
                                       uShortRangeTestPrivate_t *pParameters)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    int32_t shortRangeHandle;
    const uShortRangePrivateModule_t *pModule;

    // Set some defaults
    pParameters->uartHandle = -1;
    pParameters->atClientHandle = NULL;
    pParameters->shortRangeHandle = -1;

    // Initialise the porting layer
    if (uPortInit() == 0) {
        uPortLog("U_SHORT_RANGE_TEST_PRIVATE: opening UART %d...\n",
                 U_CFG_APP_SHORT_RANGE_UART);
        // Open a UART with the standard parameters
        pParameters->uartHandle = uPortUartOpen(U_CFG_APP_SHORT_RANGE_UART,
                                                U_SHORT_RANGE_UART_BAUD_RATE,
                                                NULL,
                                                U_SHORT_RANGE_UART_BUFFER_LENGTH_BYTES,
                                                U_CFG_APP_PIN_SHORT_RANGE_TXD,
                                                U_CFG_APP_PIN_SHORT_RANGE_RXD,
                                                U_CFG_APP_PIN_SHORT_RANGE_CTS,
                                                U_CFG_APP_PIN_SHORT_RANGE_RTS);
    }

    if (pParameters->uartHandle >= 0) {
        if (streamType == U_AT_CLIENT_STREAM_TYPE_EDM) {
            if (uShortRangeEdmStreamInit() == 0) {
                pParameters->edmStreamHandle = uShortRangeEdmStreamOpen(pParameters->uartHandle);
                if (pParameters->edmStreamHandle >= 0) {
                    char atCommand[] = "\r\nATO2\r\n";
                    uShortRangeEdmStreamAtWrite(pParameters->edmStreamHandle, (char *)&atCommand[0], 8);
                    if (uAtClientInit() == 0) {
                        uPortLog("U_SHORT_RANGE_TEST_PRIVATE: adding an AT client on EDM...\n");
                        pParameters->atClientHandle = uAtClientAdd(pParameters->edmStreamHandle,
                                                                   U_AT_CLIENT_STREAM_TYPE_EDM,
                                                                   NULL,
                                                                   U_SHORT_RANGE_AT_BUFFER_LENGTH_BYTES);
                        if (pParameters->atClientHandle != NULL) {
                            uShortRangeEdmStreamSetAtHandle(pParameters->edmStreamHandle, pParameters->atClientHandle);
                        }
                    }

                }
            }
        } else {
            if (uAtClientInit() == 0) {
                uPortLog("U_SHORT_RANGE_TEST_PRIVATE: adding an AT client on UART %d...\n",
                         pParameters->uartHandle);
                pParameters->atClientHandle = uAtClientAdd(pParameters->uartHandle,
                                                           U_AT_CLIENT_STREAM_TYPE_UART,
                                                           NULL,
                                                           U_SHORT_RANGE_AT_BUFFER_LENGTH_BYTES);
            }
        }
    }

    if (pParameters->atClientHandle != NULL) {
        // So that we can see what we're doing
        uAtClientTimeoutSet(pParameters->atClientHandle, 2000);
        uAtClientPrintAtSet(pParameters->atClientHandle, true);
        uAtClientDebugSet(pParameters->atClientHandle, true);
        if (uShortRangeInit() == 0) {
            uPortLog("U_SHORT_RANGE_TEST_PRIVATE: adding a short range instance on"
                     " the AT client...\n");
            pParameters->shortRangeHandle = uShortRangeAdd(moduleType,
                                                           pParameters->atClientHandle);
        }
    }

    if (pParameters->shortRangeHandle >= 0) {
        shortRangeHandle = pParameters->shortRangeHandle;
        uPortLog("U_SHORT_RANGE_TEST_PRIVATE: Detecting...\n");
        uShortRangeModuleType_t module = uShortRangeDetectModule(shortRangeHandle);

        if (module != U_SHORT_RANGE_MODULE_TYPE_INVALID) {
            errorCode = (int32_t) U_ERROR_COMMON_UNKNOWN;
            pModule = pUShortRangePrivateGetModule(shortRangeHandle);
            if (pModule != NULL) {
                uPortLog("U_SHORT_RANGE_TEST_PRIVATE: Module: %d\n", pModule->moduleType);
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            }

            if (errorCode == 0) {
                uPortLog("U_SHORT_RANGE_TEST_PRIVATE: module is powered-up and configured for testing.\n");
            }
        }
    }

    return errorCode;
}

/** The standard postamble for a short range test.
 */
void uShortRangeTestPrivatePostamble(uShortRangeTestPrivate_t *pParameters)
{
    uPortLog("U_SHORT_RANGE_TEST_PRIVATE: deinitialising short range API...\n");
    // Let uShortRangeDeinit() remove the short range handle
    uShortRangeDeinit();

    uPortLog("U_SHORT_RANGE_TEST_PRIVATE: removing AT client...\n");
    uAtClientRemove(pParameters->atClientHandle);
    uAtClientDeinit();

    if (pParameters->edmStreamHandle >= 0) {
        uShortRangeEdmStreamClose(pParameters->edmStreamHandle);
        uShortRangeEdmStreamDeinit();
        pParameters->edmStreamHandle = -1;
    }

    uPortUartClose(pParameters->uartHandle);
    pParameters->uartHandle = -1;

    uPortDeinit();
}

/** The standard clean-up for a short range test.
 */
void uShortRangeTestPrivateCleanup(uShortRangeTestPrivate_t *pParameters)
{
    uShortRangeDeinit();
    uAtClientDeinit();
    if (pParameters->edmStreamHandle >= 0) {
        uShortRangeEdmStreamClose(pParameters->edmStreamHandle);
        uShortRangeEdmStreamDeinit();
        pParameters->edmStreamHandle = -1;
    }

    if (pParameters->uartHandle >= 0) {
        uPortUartClose(pParameters->uartHandle);
    }
    pParameters->uartHandle = -1;
    uPortDeinit();
}

// End of file
