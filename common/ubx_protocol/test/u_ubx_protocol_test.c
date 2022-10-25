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
 * @brief Test for the UBX API: these should pass on all platforms.
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the U_PORT_TEST_FUNCTION()
 * macro.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memcmp()/memset()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_os.h"

#include "u_ubx_protocol.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_UBX_PROTOCOL_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#ifndef U_UBX_PROTOCOL_TEST_MAX_BODY_SIZE
/** The maximum UBX protocol message body size to test with.
 */
# define U_UBX_PROTOCOL_TEST_MAX_BODY_SIZE 1024
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

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TESTS
 * -------------------------------------------------------------- */

/** Back-to-back testing of the UBX protocol encoder/decoder.
 */
U_PORT_TEST_FUNCTION("[ubxProtocol]", "ubxProtocolBackToBack")
{
    int32_t classIn;
    int32_t idIn;
    char *pBodyIn;
    int32_t classOut;
    int32_t idOut;
    char *pBodyOut;
    char *pBuffer;
    const char *pTmp;
    uint64_t z = 0xf0f1f2f3f4f5f6f7ULL;
    uint64_t intBuffer;

    pBodyIn = (char *) pUPortMalloc(U_UBX_PROTOCOL_TEST_MAX_BODY_SIZE);
    U_PORT_TEST_ASSERT(pBodyIn != NULL);
    pBodyOut = (char *) pUPortMalloc(U_UBX_PROTOCOL_TEST_MAX_BODY_SIZE);
    U_PORT_TEST_ASSERT(pBodyOut != NULL);
    pBuffer = (char *) pUPortMalloc(U_UBX_PROTOCOL_TEST_MAX_BODY_SIZE +
                                    U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(pBuffer != NULL);

    for (size_t x = 0; x < U_UBX_PROTOCOL_TEST_MAX_BODY_SIZE; x += 10) {
        // For each message size in steps of 10 perform an
        // encode and a decode
        for (size_t y = 0; y < x; y++) {
            //lint -e(613) Suppress possible nullness in pBodyIn, it is checked above
            *(pBodyIn + y) = (char) y;
        }
        classIn = x % 0xFF;
        idIn = (x + 16) % 0xFF;
        U_PORT_TEST_ASSERT(uUbxProtocolEncode(classIn, idIn, pBodyIn, x,
                                              pBuffer) == x + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES);
        //lint -e(650) Suppress constant out of range; it isn't
        U_PORT_TEST_ASSERT(*pBuffer == (char) 0xb5);
        U_PORT_TEST_ASSERT(*(pBuffer + 1) == 0x62);
        U_PORT_TEST_ASSERT(*(pBuffer + 2) == (char) classIn);
        U_PORT_TEST_ASSERT(*(pBuffer + 3) == (char) idIn);
        U_PORT_TEST_ASSERT(*(pBuffer + 4) == (char) x);
        U_PORT_TEST_ASSERT(*(pBuffer + 5) == (char) (x >> 8));
        //lint -e(668) Suppress possible nullness in pBodyOut, it is checked above
        memset(pBodyOut, 0xff, U_UBX_PROTOCOL_TEST_MAX_BODY_SIZE);
        U_PORT_TEST_ASSERT(uUbxProtocolDecode(pBuffer, x + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES,
                                              &classOut, &idOut, pBodyOut,
                                              U_UBX_PROTOCOL_TEST_MAX_BODY_SIZE, &pTmp) == x);
        U_PORT_TEST_ASSERT(classOut == classIn);
        U_PORT_TEST_ASSERT(idOut == idIn);
        U_PORT_TEST_ASSERT(pTmp == pBuffer + x + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES);
        //lint -e(668) Suppress possible nullness in pBodyIn, it is checked above
        U_PORT_TEST_ASSERT(memcmp(pBodyOut, pBodyIn, x) == 0);
        for (size_t y = x; y < U_UBX_PROTOCOL_TEST_MAX_BODY_SIZE; y++) {
            //lint -e(650) Suppress constant out of range; it isn't
            U_PORT_TEST_ASSERT(*(pBodyOut + y) == (char) 0xff);
        }
        // No very good way to test CRC here but check that changing it
        // in the encoded message causes a decode failure
        (*(pBuffer + x + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES - 1))++;
        U_PORT_TEST_ASSERT(uUbxProtocolDecode(pBuffer, x + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES,
                                              &classOut, &idOut, pBodyOut,
                                              U_UBX_PROTOCOL_TEST_MAX_BODY_SIZE, &pTmp) < 0);
    }

    // Test that the pointer parameters can be NULL
    U_PORT_TEST_ASSERT(uUbxProtocolEncode(classIn, idIn, pBodyIn, 10,
                                          pBuffer) == 10 + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(uUbxProtocolDecode(pBuffer, 10 + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES,
                                          NULL, NULL, NULL, 0, NULL) == 10);

    // Test the integer encode/decode functions
    // There is a bug in Zephyr (https://github.com/zephyrproject-rtos/zephyr/issues/30723)
    // where malloc does not return a pointer that is aligned for 64-bit
    // access (i.e. to an 8-byte boundary) so here we use intBuffer,
    // which is just a 64-bit variable, instead of using the more
    // obvious pBuffer.
    intBuffer = uUbxProtocolUint16Encode((uint16_t) z);
    U_PORT_TEST_ASSERT(uUbxProtocolUint16Decode((char *) &intBuffer) == (uint16_t) z);
    intBuffer = uUbxProtocolUint32Encode((uint32_t) z);
    U_PORT_TEST_ASSERT(uUbxProtocolUint32Decode((char *) &intBuffer) == (uint32_t) z);
    intBuffer = uUbxProtocolUint64Encode((uint64_t) z);
    U_PORT_TEST_ASSERT(uUbxProtocolUint64Decode((char *) &intBuffer) == z);

    // Free memory
    uPortFree(pBodyIn);
    uPortFree(pBodyOut);
    uPortFree(pBuffer);
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[ubxProtocol]", "ubxProtocolCleanUp")
{
    int32_t x;

    x = uPortTaskStackMinFree(NULL);
    if (x != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_TEST_PRINT_LINE("main task stack had a minimum of %d byte(s)"
                          " free at the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);
    }

    uPortDeinit();

    x = uPortGetHeapMinFree();
    if (x >= 0) {
        U_TEST_PRINT_LINE("heap had a minimum of %d byte(s) free"
                          " at the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

// End of file
