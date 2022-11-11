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
 * @brief Implementation of the internal location API that is shared
 * with the network API; not intended to be part of the main location
 * API.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"

#include "u_port_heap.h"
#include "u_port_os.h"

#include "u_location.h"
#include "u_location_shared.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * SHARED VARIABLES
 * -------------------------------------------------------------- */

/** Mutex to protect the FIFO.
 */
uPortMutexHandle_t gULocationMutex = NULL;

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/** Hook for a FIFO of GNSS location requests.
 */
static uLocationSharedFifoEntry_t *gpLocationGnssFifo = NULL;

/** Hook for a FIFO of Cell Locate location requests.
 */
static uLocationSharedFifoEntry_t *gpLocationCellLocateFifo = NULL;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise the internally shared location API.
int32_t uLocationSharedInit()
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

    if (gULocationMutex == NULL) {
        errorCode = uPortMutexCreate(&gULocationMutex);
    }

    return errorCode;
}

// De-initialise the internally shared location API:.
void uLocationSharedDeinit()
{
    uLocationSharedFifoEntry_t *pEntry;

    if (gULocationMutex != NULL) {
        // Free anything in any FIFO
        U_PORT_MUTEX_LOCK(gULocationMutex);
        for (int32_t x = (int32_t) U_LOCATION_TYPE_GNSS;
             x < (int32_t) U_LOCATION_TYPE_MAX_NUM;
             x++) {
            while ((pEntry = pULocationSharedRequestPop((uLocationType_t) x)) != NULL) {
                uPortFree(pEntry);
            }
        }
        U_PORT_MUTEX_UNLOCK(gULocationMutex);
        uPortMutexDelete(gULocationMutex);
        gULocationMutex = NULL;
    }
}

// Add a new location request to the FIFO.
int32_t uLocationSharedRequestPush(uDeviceHandle_t devHandle,
                                   uLocationType_t type,
                                   void (*pCallback) (uDeviceHandle_t devHandle,
                                                      int32_t errorCode,
                                                      const uLocation_t *pLocation))
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uLocationSharedFifoEntry_t **ppThis = NULL;
    uLocationSharedFifoEntry_t *pSaved = NULL;

    switch (type) {
        case U_LOCATION_TYPE_GNSS:
            ppThis = &gpLocationGnssFifo;
            pSaved = gpLocationGnssFifo;
            break;
        case U_LOCATION_TYPE_CLOUD_CELL_LOCATE:
            ppThis = &gpLocationCellLocateFifo;
            pSaved = gpLocationCellLocateFifo;
            break;
        case U_LOCATION_TYPE_CLOUD_GOOGLE:
        //lint -fallthrough
        case U_LOCATION_TYPE_CLOUD_SKYHOOK:
        //lint -fallthrough
        case U_LOCATION_TYPE_CLOUD_HERE:
        //lint -fallthrough
        case U_LOCATION_TYPE_NONE:
        //lint -fallthrough
        default:
            break;
    }

    if (ppThis != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
        // Add the new entry at the start of the list
        *ppThis = (uLocationSharedFifoEntry_t *) pUPortMalloc(sizeof(**ppThis));
        if (*ppThis != NULL) {
            (*ppThis)->devHandle = devHandle;
            (*ppThis)->pCallback = pCallback;
            (*ppThis)->pNext = pSaved;
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
    }

    return errorCode;
}

// Get the oldest location request from the given FIFO.
uLocationSharedFifoEntry_t *pULocationSharedRequestPop(uLocationType_t type)
{
    uLocationSharedFifoEntry_t **ppThis = NULL;
    uLocationSharedFifoEntry_t *pSaved = NULL;
    uLocationSharedFifoEntry_t *pPrevious = NULL;

    switch (type) {
        case U_LOCATION_TYPE_GNSS:
            ppThis = &gpLocationGnssFifo;
            break;
        case U_LOCATION_TYPE_CLOUD_CELL_LOCATE:
            ppThis = &gpLocationCellLocateFifo;
            break;
        case U_LOCATION_TYPE_CLOUD_GOOGLE:
        //lint -fallthrough
        case U_LOCATION_TYPE_CLOUD_SKYHOOK:
        //lint -fallthrough
        case U_LOCATION_TYPE_CLOUD_HERE:
        //lint -fallthrough
        case U_LOCATION_TYPE_NONE:
        //lint -fallthrough
        default:
            break;
    }

    if (ppThis != NULL) {
        // Find the end of the list
        while (*ppThis != NULL) {
            pPrevious = pSaved;
            pSaved = *ppThis;
            *ppThis = (*ppThis)->pNext;
        }

        // Remove the entry from the list
        if (pPrevious != NULL) {
            pPrevious->pNext = NULL;
        } else {
            // Must be at the head
            *ppThis = NULL;
        }
    }

    return pSaved;
}

// End of file
