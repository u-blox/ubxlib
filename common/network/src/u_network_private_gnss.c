/*
 * Copyright 2022 u-blox
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
 * @brief Implementation of the GNSS portion of the network API. The
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

#include "u_cfg_os_platform_specific.h" // For U_CFG_OS_CLIB_LEAKS

#include "u_error_common.h"

#include "u_port_uart.h"

#include "u_at_client.h"

#include "u_cell_module_type.h"
#include "u_cell.h"                 // For uCellAtClientHandleGet()
#include "u_cell_loc.h"             // For uCellLocSetPinGnssPwr()/uCellLocSetPinGnssDataReady()

#include "u_short_range_module_type.h"
#include "u_short_range.h"          // For uShortRangeAtClientHandleGet()

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss.h"
#include "u_gnss_pwr.h"

#include "u_network.h"
#include "u_network_config_gnss.h"
#include "u_network_private_gnss.h"

#include "u_device.h"
#include "u_device_shared.h"
#include "u_device_shared_gnss.h"

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

// Bring a GNSS interface up or take it down.
int32_t uNetworkPrivateChangeStateGnss(uDeviceHandle_t devHandle,
                                       const uNetworkCfgGnss_t *pCfg,
                                       bool upNotDown)
{
    uDeviceGnssInstance_t *pContext;
    uDeviceInstance_t *pDevInstance;
    int32_t errorCode = uDeviceGetInstance(devHandle, &pDevInstance);

    if (errorCode == 0) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pContext = (uDeviceGnssInstance_t *) pDevInstance->pContext;
        if ((pCfg != NULL) && (pCfg->version == 0) &&
            (pCfg->type == U_NETWORK_TYPE_GNSS) && (pContext != NULL)) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            // If the GNSS chip was directly connected to this MCU
            // then it would have been powered up by the device API.
            // If it is used over an AT interface then it will have
            // been left powered off so that Cell Locate could use it,
            // hence we need to manage the power here.
            if (pContext->transportType == (int32_t) U_GNSS_TRANSPORT_UBX_AT) {
                if (upNotDown) {
                    errorCode = uGnssPwrOn(devHandle);
                } else {
                    errorCode = uGnssPwrOff(devHandle);
                }
            }
        }
    }

    return errorCode;
}

// End of file
