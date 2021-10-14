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
 * @brief Common stuff used in testing of the ble API.
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
#include "u_port_uart.h"

#include "u_at_client.h"

#include "u_short_range_module_type.h"
#include "u_short_range.h"
#include "u_short_range_edm_stream.h"

#include "u_ble_module_type.h"
#include "u_ble.h"

#include "u_ble_test_private.h"

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

// The standard preamble for a ble test.
int32_t uBleTestPrivatePreamble(uBleModuleType_t moduleType,
                                uBleTestPrivate_t *pParameters)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    int32_t bleHandle;

    // Set some defaults
    pParameters->uartHandle = -1;
    pParameters->edmStreamHandle = -1;
    pParameters->atClientHandle = NULL;
    pParameters->bleHandle = -1;

    // Initialise the porting layer
    if (uPortInit() == 0) {
#ifndef U_CFG_BLE_MODULE_INTERNAL
        uPortLog("U_BLE_TEST_PRIVATE: opening UART %d...\n",
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
#endif
    }

#ifndef U_CFG_BLE_MODULE_INTERNAL
    if (pParameters->uartHandle >= 0) {
        if (uShortRangeEdmStreamInit() == 0) {
            pParameters->edmStreamHandle = uShortRangeEdmStreamOpen(pParameters->uartHandle);
            if (pParameters->edmStreamHandle >= 0) {
                if (uAtClientInit() == 0) {
                    uPortLog("U_BLE_TEST_PRIVATE: adding an AT client on EDM...\n");
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
    }

    if (pParameters->atClientHandle != NULL) {
        // So that we can see what we're doing
        uAtClientTimeoutSet(pParameters->atClientHandle, 2000);
        uAtClientPrintAtSet(pParameters->atClientHandle, true);
        uAtClientDebugSet(pParameters->atClientHandle, true);
        if (uBleInit() == 0) {
            uPortLog("U_BLE_TEST_PRIVATE: adding a short range instance on"
                     " the AT client...\n");
            pParameters->bleHandle = uBleAdd(moduleType,
                                             pParameters->atClientHandle);
        }
    }
#else
    pParameters->bleHandle = uBleAdd(moduleType, NULL);
#endif

    if (pParameters->bleHandle >= 0) {
        bleHandle = pParameters->bleHandle;
        uPortLog("U_BLE_TEST_PRIVATE: Detecting...\n");
        uBleModuleType_t module = uBleDetectModule(bleHandle);

        if (module != U_BLE_MODULE_TYPE_INVALID) {
            uPortLog("U_BLE_TEST_PRIVATE: Module: %d\n", module);
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }

        if (errorCode == 0) {
            uPortLog("U_BLE_TEST_PRIVATE: module is connected and configured for testing.\n");
        }
    }

    return errorCode;
}

// The standard postamble for a ble test.
void uBleTestPrivatePostamble(uBleTestPrivate_t *pParameters)
{
    uPortLog("U_BLE_TEST_PRIVATE: deinitialising ble API...\n");
    uBleDeinit();

#ifndef U_CFG_BLE_MODULE_INTERNAL
    uPortLog("U_BLE_TEST_PRIVATE: removing AT client...\n");
    uShortRangeEdmStreamClose(pParameters->edmStreamHandle);
    uShortRangeEdmStreamDeinit();
    pParameters->edmStreamHandle = -1;

    uAtClientRemove(pParameters->atClientHandle);
    uAtClientDeinit();

    uPortUartClose(pParameters->uartHandle);
    pParameters->uartHandle = -1;
#else
    (void)pParameters;
#endif

    uPortDeinit();
}

// The standard clean-up for a ble test.
void uBleTestPrivateCleanup(uBleTestPrivate_t *pParameters)
{
    uBleDeinit();
    uShortRangeEdmStreamClose(pParameters->edmStreamHandle);
    pParameters->edmStreamHandle = -1;
    uAtClientDeinit();
    uPortUartClose(pParameters->uartHandle);
    pParameters->uartHandle = -1;
}

// End of file
