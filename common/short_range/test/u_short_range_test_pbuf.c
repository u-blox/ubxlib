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
#include "u_mempool.h"
#include "u_short_range_pbuf.h"

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
static char *generatePayLoad(uMemPoolDesc_t *pMemPool, uint32_t size)
{
    char *pPayLoad;
    uint32_t i;

    pPayLoad = (char *)uMemPoolAllocMem(pMemPool);

    if (pPayLoad != NULL) {
        for (i = 0; i < size; i++) {

            pPayLoad[i] = rand() % 128;

        }
    }
    return pPayLoad;
}
/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TESTS
 * -------------------------------------------------------------- */
U_PORT_TEST_FUNCTION("[pbuf]", "pbufInsertPayload")
{
    int32_t errCode;
    uMemPoolDesc_t mempoolDesc;
    uShortRangePbufList_t *pPbufList;
    int32_t numOfBlks = 8;
    uint32_t sizeOfBlk = 64;
    char *pBuf;
    int32_t heapUsed;
    char *pBuf2;
    char *pBuf3;
    size_t copiedLen = 0;
    int32_t i;
    //lint -e{679} suppress loss of precision
    //lint -e{647} suppress suspicious truncation
    size_t totalLen = numOfBlks * sizeOfBlk;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    rand();
    heapUsed = uPortGetHeapFree();

    errCode = uMemPoolInit(&mempoolDesc, sizeOfBlk, numOfBlks);
    U_PORT_TEST_ASSERT(errCode == (int32_t)U_ERROR_COMMON_SUCCESS);

    errCode = uShortRangeMemPoolInit();
    U_PORT_TEST_ASSERT(errCode == (int32_t)U_ERROR_COMMON_SUCCESS);

    pPbufList = pUShortRangeAllocPbufList();
    U_PORT_TEST_ASSERT(pPbufList != NULL);

    pBuf2 = (char *)malloc(totalLen);
    U_PORT_TEST_ASSERT(pBuf2 != NULL);
    //lint -e{668} suppress Possibly passing a. null pointer to function memset - we are not!
    memset(pBuf2, 0, totalLen);

    pBuf3 = (char *)malloc(totalLen);
    U_PORT_TEST_ASSERT(pBuf3 != NULL);
    //lint -e{668} suppress Possibly passing a. null pointer to function memset - we are not!
    memset(pBuf3, 0, totalLen);

    for (i = 0; i < numOfBlks; i++) {
        pBuf = generatePayLoad(&mempoolDesc, sizeOfBlk);
        //lint -e{679} suppress Suspicious Truncation in arithmetic expression combining with pointer
        memcpy(&pBuf2[i * sizeOfBlk], pBuf, sizeOfBlk);
        errCode = uShortRangeInsertPayloadToPbufList(pPbufList, pBuf, sizeOfBlk);
        U_PORT_TEST_ASSERT(errCode == (int32_t)U_ERROR_COMMON_SUCCESS);
    }

    for (i = 0; i < (int32_t)totalLen; i++) {
        copiedLen += uShortRangeMovePayloadFromPbufList(pPbufList, &pBuf3[i], 1);
    }

    U_PORT_TEST_ASSERT(copiedLen == totalLen);

    errCode = memcmp(pBuf2, pBuf3, totalLen);
    U_PORT_TEST_ASSERT(errCode == (int32_t)U_ERROR_COMMON_SUCCESS);

    uShortRangeMemPoolDeInit();
    uMemPoolDeinit(&mempoolDesc);
    free(pBuf2);
    free(pBuf3);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_SHORT_RANGE_PBUF_TEST: we have leaked %d byte(s).\n", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed == 0) || (heapUsed == (int32_t)U_ERROR_COMMON_NOT_SUPPORTED));
}

U_PORT_TEST_FUNCTION("[pbuf]", "pbufPktList")
{
    int32_t errCode;
    uMemPoolDesc_t mempoolDesc;
    uShortRangePbufList_t *pPbufList1;
    uShortRangePbufList_t *pPbufList2;
    uShortRangePktList_t pktList;
    int32_t numOfBlks = 8;
    uint32_t sizeOfBlk = 64;
    char *pBuf;
    int32_t heapUsed;
    char *pBuf1;
    char *pBuf2;
    char *pBuf3;
    int32_t i;
    //lint -e{679} suppress loss of precision
    //lint -e{647} suppress suspicious truncation
    size_t totalLen = numOfBlks * sizeOfBlk;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();

    rand();
    heapUsed = uPortGetHeapFree();

    errCode = uMemPoolInit(&mempoolDesc, sizeOfBlk, numOfBlks);
    U_PORT_TEST_ASSERT(errCode == (int32_t)U_ERROR_COMMON_SUCCESS);

    errCode = uShortRangeMemPoolInit();
    U_PORT_TEST_ASSERT(errCode == (int32_t)U_ERROR_COMMON_SUCCESS);

    pPbufList1 = pUShortRangeAllocPbufList();
    U_PORT_TEST_ASSERT(pPbufList1 != NULL);

    pPbufList2 = pUShortRangeAllocPbufList();
    U_PORT_TEST_ASSERT(pPbufList2 != NULL);

    pBuf1 = (char *)malloc(totalLen);
    U_PORT_TEST_ASSERT(pBuf1 != NULL);
    //lint -e{668} suppress Possibly passing a. null pointer to function memset - we are not!
    memset(pBuf1, 0, totalLen);

    pBuf2 = (char *)malloc(totalLen);
    U_PORT_TEST_ASSERT(pBuf2 != NULL);
    //lint -e{668} suppress Possibly passing a. null pointer to function memset - we are not!
    memset(pBuf2, 0, totalLen);

    pBuf3 = (char *)malloc(totalLen);
    U_PORT_TEST_ASSERT(pBuf3 != NULL);
    //lint -e{668} suppress Possibly passing a. null pointer to function memset - we are not!
    memset(pBuf3, 0, totalLen);

    for (i = 0; i < numOfBlks; i++) {
        pBuf = generatePayLoad(&mempoolDesc, sizeOfBlk);
        //lint -e{679} suppress Suspicious Truncation in arithmetic expression combining with pointer
        memcpy(&pBuf1[i * sizeOfBlk], pBuf, sizeOfBlk);
        errCode = uShortRangeInsertPayloadToPbufList(pPbufList1, pBuf, sizeOfBlk);
        U_PORT_TEST_ASSERT(errCode == (int32_t)U_ERROR_COMMON_SUCCESS);
    }

    for (i = 0; i < numOfBlks; i++) {
        pBuf = generatePayLoad(&mempoolDesc, sizeOfBlk);
        //lint -e{679} suppress Suspicious Truncation in arithmetic expression combining with pointer
        memcpy(&pBuf2[i * sizeOfBlk], pBuf, sizeOfBlk);
        errCode = uShortRangeInsertPayloadToPbufList(pPbufList2, pBuf, sizeOfBlk);
        U_PORT_TEST_ASSERT(errCode == (int32_t)U_ERROR_COMMON_SUCCESS);
    }

    memset((void *)&pktList, 0, sizeof(uShortRangePktList_t));

    errCode = uShortRangeInsertPktToPktList(&pktList, pPbufList1);
    U_PORT_TEST_ASSERT(errCode == (int32_t)U_ERROR_COMMON_SUCCESS);

    errCode = uShortRangeInsertPktToPktList(&pktList, pPbufList2);
    U_PORT_TEST_ASSERT(errCode == (int32_t)U_ERROR_COMMON_SUCCESS);

    errCode = uShortRangeReadPktFromPktList(&pktList, pBuf3, (size_t *)&totalLen, NULL);
    U_PORT_TEST_ASSERT(errCode == (int32_t)U_ERROR_COMMON_SUCCESS);

    errCode = memcmp(pBuf3, pBuf1, totalLen);
    U_PORT_TEST_ASSERT(errCode == (int32_t)U_ERROR_COMMON_SUCCESS);

    //lint -e{668} suppress Possibly passing a. null pointer to function memset - we are not!
    memset(pBuf3, 0, totalLen);

    errCode = uShortRangeReadPktFromPktList(&pktList, pBuf3, &totalLen, NULL);
    U_PORT_TEST_ASSERT(errCode == (int32_t)U_ERROR_COMMON_SUCCESS);

    errCode = memcmp(pBuf3, pBuf2, totalLen);
    U_PORT_TEST_ASSERT(errCode == (int32_t)U_ERROR_COMMON_SUCCESS);


    errCode = uShortRangeReadPktFromPktList(&pktList, pBuf3, &totalLen, NULL);
    U_PORT_TEST_ASSERT(errCode == (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);

    uShortRangeMemPoolDeInit();
    uMemPoolDeinit(&mempoolDesc);
    free(pBuf1);
    free(pBuf2);
    free(pBuf3);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_SHORT_RANGE_PBUF_TEST: we have leaked %d byte(s).\n", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed == 0) || (heapUsed == (int32_t)U_ERROR_COMMON_NOT_SUPPORTED));
}

// End of file
