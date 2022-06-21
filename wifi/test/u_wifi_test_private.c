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
 * @brief Common stuff used in testing of the wifi API.
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
#include "u_cfg_os_platform_specific.h"  // For #define U_CFG_OS_CLIB_LEAKS

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_uart.h"

#include "u_at_client.h"
#include "u_short_range_module_type.h"
#include "u_short_range.h"

#include "u_wifi_module_type.h"
#include "u_wifi.h"

#include "u_wifi_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_WIFI_TEST_PRIVATE: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

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

// The standard preamble for a wifi test.
int32_t uWifiTestPrivatePreamble(uWifiModuleType_t moduleType,
                                 const uShortRangeUartConfig_t *pUartConfig,
                                 uWifiTestPrivate_t *pParameters)
{
    int32_t errorCodeOrHandle = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    const uShortRangeModuleInfo_t *pModule;

    // Set some defaults
    pParameters->uartHandle = -1;
    pParameters->edmStreamHandle = -1;
    pParameters->atClientHandle = NULL;
    pParameters->devHandle = NULL;

    // Initialise the porting layer and wifi
    if ((uPortInit() == 0) && (uWifiInit() == 0) && (uAtClientInit() == 0)) {
        uDeviceHandle_t devHandle = NULL;
        U_TEST_PRINT_LINE("opening UART %d...", U_CFG_APP_SHORT_RANGE_UART);

        errorCodeOrHandle = uShortRangeOpenUart((uShortRangeModuleType_t)moduleType, pUartConfig,
                                                true, &devHandle);

        if (errorCodeOrHandle >= (int32_t) U_ERROR_COMMON_SUCCESS) {
            errorCodeOrHandle = uShortRangeGetUartHandle(devHandle);
            pParameters->uartHandle = errorCodeOrHandle;
        }


        if (errorCodeOrHandle >= (int32_t) U_ERROR_COMMON_SUCCESS) {
            errorCodeOrHandle = uShortRangeGetEdmStreamHandle(devHandle);
            pParameters->edmStreamHandle = errorCodeOrHandle;
        }

        if (errorCodeOrHandle >= (int32_t) U_ERROR_COMMON_SUCCESS) {
            errorCodeOrHandle = uShortRangeAtClientHandleGet(devHandle, &pParameters->atClientHandle);

            if (errorCodeOrHandle >= (int32_t) U_ERROR_COMMON_SUCCESS) {
                // So that we can see what we're doing
                uAtClientTimeoutSet(pParameters->atClientHandle, 2000);
                uAtClientPrintAtSet(pParameters->atClientHandle, true);
                uAtClientDebugSet(pParameters->atClientHandle, true);
            }
        }

        if (errorCodeOrHandle >= (int32_t) U_ERROR_COMMON_SUCCESS) {
            if ((uShortRangeModuleType_t) moduleType != U_SHORT_RANGE_MODULE_TYPE_INVALID) {
                errorCodeOrHandle = (int32_t) U_ERROR_COMMON_UNKNOWN;
                pModule = uShortRangeGetModuleInfo((uShortRangeModuleType_t)moduleType);
                if (pModule != NULL) {
                    U_TEST_PRINT_LINE("module: %d.", pModule->moduleType);
                    errorCodeOrHandle = (int32_t) U_ERROR_COMMON_SUCCESS;
                }

                if (errorCodeOrHandle == 0) {
                    pParameters->devHandle = devHandle;
                    U_TEST_PRINT_LINE("module is powered-up and configured for testing.");
                }
            }
        }
    }

    return errorCodeOrHandle;
}

// The standard postamble for a wifi test.
void uWifiTestPrivatePostamble(uWifiTestPrivate_t *pParameters)
{
    uShortRangeClose(pParameters->devHandle);
    pParameters->uartHandle = -1;
    pParameters->edmStreamHandle = -1;
    pParameters->atClientHandle = NULL;
    pParameters->devHandle = NULL;

    uWifiDeinit();
    uAtClientDeinit();
    uPortDeinit();
}

// The standard clean-up for a wifi test.
void uWifiTestPrivateCleanup(uWifiTestPrivate_t *pParameters)
{
    uShortRangeClose(pParameters->devHandle);
    pParameters->uartHandle = -1;
    pParameters->edmStreamHandle = -1;
    pParameters->atClientHandle = NULL;
    pParameters->devHandle = NULL;

    uWifiDeinit();
    uAtClientDeinit();
    uPortDeinit();
}

// End of file
