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

#include "u_port_os.h"
#include "u_port_heap.h"

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

/** Hook for a FIFO of Wifi-based location requests.
 */
static uLocationSharedFifoEntry_t *gpLocationWifiFifo = NULL;

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
            while ((pEntry = pULocationSharedRequestPop((uLocationSharedFifo_t) x)) != NULL) {
                uLocationSharedFifoEntryFree(pEntry);
            }
        }
        U_PORT_MUTEX_UNLOCK(gULocationMutex);
        uPortMutexDelete(gULocationMutex);
        gULocationMutex = NULL;
    }
}

// Add a new location request to the FIFO.
int32_t uLocationSharedRequestPush(uDeviceHandle_t devHandle,
                                   uLocationSharedFifo_t fifo,
                                   uLocationType_t type,
                                   int32_t desiredRateMs,
                                   const uLocationSharedWifiSettings_t *pWifiSettings,
                                   void (*pCallback) (uDeviceHandle_t devHandle,
                                                      int32_t errorCode,
                                                      const uLocation_t *pLocation))
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uLocationSharedFifoEntry_t **ppThis = NULL;
    uLocationSharedFifoEntry_t *pSaved = NULL;

    switch (fifo) {
        case U_LOCATION_SHARED_FIFO_GNSS:
            ppThis = &gpLocationGnssFifo;
            pSaved = gpLocationGnssFifo;
            break;
        case U_LOCATION_SHARED_FIFO_CELL_LOCATE:
            ppThis = &gpLocationCellLocateFifo;
            pSaved = gpLocationCellLocateFifo;
            break;
        case U_LOCATION_SHARED_FIFO_WIFI:
            ppThis = &gpLocationWifiFifo;
            pSaved = gpLocationWifiFifo;
            break;
        case U_LOCATION_SHARED_FIFO_NONE:
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
            (*ppThis)->desiredRateMs = desiredRateMs;
            (*ppThis)->type = type;
            (*ppThis)->pWifiSettings = pWifiSettings;
            (*ppThis)->pCallback = pCallback;
            (*ppThis)->pNext = pSaved;
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
    }

    return errorCode;
}

// Get the oldest location request from the given FIFO.
uLocationSharedFifoEntry_t *pULocationSharedRequestPop(uLocationSharedFifo_t fifo)
{
    uLocationSharedFifoEntry_t **ppThis = NULL;
    uLocationSharedFifoEntry_t *pSaved = NULL;
    uLocationSharedFifoEntry_t *pPrevious = NULL;
    size_t x = 0;

    switch (fifo) {
        case U_LOCATION_SHARED_FIFO_GNSS:
            ppThis = &gpLocationGnssFifo;
            break;
        case U_LOCATION_SHARED_FIFO_CELL_LOCATE:
            ppThis = &gpLocationCellLocateFifo;
            break;
        case U_LOCATION_SHARED_FIFO_WIFI:
            ppThis = &gpLocationWifiFifo;
            break;
        case U_LOCATION_SHARED_FIFO_NONE:
        // fall-through
        default:
            break;
    }

    if (ppThis != NULL) {
        // Find the end of the list
        while (*ppThis != NULL) {
            x++;
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

// Free a FIFO entry.
void uLocationSharedFifoEntryFree(uLocationSharedFifoEntry_t *pFifoEntry)
{
    if (pFifoEntry != NULL) {
        uPortFree((void *) pFifoEntry->pWifiSettings);
        uPortFree(pFifoEntry);
    }
}

// End of file
