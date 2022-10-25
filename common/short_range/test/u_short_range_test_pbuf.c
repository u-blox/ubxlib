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
#include "stdlib.h"    // rand()
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
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_mempool.h"
#include "u_short_range_pbuf.h"
#include "u_short_range_edm.h" // For U_SHORT_RANGE_EDM_BLK_SIZE

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_SHORT_RANGE_PBUF_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */
static int32_t generatePayLoad(uShortRangePbuf_t **ppBuf)
{
    int32_t errorCode;

    errorCode = uShortRangePbufAlloc(ppBuf);

    if (errorCode > 0) {
        for (int i = 0; i < errorCode; i++) {
            (*ppBuf)->data[i] = rand() % 128;
            (*ppBuf)->length++;
        }
    }
    return errorCode;
}
/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TESTS
 * -------------------------------------------------------------- */
U_PORT_TEST_FUNCTION("[pbuf]", "pbufInsertPayload")
{
    int32_t errCode;
    uShortRangePbufList_t *pPbufList;
    int32_t numOfBlks = 8;
    uShortRangePbuf_t *pBuf;
    int32_t heapUsed;
    char *pBuffer2;
    char *pBuffer3;
    size_t copiedLen = 0;
    int32_t i;
    //lint -e{679} suppress loss of precision
    //lint -e{647} suppress suspicious truncation
    size_t totalLen = numOfBlks * U_SHORT_RANGE_EDM_BLK_SIZE;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    rand();
    heapUsed = uPortGetHeapFree();

    errCode = uShortRangeMemPoolInit();
    U_PORT_TEST_ASSERT(errCode == (int32_t)U_ERROR_COMMON_SUCCESS);

    pPbufList = pUShortRangePbufListAlloc();
    U_PORT_TEST_ASSERT(pPbufList != NULL);

    pBuffer2 = (char *)pUPortMalloc(totalLen);
    U_PORT_TEST_ASSERT(pBuffer2 != NULL);
    memset(pBuffer2, 0, totalLen);

    pBuffer3 = (char *)pUPortMalloc(totalLen);
    U_PORT_TEST_ASSERT(pBuffer3 != NULL);
    memset(pBuffer3, 0, totalLen);

    for (i = 0; i < numOfBlks; i++) {
        int32_t sizeOfBlk = generatePayLoad(&pBuf);
        U_PORT_TEST_ASSERT_EQUAL(U_SHORT_RANGE_EDM_BLK_SIZE, sizeOfBlk);
        memcpy(&pBuffer2[i * sizeOfBlk], &pBuf->data[0], sizeOfBlk);
        errCode = uShortRangePbufListAppend(pPbufList, pBuf);
        U_PORT_TEST_ASSERT(errCode == (int32_t)U_ERROR_COMMON_SUCCESS);
    }

    for (i = 0; i < (int32_t)totalLen; i++) {
        copiedLen += uShortRangePbufListConsumeData(pPbufList, &pBuffer3[i], 1);
    }

    U_PORT_TEST_ASSERT(copiedLen == totalLen);

    errCode = memcmp(pBuffer2, pBuffer3, totalLen);
    U_PORT_TEST_ASSERT(errCode == (int32_t)U_ERROR_COMMON_SUCCESS);

    uShortRangeMemPoolDeInit();
    uPortFree(pBuffer2);
    uPortFree(pBuffer3);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed == 0) || (heapUsed == (int32_t)U_ERROR_COMMON_NOT_SUPPORTED));
}

U_PORT_TEST_FUNCTION("[pbuf]", "pbufPktList")
{
    int32_t errCode;
    uShortRangePbufList_t *pPbufList1;
    uShortRangePbufList_t *pPbufList2;
    uShortRangePktList_t pktList;
    int32_t numOfBlks = 8;
    uShortRangePbuf_t *pBuf;
    int32_t heapUsed;
    char *pBuffer1;
    char *pBuffer2;
    char *pBuffer3;
    int32_t i;
    //lint -e{679} suppress loss of precision
    //lint -e{647} suppress suspicious truncation
    size_t totalLen = numOfBlks * U_SHORT_RANGE_EDM_BLK_SIZE;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();

    rand();
    heapUsed = uPortGetHeapFree();

    errCode = uShortRangeMemPoolInit();
    U_PORT_TEST_ASSERT(errCode == (int32_t)U_ERROR_COMMON_SUCCESS);

    pPbufList1 = pUShortRangePbufListAlloc();
    U_PORT_TEST_ASSERT(pPbufList1 != NULL);

    pPbufList2 = pUShortRangePbufListAlloc();
    U_PORT_TEST_ASSERT(pPbufList2 != NULL);

    pBuffer1 = (char *)pUPortMalloc(totalLen);
    U_PORT_TEST_ASSERT(pBuffer1 != NULL);
    memset(pBuffer1, 0, totalLen);

    pBuffer2 = (char *)pUPortMalloc(totalLen);
    U_PORT_TEST_ASSERT(pBuffer2 != NULL);
    memset(pBuffer2, 0, totalLen);

    pBuffer3 = (char *)pUPortMalloc(totalLen);
    U_PORT_TEST_ASSERT(pBuffer3 != NULL);
    memset(pBuffer3, 0, totalLen);

    // Generate packet 1 half the size of the memory pool
    // Fill it with random data
    for (i = 0; i < numOfBlks / 2; i++) {
        int32_t sizeOfBlk = generatePayLoad(&pBuf);
        U_PORT_TEST_ASSERT_EQUAL(U_SHORT_RANGE_EDM_BLK_SIZE, sizeOfBlk);
        memcpy(&pBuffer1[i * sizeOfBlk], &pBuf->data[0], sizeOfBlk);
        errCode = uShortRangePbufListAppend(pPbufList1, pBuf);
        U_PORT_TEST_ASSERT(errCode == (int32_t)U_ERROR_COMMON_SUCCESS);
    }

    // Generate packet 2 half the size of the memory pool
    // Fill it with random data
    for (i = 0; i < numOfBlks / 2; i++) {
        int32_t sizeOfBlk = generatePayLoad(&pBuf);
        U_PORT_TEST_ASSERT(sizeOfBlk > 0);
        memcpy(&pBuffer2[i * sizeOfBlk], &pBuf->data[0], sizeOfBlk);
        errCode = uShortRangePbufListAppend(pPbufList2, pBuf);
        U_PORT_TEST_ASSERT(errCode == (int32_t)U_ERROR_COMMON_SUCCESS);
    }

    memset((void *)&pktList, 0, sizeof(uShortRangePktList_t));

    // Add the two packets to a packet list
    errCode = uShortRangePktListAppend(&pktList, pPbufList1);
    U_PORT_TEST_ASSERT(errCode == (int32_t)U_ERROR_COMMON_SUCCESS);
    errCode = uShortRangePktListAppend(&pktList, pPbufList2);
    U_PORT_TEST_ASSERT(errCode == (int32_t)U_ERROR_COMMON_SUCCESS);

    // Read out the first packet
    errCode = uShortRangePktListConsumePacket(&pktList, pBuffer3, (size_t *)&totalLen, NULL);
    U_PORT_TEST_ASSERT(errCode == (int32_t)U_ERROR_COMMON_SUCCESS);

    // Verify that the content we read out is the same as what we put in
    errCode = memcmp(pBuffer3, pBuffer1, totalLen);
    U_PORT_TEST_ASSERT(errCode == (int32_t)U_ERROR_COMMON_SUCCESS);

    memset(pBuffer3, 0, totalLen);

    errCode = uShortRangePktListConsumePacket(&pktList, pBuffer3, &totalLen, NULL);
    U_PORT_TEST_ASSERT(errCode == (int32_t)U_ERROR_COMMON_SUCCESS);

    errCode = memcmp(pBuffer3, pBuffer2, totalLen);
    U_PORT_TEST_ASSERT(errCode == (int32_t)U_ERROR_COMMON_SUCCESS);

    errCode = uShortRangePktListConsumePacket(&pktList, pBuffer3, &totalLen, NULL);
    U_PORT_TEST_ASSERT(errCode == (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);

    uShortRangeMemPoolDeInit();
    uPortFree(pBuffer1);
    uPortFree(pBuffer2);
    uPortFree(pBuffer3);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed == 0) || (heapUsed == (int32_t)U_ERROR_COMMON_NOT_SUPPORTED));
}

// End of file
