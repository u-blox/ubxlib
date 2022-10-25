/*
 * Copyright 2019-2022 u-blox Ltd
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
 * @brief Implementaion of C library functions.
 */

#define TXM_MODULE

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "txm_module.h"
#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"

#include "u_cfg_sw.h"
#include "u_port.h"
#include "u_error_common.h"
#include "u_port_debug.h"
#include "u_cfg_os_platform_specific.h"
#include "u_port_clib_platform_specific.h"
#include "u_port_heap.h"
#include "u_port_os.h"
#include "u_port_debug.h"

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

// Allocate dynamic memory from heap pool.
void *malloc(size_t size)
{
    uint32_t result;
    void *ptr = NULL;

    if ((size > 0) &&  (pHeapPool != NULL)) {
        result = tx_byte_allocate(pHeapPool, &ptr, size, TX_NO_WAIT);

        if (result != TX_SUCCESS) {
            uPortLog("malloc() Heap pool exhausted.\n");
            ptr = NULL;
        }
    }

    return ptr;
}

// Free dynamically allocated memory.
void free(void *ptr)
{
    if (ptr != NULL) {
        tx_byte_release(ptr);
    }
}

// End of file
