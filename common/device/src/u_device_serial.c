/*
 * Copyright 2019-2023 u-blox
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

#include "u_error_common.h"

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
 * STATIC FUNCTIONS: DEFAULT IMPLEMENTATIONS
 * -------------------------------------------------------------- */

static int32_t serialDefaultOpen(struct uDeviceSerial_t *pDeviceSerial,
                                 void *pReceiveBuffer,
                                 size_t receiveBufferSizeBytes)
{
    (void) pDeviceSerial;
    (void) pReceiveBuffer;
    (void) receiveBufferSizeBytes;
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

static int32_t serialDefaultRead(struct uDeviceSerial_t *pDeviceSerial,
                                 void *pBuffer, size_t sizeBytes)
{
    (void) pDeviceSerial;
    (void) pBuffer;
    (void) sizeBytes;
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

static int32_t serialDefaultWrite(struct uDeviceSerial_t *pDeviceSerial,
                                  const void *pBuffer, size_t sizeBytes)
{
    (void) pDeviceSerial;
    (void) pBuffer;
    (void) sizeBytes;
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

static int32_t serialDefaultEventCallbackSet(struct uDeviceSerial_t *pDeviceSerial,
                                             uint32_t filter,
                                             void (*pFunction)(struct uDeviceSerial_t *,
                                                               uint32_t,
                                                               void *),
                                             void *pParam,
                                             size_t stackSizeBytes,
                                             int32_t priority)
{
    (void) pDeviceSerial;
    (void) filter;
    (void) pFunction;
    (void) pParam;
    (void) stackSizeBytes;
    (void) priority;
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

static uint32_t serialDefaultEventCallbackFilterGet(struct uDeviceSerial_t *pDeviceSerial)
{
    (void) pDeviceSerial;
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

static int32_t serialDefaultEventTrySend(struct uDeviceSerial_t *pDeviceSerial,
                                         uint32_t eventBitMap, int32_t delayMs)
{
    (void) pDeviceSerial;
    (void) eventBitMap;
    (void) delayMs;
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

static int32_t serialDefaultDiscardOnOverflow(struct uDeviceSerial_t *pDeviceSerial,
                                              bool onNotOff)
{
    (void) pDeviceSerial;
    (void) onNotOff;
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

static void serialDefaultVoid(struct uDeviceSerial_t *pDeviceSerial)
{
    (void) pDeviceSerial;
}

static int32_t serialDefaultInt32(struct uDeviceSerial_t *pDeviceSerial)
{
    (void) pDeviceSerial;
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

static bool serialDefaultBool(struct uDeviceSerial_t *pDeviceSerial)
{
    (void) pDeviceSerial;
    return false;
}

static int32_t serialDefaultInt32Filter(struct uDeviceSerial_t *pDeviceSerial,
                                        uint32_t filter)
{
    (void) pDeviceSerial;
    (void) filter;
    return (int32_t) U_ERROR_COMMON_NOT_IMPLEMENTED;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: INITIALISATION
 * -------------------------------------------------------------- */

// Initialisation callback for pUInterfaceCreate().
static void init(uInterfaceTable_t pInterfaceTable, void *pInitParam)
{
    uDeviceSerialInit_t pInit = (uDeviceSerialInit_t) pInitParam;
    uDeviceSerial_t *pDeviceSerial = (uDeviceSerial_t *) pInterfaceTable;

    pDeviceSerial->open = serialDefaultOpen;
    pDeviceSerial->close = serialDefaultVoid;
    pDeviceSerial->getReceiveSize = serialDefaultInt32;
    pDeviceSerial->read = serialDefaultRead;
    pDeviceSerial->write = serialDefaultWrite;
    pDeviceSerial->eventCallbackSet = serialDefaultEventCallbackSet;
    pDeviceSerial->eventCallbackRemove = serialDefaultVoid;
    pDeviceSerial->eventCallbackFilterGet = serialDefaultEventCallbackFilterGet;
    pDeviceSerial->eventCallbackFilterSet = serialDefaultInt32Filter;
    pDeviceSerial->eventSend = serialDefaultInt32Filter;
    pDeviceSerial->eventTrySend = serialDefaultEventTrySend;
    pDeviceSerial->eventIsCallback = serialDefaultBool;
    pDeviceSerial->eventStackMinFree = serialDefaultInt32;
    pDeviceSerial->isRtsFlowControlEnabled = serialDefaultBool;
    pDeviceSerial->isCtsFlowControlEnabled = serialDefaultBool;
    pDeviceSerial->ctsSuspend = serialDefaultInt32;
    pDeviceSerial->ctsResume = serialDefaultVoid;
    pDeviceSerial->discardOnOverflow = serialDefaultDiscardOnOverflow;
    pDeviceSerial->isDiscardOnOverflowEnabled = serialDefaultBool;

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
                                                 U_DEVICE_SERIAL_VERSION,
                                                 init, (void *) pInit, NULL);
}

// Delete a serial interface.
void uDeviceSerialDelete(uDeviceSerial_t *pDeviceSerial)
{
    uInterfaceDelete(pDeviceSerial);
}


// End of file
