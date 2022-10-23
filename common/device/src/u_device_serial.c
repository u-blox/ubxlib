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
 * @brief Virtual serial device creation/deletion functions.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_interface.h"
#include "u_device_serial.h"

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

// Initialisation callback for pUInterfaceCreate().
static void init(uInterfaceTable_t pInterfaceTable, void *pInitParam)
{
    uDeviceSerialInit_t pInit = (uDeviceSerialInit_t) pInitParam;
    if (pInit != NULL) {
        pInit(pInterfaceTable);
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Create a serial interface.
uDeviceSerial_t *pUDeviceSerialCreate(uDeviceSerialInit_t pInit,
                                      size_t contextSize)
{
    return (uDeviceSerial_t *) pUInterfaceCreate(sizeof(uDeviceSerial_t),
                                                 contextSize,
                                                 init, (void *) pInit, NULL);
}

// Delete a serial interface.
void uDeviceSerialDelete(uDeviceSerial_t *pDeviceSerial)
{
    uInterfaceDelete(pDeviceSerial);
}


// End of file
