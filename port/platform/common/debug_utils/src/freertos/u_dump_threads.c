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

/** @file
 * @brief Thread dumper for FreeRTOS.
 */

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"

#include "FreeRTOS.h"
#include "task.h"

#include "u_port_debug.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifdef __arm__
# include "../arch/arm/u_print_callstack_cortex.c"
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
    TaskStatus_t *pxTaskStatusArray;
    volatile UBaseType_t uxArraySize;
    unsigned long ulTotalRunTime;

    uxArraySize = uxTaskGetNumberOfTasks();

    pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));
    if (pxTaskStatusArray != NULL) {
        /* Generate raw status information about each task. */
        uxArraySize = uxTaskGetSystemState(pxTaskStatusArray,
                                           uxArraySize,
                                           &ulTotalRunTime);

        uPortLogF("### Dumping threads ###\n");
        for (int x = 0; x < uxArraySize; x++) {
            TaskStatus_t *pStat = &pxTaskStatusArray[x];
            uint32_t *pTcbData = (uint32_t *)pStat->xHandle;
            uint32_t *pSp = (uint32_t *)pTcbData[0];
            uint32_t *pStackTop = 0;
            uint32_t stackBottom = (uint32_t)pStat->pxStackBase;

            uPortLogF("  %s (%s): ", pStat->pcTaskName, stateName(pStat->eCurrentState));

            // Next we need to find pxEndOfStack in the TCB
            // Since there are no out of box API to get the stack top we use a hacky
            // way and search for it in the TCB region.
            // We first search for stack bottom (this value is retreived from uxTaskGetSystemState).
            // When this has been found we look for the task name and assume that pxEndOfStack
            // is located somwhere near after this position.
            //
            // Again hacky... but in this way we don't need to patch FreeRTOS
#if !configRECORD_STACK_HIGH_ADDRESS
# error configRECORD_STACK_HIGH_ADDRESS must be enabled for the thread dumper to work
#endif
            for (int i = 0; i < 64; i++) {
                if (pTcbData[i] == stackBottom) {
                    size_t nameLen = strlen(pStat->pcTaskName);
                    i++;
                    if ((nameLen > 0) && (memcmp(&pTcbData[i], pStat->pcTaskName, nameLen) == 0)) {
                        i += ((configMAX_TASK_NAME_LEN + 3) / 4);
                    }
                    for (int j = 0; j < 8; j++) {
                        if ((pTcbData[i + j] > stackBottom) && (pTcbData[i + j] < (stackBottom + (1024 * 128)))) {
                            pStackTop = (uint32_t *)pTcbData[i + j];
                            break;
                        }
                    }
                    break;
                }
            }
            uPortLogF("base: %08x, top: %08x, sp: %08x\n",
                      (unsigned int)stackBottom, (unsigned int)pStackTop, (unsigned int)pSp);
            uPortLogF("    ");
            uDebugUtilsPrintCallStack(pSp, pStackTop, 8);
        }

        vPortFree(pxTaskStatusArray);
    }
}
