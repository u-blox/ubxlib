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

/** @file
 * @brief Implementations of the common uPortOsResourcePerpetualAdd() and
 * uPortOsResourcePerpetualCount() functions and default implementations
 * of the resource counting functions uPortOsResourceAllocCount(),
 * uPortUartResourceAllocCount(), etc.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

/* ----------------------------------------------------------------
 * INCLUDE FILES
 * -------------------------------------------------------------- */

#include "stdint.h"      // int32_t etc.
#include "stddef.h"      // NULL, size_t etc.
#include "stdbool.h"

#include "u_compiler.h"  // U_WEAK

#include "u_cfg_os_platform_specific.h" // For U_CFG_OS_RESOURCES_PER_xxx and U_CFG_OS_MALLOCS_PER_xxx overrides

#include "u_port_os.h"
#include "u_port_heap.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_CFG_OS_RESOURCES_PER_TASK
/** The number of OS resource allocations that it takes to create
 * a task; may be overriden in u_cfg_os_platform_specific.h for
 * a given platform.
 */
# define U_CFG_OS_RESOURCES_PER_TASK 1
#endif

#ifndef U_CFG_OS_RESOURCES_PER_QUEUE
/** The number of OS resource allocations that it takes to create
 * a queue; for example, if a mutex is required to protect a queue
 * then this would be 2 rather than 1.  May be overriden in
 * u_cfg_os_platform_specific.h for a given platform.
 */
# define U_CFG_OS_RESOURCES_PER_QUEUE 1
#endif

#ifndef U_CFG_OS_RESOURCES_PER_MUTEX
/** The number of OS resource allocations that it takes to create
 * a mutex; may be overriden in u_cfg_os_platform_specific.h for
 * a given platform.
 */
# define U_CFG_OS_RESOURCES_PER_MUTEX 1
#endif

#ifndef U_CFG_OS_RESOURCES_PER_SEMAPHORE
/** The number of OS resource allocations that it takes to create
 * a semaphore; may be overriden in u_cfg_os_platform_specific.h for
 * a given platform.
 */
# define U_CFG_OS_RESOURCES_PER_SEMAPHORE 1
#endif

#ifndef U_CFG_OS_RESOURCES_PER_TIMER
/** The number of OS resource allocations that it takes to create
 * a semaphore; may be overriden in u_cfg_os_platform_specific.h for
 * a given platform.
 */
# define U_CFG_OS_RESOURCES_PER_TIMER 1
#endif

#ifndef U_CFG_OS_MALLOCS_PER_TASK
/** The number of calls to uPortHeapAllocCount() that will be
 * outstanding if a task is not deleted; for instance a call to
 * pUPortMalloc() may need to be made to allocate heap for a task,
 * hence this macro should be 1.  May be overriden in
 * u_cfg_os_platform_specific.h for a given platform.
 */
# define U_CFG_OS_MALLOCS_PER_TASK 0
#endif

#ifndef U_CFG_OS_MALLOCS_PER_QUEUE
/** The number of calls to uPortHeapAllocCount() that will be
 * outstanding if a queue is not deleted; may be overriden in
 * u_cfg_os_platform_specific.h for a given platform.
 */
# define U_CFG_OS_MALLOCS_PER_QUEUE 0
#endif

#ifndef U_CFG_OS_MALLOCS_PER_MUTEX
/** The number of calls to uPortHeapAllocCount() that will be
 * outstanding if a mutex is not deleted; may be overriden in
 * u_cfg_os_platform_specific.h for a given platform.
 */
# define U_CFG_OS_MALLOCS_PER_MUTEX 0
#endif

#ifndef U_CFG_OS_MALLOCS_PER_SEMAPHORE
/** The number of calls to uPortHeapAllocCount() that will be
 * outstanding if a semaphore is not deleted; may be overriden in
 * u_cfg_os_platform_specific.h for a given platform.
 */
# define U_CFG_OS_MALLOCS_PER_SEMAPHORE 0
#endif

#ifndef U_CFG_OS_MALLOCS_PER_TIMER
/** The number of calls to uPortHeapAllocCount() that will be
 * outstanding if a timer is not deleted; may be overriden in
 * u_cfg_os_platform_specific.h for a given platform.
 */
# define U_CFG_OS_MALLOCS_PER_TIMER 0
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Variable to keep track of the total number of OS resources
 * created that will not be deleted.
 */
static int32_t gOsPerpetualCount = 0;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Add a perpetual OS resource allocation to the count.
void uPortOsResourcePerpetualAdd(uPortOsResourceType_t type)
{
    int32_t numOsResources = 0;
    size_t numHeapAllocs = 0;

    switch (type) {
        case U_PORT_OS_RESOURCE_TYPE_TASK:
            numOsResources = U_CFG_OS_RESOURCES_PER_TASK;
            numHeapAllocs = U_CFG_OS_MALLOCS_PER_TASK;
            break;
        case U_PORT_OS_RESOURCE_TYPE_QUEUE:
            numOsResources = U_CFG_OS_RESOURCES_PER_QUEUE;
            numHeapAllocs = U_CFG_OS_MALLOCS_PER_QUEUE;
            break;
        case U_PORT_OS_RESOURCE_TYPE_MUTEX:
            numOsResources = U_CFG_OS_RESOURCES_PER_MUTEX;
            numHeapAllocs = U_CFG_OS_MALLOCS_PER_MUTEX;
            break;
        case U_PORT_OS_RESOURCE_TYPE_SEMAPHORE:
            numOsResources = U_CFG_OS_RESOURCES_PER_SEMAPHORE;
            numHeapAllocs = U_CFG_OS_MALLOCS_PER_SEMAPHORE;
            break;
        case U_PORT_OS_RESOURCE_TYPE_TIMER:
            numOsResources = U_CFG_OS_RESOURCES_PER_TIMER;
            numHeapAllocs = U_CFG_OS_MALLOCS_PER_TIMER;
            break;
        default:
            break;
    }
    gOsPerpetualCount += numOsResources;
    for (size_t x = 0; x < numHeapAllocs; x++) {
        uPortHeapPerpetualAllocAdd();
    }
}

// Return the number of perpetual resource allocations
int32_t uPortOsResourcePerpetualCount()
{
    return gOsPerpetualCount;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: WEAK FUNCTIONS, SHOULD BE IMPLEMENTED IN PORTS
 * -------------------------------------------------------------- */

#ifndef _MSC_VER
/* Note: *&!ing MSVC will simply NOT do weak-linkage, or even a
 * hacky equivalent, well enough to ignore these implementations in
 * favour of the ones under platform/windows: seem to be able to
 * do it for one (uPortGetTimezoneOffsetSeconds()) but not for more
 * than one.  Hence just ifndef'ing these out for MSVC.
 */
U_WEAK int32_t uPortOsResourceAllocCount()
{
    return 0;
}

U_WEAK int32_t uPortUartResourceAllocCount()
{
    return 0;
}
#endif

U_WEAK int32_t uPortI2cResourceAllocCount()
{
    return 0;
}

U_WEAK int32_t uPortSpiResourceAllocCount()
{
    return 0;
}

// End of file
