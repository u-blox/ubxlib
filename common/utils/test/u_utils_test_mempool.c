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
#include "stdlib.h"    // malloc(), free()
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
 * PUBLIC FUNCTIONS: TESTS
 * -------------------------------------------------------------- */
U_PORT_TEST_FUNCTION("[mempool]", "mempoolBasic")
{
    int32_t errCode;
    uMemPoolDesc_t mempoolDesc;
    int32_t numOfBlks = 8;
    uint32_t sizeOfBlk = 64;
    uint8_t *pBuf1;
    uint8_t *pBuf2;
    int32_t heapUsed;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    errCode = uMemPoolInit(&mempoolDesc, sizeOfBlk, numOfBlks);
    U_PORT_TEST_ASSERT(errCode == U_ERROR_COMMON_SUCCESS);

    pBuf1 = (uint8_t *)uMemPoolAllocMem(&mempoolDesc);
    U_PORT_TEST_ASSERT(pBuf1 != NULL);

    pBuf2 = (uint8_t *)uMemPoolAllocMem(&mempoolDesc);
    U_PORT_TEST_ASSERT(pBuf2 != NULL);

    U_PORT_TEST_ASSERT((uint32_t)(pBuf2 - pBuf1) == sizeOfBlk);

    uMemPoolFreeMem(&mempoolDesc, (void *)pBuf1);
    uMemPoolFreeMem(&mempoolDesc, (void *)pBuf2);

    uMemPoolDeinit(&mempoolDesc);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_MEMPOOL_TEST: we have leaked %d byte(s).\n", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed == 0) || (heapUsed == (int32_t)U_ERROR_COMMON_NOT_SUPPORTED));
}


U_PORT_TEST_FUNCTION("[mempool]", "mempoolResize")
{
    int32_t errCode;
    uMemPoolDesc_t mempoolDesc;
    int32_t numOfBlks = 8;
    uint32_t sizeOfBlk = 64;
    uint8_t *pBuf[16];
    int32_t heapUsed;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    errCode = uMemPoolInit(&mempoolDesc, sizeOfBlk, numOfBlks);
    U_PORT_TEST_ASSERT(errCode == U_ERROR_COMMON_SUCCESS);

    for (int32_t i = 0; i < sizeof(pBuf) / sizeof(uint8_t *); i++) {
        pBuf[i] = (uint8_t *)uMemPoolAllocMem(&mempoolDesc);
        U_PORT_TEST_ASSERT(pBuf[i] != NULL);
    }

    for (int32_t i = 0; i < sizeof(pBuf) / sizeof(uint8_t *); i++) {
        uMemPoolFreeMem(&mempoolDesc, (void *)pBuf[i]);
    }

    uMemPoolDeinit(&mempoolDesc);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_MEMPOOL_TEST: we have leaked %d byte(s).\n", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed == 0) || (heapUsed == (int32_t)U_ERROR_COMMON_NOT_SUPPORTED));
}

U_PORT_TEST_FUNCTION("[mempool]", "mempoolFreeAllMem")
{
    int32_t errCode;
    uMemPoolDesc_t mempoolDesc;
    int32_t numOfBlks = 8;
    uint32_t sizeOfBlk = 64;
    uint8_t *pBuf1[16];
    uint8_t *pBuf2[16];
    int32_t heapUsed;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();
    uPortLog("Heap used at start %d\n", heapUsed);

    errCode = uMemPoolInit(&mempoolDesc, sizeOfBlk, numOfBlks);
    U_PORT_TEST_ASSERT(errCode == U_ERROR_COMMON_SUCCESS);

    for (int32_t i = 0; i < sizeof(pBuf1) / sizeof(uint8_t *); i++) {
        pBuf1[i] = (uint8_t *)uMemPoolAllocMem(&mempoolDesc);
        U_PORT_TEST_ASSERT(pBuf1[i] != NULL);
    }


    // Now free all the allocated blocks
    uMemPoolFreeAllMem(&mempoolDesc);

    // Allocate the memory for blocks again
    for (int32_t i = 0; i < sizeof(pBuf2) / sizeof(uint8_t *); i++) {
        pBuf2[i] = (uint8_t *)uMemPoolAllocMem(&mempoolDesc);
        U_PORT_TEST_ASSERT(pBuf2[i] != NULL);

    }

    // Check all the memory that were added to free list were
    // allocated again
    for (int32_t i = 0; i < sizeof(pBuf1) / sizeof(uint8_t *); i++) {

        U_PORT_TEST_ASSERT(pBuf1[i] == pBuf2[i]);
    }

    uMemPoolDeinit(&mempoolDesc);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_MEMPOOL_TEST: we have leaked %d byte(s).\n", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed == 0) || (heapUsed == (int32_t)U_ERROR_COMMON_NOT_SUPPORTED));

}

U_PORT_TEST_FUNCTION("[mempool]", "mempoolCrossThreshold")
{
    int32_t errCode;
    uMemPoolDesc_t mempoolDesc;
    // Pool can grow up till the twice the number of blocks
    int32_t numOfBlks = 4;
    uint32_t sizeOfBlk = 64;
    uint8_t *pBuf[8];
    uint8_t *pBuf2;
    int32_t heapUsed;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    errCode = uMemPoolInit(&mempoolDesc, sizeOfBlk, numOfBlks);
    U_PORT_TEST_ASSERT(errCode == U_ERROR_COMMON_SUCCESS);

    for (int32_t i = 0; i < sizeof(pBuf) / sizeof(uint8_t *); i++) {
        pBuf[i] = (uint8_t *)uMemPoolAllocMem(&mempoolDesc);
        U_PORT_TEST_ASSERT(pBuf[i] != NULL);
    }

    // Exceeded the max number of blocks (8)
    // Trying to allocate for 9th should fail
    pBuf2 = (uint8_t *)uMemPoolAllocMem(&mempoolDesc);
    U_PORT_TEST_ASSERT(pBuf2 == NULL);

    uMemPoolDeinit(&mempoolDesc);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_MEMPOOL_TEST: we have leaked %d byte(s).\n", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed == 0) || (heapUsed == (int32_t)U_ERROR_COMMON_NOT_SUPPORTED));
}
// End of file