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
 * @brief Test for the ringbuffer API
 */


#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // strncpy(), strcmp(), memcpy(), memset()

#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"

#include "u_ringbuffer.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_RINGBUFFER_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

/** The maximum number of read handles to use when
 * testing the "read handle" form of ring buffer.
 */
#define U_TEST_UTILS_RINGBUFFER_READ_HANDLES_MAX_NUM 2

#ifndef U_TEST_UTILS_RINGBUFFER_SIZE
/** The ring buffer size to test.
 */
# define U_TEST_UTILS_RINGBUFFER_SIZE 10
#endif

#ifndef U_TEST_UTILS_RINGBUFFER_FILL_CHAR
/** The fill character to use when testing.
 */
# define U_TEST_UTILS_RINGBUFFER_FILL_CHAR 0x5a
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

// Print out binary.
static void printBuffer(const char *pTitle, const char *pStr, size_t length)
{
    char c;

    if (pTitle != NULL) {
        uPortLog(U_TEST_PREFIX "%s: ", pTitle);
    }
    for (size_t x = 0; x < length; x++) {
        c = *pStr++;
        uPortLog("[%02x]", c);
    }
    if (pTitle != NULL) {
        uPortLog(" (buffer size %d bytes).\n", length);
    }
    // Some platforms have problems with lots of debug strings at
    // once, so pause to let those have a breather
    uPortTaskBlock(10);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TESTS
 * -------------------------------------------------------------- */

U_PORT_TEST_FUNCTION("[ringbuffer]", "ringbufferBasic")
{
    int32_t heapUsed;
    uRingBuffer_t ringBuffer = {0};
    char linearBuffer[U_TEST_UTILS_RINGBUFFER_SIZE + 1];
    char bufferOut[U_TEST_UTILS_RINGBUFFER_SIZE + 1];
    char bufferIn[U_TEST_UTILS_RINGBUFFER_SIZE + 1];
    size_t handle[U_TEST_UTILS_RINGBUFFER_READ_HANDLES_MAX_NUM];
    size_t addLoss = 0;
    size_t readLoss = 0;
    size_t readLossHandle[U_TEST_UTILS_RINGBUFFER_READ_HANDLES_MAX_NUM] = {0};
    char b = ~U_TEST_UTILS_RINGBUFFER_FILL_CHAR;
    size_t y;
    size_t z;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    U_TEST_PRINT_LINE("testing ring buffer.");
    for (size_t x = 0; x < sizeof(bufferIn); x++) {
        bufferIn[x] = (char) x;
    }
    printBuffer(" test data is", bufferIn, sizeof(bufferIn));
    memset(linearBuffer, 0, sizeof(linearBuffer));
    printBuffer(" ring buffer starts out as", linearBuffer, sizeof(linearBuffer));

    // Try to do stuff to an uninitialised ring buffer, should fail
    // or return nothing
    U_TEST_PRINT_LINE("testing uninitialised ring buffer [with handles]...");
    U_PORT_TEST_ASSERT(!uRingBufferAdd(&ringBuffer, bufferIn, 5));
    U_PORT_TEST_ASSERT(!uRingBufferForceAdd(&ringBuffer, bufferIn, 5));
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(uRingBufferDataSize(&ringBuffer) == 0);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == 0);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) == 0);
    U_PORT_TEST_ASSERT(uRingBufferRead(&ringBuffer, bufferOut, sizeof(bufferOut)) == 0);
    U_PORT_TEST_ASSERT(uRingBufferRead(&ringBuffer, bufferOut, sizeof(bufferOut)) == 0);
    U_PORT_TEST_ASSERT(bufferOut[0] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(uRingBufferTakeReadHandle(&ringBuffer) < 0);
    U_PORT_TEST_ASSERT(uRingBufferDataSizeHandle(&ringBuffer, 1) == 0);
    U_PORT_TEST_ASSERT(uRingBufferStatReadLoss(&ringBuffer) == readLoss);
    U_PORT_TEST_ASSERT(uRingBufferStatAddLoss(&ringBuffer) == addLoss);
    U_PORT_TEST_ASSERT(uRingBufferStatReadLossHandle(&ringBuffer, 1) == readLossHandle[1]);

    // Now create a ring buffer (with handles) and try to read data from it
    // with no data added
    U_TEST_PRINT_LINE("testing reads from an empty ring buffer...");
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(uRingBufferCreateWithReadHandle(&ringBuffer, linearBuffer, sizeof(linearBuffer),
                                                       U_TEST_UTILS_RINGBUFFER_READ_HANDLES_MAX_NUM) == 0);
    U_PORT_TEST_ASSERT(!uRingBufferGetReadRequiresHandle(&ringBuffer));
    U_PORT_TEST_ASSERT(uRingBufferDataSize(&ringBuffer) == 0);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == sizeof(linearBuffer) - 1);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) == sizeof(linearBuffer) - 1);
    U_PORT_TEST_ASSERT(uRingBufferRead(&ringBuffer, bufferOut, sizeof(bufferOut)) == 0);
    U_PORT_TEST_ASSERT(bufferOut[0] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    handle[0] = uRingBufferTakeReadHandle(&ringBuffer);
    U_PORT_TEST_ASSERT(uRingBufferDataSizeHandle(&ringBuffer, handle[0]) == 0);
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(uRingBufferReadHandle(&ringBuffer, handle[0], bufferOut,
                                             sizeof(bufferOut)) == 0);
    U_PORT_TEST_ASSERT(bufferOut[0] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    uRingBufferGiveReadHandle(&ringBuffer, handle[0]);

    // Add one byte of data and read it
    U_TEST_PRINT_LINE("testing the addition of one byte of data...");
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    printBuffer("  output buffer initially contains", bufferOut, sizeof(bufferOut));
    handle[0] = uRingBufferTakeReadHandle(&ringBuffer);
    U_TEST_PRINT_LINE(" adding 1 byte of data, value 0x%02x.", b);
    U_PORT_TEST_ASSERT(uRingBufferAdd(&ringBuffer, &b, sizeof(b)));
    printBuffer("  ring buffer now contains", linearBuffer, sizeof(linearBuffer));
    U_PORT_TEST_ASSERT(uRingBufferDataSize(&ringBuffer) == sizeof(b));
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == sizeof(linearBuffer) - 1 - sizeof(b));
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) == sizeof(linearBuffer) - 1);
    // Now do the reading part, normal peek first
    y = uRingBufferPeek(&ringBuffer, bufferOut, sizeof(bufferOut), 0);
    U_TEST_PRINT_LINE(" normal peek returned %d byte(s), %d byte(s) still in the buffer.",
                      y, sizeof(linearBuffer) - 1 - uRingBufferAvailableSize(&ringBuffer));
    U_PORT_TEST_ASSERT(y == sizeof(b));
    printBuffer("  output buffer now contains", bufferOut, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(bufferOut[0] == b);
    U_PORT_TEST_ASSERT(bufferOut[1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    // Now a normal read
    y = uRingBufferRead(&ringBuffer, bufferOut, sizeof(bufferOut));
    U_TEST_PRINT_LINE(" normal read returned %d byte(s), %d byte(s) still in the buffer.",
                      y, sizeof(linearBuffer) - 1 - uRingBufferAvailableSize(&ringBuffer));
    U_PORT_TEST_ASSERT(y == sizeof(b));
    printBuffer("  output buffer now contains", bufferOut, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(bufferOut[0] == b);
    U_PORT_TEST_ASSERT(bufferOut[1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(uRingBufferDataSize(&ringBuffer) == 0);
    // The available size won't change as we have a "handled read" that has
    // not yet consumed the new data
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == sizeof(linearBuffer) - 1 - sizeof(b));
    // Available size max will have changed though, as the handle is not locked
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) == sizeof(linearBuffer) - 1);
    U_PORT_TEST_ASSERT(uRingBufferRead(&ringBuffer, bufferOut, sizeof(bufferOut)) == 0);
    U_PORT_TEST_ASSERT(bufferOut[0] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(uRingBufferDataSizeHandle(&ringBuffer, handle[0]) == sizeof(b));
    // Now the "handled" peek
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    printBuffer("  output buffer reset to", bufferOut, sizeof(bufferOut));
    y = uRingBufferPeekHandle(&ringBuffer, handle[0], bufferOut, sizeof(bufferOut), 0);
    U_TEST_PRINT_LINE(" peek using handle 0x%08x returned %d byte(s), %d byte(s) still in the buffer.",
                      handle[0], y, sizeof(linearBuffer) - 1 - uRingBufferAvailableSize(&ringBuffer));
    U_PORT_TEST_ASSERT(y == sizeof(b));
    printBuffer("  output buffer now contains", bufferOut, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(bufferOut[0] == b);
    U_PORT_TEST_ASSERT(bufferOut[1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    // Now the "handled" read
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    printBuffer("  output buffer reset to", bufferOut, sizeof(bufferOut));
    y = uRingBufferReadHandle(&ringBuffer, handle[0], bufferOut, sizeof(bufferOut));
    U_TEST_PRINT_LINE(" read using handle 0x%08x returned %d byte(s), %d byte(s) still in the buffer.",
                      handle[0], y, sizeof(linearBuffer) - 1 - uRingBufferAvailableSize(&ringBuffer));
    U_PORT_TEST_ASSERT(y == sizeof(b));
    printBuffer("  output buffer now contains", bufferOut, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(bufferOut[0] == b);
    U_PORT_TEST_ASSERT(bufferOut[1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(uRingBufferDataSizeHandle(&ringBuffer, handle[0]) == 0);
    // Now the whole ring buffer should be available again
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == sizeof(linearBuffer) - 1);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) == sizeof(linearBuffer) - 1);
    U_PORT_TEST_ASSERT(bufferOut[0] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(uRingBufferStatReadLoss(&ringBuffer) == readLoss);
    U_PORT_TEST_ASSERT(uRingBufferStatAddLoss(&ringBuffer) == addLoss);
    U_PORT_TEST_ASSERT(uRingBufferStatReadLossHandle(&ringBuffer, handle[0]) == readLossHandle[0]);
    uRingBufferGiveReadHandle(&ringBuffer, handle[0]);

    // Add the maximum number of bytes of data possible and
    // read them all out
    U_TEST_PRINT_LINE("testing max data (%d byte(s))...", sizeof(bufferIn) - 1);
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    printBuffer("  output buffer reset to", bufferOut, sizeof(bufferOut));
    handle[0] = uRingBufferTakeReadHandle(&ringBuffer);
    U_TEST_PRINT_LINE(" adding %d byte(s).", sizeof(bufferIn) - 1);
    U_PORT_TEST_ASSERT(uRingBufferAdd(&ringBuffer, bufferIn, sizeof(bufferIn) - 1));
    // Should not be able to add any more (without forcing)
    U_PORT_TEST_ASSERT(!uRingBufferAdd(&ringBuffer, &b, 1));
    // This should show up as a loss of one on add
    addLoss++;
    printBuffer("  ring buffer now contains", linearBuffer, sizeof(linearBuffer));
    U_PORT_TEST_ASSERT(uRingBufferDataSize(&ringBuffer) == sizeof(bufferIn) - 1);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == 0);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) == sizeof(bufferIn) - 1);
    // Now do the reading part, normal peek with non-zero offset first
    y = uRingBufferPeek(&ringBuffer, bufferOut, sizeof(bufferOut), sizeof(bufferIn) / 2);
    U_TEST_PRINT_LINE(" normal peek with offset %d returned %d byte(s), %d byte(s) still in the buffer.",
                      sizeof(bufferIn) / 2, y, sizeof(linearBuffer) - 1 - uRingBufferAvailableSize(&ringBuffer));
    U_PORT_TEST_ASSERT(y == sizeof(bufferIn) - 1 - sizeof(bufferIn) / 2);
    printBuffer("  output buffer now contains", bufferOut, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(memcmp(bufferOut, bufferIn + sizeof(bufferIn) / 2,
                              sizeof(bufferIn) - 1 - sizeof(bufferIn) / 2) == 0);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1 - sizeof(bufferIn) / 2] ==
                       U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    // Now do the normal read part
    y = uRingBufferRead(&ringBuffer, bufferOut, sizeof(bufferOut));
    U_TEST_PRINT_LINE(" normal read returned %d byte(s), %d byte(s) still in the buffer.",
                      y, sizeof(linearBuffer) - 1 - uRingBufferAvailableSize(&ringBuffer));
    U_PORT_TEST_ASSERT(y == sizeof(bufferIn) - 1);
    printBuffer("  output buffer now contains", bufferOut, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(memcmp(bufferOut, bufferIn, sizeof(bufferIn) - 1) == 0);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(uRingBufferDataSize(&ringBuffer) == 0);
    // The available size won't change as we have a "handled read" that has
    // not yet consumed the new data
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == 0);
    // Max is there though
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) == sizeof(linearBuffer) - 1);
    U_PORT_TEST_ASSERT(uRingBufferRead(&ringBuffer, bufferOut, sizeof(bufferOut)) == 0);
    U_PORT_TEST_ASSERT(bufferOut[0] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(uRingBufferDataSizeHandle(&ringBuffer, handle[0]) == sizeof(bufferIn) - 1);
    // Now the "handled" peek
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    y = uRingBufferPeekHandle(&ringBuffer, handle[0], bufferOut, sizeof(bufferOut),
                              sizeof(bufferIn) / 2);
    U_TEST_PRINT_LINE(" peek using handle 0x%08x with offset %d returned %d byte(s), %d byte(s) still in the buffer.",
                      handle[0], sizeof(bufferIn) / 2, y,
                      sizeof(linearBuffer) - 1 - uRingBufferAvailableSize(&ringBuffer));
    U_PORT_TEST_ASSERT(y == sizeof(bufferIn) - 1 - sizeof(bufferIn) / 2);
    printBuffer("  output buffer now contains", bufferOut, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(memcmp(bufferOut, bufferIn + sizeof(bufferIn) / 2,
                              sizeof(bufferIn) - 1 - sizeof(bufferIn) / 2) == 0);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1 - sizeof(bufferIn) / 2] ==
                       U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    // Now the "handled" read
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    printBuffer("  output buffer reset to", bufferOut, sizeof(bufferOut));
    y = uRingBufferReadHandle(&ringBuffer, handle[0], bufferOut, sizeof(bufferOut));
    U_TEST_PRINT_LINE(" read using handle 0x%08x returned %d byte(s), %d byte(s) still in the buffer.",
                      handle[0], y, sizeof(linearBuffer) - 1 - uRingBufferAvailableSize(&ringBuffer));
    U_PORT_TEST_ASSERT(y == sizeof(bufferIn) - 1);
    printBuffer("  output buffer now contains", bufferOut, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(memcmp(bufferOut, bufferIn, sizeof(bufferIn) - 1) == 0);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(uRingBufferDataSizeHandle(&ringBuffer, handle[0]) == 0);
    // Now the whole ring buffer should be available again
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == sizeof(linearBuffer) - 1);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) == sizeof(linearBuffer) - 1);
    U_PORT_TEST_ASSERT(bufferOut[0] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    uRingBufferGiveReadHandle(&ringBuffer, handle[0]);

    // Try to add more than the maximum number of bytes of data possible
    U_TEST_PRINT_LINE("testing more than max data (%d byte(s))...", sizeof(bufferIn));
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    printBuffer("  output buffer reset to", bufferOut, sizeof(bufferOut));
    handle[0] = uRingBufferTakeReadHandle(&ringBuffer);
    U_PORT_TEST_ASSERT(!uRingBufferAdd(&ringBuffer, bufferIn, sizeof(bufferIn)));
    // This should show up as a loss on add
    addLoss += sizeof(bufferIn);
    U_PORT_TEST_ASSERT(!uRingBufferForceAdd(&ringBuffer, bufferIn, sizeof(bufferIn)));
    // This should show up as a loss on add
    addLoss += sizeof(bufferIn);
    U_PORT_TEST_ASSERT(uRingBufferDataSize(&ringBuffer) == 0);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == sizeof(linearBuffer) - 1);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) == sizeof(linearBuffer) - 1);
    U_PORT_TEST_ASSERT(uRingBufferRead(&ringBuffer, bufferOut, sizeof(bufferOut)) == 0);
    U_PORT_TEST_ASSERT(bufferOut[0] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(uRingBufferDataSizeHandle(&ringBuffer, handle[0]) == 0);
    U_PORT_TEST_ASSERT(uRingBufferReadHandle(&ringBuffer, handle[0], bufferOut,
                                             sizeof(bufferOut)) == 0);
    U_PORT_TEST_ASSERT(bufferOut[0] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(uRingBufferStatReadLoss(&ringBuffer) == readLoss);
    U_PORT_TEST_ASSERT(uRingBufferStatAddLoss(&ringBuffer) == addLoss);
    U_PORT_TEST_ASSERT(uRingBufferStatReadLossHandle(&ringBuffer, handle[0]) == readLossHandle[0]);
    uRingBufferGiveReadHandle(&ringBuffer, handle[0]);

    // Check that we can set "handled reads" only and that data
    // munging works in that case
    U_TEST_PRINT_LINE("testing \"handled reads only\" case...");
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    printBuffer("  output buffer reset to", bufferOut, sizeof(bufferOut));
    uRingBufferSetReadRequiresHandle(&ringBuffer, true);
    U_PORT_TEST_ASSERT(uRingBufferGetReadRequiresHandle(&ringBuffer));
    handle[0] = uRingBufferTakeReadHandle(&ringBuffer);
    U_TEST_PRINT_LINE(" adding %d byte(s).", sizeof(bufferIn) - 1);
    U_PORT_TEST_ASSERT(uRingBufferAdd(&ringBuffer, bufferIn, sizeof(bufferIn) - 1));
    printBuffer("ring buffer now contains", linearBuffer, sizeof(linearBuffer));
    // This always returns zero if a handled read is required
    U_PORT_TEST_ASSERT(uRingBufferDataSize(&ringBuffer) == 0);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == 0);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) ==  sizeof(linearBuffer) - 1);
    // A normal read should return nothing
    y = uRingBufferRead(&ringBuffer, bufferOut, sizeof(bufferOut));
    U_TEST_PRINT_LINE(" normal read returned %d byte(s), %d byte(s) still in the buffer.",
                      y, sizeof(linearBuffer) - 1 - uRingBufferAvailableSize(&ringBuffer));
    U_PORT_TEST_ASSERT(y == 0);
    printBuffer("  output buffer now contains", bufferOut, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(bufferOut[0] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(uRingBufferDataSize(&ringBuffer) == 0);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == 0);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) ==  sizeof(linearBuffer) - 1);
    U_PORT_TEST_ASSERT(uRingBufferDataSizeHandle(&ringBuffer, handle[0]) == sizeof(bufferIn) - 1);
    // Now the "handled" read
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    printBuffer("  output buffer reset to", bufferOut, sizeof(bufferOut));
    y = uRingBufferReadHandle(&ringBuffer, handle[0], bufferOut, sizeof(bufferOut));
    U_TEST_PRINT_LINE(" read using handle 0x%08x returned %d byte(s), %d byte(s) still in the buffer.",
                      handle[0], y, sizeof(linearBuffer) - 1 - uRingBufferAvailableSize(&ringBuffer));
    U_PORT_TEST_ASSERT(y == sizeof(bufferIn) - 1);
    printBuffer("  output buffer now contains", bufferOut, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(memcmp(bufferOut, bufferIn, sizeof(bufferIn) - 1) == 0);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(uRingBufferDataSizeHandle(&ringBuffer, handle[0]) == 0);
    // Now the whole ring buffer should be available again
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == sizeof(linearBuffer) - 1);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) == sizeof(linearBuffer) - 1);
    U_PORT_TEST_ASSERT(bufferOut[0] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(uRingBufferStatReadLoss(&ringBuffer) == readLoss);
    U_PORT_TEST_ASSERT(uRingBufferStatAddLoss(&ringBuffer) == addLoss);
    U_PORT_TEST_ASSERT(uRingBufferStatReadLossHandle(&ringBuffer, handle[0]) == readLossHandle[0]);
    uRingBufferGiveReadHandle(&ringBuffer, handle[0]);
    uRingBufferSetReadRequiresHandle(&ringBuffer, false);
    U_PORT_TEST_ASSERT(!uRingBufferGetReadRequiresHandle(&ringBuffer));

    // Add one less than the maximum number of bytes of data possible and
    // read them out one at a time, this time with two read handles
    U_TEST_PRINT_LINE("testing incremental reads and two handles...");
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    printBuffer("  output buffer reset to", bufferOut, sizeof(bufferOut));
    handle[0] = uRingBufferTakeReadHandle(&ringBuffer);
    handle[1] = uRingBufferTakeReadHandle(&ringBuffer);
    // Should not be able to obtain any more handles
    U_PORT_TEST_ASSERT(uRingBufferTakeReadHandle(&ringBuffer) < 0);
    U_TEST_PRINT_LINE(" adding %d byte(s).", sizeof(bufferIn) - 2);
    U_PORT_TEST_ASSERT(uRingBufferAdd(&ringBuffer, bufferIn, sizeof(bufferIn) - 2));
    printBuffer("  ring buffer now contains", linearBuffer, sizeof(linearBuffer));
    U_PORT_TEST_ASSERT(uRingBufferDataSize(&ringBuffer) == sizeof(bufferIn) - 2);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == 1);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) == sizeof(linearBuffer) - 1);
    // Now do the reading part, normal read first
    z = 0;
    while (z < sizeof(bufferIn) - 2) {
        y = uRingBufferRead(&ringBuffer, bufferOut + z, 1);
        U_PORT_TEST_ASSERT(y == 1);
        z += y;
        U_PORT_TEST_ASSERT(uRingBufferDataSize(&ringBuffer) == sizeof(bufferIn) - 2 - z);
        // Part way through, do a debug print, just to prove it doesn't crash anything
        if (z == (sizeof(bufferIn) - 2) / 2) {
            U_TEST_PRINT_LINE("  with %d byte(s) \"normal\" data size, %d byte(s) in handle %d,"
                              " %d byte(s) in handle %d and %d byte(s) free, debug is:",
                              uRingBufferDataSize(&ringBuffer),
                              uRingBufferDataSizeHandle(&ringBuffer, handle[0]), handle[0],
                              uRingBufferDataSizeHandle(&ringBuffer, handle[1]), handle[1],
                              uRingBufferAvailableSize(&ringBuffer));
            uRingBufferDump(&ringBuffer);
        }
    }
    U_TEST_PRINT_LINE(" \"normally\" read a total of %d byte(s),"
                      " %d byte(s) still in the buffer.", z,
                      sizeof(linearBuffer) - 1 - uRingBufferAvailableSize(&ringBuffer));
    printBuffer("  output buffer now contains", bufferOut, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(memcmp(bufferOut, bufferIn, sizeof(bufferIn) - 2) == 0);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 2] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(uRingBufferDataSize(&ringBuffer) == 0);
    U_PORT_TEST_ASSERT(uRingBufferRead(&ringBuffer, bufferOut, sizeof(bufferOut)) == 0);
    // The available size won't change as we have a "handled read" that has
    // not yet consumed the new data
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == 1);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) == sizeof(linearBuffer) - 1);
    // First handle
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(uRingBufferDataSizeHandle(&ringBuffer, handle[0]) == sizeof(bufferIn) - 2);
    printBuffer("  output buffer reset to", bufferOut, sizeof(bufferOut));
    z = 0;
    while (z < sizeof(bufferIn) - 2) {
        y = uRingBufferReadHandle(&ringBuffer, handle[0], bufferOut + z, 1);
        U_PORT_TEST_ASSERT(y == 1);
        z += y;
        U_PORT_TEST_ASSERT(uRingBufferDataSizeHandle(&ringBuffer, handle[0]) == sizeof(bufferIn) - 2 - z);
        // Another one-off debug print
        if (z == (sizeof(bufferIn) - 2) / 2) {
            U_TEST_PRINT_LINE("  with %d byte(s) \"normal\" data size, %d byte(s) in handle %d,"
                              " %d byte(s) in handle %d and %d byte(s) free, debug is:",
                              uRingBufferDataSize(&ringBuffer),
                              uRingBufferDataSizeHandle(&ringBuffer, handle[0]), handle[0],
                              uRingBufferDataSizeHandle(&ringBuffer, handle[1]), handle[1],
                              uRingBufferAvailableSize(&ringBuffer));
            uRingBufferDump(&ringBuffer);
        }
    }
    U_TEST_PRINT_LINE(" read using handle 0x%08x returned a total of %d byte(s), %d byte(s) still in the buffer.",
                      handle[0], z, sizeof(linearBuffer) - 1 - uRingBufferAvailableSize(&ringBuffer));
    printBuffer("  output buffer now contains", bufferOut, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(memcmp(bufferOut, bufferIn, sizeof(bufferIn) - 2) == 0);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 2] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(uRingBufferDataSize(&ringBuffer) == 0);
    U_PORT_TEST_ASSERT(uRingBufferReadHandle(&ringBuffer, handle[0], bufferOut,
                                             sizeof(bufferOut)) == 0);
    // The available size won't change as we have another "handled read" that has
    // not yet consumed the new data
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == 1);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) == sizeof(linearBuffer) - 1);
    // Second handle
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(uRingBufferDataSizeHandle(&ringBuffer, handle[1]) == sizeof(bufferIn) - 2);
    printBuffer("  output buffer reset to", bufferOut, sizeof(bufferOut));
    z = 0;
    while (z < sizeof(bufferIn) - 2) {
        if (z == (sizeof(bufferIn) - 2) / 2) {
            U_TEST_PRINT_LINE("  with %d byte(s) \"normal\" data size, %d byte(s) in handle %d,"
                              " %d byte(s) in handle %d and %d byte(s) free, debug is:",
                              uRingBufferDataSize(&ringBuffer),
                              uRingBufferDataSizeHandle(&ringBuffer, handle[0]), handle[0],
                              uRingBufferDataSizeHandle(&ringBuffer, handle[1]), handle[1],
                              uRingBufferAvailableSize(&ringBuffer));
            uRingBufferDump(&ringBuffer);
        }
        y = uRingBufferReadHandle(&ringBuffer, handle[1], bufferOut + z, 1);
        U_PORT_TEST_ASSERT(y == 1);
        z += y;
        U_PORT_TEST_ASSERT(uRingBufferDataSizeHandle(&ringBuffer, handle[1]) == sizeof(bufferIn) - 2 - z);
        // Now the available size should increase each time
        U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == 1 + z);
        // Another one-off debug print
    }
    U_TEST_PRINT_LINE(" read using handle 0x%08x returned a total of %d byte(s), %d byte(s) still in the buffer.",
                      handle[1], z, sizeof(linearBuffer) - 1 - uRingBufferAvailableSize(&ringBuffer));
    printBuffer("  output buffer now contains", bufferOut, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(memcmp(bufferOut, bufferIn, sizeof(bufferIn) - 2) == 0);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 2] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(uRingBufferDataSize(&ringBuffer) == 0);
    U_PORT_TEST_ASSERT(uRingBufferReadHandle(&ringBuffer, handle[1], bufferOut,
                                             sizeof(bufferOut)) == 0);
    // Available bytes should now be back at the maximum
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == sizeof(linearBuffer) - 1);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) == sizeof(linearBuffer) - 1);
    U_PORT_TEST_ASSERT(uRingBufferStatReadLoss(&ringBuffer) == readLoss);
    U_PORT_TEST_ASSERT(uRingBufferStatAddLoss(&ringBuffer) == addLoss);
    U_PORT_TEST_ASSERT(uRingBufferStatReadLossHandle(&ringBuffer, handle[0]) == readLossHandle[0]);
    U_PORT_TEST_ASSERT(uRingBufferStatReadLossHandle(&ringBuffer, handle[1]) == readLossHandle[1]);
    uRingBufferGiveReadHandle(&ringBuffer, handle[0]);
    uRingBufferGiveReadHandle(&ringBuffer, handle[1]);

    // Check that reset works as advertised
    U_TEST_PRINT_LINE("testing reset...");
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    handle[0] = uRingBufferTakeReadHandle(&ringBuffer);
    U_PORT_TEST_ASSERT(uRingBufferAdd(&ringBuffer, &b, sizeof(b)));
    U_PORT_TEST_ASSERT(uRingBufferDataSize(&ringBuffer) == sizeof(b));
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == sizeof(linearBuffer) - 1 - sizeof(b));
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) == sizeof(linearBuffer) - 1);
    uRingBufferReset(&ringBuffer);
    U_PORT_TEST_ASSERT(uRingBufferDataSize(&ringBuffer) == 0);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == sizeof(linearBuffer) - 1);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) == sizeof(linearBuffer) - 1);
    U_PORT_TEST_ASSERT(uRingBufferRead(&ringBuffer, bufferOut, sizeof(bufferOut)) == 0);
    U_PORT_TEST_ASSERT(bufferOut[0] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(uRingBufferDataSizeHandle(&ringBuffer, handle[0]) == 0);
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(uRingBufferReadHandle(&ringBuffer, handle[0], bufferOut,
                                             sizeof(bufferOut)) == 0);
    U_PORT_TEST_ASSERT(bufferOut[0] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    uRingBufferGiveReadHandle(&ringBuffer, handle[0]);

    // Check that forced-add moves the read pointers around correctly
    U_TEST_PRINT_LINE("testing forced add...");
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    printBuffer("  output buffer reset to", bufferOut, sizeof(bufferOut));
    handle[0] = uRingBufferTakeReadHandle(&ringBuffer);
    handle[1] = uRingBufferTakeReadHandle(&ringBuffer);
    // Should not be able to obtain any more handles
    U_PORT_TEST_ASSERT(uRingBufferTakeReadHandle(&ringBuffer) < 0);
    U_TEST_PRINT_LINE(" adding the maximum number of byte(s) (%d).", sizeof(bufferIn) - 1);
    U_PORT_TEST_ASSERT(uRingBufferAdd(&ringBuffer, bufferIn, sizeof(bufferIn) - 1));
    printBuffer("  ring buffer now contains", linearBuffer, sizeof(linearBuffer));
    U_PORT_TEST_ASSERT(uRingBufferDataSize(&ringBuffer) == sizeof(bufferIn) - 1);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == 0);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) == sizeof(linearBuffer) - 1);
    // Now don't read any of that out but force-add one more byte;
    // this should push out the oldest byte at every read pointer
    U_TEST_PRINT_LINE(" forcing in one more byte (value 0x%02x).", bufferIn[sizeof(bufferIn) - 1]);
    U_PORT_TEST_ASSERT(uRingBufferForceAdd(&ringBuffer, bufferIn + sizeof(bufferIn) - 1, 1));
    // This should show up as a loss of one on all read handles
    readLoss++;
    readLossHandle[0]++;
    readLossHandle[1]++;
    // Forcing in more than the buffer size should always fail
    U_PORT_TEST_ASSERT(!uRingBufferForceAdd(&ringBuffer, bufferIn, sizeof(bufferIn)));
    // This should show up as a loss on add
    addLoss += sizeof(bufferIn);
    printBuffer("  ring buffer now contains", linearBuffer, sizeof(linearBuffer));
    U_PORT_TEST_ASSERT(uRingBufferStatReadLoss(&ringBuffer) == readLoss);
    U_PORT_TEST_ASSERT(uRingBufferStatAddLoss(&ringBuffer) == addLoss);
    U_PORT_TEST_ASSERT(uRingBufferStatReadLossHandle(&ringBuffer, handle[0]) == readLossHandle[0]);
    U_PORT_TEST_ASSERT(uRingBufferStatReadLossHandle(&ringBuffer, handle[1]) == readLossHandle[1]);
    // Now do the reading part, normal read first
    y = uRingBufferRead(&ringBuffer, bufferOut, sizeof(bufferOut));
    U_TEST_PRINT_LINE(" normal read returned %d byte(s), %d byte(s) still in the buffer.",
                      y, sizeof(linearBuffer) - 1 - uRingBufferAvailableSize(&ringBuffer));
    U_PORT_TEST_ASSERT(y == sizeof(bufferIn) - 1);
    printBuffer("  output buffer now contains", bufferOut, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(memcmp(bufferOut, bufferIn + 1, sizeof(bufferIn) - 1) == 0);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(uRingBufferDataSize(&ringBuffer) == 0);
    // The available size won't change as we have a "handled read" that has
    // not yet consumed the new data
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == 0);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) == sizeof(linearBuffer) - 1);
    U_PORT_TEST_ASSERT(uRingBufferRead(&ringBuffer, bufferOut, sizeof(bufferOut)) == 0);
    U_PORT_TEST_ASSERT(bufferOut[0] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    // First handle
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(uRingBufferDataSizeHandle(&ringBuffer, handle[0]) == sizeof(bufferIn) - 1);
    printBuffer("  output buffer reset to", bufferOut, sizeof(bufferOut));
    y = uRingBufferReadHandle(&ringBuffer, handle[0], bufferOut, sizeof(bufferOut));
    U_TEST_PRINT_LINE(" read using handle 0x%08x returned %d byte(s), %d byte(s) still in the buffer.",
                      handle[0], y, sizeof(linearBuffer) - 1 - uRingBufferAvailableSize(&ringBuffer));
    U_PORT_TEST_ASSERT(y == sizeof(bufferIn) - 1);
    printBuffer("  output buffer now contains", bufferOut, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(memcmp(bufferOut, bufferIn + 1, sizeof(bufferIn) - 1) == 0);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(uRingBufferReadHandle(&ringBuffer, handle[0], bufferOut,
                                             sizeof(bufferOut)) == 0);
    U_PORT_TEST_ASSERT(bufferOut[0] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    // The available size still won't have changed
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == 0);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) == sizeof(linearBuffer) - 1);
    // Second handle
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(uRingBufferDataSizeHandle(&ringBuffer, handle[1]) == sizeof(bufferIn) - 1);
    printBuffer("  output buffer reset to", bufferOut, sizeof(bufferOut));
    y = uRingBufferReadHandle(&ringBuffer, handle[1], bufferOut, sizeof(bufferOut));
    U_TEST_PRINT_LINE(" read using handle 0x%08x returned %d byte(s), %d byte(s) still in the buffer.",
                      handle[1], y, sizeof(linearBuffer) - 1 - uRingBufferAvailableSize(&ringBuffer));
    U_PORT_TEST_ASSERT(y == sizeof(bufferIn) - 1);
    printBuffer("  output buffer now contains", bufferOut, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(memcmp(bufferOut, bufferIn + 1, sizeof(bufferIn) - 1) == 0);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(uRingBufferDataSizeHandle(&ringBuffer, handle[1]) == 0);
    U_PORT_TEST_ASSERT(uRingBufferReadHandle(&ringBuffer, handle[1], bufferOut,
                                             sizeof(bufferOut)) == 0);
    U_PORT_TEST_ASSERT(bufferOut[0] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    // Now the whole ring buffer should be available again
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == sizeof(linearBuffer) - 1);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) == sizeof(linearBuffer) - 1);
    // Check stats
    U_PORT_TEST_ASSERT(uRingBufferStatReadLoss(&ringBuffer) == readLoss);
    U_PORT_TEST_ASSERT(uRingBufferStatAddLoss(&ringBuffer) == addLoss);
    U_PORT_TEST_ASSERT(uRingBufferStatReadLossHandle(&ringBuffer, handle[0]) == readLossHandle[0]);
    U_PORT_TEST_ASSERT(uRingBufferStatReadLossHandle(&ringBuffer, handle[1]) == readLossHandle[1]);
    uRingBufferGiveReadHandle(&ringBuffer, handle[0]);
    uRingBufferGiveReadHandle(&ringBuffer, handle[1]);

    // Check that locking a read pointer stops forced-add
    U_TEST_PRINT_LINE("testing locks...");
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    printBuffer("  output buffer reset to", bufferOut, sizeof(bufferOut));
    handle[0] = uRingBufferTakeReadHandle(&ringBuffer);
    readLossHandle[0] = 0;
    U_PORT_TEST_ASSERT(!uRingBufferReadHandleIsLocked(&ringBuffer, handle[0]));
    handle[1] = uRingBufferTakeReadHandle(&ringBuffer);
    readLossHandle[1] = 0;
    U_PORT_TEST_ASSERT(!uRingBufferReadHandleIsLocked(&ringBuffer, handle[1]));
    // Should not be able to obtain any more handles
    U_PORT_TEST_ASSERT(uRingBufferTakeReadHandle(&ringBuffer) < 0);
    U_TEST_PRINT_LINE(" adding the maximum number of byte(s) (%d).", sizeof(bufferIn) - 1);
    U_PORT_TEST_ASSERT(uRingBufferAdd(&ringBuffer, bufferIn, sizeof(bufferIn) - 1));
    printBuffer("  ring buffer now contains", linearBuffer, sizeof(linearBuffer));
    U_PORT_TEST_ASSERT(uRingBufferDataSize(&ringBuffer) == sizeof(bufferIn) - 1);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == 0);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) == sizeof(linearBuffer) - 1);
    // Now lock the second read pointer
    U_TEST_PRINT_LINE(" locking read handle %d.", handle[1]);
    U_PORT_TEST_ASSERT(uRingBufferLockReadHandle(&ringBuffer, handle[1]) == sizeof(bufferIn) - 1);
    U_PORT_TEST_ASSERT(uRingBufferReadHandleIsLocked(&ringBuffer, handle[1]));
    U_PORT_TEST_ASSERT(!uRingBufferReadHandleIsLocked(&ringBuffer, handle[0]));
    // Available size max should now take into account the locked read pointer
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) == 0);
    // Don't read any of the data but force-add one more byte;
    // since we have a read pointer locked and are at maximum
    // extent, force-add should fail
    U_TEST_PRINT_LINE(" trying to force in one more byte (value 0x%02x).", b);
    U_PORT_TEST_ASSERT(!uRingBufferForceAdd(&ringBuffer, &b, 1));
    // This should show up as a loss of one on add
    addLoss++;
    printBuffer("  ring buffer now contains", linearBuffer, sizeof(linearBuffer));
    // Unlock the read pointer
    U_TEST_PRINT_LINE(" unlocking read handle %d.", handle[1]);
    uRingBufferUnlockReadHandle(&ringBuffer, handle[1]);
    U_PORT_TEST_ASSERT(!uRingBufferReadHandleIsLocked(&ringBuffer, handle[1]));
    U_PORT_TEST_ASSERT(!uRingBufferReadHandleIsLocked(&ringBuffer, handle[0]));
    // Now force-add should work
    U_TEST_PRINT_LINE(" forcing in one more byte (value 0x%02x).", bufferIn[sizeof(bufferIn) - 1]);
    U_PORT_TEST_ASSERT(uRingBufferForceAdd(&ringBuffer, bufferIn + sizeof(bufferIn) - 1, 1));
    // This should show up as a loss of one on all read handles
    readLoss++;
    readLossHandle[0]++;
    readLossHandle[1]++;
    // Forcing in more than the buffer size should always fail
    U_PORT_TEST_ASSERT(!uRingBufferForceAdd(&ringBuffer, bufferIn, sizeof(bufferIn)));
    // This should show up as a loss on add
    addLoss += sizeof(bufferIn);
    // Now do the reading part, normal read first
    y = uRingBufferRead(&ringBuffer, bufferOut, sizeof(bufferOut));
    U_TEST_PRINT_LINE(" normal read returned %d byte(s), %d byte(s) still in the buffer.",
                      y, sizeof(linearBuffer) - 1 - uRingBufferAvailableSize(&ringBuffer));
    U_PORT_TEST_ASSERT(y == sizeof(bufferIn) - 1);
    printBuffer("  output buffer now contains", bufferOut, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(memcmp(bufferOut, bufferIn + 1, sizeof(bufferIn) - 1) == 0);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(uRingBufferDataSize(&ringBuffer) == 0);
    // The available size won't change as we have a "handled read" that has
    // not yet consumed the new data
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == 0);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) == sizeof(linearBuffer) - 1);
    U_PORT_TEST_ASSERT(uRingBufferRead(&ringBuffer, bufferOut, sizeof(bufferOut)) == 0);
    U_PORT_TEST_ASSERT(bufferOut[0] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    // First handle
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(uRingBufferDataSizeHandle(&ringBuffer, handle[0]) == sizeof(bufferIn) - 1);
    printBuffer("  output buffer reset to", bufferOut, sizeof(bufferOut));
    y = uRingBufferReadHandle(&ringBuffer, handle[0], bufferOut, sizeof(bufferOut));
    U_TEST_PRINT_LINE(" read using handle 0x%08x returned %d byte(s), %d byte(s) still in the buffer.",
                      handle[0], y, sizeof(linearBuffer) - 1 - uRingBufferAvailableSize(&ringBuffer));
    U_PORT_TEST_ASSERT(y == sizeof(bufferIn) - 1);
    printBuffer("  output buffer now contains", bufferOut, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(memcmp(bufferOut, bufferIn + 1, sizeof(bufferIn) - 1) == 0);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(uRingBufferReadHandle(&ringBuffer, handle[0], bufferOut,
                                             sizeof(bufferOut)) == 0);
    U_PORT_TEST_ASSERT(bufferOut[0] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    // The available size still won't have changed
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == 0);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) == sizeof(linearBuffer) - 1);
    // Second handle
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(uRingBufferDataSizeHandle(&ringBuffer, handle[1]) == sizeof(bufferIn) - 1);
    printBuffer("  output buffer reset to", bufferOut, sizeof(bufferOut));
    y = uRingBufferReadHandle(&ringBuffer, handle[1], bufferOut, sizeof(bufferOut));
    U_TEST_PRINT_LINE(" read using handle 0x%08x returned %d byte(s), %d byte(s) still in the buffer.",
                      handle[1], y, sizeof(linearBuffer) - 1 - uRingBufferAvailableSize(&ringBuffer));
    U_PORT_TEST_ASSERT(y == sizeof(bufferIn) - 1);
    printBuffer("  output buffer now contains", bufferOut, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(memcmp(bufferOut, bufferIn + 1, sizeof(bufferIn) - 1) == 0);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(uRingBufferDataSizeHandle(&ringBuffer, handle[1]) == 0);
    U_PORT_TEST_ASSERT(uRingBufferReadHandle(&ringBuffer, handle[1], bufferOut,
                                             sizeof(bufferOut)) == 0);
    U_PORT_TEST_ASSERT(bufferOut[0] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    // Now the whole ring buffer should be available again
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == sizeof(linearBuffer) - 1);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) == sizeof(linearBuffer) - 1);
    // Check stats
    U_PORT_TEST_ASSERT(uRingBufferStatReadLoss(&ringBuffer) == readLoss);
    U_PORT_TEST_ASSERT(uRingBufferStatAddLoss(&ringBuffer) == addLoss);
    U_PORT_TEST_ASSERT(uRingBufferStatReadLossHandle(&ringBuffer, handle[0]) == readLossHandle[0]);
    U_PORT_TEST_ASSERT(uRingBufferStatReadLossHandle(&ringBuffer, handle[1]) == readLossHandle[1]);
    uRingBufferGiveReadHandle(&ringBuffer, handle[0]);
    uRingBufferGiveReadHandle(&ringBuffer, handle[1]);

    // Test flushing
    U_TEST_PRINT_LINE("testing flush...");
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    handle[0] = uRingBufferTakeReadHandle(&ringBuffer);
    readLossHandle[0] = 0;
    handle[1] = uRingBufferTakeReadHandle(&ringBuffer);
    readLossHandle[1] = 0;
    U_PORT_TEST_ASSERT(uRingBufferAdd(&ringBuffer, bufferIn, sizeof(bufferIn) - 1));
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == 0);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) == sizeof(linearBuffer) - 1);
    U_PORT_TEST_ASSERT(uRingBufferDataSize(&ringBuffer) == sizeof(bufferIn) - 1);
    U_PORT_TEST_ASSERT(uRingBufferDataSizeHandle(&ringBuffer, handle[0]) == sizeof(bufferIn) - 1);
    U_PORT_TEST_ASSERT(uRingBufferDataSizeHandle(&ringBuffer, handle[1]) == sizeof(bufferIn) - 1);
    // Now flush one handle
    uRingBufferFlushHandle(&ringBuffer, handle[0]);
    U_PORT_TEST_ASSERT(uRingBufferDataSizeHandle(&ringBuffer, handle[0]) == 0);
    U_PORT_TEST_ASSERT(uRingBufferDataSize(&ringBuffer) == sizeof(bufferIn) - 1);
    U_PORT_TEST_ASSERT(uRingBufferDataSizeHandle(&ringBuffer, handle[1]) == sizeof(bufferIn) - 1);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == 0);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) == sizeof(linearBuffer) - 1);
    // Then the "normal" read handle
    uRingBufferFlush(&ringBuffer);
    U_PORT_TEST_ASSERT(uRingBufferDataSize(&ringBuffer) == 0);
    U_PORT_TEST_ASSERT(uRingBufferDataSizeHandle(&ringBuffer, handle[0]) == 0);
    U_PORT_TEST_ASSERT(uRingBufferDataSizeHandle(&ringBuffer, handle[1]) == sizeof(bufferIn) - 1);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == 0);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) == sizeof(linearBuffer) - 1);
    // Then the last handle
    uRingBufferFlushHandle(&ringBuffer, handle[1]);
    U_PORT_TEST_ASSERT(uRingBufferDataSizeHandle(&ringBuffer, handle[1]) == 0);
    U_PORT_TEST_ASSERT(uRingBufferDataSize(&ringBuffer) == 0);
    U_PORT_TEST_ASSERT(uRingBufferDataSizeHandle(&ringBuffer, handle[0]) == 0);
    // Now the whole ring buffer should be available again
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == sizeof(linearBuffer) - 1);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) == sizeof(linearBuffer) - 1);
    uRingBufferGiveReadHandle(&ringBuffer, handle[0]);
    uRingBufferGiveReadHandle(&ringBuffer, handle[1]);

    // Check that delete does what it says on the tin
    U_TEST_PRINT_LINE("deleting ring buffer...");
    uRingBufferDelete(&ringBuffer);
    U_PORT_TEST_ASSERT(!uRingBufferAdd(&ringBuffer, bufferIn, 5));
    U_PORT_TEST_ASSERT(!uRingBufferForceAdd(&ringBuffer, bufferIn, 5));
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(uRingBufferDataSize(&ringBuffer) == 0);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == 0);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSizeMax(&ringBuffer) == 0);
    U_PORT_TEST_ASSERT(uRingBufferRead(&ringBuffer, bufferOut, sizeof(bufferOut)) == 0);
    U_PORT_TEST_ASSERT(bufferOut[0] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(uRingBufferTakeReadHandle(&ringBuffer) < 0);
    U_PORT_TEST_ASSERT(uRingBufferDataSizeHandle(&ringBuffer, 1) == 0);
    U_PORT_TEST_ASSERT(uRingBufferReadHandle(&ringBuffer, 1, bufferOut, sizeof(bufferOut)) == 0);

    // Now do a test of the non-handled version
    U_TEST_PRINT_LINE("testing non-handled version...");
    memset(&ringBuffer, 0, sizeof(ringBuffer));
    memset(linearBuffer, 0, sizeof(linearBuffer));
    readLoss = 0;
    readLossHandle[0] = 0;
    readLossHandle[1] = 0;
    addLoss = 0;
    printBuffer("  ring buffer reset to", linearBuffer, sizeof(linearBuffer));
    U_PORT_TEST_ASSERT(uRingBufferCreate(&ringBuffer, linearBuffer, sizeof(linearBuffer)) == 0);
    U_PORT_TEST_ASSERT(uRingBufferDataSize(&ringBuffer) == 0);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == sizeof(linearBuffer) - 1);
    U_PORT_TEST_ASSERT(uRingBufferRead(&ringBuffer, bufferOut, sizeof(bufferOut)) == 0);
    U_PORT_TEST_ASSERT(bufferOut[0] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    printBuffer("  output buffer reset to", bufferOut, sizeof(bufferOut));
    // Trying to take a handle should fail
    U_PORT_TEST_ASSERT(uRingBufferTakeReadHandle(&ringBuffer) < 0);
    U_TEST_PRINT_LINE(" adding %d byte(s).", sizeof(bufferIn) - 1);
    U_PORT_TEST_ASSERT(uRingBufferAdd(&ringBuffer, bufferIn, sizeof(bufferIn) - 1));
    printBuffer("  ring buffer now contains", linearBuffer, sizeof(linearBuffer));
    U_PORT_TEST_ASSERT(uRingBufferDataSize(&ringBuffer) == sizeof(bufferIn) - 1);
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == 0);
    // Now do the reading part
    y = uRingBufferRead(&ringBuffer, bufferOut, sizeof(bufferOut));
    U_TEST_PRINT_LINE(" normal read returned %d byte(s), %d byte(s) still in the buffer.",
                      y, sizeof(linearBuffer) - 1 - uRingBufferAvailableSize(&ringBuffer));
    U_PORT_TEST_ASSERT(y == sizeof(bufferIn) - 1);
    printBuffer("  output buffer now contains", bufferOut, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(memcmp(bufferOut, bufferIn, sizeof(bufferIn) - 1) == 0);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    memset(bufferOut, U_TEST_UTILS_RINGBUFFER_FILL_CHAR, sizeof(bufferOut));
    U_PORT_TEST_ASSERT(uRingBufferDataSize(&ringBuffer) == 0);
    // Now the whole ring buffer should be available again
    U_PORT_TEST_ASSERT(uRingBufferAvailableSize(&ringBuffer) == sizeof(linearBuffer) - 1);
    U_PORT_TEST_ASSERT(uRingBufferRead(&ringBuffer, bufferOut, sizeof(bufferOut)) == 0);
    U_PORT_TEST_ASSERT(bufferOut[0] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    U_PORT_TEST_ASSERT(bufferOut[sizeof(bufferOut) - 1] == U_TEST_UTILS_RINGBUFFER_FILL_CHAR);
    // Check stats
    U_PORT_TEST_ASSERT(uRingBufferStatReadLoss(&ringBuffer) == readLoss);
    U_PORT_TEST_ASSERT(uRingBufferStatAddLoss(&ringBuffer) == addLoss);
    U_PORT_TEST_ASSERT(uRingBufferStatReadLossHandle(&ringBuffer, handle[0]) == readLossHandle[0]);
    U_PORT_TEST_ASSERT(uRingBufferStatReadLossHandle(&ringBuffer, handle[1]) == readLossHandle[1]);

    // Done
    U_TEST_PRINT_LINE("deleting ring buffer...");
    uRingBufferDelete(&ringBuffer);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed == 0) || (heapUsed == (int32_t)U_ERROR_COMMON_NOT_SUPPORTED));
}

// End of file