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
 * @brief Implementation of the cellular portion of the network API. The
 * contents of this file aren't any more "private" than the other
 * sources files but the associated header file should be private and
 * this is simply named to match.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_uart.h"

#include "u_at_client.h"

#include "u_device.h"
#include "u_device_shared.h"
#include "u_device_shared_cell.h"

#include "u_cell_module_type.h"
#include "u_cell.h"
#include "u_cell_net.h"
#include "u_cell_pwr.h"

#include "u_network.h"
#include "u_network_config_cell.h"
#include "u_network_private_cell.h"

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

// Call-back for connect/disconnect timeout.
static bool keepGoingCallback(uDeviceHandle_t devHandle)
{
    uDeviceCellContext_t *pContext;
    uDeviceInstance_t *pDevInstance = NULL;
    bool keepGoing = false;

    if (uDeviceGetInstance(devHandle, &pDevInstance) == 0) {
        pContext = (uDeviceCellContext_t *) pDevInstance->pContext;
        if ((pContext == NULL) ||
            (uPortGetTickTimeMs() < pContext->stopTimeMs)) {
            keepGoing = true;
        }
    }

    return keepGoing;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Bring a cellular interface up or take it down.
int32_t uNetworkPrivateChangeStateCell(uDeviceHandle_t devHandle,
                                       const uNetworkCfgCell_t *pCfg,
                                       bool upNotDown)
{
    uDeviceCellContext_t *pContext;
    uDeviceInstance_t *pDevInstance;
    int32_t errorCode = uDeviceGetInstance(devHandle, &pDevInstance);

    if (errorCode == 0) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pContext = (uDeviceCellContext_t *) pDevInstance->pContext;
        if ((pCfg != NULL) && (pCfg->version == 0) &&
            (pCfg->type == U_NETWORK_TYPE_CELL) && (pContext != NULL)) {
            // Set the stop time for the connect/disconnect calls
            pContext->stopTimeMs = uPortGetTickTimeMs() +
                                   (((int64_t) pCfg->timeoutSeconds) * 1000);
            if (upNotDown) {
                // Connect using automatic selection,
                // default no user name or password for the APN
                errorCode = uCellNetConnect(devHandle, NULL,
                                            pCfg->pApn,
                                            NULL, NULL,
                                            keepGoingCallback);
            } else {
                // Disconnect
                errorCode = uCellNetDisconnect(devHandle, keepGoingCallback);
            }
        }
    }

    return errorCode;
}

// End of file
