/*
 * Copyright 2019-2024 u-blox
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
 * @brief Implementation of the cfg API for Wifi.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // strol(), atoi(), strol(), strtof()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"

#include "u_port_os.h"

#include "u_timeout.h"

#include "u_at_client.h"

#include "u_short_range_module_type.h"
#include "u_short_range.h"
#include "u_short_range_private.h"
#include "u_wifi_module_type.h"
#include "u_sock.h"
#include "u_wifi_cfg.h"

#include "u_cx_urc.h"
#include "u_cx_system.h"
#include "u_cx_wifi.h"

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
int32_t uWifiCfgConfigure(uDeviceHandle_t devHandle,
                          const uWifiCfg_t *pCfg)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    uCxHandle_t *pUcxHandle = pShortRangePrivateGetUcxHandle(devHandle);
    if (pUcxHandle != NULL) {
        uSockIpAddress_t ipAddr, subnetMask, gateway, primDns, secDns;
        uCxStringToIpAddress((const char *)pCfg->wifiIpCfg.IPv4Addr, &ipAddr);
        uCxStringToIpAddress((const char *)pCfg->wifiIpCfg.subnetMask, &subnetMask);
        uCxStringToIpAddress((const char *)pCfg->wifiIpCfg.defaultGW, &gateway);
        uCxStringToIpAddress((const char *)pCfg->wifiIpCfg.DNS1, &primDns);
        uCxStringToIpAddress((const char *)pCfg->wifiIpCfg.DNS2, &secDns);
        errorCode = uCxWifiStationSetIpConfigStatic6(pUcxHandle, 0, &ipAddr, &subnetMask, &gateway,
                                                     &primDns, &secDns);
    }
    return errorCode;
}

// End of file
