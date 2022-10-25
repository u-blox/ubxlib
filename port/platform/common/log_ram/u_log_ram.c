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
 * @brief The implementation of the RAM logging utility.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memcpy()/memset()

#include "u_cfg_sw.h"

#include "u_port.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_os.h"

#include "u_log_ram.h"
#include "u_log_ram_enum.h"
#include "u_log_ram_string.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** A pointer to the logging context data.
 */
static uLogRamContext_t *gpContext = NULL;

/** Keep track of whether we allocated gpContext.
 */
static bool gContextMalloced = false;

/** Mutex to arbitrate logging.
 */
static uPortMutexHandle_t gMutex = NULL;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Print a single item from a log.
static void printItem(const uLogRamEntry_t *pItem, size_t itemIndex)
{
    if (pItem->event > gULogRamNumStrings) {
        uPortLog("%10d: out of range event at entry %u (%u when max is %d).\n",
                 pItem->timestamp, itemIndex, pItem->event, gULogRamNumStrings);
    } else {
        uPortLog("%10d: [%3u] %s %d (%#x)\n",  pItem->timestamp,
                 pItem->event, gULogRamString[pItem->event],
                 pItem->parameter, pItem->parameter);
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise RAM logging.
bool uLogRamInit(void *pBuffer)
{
    bool success = false;
    bool freshStart = false;

    if (gMutex == NULL) {
        uPortMutexCreate(&gMutex);
    }

    if (gMutex != NULL) {
        if (pBuffer == NULL) {
            pBuffer = pUPortMalloc(U_LOG_RAM_STORE_SIZE);
            memset(pBuffer, 0, U_LOG_RAM_STORE_SIZE);
            gContextMalloced = true;
        }
        if (pBuffer != NULL) {
            gpContext = (uLogRamContext_t *) pBuffer;
        }
        if (gpContext != NULL) {
            // If the context is uninitialised, initialise it
            if ((gpContext->magicWord != 0x123456) ||
                (gpContext->version != U_LOG_RAM_VERSION)) {
                freshStart = true;
                memset(gpContext, 0, sizeof(*gpContext));
                gpContext->version = U_LOG_RAM_VERSION;
                gpContext->pLog = (uLogRamEntry_t * ) ((char *) pBuffer + sizeof(*gpContext));
                gpContext->pLogNextEmpty = gpContext->pLog;
                gpContext->pLogFirstFull = gpContext->pLog;
                gpContext->numLogItems = 0;
                gpContext->logEntriesOverwritten = 0;
                gpContext->lastLogTime = uPortGetTickTimeMs();
                gpContext->magicWord = 0x123456;
            }

            if (freshStart) {
                uLogRam(U_LOG_RAM_EVENT_START, U_LOG_RAM_VERSION);
            } else {
                uLogRam(U_LOG_RAM_EVENT_START_AGAIN, U_LOG_RAM_VERSION);
            }
            success = true;
        }
    }

    return success;
}

// Close down RAM logging.
void uLogRamDeinit()
{
    if ((gpContext != NULL) && (gMutex != NULL)) {

        U_PORT_MUTEX_LOCK(gMutex);

        uLogRam(U_LOG_RAM_EVENT_STOP, U_LOG_RAM_VERSION);
        if (gContextMalloced) {
            uPortFree(gpContext);
            // Only reset the context if we allocated
            // it otherwise leave it there so that it can
            // still be printed
            gpContext = NULL;
            gContextMalloced = false;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
        uPortMutexDelete(gMutex);
        gMutex = NULL;
    }
}

// Log an event plus parameter.
void uLogRam(uLogRamEvent_t event, int32_t parameter)
{
    int32_t timestamp = uPortGetTickTimeMs();

    if ((gpContext != NULL) && (gpContext->pLogNextEmpty)) {
        // Check if the timestamp has wrapped and
        // insert a log point before this one if that's the
        // case (coding gods: please excuse my recursion)
        if (timestamp < gpContext->lastLogTime) {
            gpContext->lastLogTime = timestamp;
            uLogRam(U_LOG_RAM_EVENT_TIME_WRAP, timestamp);
        }
        gpContext->lastLogTime = timestamp;
        gpContext->pLogNextEmpty->timestamp = timestamp;
        gpContext->pLogNextEmpty->event = (uint32_t) event;
        gpContext->pLogNextEmpty->parameter = parameter;
#if defined(U_LOG_RAM_PRINT) || defined(U_LOG_RAM_PRINT_ONLY)
        printItem(gpContext->pLogNextEmpty, 0);
#endif
#ifndef U_LOG_RAM_PRINT_ONLY
        if (gpContext->pLogNextEmpty < gpContext->pLog + U_LOG_RAM_ENTRIES_MAX_NUM - 1) {
            gpContext->pLogNextEmpty++;
        } else {
            gpContext->pLogNextEmpty = gpContext->pLog;
        }

        if (gpContext->pLogNextEmpty == gpContext->pLogFirstFull) {
            // Logging has wrapped, so move the
            // first pointer on to reflect the
            // overwrite
            if (gpContext->pLogFirstFull < gpContext->pLog + U_LOG_RAM_ENTRIES_MAX_NUM - 1) {
                gpContext->pLogFirstFull++;
            } else {
                gpContext->pLogFirstFull = gpContext->pLog;
            }
            gpContext->logEntriesOverwritten++;
        } else {
            gpContext->numLogItems++;
        }
#endif
    }
}

// Log an event plus parameter, this time with mutex protection.
void uLogRamX(uLogRamEvent_t event, int32_t parameter)
{
    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        uLogRam(event, parameter);

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
}

// Get the first N RAM log entries.
size_t uLogRamGet(uLogRamEntry_t *pEntries, size_t numEntries)
{
    const uLogRamEntry_t *pItem;
    size_t itemCount = 0;

    if ((gpContext != NULL) && (gMutex != NULL)) {

        U_PORT_MUTEX_LOCK(gMutex);

        pItem = gpContext->pLogFirstFull;
        while ((pItem != gpContext->pLogNextEmpty) &&
               (itemCount < numEntries)) {
            if (gpContext->logEntriesOverwritten > 0) {
                uLogRamEntry_t insert = {pItem->timestamp,
                                         U_LOG_RAM_EVENT_ENTRIES_OVERWRITTEN,
                                         (int32_t) gpContext->logEntriesOverwritten
                                        };
                memcpy(pEntries, &insert, sizeof(*pEntries));
                itemCount++;
                pEntries++;
                gpContext->logEntriesOverwritten = 0;
            }
            if (itemCount < numEntries) {
                memcpy(pEntries, pItem, sizeof(*pEntries));
                itemCount++;
                pEntries++;
                pItem++;
                if (gpContext->numLogItems > 0) {
                    gpContext->numLogItems--;
                }
                if (pItem >= gpContext->pLog + U_LOG_RAM_ENTRIES_MAX_NUM) {
                    pItem = gpContext->pLog;
                }
                gpContext->pLogFirstFull = pItem;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return itemCount;
}

// Get the number of log entries.
size_t uLogRamGetNumEntries()
{
    size_t numLogItems = 0;

    if ((gpContext != NULL) && (gMutex != NULL)) {

        U_PORT_MUTEX_LOCK(gMutex);

        numLogItems = gpContext->numLogItems;

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return numLogItems;
}

// Print out the log.
void uLogRamPrint()
{
    const uLogRamEntry_t *pItem;
    size_t x = 0;

    if (gpContext != NULL) {

        if (gMutex != NULL) {
            U_PORT_MUTEX_LOCK(gMutex);
        }

        uPortLog("------------- uLogRam starts -------------\n");
        // Print the log items from RAM
        pItem = gpContext->pLogFirstFull;
        while (pItem != gpContext->pLogNextEmpty) {
            printItem(pItem, x);
            x++;
            pItem++;
            if (pItem >= gpContext->pLog + U_LOG_RAM_ENTRIES_MAX_NUM) {
                pItem = gpContext->pLog;
            }
        }
        uPortLog("-------------- uLogRam ends --------------\n");

        if (gMutex != NULL) {
            U_PORT_MUTEX_UNLOCK(gMutex);
        }
    }
}

// End of file
