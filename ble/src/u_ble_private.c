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
 * @brief Implementation of the "general" API for ble.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "string.h"
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_ble_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PROTOTYPES
 * -------------------------------------------------------------- */
static void intToHex(const uint8_t in, char *pOut);

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */
static void intToHex(const uint8_t in, char *pOut)
{
    uint32_t i;

    for (i = 0; i < 2; i++) {
        uint8_t nibble = (in >> ((1 - i) * 4)) & 0xf;
        if (nibble < 10) {
            *pOut++ = (char)('0' + nibble);
        } else {
            *pOut++ = (char)('A' + nibble - 10);
        }
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

void addrArrayToString(const uint8_t *pAddrIn, uPortBtLeAddressType_t addrType, bool msbLast,
                       char *pAddrOut)
{
    uint32_t i;

    for (i = 0; i < 6; i++) {
        uint32_t byteIndex;
        if (msbLast) {
            byteIndex = 5 - i;
        } else {
            byteIndex = i;
        }
        intToHex(*(pAddrIn + byteIndex), pAddrOut);
        pAddrOut += 2;
    }
    switch (addrType) {

        case U_PORT_BT_LE_ADDRESS_TYPE_RANDOM:
            *pAddrOut++ = 'r';
            break;

        case U_PORT_BT_LE_ADDRESS_TYPE_PUBLIC:
            *pAddrOut++ = 'p';
            break;

        case U_PORT_BT_LE_ADDRESS_TYPE_UNKNOWN:
            *pAddrOut++ = '\0';
            break;
    }
    *pAddrOut = '\0';
}

// End of file
