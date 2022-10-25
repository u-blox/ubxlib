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
 * @brief Test for the SPARTN API: these should pass on all platforms.
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
#include "stdlib.h"    // rand()
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

#include "u_spartn.h"
#include "u_spartn_crc.h"
#include "u_spartn_test_data.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_SPARTN_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#ifndef U_SPARTN_TEST_BUFFER_EXTRA_SIZE_BYTES
/** Amount of extra buffer space to provide over and above
 * #U_SPARTN_MESSAGE_LENGTH_MAX_BYTES when assembling a buffer
 * full of random stuff, plus a SPARTN message.
 */
# define U_SPARTN_TEST_BUFFER_EXTRA_SIZE_BYTES 50
#endif

#ifndef U_SPARTN_TEST_BUFFER_SIZE_BYTES
/** Buffer length for checking that the SPARTN message detection/
 * validation functions find the start of a message correctly.
 */
# define U_SPARTN_TEST_BUFFER_SIZE_BYTES (U_SPARTN_MESSAGE_LENGTH_MAX_BYTES + U_SPARTN_TEST_BUFFER_EXTRA_SIZE_BYTES)
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Struct to hold the test data for a CRC algorithm.
 */
typedef struct {
    uSpartnCrcType_t type;
    const char *pData;
    size_t size;
    uint32_t result;
} uSpartnTestCrc_t;

/** Struct to hold the test data for a SPARTN message.
 */
typedef struct {
    const char *pData;
    size_t size;
    uint32_t result;
} uSpartnTest_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** The test input (the one Phil Dubach used to test the original code
 * deilvery into ubxlib, just for consistency).
 */
static const char gpInput[] = { 'u', 'b', 'x', 'l', 'i', 'b', ' ', 'f', 'o', 'r', 'e', 'v', 'e', 'r' };

/** Test data for CRC-4; could not find an on-line
 * checker which had the right polynomial (x^3 + 1) and
 * reflected both inputs and outputs, hence the generated
 * result here is just that printed by the original code
 * as delivered into ubxlib when compiled under MSVC.
 */
static const uSpartnTestCrc_t gCrc4Ccitt = {
    .type = U_SPARTN_CRC_TYPE_4,
    .pData = gpInput,
    .size = sizeof(gpInput),
    .result = 0xa
};

/** Test data for CRC-8, result generated using all of:
 *
 * https://www.lddgo.net/en/encrypt/crc with setting
 * CRC-8 (x^8+x^2+x+1, 0 initial value, 0 XOR output).
 *
 * http://www.ghsi.de/pages/subpages/Online%20CRC%20Calculation
 * with polynomial 100000111.
 *
 * https://crccalc.com/ for type "CRC-8".
 *
 * result printed by the original code as delivered into ubxlib
 * when compiled under MSVC.
 */
static const uSpartnTestCrc_t gCrc8Ccitt = {
    .type = U_SPARTN_CRC_TYPE_8,
    .pData = gpInput,
    .size = sizeof(gpInput),
    .result = 0x9e
};

/** Test data for CRC-16, result generated using all of:
 *
 * https://www.lddgo.net/en/encrypt/crc with setting
 * CRC-16-XMODEM (x^16+x^12+x^5+1, 0 initial value, 0 XOR output).
 *
 * http://www.ghsi.de/pages/subpages/Online%20CRC%20Calculation
 * with polynomial 10001000000100001.
 *
 * https://crccalc.com/ for type "CRC-16/XMODEM".
 *
 * result printed by the original code as delivered into ubxlib
 * when compiled under MSVC.
 */
static const uSpartnTestCrc_t gCrc16Ccitt = {
    .type = U_SPARTN_CRC_TYPE_16,
    .pData = gpInput,
    .size = sizeof(gpInput),
    .result = 0x5664
};

/** Test data for CRC-32, result generated using:
 *
 * https://crccalc.com/ with type "CRC-32/BZIP2".
 *
 * result printed by the original code as delivered into ubxlib
 * when compiled under MSVC.
 */
static const uSpartnTestCrc_t gCrc32Ccitt = {
    .type = U_SPARTN_CRC_TYPE_32,
    .pData = gpInput,
    .size = sizeof(gpInput),
    .result = 0xE92E0360
};

/** Array of the CRC test data.
 */
static const uSpartnTestCrc_t *gpTestData[] = {&gCrc4Ccitt, &gCrc8Ccitt, &gCrc16Ccitt, &gCrc32Ccitt};

#ifndef __ZEPHYR__

/** A shortish valid SPARTN message.
 */
static const char gpSpartnMessage[] = {
    0x73, 0x02, 0xF1, 0xE8, 0x28, 0xBF, 0x33, 0xD0, 0xF0, 0x6C, 0x28, 0x08, 0x14, 0xDE, 0x18, 0x45,
    0x68, 0xFB, 0xB5, 0x07, 0x67, 0xD7, 0x29, 0xF2, 0xE9, 0x84, 0xCF, 0x12, 0x52, 0xEB, 0x04, 0x5F,
    0x8A, 0x5C, 0xE2, 0xB0, 0x17, 0x5C, 0x0F, 0xF2, 0xF5, 0x6F, 0x79, 0x5E, 0x47, 0x45, 0xDB, 0x56,
    0xAC, 0x9B, 0x32, 0xFC, 0xC5, 0xBC, 0x67, 0x77, 0xD8, 0x35, 0x3F, 0x75, 0x1F, 0x85, 0x6D, 0xA5,
    0x80, 0x0A, 0xFA, 0x4B, 0x54, 0x24, 0xC4, 0x78, 0x87, 0xAF, 0xD2, 0x1B, 0x5F, 0x0F, 0xE9, 0xBC,
    0x38, 0x5E, 0xEC, 0x1B, 0x69, 0xFB, 0x5B, 0xF8, 0x3B, 0xE2, 0xFC, 0xAA, 0xD6, 0x61, 0xD3, 0x41,
    0x9E, 0x82, 0x02, 0x45, 0x00, 0xA8, 0x9C, 0xD7, 0x42, 0x86, 0x7B, 0xB3, 0x57, 0x73, 0x1D, 0xF7,
    0x0C, 0x44, 0x86, 0xC4, 0xD5, 0x2B, 0x47, 0x74, 0xE9, 0x44, 0x59, 0xB1, 0xE5, 0x01, 0xF0, 0x98,
    0x7A, 0xE7, 0x72, 0x49, 0x1F, 0x1A, 0xC6, 0x5B, 0x3A, 0xAA, 0x9E, 0x21, 0x0E, 0xC2, 0x60, 0x59,
    0x7D, 0xCE, 0x55, 0xCC, 0x48, 0x06, 0x8E, 0x85, 0xBC, 0x62, 0xDD, 0x9A, 0xF3, 0xE2, 0x05, 0x8D,
    0x03, 0xE9, 0xF3, 0xD6, 0x9C, 0x46, 0xB2, 0xCE, 0x4B, 0x67, 0x83, 0x77, 0xB8, 0xFB, 0xE1, 0x23,
    0x5F, 0x63, 0x56, 0xEF, 0x91, 0x13, 0xC1, 0x02, 0x67, 0x5F, 0x3B, 0x49, 0x57, 0x1A, 0x24, 0xEC,
    0x8F, 0xE7, 0x90, 0x72, 0x6C, 0x07, 0x81, 0xCE, 0x71, 0x9F, 0xD2, 0x19, 0xE6, 0x78, 0x3A, 0x7A,
    0x22, 0xEA, 0x28, 0xD0, 0xEE, 0x7B, 0xBA, 0x4D, 0x7E, 0x68, 0x2B, 0xC4, 0x6A, 0x3B, 0x65, 0x9D,
    0x6F, 0xAD, 0xD4, 0x6C, 0xC4, 0x70, 0x71, 0xDB, 0x57, 0x22, 0x77, 0x82, 0x40, 0x3B, 0x9C, 0x88,
    0x2F, 0xB9, 0x1E, 0x1C, 0x30, 0xCC, 0x02, 0x46, 0xCD, 0xE0, 0x86, 0x3F, 0x61, 0xEC, 0x56, 0x12,
    0xE1, 0x94, 0x59, 0xBA, 0xF1, 0x24, 0x7C, 0x34, 0xFF, 0x17, 0x2B, 0x06, 0x98, 0xB0, 0xEB, 0x12,
    0xED, 0xF9, 0x75, 0x2B, 0x21, 0xDA, 0xBB, 0x26, 0x7D, 0xFD, 0x1D, 0x26, 0xAE, 0x00, 0xC4, 0x70,
    0x51, 0x10, 0xF9, 0xD0, 0x00, 0x1F, 0x73, 0x8E, 0x21, 0x79, 0xFE, 0x9C, 0xA7, 0xC7, 0xB4, 0xBA,
    0x53, 0xD1, 0x22, 0x92, 0xF9, 0xDA, 0x32, 0x1B, 0xA8, 0x44, 0x28, 0x86, 0x4C, 0x29, 0x9A, 0xBA,
    0x73, 0xE2, 0xE0, 0xEE, 0xBE, 0xE3, 0x55, 0x11, 0x6F, 0x77, 0x32, 0x9D, 0x64, 0xEA, 0x01, 0x7E,
    0xEF, 0xE0, 0x09, 0xCF, 0x7C, 0x00, 0xB4, 0x40, 0x18, 0x32, 0x6A, 0xC1, 0x20, 0xE9, 0x6B, 0x04,
    0xB6, 0xCA, 0xF2, 0x57, 0x7D, 0xAD, 0xEC, 0x63, 0xA3, 0xA5, 0xA9, 0xC0, 0x14, 0xB8, 0x45, 0xDD,
    0x00, 0xBE, 0xCF, 0x7A, 0x66, 0x77, 0x6B, 0x6A, 0x81, 0xF3, 0xA6, 0x29, 0x19, 0x7C, 0xEC, 0x48,
    0x64, 0xE1, 0x2F, 0x0F, 0x3F, 0x99, 0x88, 0x0B, 0xB5, 0xFA, 0xA7, 0xAA, 0xA2, 0x3D, 0xA0, 0x08,
    0x7B, 0x45, 0xB8, 0x31, 0xCE, 0xEB, 0xE5, 0xD3, 0x0D, 0x4A, 0x13, 0x38, 0x58, 0xDA, 0xC0, 0x21,
    0x9D, 0xEE, 0x6E, 0xDA, 0xE4, 0x25, 0xF6, 0x61, 0x31, 0xF2, 0xB8, 0xF1, 0x1D, 0xA7, 0x8E, 0xC8,
    0xB1, 0x47, 0xE8, 0x24, 0x3A, 0x52, 0x3A, 0x5D, 0x80, 0xE0, 0xFF, 0x75, 0x11, 0xAE, 0x78, 0x88,
    0xD6, 0x11, 0xF8, 0xFF, 0x5C, 0x60, 0x68, 0x14, 0x34, 0x74, 0x6D, 0x43, 0x9A, 0xAD, 0x1C, 0xFD,
    0xDB, 0xE5, 0x0D, 0xB1, 0x45, 0x59, 0x3F, 0x60, 0xD1, 0xC6, 0x3E, 0xDD, 0x61, 0xE6, 0x3C, 0xA8,
    0x04, 0x54, 0x67, 0x66, 0xA1, 0xBA, 0xA0, 0x52, 0x5D, 0x2D, 0xD0, 0x2A, 0x8D, 0x9E, 0xA8, 0xF1,
    0x8A, 0x27
};

#endif // __ZEPHYR__

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// The implementation of CRC-24 Radix 64 cut and pasted
// from https://datatracker.ietf.org/doc/html/rfc4880#page-59.
#define CRC24_INIT 0
#define CRC24_POLY 0x1864CFBL
typedef long crc24;
static crc24 crc_octets(unsigned char *octets, size_t len)
{
    crc24 crc = CRC24_INIT;
    int i;
    while (len--) {
        crc ^= (*octets++) << 16;
        for (i = 0; i < 8; i++) {
            crc <<= 1;
            if (crc & 0x1000000) {
                crc ^= CRC24_POLY;
            }
        }
    }
    return crc & 0xFFFFFFL;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TESTS
 * -------------------------------------------------------------- */

/** Test CRCs.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */
U_PORT_TEST_FUNCTION("[spartn]", "spartnCrc")
{
    int32_t heapUsed;
    const uSpartnTestCrc_t *pTestData;
    uint32_t calculated;
    uint32_t expected;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    U_TEST_PRINT_LINE("testing CRCs.");

    for (size_t x = 0; x < sizeof(gpTestData) / sizeof(gpTestData[0]); x++) {
        pTestData = gpTestData[x];
        switch (pTestData->type) {
            case U_SPARTN_CRC_TYPE_4:
                calculated = uSpartnCrc4(pTestData->pData, pTestData->size);
                U_TEST_PRINT_LINE("CRC-4: calculated 0x%1x, expected 0x%1x.", calculated, pTestData->result);
                U_PORT_TEST_ASSERT(calculated == (uint8_t) pTestData->result);
                break;
            case U_SPARTN_CRC_TYPE_8:
                calculated = uSpartnCrc8(pTestData->pData, pTestData->size);
                U_TEST_PRINT_LINE("CRC-8: calculated 0x%02x, expected 0x%02x.", calculated, pTestData->result);
                U_PORT_TEST_ASSERT(calculated == (uint8_t) pTestData->result);
                break;
            case U_SPARTN_CRC_TYPE_16:
                calculated = uSpartnCrc16(pTestData->pData, pTestData->size);
                U_TEST_PRINT_LINE("CRC-16: calculated 0x%04x, expected 0x%04x.", calculated, pTestData->result);
                U_PORT_TEST_ASSERT(calculated == (uint16_t) pTestData->result);
                break;
            case U_SPARTN_CRC_TYPE_32:
                calculated = uSpartnCrc32(pTestData->pData, pTestData->size);
                U_TEST_PRINT_LINE("CRC-32: calculated 0x%08x, expected 0x%08x.", calculated, pTestData->result);
                U_PORT_TEST_ASSERT(calculated == pTestData->result);
                break;
            default:
                U_PORT_TEST_ASSERT(false);
                break;
        }
    }

    // CRC24 Radix 64 is tested directly against the sample code provided in RFC4880
    calculated = uSpartnCrc24(gpInput, sizeof(gpInput));
    expected = crc_octets((unsigned char *) gpInput, sizeof(gpInput));
    U_TEST_PRINT_LINE("CRC-24: calculated 0x%08x, expected 0x%08x.", calculated, expected);
    U_PORT_TEST_ASSERT(calculated == expected);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed == 0) || (heapUsed == (int32_t)U_ERROR_COMMON_NOT_SUPPORTED));
}

#ifndef __ZEPHYR__

/** Testing of the SPARTN protocol utility functions against
 * SPARTN message data kept in u_spartn_test_data.c.
 *
 * Note that we do not run these tests on Zephyr because it has
 * proved pretty much impossible to get Zephyr-on-NRF52 to provide a
 * working rand() function; the maze of KConfig possiblities is just
 * too great for anyone, including Nordic support, to navigate to a
 * successful conclusion in our case; either KConfig errors result
 * or the rand() function causes a memory exception when called.
 * So we gave up.
 *
 * This is not a huge problem as none of the ubxlib operations here are
 * likely to be platform specific in nature, testing on the other
 * platforms should suffice.
 */
U_PORT_TEST_FUNCTION("[spartn]", "spartnMessage")
{
    int32_t heapUsed;
    uint32_t messageCount;
    size_t y;
    size_t offset;
    const char *pData;
    const char *pMessage;
    int32_t messageLength;
    char *pBuffer;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    U_TEST_PRINT_LINE("testing SPARTN message parsing.");

    // Pass random lengths of data at random offsets,
    // stepping through the data set, detecting SPARTN
    // messages and then, on the second time around the loop,
    // validating them
    for (size_t x = 0; x < 2; x++) {
        messageCount = 0;
        pData = gUSpartnTestData;
        y = 0;
        while ((pData - gUSpartnTestData) + y < gUSpartnTestDataSize) {
            y += (rand() % 100) + 1;
            if (pData + y >= gUSpartnTestData + gUSpartnTestDataSize) {
                y = gUSpartnTestData + gUSpartnTestDataSize - pData;
            }
            pMessage = NULL;
            if (x == 0) {
                messageLength = uSpartnDetect(pData, y, &pMessage);
            } else {
                messageLength = uSpartnValidate(pData, y, &pMessage);
            }
            if (messageLength > 0) {
                messageCount++;
                U_PORT_TEST_ASSERT(pMessage != NULL);
                U_PORT_TEST_ASSERT(messageLength <= U_SPARTN_MESSAGE_LENGTH_MAX_BYTES);
                pData = pMessage + messageLength;
                y = 0;
            } else {
                U_PORT_TEST_ASSERT(pMessage == NULL);
            }
        }
        if (x == 0) {
            uPortLog(U_TEST_PREFIX "detected");
        } else {
            uPortLog(U_TEST_PREFIX "validated");
        }
        uPortLog(" %d message(s) out of %d.\n", messageCount, gUSpartnTestDataNumMessages);
        U_PORT_TEST_ASSERT(messageCount == gUSpartnTestDataNumMessages);
    }

    // Fill a buffer full of random rubbish, drop a valid
    // SPARTN message into it at a random offset and check
    // that the start of the SPARTN message is reported
    // correctly; can't do this for uSpartnDetect as the CRC-4
    // on the header provides only light protection, it is
    // possible to detect a SPARTN message header in the random
    // data.
    pBuffer = (char *) pUPortMalloc(U_SPARTN_TEST_BUFFER_SIZE_BYTES);
    U_PORT_TEST_ASSERT(pBuffer != NULL);

    // Do this lots of times for good randomness
    for (size_t z = 0; z < 1000; z++) {
        for (size_t x = 0; x < U_SPARTN_TEST_BUFFER_SIZE_BYTES; x++) {
            *(pBuffer + x) = (char) rand();
        }
        // Copy the valid message into the buffer at a random offset
        y = rand() % U_SPARTN_TEST_BUFFER_EXTRA_SIZE_BYTES;
        memcpy(pBuffer + y, gpSpartnMessage, sizeof(gpSpartnMessage));
        // Need to call the function multiple times as it may
        // return U_ERROR_COMMON_NOT_FOUND if it finds what looks
        // like a partial SPARTN message in the random data that
        // then fails CRC checking.  Hopefully.
        offset = 0;
        messageLength = 0;
        for (size_t x = 0; (x < U_SPARTN_TEST_BUFFER_EXTRA_SIZE_BYTES) &&
             (messageLength != sizeof(gpSpartnMessage)); x++) {
            pMessage = NULL;
            messageLength = uSpartnValidate(pBuffer + offset,
                                            U_SPARTN_TEST_BUFFER_SIZE_BYTES,
                                            &pMessage);
            if (messageLength == (int32_t) U_ERROR_COMMON_NOT_FOUND) {
                // If there was what looks like the start of a
                // SPARTN message in the random data, which will
                // have failed the CRC check, move the offset
                // forward for next time so that we eventually get
                // past it
                offset++;
            }
        }
        U_PORT_TEST_ASSERT(pMessage == pBuffer + y);
        U_PORT_TEST_ASSERT(messageLength == sizeof(gpSpartnMessage));
    }

    // Check that we can call the functions with pMessage left as NULL
    U_PORT_TEST_ASSERT(uSpartnDetect(pBuffer,
                                     U_SPARTN_TEST_BUFFER_SIZE_BYTES,
                                     NULL) == sizeof(gpSpartnMessage));
    U_PORT_TEST_ASSERT(uSpartnValidate(pBuffer,
                                       U_SPARTN_TEST_BUFFER_SIZE_BYTES,
                                       NULL) == sizeof(gpSpartnMessage));

    // Free memory
    uPortFree(pBuffer);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed == 0) || (heapUsed == (int32_t)U_ERROR_COMMON_NOT_SUPPORTED));
}

#endif // __ZEPHYR__

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[spartn]", "spartnCleanUp")
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
