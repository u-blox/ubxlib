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

/** @file
 * @brief  Functions to check for leakage of heap, OS resources (tasks etc.)
 * and transports (UARTs etc.).
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_uart.h"
#include "u_port_i2c.h"
#include "u_port_spi.h"

#ifdef U_CFG_TEST_ENABLE_INACTIVITY_DETECTOR
#include "u_debug_utils.h"
#endif
#ifdef U_CFG_MUTEX_DEBUG
#include "u_mutex_debug.h"
#endif

#include "u_test_util_resource_check.h"

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

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Get the current number of resources in use that might be freed.
int32_t uTestUtilGetDynamicResourceCount()
{
    int32_t resources = 0;
    int32_t x;
    int32_t y;

    x = uPortHeapAllocCount();
    y = uPortHeapPerpetualAllocCount();
    if (x > 0) {
        resources += x;
        if (y > x) {
            y = x;
        }
        resources -= y;
    }
    x = uPortOsResourceAllocCount();
    y = uPortOsResourcePerpetualCount();
    if (x > 0) {
        resources += x;
        if (y > x) {
            y = x;
        }
        resources -= y;
    }
    x = uPortUartResourceAllocCount();
    if (x > 0) {
        resources += x;
    }
    x = uPortI2cResourceAllocCount();
    if (x > 0) {
        resources += x;
    }
    x = uPortSpiResourceAllocCount();
    if (x > 0) {
        resources += x;
    }

    return resources;
}

// Check that resources are within limits and have been cleaned up.
bool uTestUtilResourceCheck(const char *pPrefix,
                            const char *pErrorMarker,
                            bool printIt)
{
    bool resourcesClean = true;
    int32_t x;
    int32_t osShouldBeOutstanding = uPortOsResourcePerpetualCount();
    int32_t heapShouldBeOutstanding = uPortHeapPerpetualAllocCount();

    if (pPrefix == NULL) {
        pPrefix = "";
    }

    if (pErrorMarker == NULL) {
        pErrorMarker = "";
    }

    // Check main task stack against our limit
    x = uPortTaskStackMinFree(NULL);
    if (x != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        if (x < U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES) {
            resourcesClean = false;
        }
        if (printIt) {
            uPortLog("%s%smain task stack had a minimum of %d byte(s) free"
                     " (minimum is %d).\n", pPrefix,
                     resourcesClean ? "" : pErrorMarker,
                     x, U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);
        }
    }

    // Check all-time heap usage against our limit
    x = uPortGetHeapMinFree();
    if (x >= 0) {
        if (x < U_CFG_TEST_HEAP_MIN_FREE_BYTES) {
            resourcesClean = false;
        }
        if (printIt) {
            uPortLog("%s%sheap had a minimum of %d byte(s) free"
                     " (minimum is %d).\n", pPrefix,
                     resourcesClean ? "" : pErrorMarker,
                     x, U_CFG_TEST_HEAP_MIN_FREE_BYTES);
        }
    }

    // Check that all heap pUPortMalloc()s have uPortFree()s
    x = uPortHeapAllocCount();
    if (x > heapShouldBeOutstanding) {
        if (printIt) {
            uPortLog("%s%sexpected %d outstanding call(s) to pUPortMalloc()"
                     " but got %d%s.\n",
                     pPrefix, pErrorMarker, heapShouldBeOutstanding, x,
                     (x > heapShouldBeOutstanding) ? "; they might yet be cleaned up" : "");
            uPortHeapDump(pPrefix);
        }
        resourcesClean = false;
    }

    // Check that all OS resources have been free'd
    x = uPortOsResourceAllocCount();
    if (x != osShouldBeOutstanding) {
        if (printIt) {
            uPortLog("%s%sexpected %d outstanding OS resource(s) (tasks etc.)"
                     " but got %d%s.\n",
                     pPrefix, pErrorMarker, osShouldBeOutstanding, x,
                     (x > osShouldBeOutstanding) ? "; they might yet be cleaned up" : "");
        }
        resourcesClean = false;
    }
    x = uPortUartResourceAllocCount();
    if (x > 0) {
        if (printIt) {
            uPortLog("%s%s%d UART resource(s) outstanding.\n", pPrefix, pErrorMarker, x);
        }
        resourcesClean = false;
    }
    x = uPortI2cResourceAllocCount();
    if (x > 0) {
        if (printIt) {
            uPortLog("%s%s%d I2C resource(s) outstanding.\n", pPrefix, pErrorMarker, x);
        }
        resourcesClean = false;
    }
    x = uPortSpiResourceAllocCount();
    if (x > 0) {
        if (printIt) {
            uPortLog("%s%s%d SPI resource(s) outstanding.\n", pPrefix, pErrorMarker, x);
        }
        resourcesClean = false;
    }
    if (resourcesClean && printIt) {
        uPortLog("%sresources are good (%d outstanding OS resource(s), as expected).\n",
                 pPrefix, osShouldBeOutstanding);
    }

    return resourcesClean;
}

// End of file
