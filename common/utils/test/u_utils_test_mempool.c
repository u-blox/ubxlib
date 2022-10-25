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
 * @brief Test for the mempool API
 */


#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "errno.h"
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"        // strncpy(), strcmp(), memcpy(), memset()

#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"
#include "u_cfg_os_platform_specific.h"  // For #define U_CFG_OS_CLIB_LEAKS

#include "u_error_common.h"

#include "u_port_clib_platform_specific.h" /* struct timeval in some cases. */
#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_event_queue.h"
#include "u_mempool.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_MEMPOOL_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#define TEST_BLOCK_COUNT 8
#define TEST_BLOCK_SIZE  64

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static bool isAllBytes(uint8_t *pBuf, size_t size, uint8_t cmpByte)
{
    for (size_t i = 0; i < size; i++) {
        if (pBuf[i] != cmpByte) {
            return false;
        }
    }
    return true;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TESTS
 * -------------------------------------------------------------- */
U_PORT_TEST_FUNCTION("[mempool]", "mempoolBasic")
{
    int32_t errCode;
    uMemPoolDesc_t mempoolDesc;
    uint8_t *pBuf1;
    uint8_t *pBuf2;
    int32_t heapUsed;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    errCode = uMemPoolInit(&mempoolDesc, TEST_BLOCK_SIZE, TEST_BLOCK_COUNT);
    U_PORT_TEST_ASSERT(errCode == U_ERROR_COMMON_SUCCESS);

    // Allocate first buffer and fill with all 0xff
    pBuf1 = (uint8_t *)uMemPoolAllocMem(&mempoolDesc);
    U_PORT_TEST_ASSERT(pBuf1 != NULL);
    memset(pBuf1, 0xFF, TEST_BLOCK_SIZE);

    // Allocate first buffer and fill with all 0xee
    pBuf2 = (uint8_t *)uMemPoolAllocMem(&mempoolDesc);
    U_PORT_TEST_ASSERT(pBuf2 != NULL);
    memset(pBuf2, 0xEE, TEST_BLOCK_SIZE);

    // Now check that no bytes "leaked" over to the other buffer
    U_PORT_TEST_ASSERT(isAllBytes(pBuf1, TEST_BLOCK_SIZE, 0xFF));
    U_PORT_TEST_ASSERT(isAllBytes(pBuf2, TEST_BLOCK_SIZE, 0xEE));

    uMemPoolFreeMem(&mempoolDesc, (void *)pBuf1);
    uMemPoolFreeMem(&mempoolDesc, (void *)pBuf2);

    uMemPoolDeinit(&mempoolDesc);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed == 0) || (heapUsed == (int32_t)U_ERROR_COMMON_NOT_SUPPORTED));
}


U_PORT_TEST_FUNCTION("[mempool]", "mempoolFull")
{
    int32_t errCode;
    uMemPoolDesc_t mempoolDesc;
    uint8_t *pBuf[TEST_BLOCK_COUNT];
    int32_t heapUsed;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    errCode = uMemPoolInit(&mempoolDesc, TEST_BLOCK_SIZE, TEST_BLOCK_COUNT);
    U_PORT_TEST_ASSERT(errCode == U_ERROR_COMMON_SUCCESS);

    // Allocate all buffers available in the pool
    for (int32_t i = 0; i < TEST_BLOCK_COUNT; i++) {
        pBuf[i] = (uint8_t *)uMemPoolAllocMem(&mempoolDesc);
        U_PORT_TEST_ASSERT(pBuf[i] != NULL);
    }
    // Now we should have allocated each block so make sure uMemPoolAllocMem returns NULL
    U_PORT_TEST_ASSERT(uMemPoolAllocMem(&mempoolDesc) == NULL);

    // Free one buffer and make sure the we then can allocate it again
    uMemPoolFreeMem(&mempoolDesc, (void *)pBuf[0]);
    pBuf[0] = (uint8_t *)uMemPoolAllocMem(&mempoolDesc);
    U_PORT_TEST_ASSERT(pBuf[0] != NULL);

    for (int32_t i = 0; i < TEST_BLOCK_COUNT; i++) {
        uMemPoolFreeMem(&mempoolDesc, (void *)pBuf[i]);
    }

    uMemPoolDeinit(&mempoolDesc);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed == 0) || (heapUsed == (int32_t)U_ERROR_COMMON_NOT_SUPPORTED));
}

U_PORT_TEST_FUNCTION("[mempool]", "mempoolFreeAllMem")
{
    int32_t errCode;
    uMemPoolDesc_t mempoolDesc;
    uint8_t *pBuf1[TEST_BLOCK_COUNT];
    uint8_t *pBuf2[TEST_BLOCK_COUNT];
    int32_t heapUsed;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();
    U_TEST_PRINT_LINE("heap used at start %d.", heapUsed);

    errCode = uMemPoolInit(&mempoolDesc, TEST_BLOCK_SIZE, TEST_BLOCK_COUNT);
    U_PORT_TEST_ASSERT(errCode == U_ERROR_COMMON_SUCCESS);

    // Allocate all buffers available in the pool
    for (int32_t i = 0; i < TEST_BLOCK_COUNT; i++) {
        pBuf1[i] = (uint8_t *)uMemPoolAllocMem(&mempoolDesc);
        U_PORT_TEST_ASSERT(pBuf1[i] != NULL);
    }

    // Now free all the allocated blocks
    uMemPoolFreeAllMem(&mempoolDesc);

    // Allocate the memory for blocks again
    for (int32_t i = 0; i < TEST_BLOCK_COUNT; i++) {
        pBuf2[i] = (uint8_t *)uMemPoolAllocMem(&mempoolDesc);
        U_PORT_TEST_ASSERT(pBuf2[i] != NULL);
    }

    // Check all the memory that were added to free list were
    // allocated again
    for (int32_t i = 0; i < TEST_BLOCK_COUNT; i++) {
        U_PORT_TEST_ASSERT(pBuf1[i] == pBuf2[i]);
    }

    uMemPoolDeinit(&mempoolDesc);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed == 0) || (heapUsed == (int32_t)U_ERROR_COMMON_NOT_SUPPORTED));

}

// End of file