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

#ifndef _U_LOG_RAM_H_
#define _U_LOG_RAM_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "stdint.h"
#include "stdbool.h"

#include "u_log_ram_enum.h"

/** @file
 * @brief This logging utility allows events to be logged to RAM at minimal
 * run-time cost.  Each entry includes an event, a 32 bit parameter (which
 * is printed with the event) and a millisecond time-stamp.  This code
 * is not multithreaded in that there can only be a single log buffer
 * at any one time, however the functions, aside from uLogRam(),
 * are mutex-protected.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The number of log entries (must be 1 or greater).
 */
#ifndef U_LOG_RAM_ENTRIES_MAX_NUM
# define U_LOG_RAM_ENTRIES_MAX_NUM 500
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** An entry in the log.
 */
typedef struct {
    int32_t timestamp;
    uint32_t event; // This will be #uLogRamEvent_t but it is stored as an int
    // so that we are guaranteed to get a 32-bit value,
    // making it easier to decode logs on another platform
    int32_t parameter;
} uLogRamEntry_t;


/** Type used to store logging context data.
 */
typedef struct {
    uint32_t magicWord;
    int32_t version;
    uLogRamEntry_t *pLog;
    uLogRamEntry_t *pLogNextEmpty;
    uLogRamEntry_t const *pLogFirstFull;
    size_t numLogItems;
    size_t logEntriesOverwritten;
    int32_t lastLogTime;
} uLogRamContext_t;

/** The size of the log store, given the number of entries requested.
 */
#define U_LOG_RAM_STORE_SIZE (sizeof(uLogRamContext_t) + (sizeof(uLogRamEntry_t) * U_LOG_RAM_ENTRIES_MAX_NUM))

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Initialise RAM logging.
 *
 * @param pBuffer    must point to #U_LOG_RAM_STORE_SIZE bytes
 *                   of storage.  If pBuffer is in RAM which is not
 *                   initialised at a reset then logging to RAM will
 *                   also survive across a reset.  If pBuffer
 *                   is NULL then memory will be allocated for the
 *                   log and will be free'ed on deinitialisation.
 * @return           true if successful, else false.
 */
bool uLogRamInit(void *pBuffer);

/** Close down logging.
 */
void uLogRamDeinit();

/** Log an event plus parameter to RAM.
 *
 * @param event     the event.
 * @param parameter the parameter.
 */
void uLogRam(uLogRamEvent_t event, int32_t parameter);

/** Log an event plus parameter to RAM, employing a mutex to protect the
 * log contents.  This will take longer, potentially a lot longer,
 * than uLogRam() so call this only in applications where you don't
 * care about speed.
 *
 * @param event     the event.
 * @param parameter the parameter.
 */
void uLogRamX(uLogRamEvent_t event, int32_t parameter);

/** Get the first N log entries that are in RAM, removing
 * them from the log storage.
 *
 * @param pEntries   a pointer to the place to store the entries.
 * @param numEntries the number of entries pointed to by pEntries.
 * @return           the number of entries returned.
 */
size_t uLogRamGet(uLogRamEntry_t *pEntries, size_t  numEntries);

/** Get the number of log entries currently in RAM.
 */
size_t uLogRamGetNumEntries();

/** Print out the currently logged items.
 */
void uLogRamPrint();

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_LOG_RAM_H_

// End of file
