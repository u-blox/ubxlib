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
 * @brief Debug utilities.
 */

#include "stdint.h"    // int32_t etc.
#include "stddef.h"
#include "stdbool.h"

#include "u_cfg_os_platform_specific.h"
#include "u_error_common.h"
#include "u_port_os.h"
#include "u_port_debug.h"

#include "u_debug_utils.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifdef U_DEBUG_UTILS_DUMP_THREADS
# ifdef __ZEPHYR__
#  include "zephyr/u_dump_threads.c"
# else
// The thread dumper only supports FreeRTOS and Zephyr
// Zephyr can be detected but FreeRTOS cannot so instead we just assume
// that if it's not Zephyr it must be FreeRTOS - but to verify we include
// FreeRTOS.h. This means that if you are running on another OS you will
// get an error here since your OS then isn't supported.
#  ifdef ESP_PLATFORM
#   include "freertos/FreeRTOS.h"
#  else
#   include "FreeRTOS.h"
#  endif
#  include "freertos/u_dump_threads.c"
# endif
#endif

#ifndef U_DEBUG_UTILS_INACTIVITY_TASK_STACK_SIZE
/** The stack size for the inactivity task.
 */
# define U_DEBUG_UTILS_INACTIVITY_TASK_STACK_SIZE (1024 * 2)
#endif

#ifndef U_DEBUG_UTILS_INACTIVITY_TASK_PRIORITY
/** Since the inactivity task is used for detecting starvation the
 *  priority must be higher than the tasks causing the issue.
 */
# define U_DEBUG_UTILS_INACTIVITY_TASK_PRIORITY U_CFG_OS_PRIORITY_MAX
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** The handle of the inactivity task.
 */
static uPortTaskHandle_t gInactivityTaskHandle = NULL;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static void inactivityTask(void *pParam)
{
    int32_t lastActivityCounter = -1;
    int32_t inactiveCounter = 0;
    volatile  int32_t *pActivityCounter = (volatile  int32_t *)pParam;
    while (1) {
        uPortTaskBlock(1000 * U_DEBUG_UTILS_INACTIVITY_TASK_CHECK_PERIOD_SEC);
        if (*pActivityCounter == lastActivityCounter) {
            inactiveCounter++;
        } else {
            inactiveCounter = 0;
        }

        if (inactiveCounter == 1) {
            uPortLogF("### Inactivity Detected ###\n");
#ifdef U_DEBUG_UTILS_DUMP_THREADS
            uDebugUtilsDumpThreads();
#endif
        }
        lastActivityCounter = *pActivityCounter;
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

int32_t uDebugUtilsInitInactivityDetector(volatile int32_t *pActivityCounter)
{
    if (gInactivityTaskHandle != NULL) {
        return (int32_t) U_ERROR_COMMON_SUCCESS;
    }
    return uPortTaskCreate(inactivityTask,
                           "inactivity",
                           U_DEBUG_UTILS_INACTIVITY_TASK_STACK_SIZE,
                           (void *)pActivityCounter,
                           U_DEBUG_UTILS_INACTIVITY_TASK_PRIORITY,
                           &gInactivityTaskHandle);

}
