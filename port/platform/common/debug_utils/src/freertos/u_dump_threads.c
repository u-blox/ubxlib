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

/** @file
 * @brief Thread dumper for FreeRTOS.
 */

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"

#ifdef ESP_PLATFORM
# include "freertos/FreeRTOS.h"
# include "freertos/task.h"
// This code is not present in v4.3 ESP-IDF, hence we include a copy,
// otherwise we can just use their version
# include "esp_idf_version.h"
# if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 4, 0))
#  include "freertos/additions/task_snapshot.h"
# else
#  include "task_snapshot.h"
#endif
#else
# include "FreeRTOS.h"
# include "task.h"
# include "task_snapshot.h"
#endif


#include "u_port_debug.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifdef __arm__
# include "../arch/arm/u_stack_frame_cortex.c"
#elif defined(__XTENSA__)
# include "../arch/xtensa/u_stack_frame.c"
#else
# error "Unsupported architecture"
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static const char *stateName(eTaskState state)
{
    switch (state) {
        case eReady:
            return "READY";
        case eRunning:
            return "RUNNING";
        case eBlocked:
            return "BLOCKED";
        case eSuspended:
            return "SUSPENDED";
        case eDeleted:
            return "DELETED";
        default:
            break;
    }
    return "UNKNOWN";
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

void uDebugUtilsDumpThreads(void)
{
    TaskHandle_t iter = pxTaskGetNext(NULL);
    uPortLogF("### Dumping threads ###\n");
    while (iter != NULL) {
        TaskSnapshot_t snapshot;
        eTaskState state;
        char *pName;
        vTaskGetSnapshot(iter, &snapshot);
        pName = pcTaskGetName(iter);
        state = eTaskGetState(iter);
        uPortLogF("  %s (%s): ", pName, stateName(state));
        uPortLogF("top: %08x, sp: %08x\n",
                  (unsigned int)snapshot.pxEndOfStack, (unsigned int)snapshot.pxTopOfStack);
        uPortLogF("    ");
        uDebugUtilsPrintCallStack((uint32_t)snapshot.pxTopOfStack, (uint32_t)snapshot.pxEndOfStack, 8);

        iter = pxTaskGetNext(iter);
    }
}
