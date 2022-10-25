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
 * no platform stuff and no OS stuff. Anything required from the
 * platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Tests for the file cellular system API: these should pass on all
 * platforms that have file system support. They are only
 * compiled if U_CFG_TEST_CELL_MODULE_TYPE is defined.
 */

#ifdef U_CFG_TEST_CELL_MODULE_TYPE

# ifdef U_CFG_OVERRIDE
#  include "u_cfg_override.h" // For a customer's configuration override
# endif

#include "stdlib.h"    // strtol()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdio.h"     // snprintf()
#include "string.h"    // memset(), strcmp()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port_clib_platform_specific.h" /* Integer stdio, must be included
                                              before the other port files if
                                              any print or scan function is used. */
#include "u_port.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_os.h"   // Required by u_cell_private.h

#include "u_at_client.h"

#include "u_cell_module_type.h"
#include "u_cell.h"
#include "u_cell_file.h"
#include "u_cell_net.h"     // Required by u_cell_private.h
#include "u_cell_private.h" // So that we can get at some innards

#include "u_cell_test_cfg.h"
#include "u_cell_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_CELL_FILE_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

/** The name of the file to use when testing.
 */
#define U_CELL_FILE_TEST_FILE_NAME "test"

/** strlen(U_CELL_FILE_TEST_FILE_NAME).
 */
#define U_CELL_FILE_TEST_FILE_NAME_LENGTH 4

/** The number of files to test for in the re-entrant listing
 * version
 */
#define U_CELL_FILE_TEST_REENTRANT_NUM 3

/** The string to write to a file used in the re-entrant list testing.
 */
#define U_CELL_FILE_TEST_REENTRANT_STRING "delete me"

/** strlen(U_CELL_FILE_TEST_REENTRANT_STRING).
 */
#define U_CELL_FILE_TEST_REENTRANT_STRING_SIZE 9

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Handle.
*/
static uCellTestPrivate_t gHandles = U_CELL_TEST_PRIVATE_DEFAULTS;

/* ----------------------------------------------------------------
* STATIC FUNCTIONS
* -------------------------------------------------------------- */

// Update a tracking array, used by cellFileListAllReentrant().
static void updateTracker(const char *pFileName, bool *pTracker,
                          size_t size)
{
    size_t x;

    if (strlen(pFileName) == U_CELL_FILE_TEST_FILE_NAME_LENGTH + 1) {
        x = strtol(pFileName + U_CELL_FILE_TEST_FILE_NAME_LENGTH, NULL, 10);
        if (x < size) {
            *(pTracker + x) = true;
        }
    }
}

// Check a tracking array, used by cellFileListAllReentrant();
// return true only if all elements are true.
static bool checkTracker(bool *pTracker, size_t size)
{
    bool isGood = true;

    for (size_t x = 0; (x < size) && isGood; x++, pTracker++) {
        isGood = *pTracker;
    }

    return isGood;
}

/* ----------------------------------------------------------------
* PUBLIC FUNCTIONS
* -------------------------------------------------------------- */

/** Test writing data into file.
 */
U_PORT_TEST_FUNCTION("[cellFile]", "cellFileWrite")
{
    int32_t heapUsed;
    uDeviceHandle_t cellHandle;
    const uCellPrivateModule_t *pModule;
    int32_t result;
    const char *pBuffer = "DEADBEEFDEADBEEF";
    size_t length;
    size_t y = 1;

    length = strlen(pBuffer);

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Make sure the test file name has been deleted in case a
    // previous test was aborted half way
    uCellFileDelete(cellHandle, U_CELL_FILE_TEST_FILE_NAME);

    // Get the private module data as we need it for testing
    pModule = pUCellPrivateGetModule(cellHandle);
    U_PORT_TEST_ASSERT(pModule != NULL);
    //lint -esym(613, pModule) Suppress possible use of NULL pointer
    // for pModule from now on

    // Do this twice if tags are supported
    if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_FILE_SYSTEM_TAG)) {
        y = 2;
    }

    for (size_t x = 0; x < y; x++) {
        if (x > 0) {
            U_TEST_PRINT_LINE("repeating with tag...");
            U_PORT_TEST_ASSERT(uCellFileSetTag(cellHandle, "USER") == 0);
        } else {
            U_PORT_TEST_ASSERT(pUCellFileGetTag(cellHandle) == NULL);
        }

        // Open file in write mode and write data into the file
        U_TEST_PRINT_LINE("writing data into file...");
        result = uCellFileWrite(cellHandle, // Cellular Handle
                                U_CELL_FILE_TEST_FILE_NAME, // File name
                                pBuffer, // Data to write into the file
                                length); // Data size
        U_TEST_PRINT_LINE("number of bytes written into the file = %d.", result);
        U_PORT_TEST_ASSERT(result == length);

        if (x > 0) {
            U_PORT_TEST_ASSERT(strcmp(pUCellFileGetTag(cellHandle), "USER") == 0);
            U_PORT_TEST_ASSERT(uCellFileSetTag(cellHandle, NULL) == 0);
        }
    }

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Test reading file size.
 */
U_PORT_TEST_FUNCTION("[cellFile]", "cellFileSize")
{
    int32_t heapUsed;
    uDeviceHandle_t cellHandle;
    const uCellPrivateModule_t *pModule;
    int32_t fileSize = 0;
    size_t y = 1;

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Get the private module data as we need it for testing
    pModule = pUCellPrivateGetModule(cellHandle);
    U_PORT_TEST_ASSERT(pModule != NULL);
    //lint -esym(613, pModule) Suppress possible use of NULL pointer
    // for pModule from now on

    // Do this twice if tags are supported
    if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_FILE_SYSTEM_TAG)) {
        y = 2;
    }

    for (size_t x = 0; x < y; x++) {
        if (x > 0) {
            U_TEST_PRINT_LINE("repeating with tag...");
            U_PORT_TEST_ASSERT(uCellFileSetTag(cellHandle, "USER") == 0);
        } else {
            U_PORT_TEST_ASSERT(pUCellFileGetTag(cellHandle) == NULL);
        }

        // Read size of file
        U_TEST_PRINT_LINE("reading file size...");
        fileSize = uCellFileSize(cellHandle, // Cellular Handle
                                 U_CELL_FILE_TEST_FILE_NAME); // File name
        U_TEST_PRINT_LINE("file size = %d.", fileSize);
        // This should pass if previous test has passed
        U_PORT_TEST_ASSERT(fileSize > 0);

        if (x > 0) {
            U_PORT_TEST_ASSERT(strcmp(pUCellFileGetTag(cellHandle), "USER") == 0);
            U_PORT_TEST_ASSERT(uCellFileSetTag(cellHandle, NULL) == 0);
        }
    }

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Test block reading from file.
 */
U_PORT_TEST_FUNCTION("[cellFile]", "cellFileBlockRead")
{
    int32_t heapUsed;
    uDeviceHandle_t cellHandle;
    const uCellPrivateModule_t *pModule;
    int32_t result;
    int32_t offset;
    size_t length;
    char buffer[9];

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Get the private module data as we need it for testing
    pModule = pUCellPrivateGetModule(cellHandle);
    U_PORT_TEST_ASSERT(pModule != NULL);
    //lint -esym(613, pModule) Suppress possible use of NULL pointer
    // for pModule from now on

    // Block read from file
    length = 8;
    offset = 7;
    U_TEST_PRINT_LINE("reading data (block read) from file...");
    U_TEST_PRINT_LINE("read %d bytes with the offset of %d bytes.",
                      length, offset);
    memset(buffer, 0xaa, sizeof(buffer));
    result = uCellFileBlockRead(cellHandle, // Cellular Handle
                                U_CELL_FILE_TEST_FILE_NAME, // File name
                                buffer, // Buffer to store file contents
                                offset, // offset from the beginning of file
                                length); // Number of bytes to read
    U_TEST_PRINT_LINE("number of bytes read = %d.", result);
    U_TEST_PRINT_LINE("data read \"%.*s\".", length, buffer);
    U_PORT_TEST_ASSERT(result == length);
    U_PORT_TEST_ASSERT(memcmp(buffer, "FDEADBEE", length) == 0);
    U_PORT_TEST_ASSERT(*(buffer + length) == (char) 0xaa);

    // Confirm that an error is returned if a tag is set
    if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_FILE_SYSTEM_TAG)) {
        U_PORT_TEST_ASSERT(uCellFileSetTag(cellHandle, "USER") == 0);
        U_PORT_TEST_ASSERT(uCellFileBlockRead(cellHandle,
                                              U_CELL_FILE_TEST_FILE_NAME,
                                              buffer, 0, 8) < 0);
        U_PORT_TEST_ASSERT(uCellFileSetTag(cellHandle, NULL) == 0);
    }

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Test reading whole file.
 */
U_PORT_TEST_FUNCTION("[cellFile]", "cellFileRead")
{
    int32_t heapUsed;
    uDeviceHandle_t cellHandle;
    const uCellPrivateModule_t *pModule;
    int32_t length = 0;
    char buffer[50];
    size_t y = 1;

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Get the private module data as we need it for testing
    pModule = pUCellPrivateGetModule(cellHandle);
    U_PORT_TEST_ASSERT(pModule != NULL);
    //lint -esym(613, pModule) Suppress possible use of NULL pointer
    // for pModule from now on

    // Do this twice if tags are supported
    if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_FILE_SYSTEM_TAG)) {
        y = 2;
    }

    for (size_t x = 0; x < y; x++) {
        if (x > 0) {
            U_TEST_PRINT_LINE("repeating with tag...");
            U_PORT_TEST_ASSERT(uCellFileSetTag(cellHandle, "USER") == 0);
        } else {
            U_PORT_TEST_ASSERT(pUCellFileGetTag(cellHandle) == NULL);
        }

        // Read contents of file
        U_TEST_PRINT_LINE("reading whole file...");
        length = uCellFileRead(cellHandle, // Cellular Handle
                               U_CELL_FILE_TEST_FILE_NAME, // File name
                               buffer, sizeof(buffer)); // Buffer to store file contents
        U_TEST_PRINT_LINE("number of bytes read = %d.", length);
        U_TEST_PRINT_LINE("data read \"%.*s\".", length, buffer);
        U_PORT_TEST_ASSERT(length > 0);

        if (x > 0) {
            U_PORT_TEST_ASSERT(strcmp(pUCellFileGetTag(cellHandle), "USER") == 0);
            U_PORT_TEST_ASSERT(uCellFileSetTag(cellHandle, NULL) == 0);
        }
    }

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Test list all files.
 */
U_PORT_TEST_FUNCTION("[cellFile]", "cellFileListAll")
{
    int32_t heapUsed;
    uDeviceHandle_t cellHandle;
    const uCellPrivateModule_t *pModule;
    bool found;
    char *pFileName;
    size_t y = 1;

    pFileName = (char *) pUPortMalloc(U_CELL_FILE_NAME_MAX_LENGTH + 1);
    U_PORT_TEST_ASSERT(pFileName != NULL);

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Get the private module data as we need it for testing
    pModule = pUCellPrivateGetModule(cellHandle);
    U_PORT_TEST_ASSERT(pModule != NULL);
    //lint -esym(613, pModule) Suppress possible use of NULL pointer
    // for pModule from now on

    // Do this twice if tags are supported
    if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_FILE_SYSTEM_TAG)) {
        y = 2;
    }

    for (size_t x = 0; x < y; x++) {
        if (x > 0) {
            U_TEST_PRINT_LINE("repeating with tag...");
            U_PORT_TEST_ASSERT(uCellFileSetTag(cellHandle, "USER") == 0);
        } else {
            U_PORT_TEST_ASSERT(pUCellFileGetTag(cellHandle) == NULL);
        }

        U_TEST_PRINT_LINE("listing all the files...");
        found = false;
        for (int32_t x = uCellFileListFirst(cellHandle, pFileName);
             x >= 0;
             x = uCellFileListNext(cellHandle, pFileName)) {
            U_TEST_PRINT_LINE("\"%s\".", pFileName);
            if (strcmp(pFileName, U_CELL_FILE_TEST_FILE_NAME) == 0) {
                found = true;
            }
        }

        if (x > 0) {
            U_PORT_TEST_ASSERT(strcmp(pUCellFileGetTag(cellHandle), "USER") == 0);
            U_PORT_TEST_ASSERT(uCellFileSetTag(cellHandle, NULL) == 0);
        }

        U_PORT_TEST_ASSERT(found);
    }

    uPortFree(pFileName);

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Test list all files, re-entrant version.
 */
U_PORT_TEST_FUNCTION("[cellFile]", "cellFileListAllReentrant")
{
    int32_t heapUsed;
    uDeviceHandle_t cellHandle;
    int32_t result = U_CELL_FILE_TEST_REENTRANT_STRING_SIZE;
    // Enough room for the test file name plus a single number
    // plus a null terminator
    size_t y = 0;
    char buffer[U_CELL_FILE_TEST_FILE_NAME_LENGTH + 2];
    char *pFileName;
    void *pRentrantOuter;
    bool trackerOuter[U_CELL_FILE_TEST_REENTRANT_NUM] = {0};
    void *pRentrantInner;
    bool trackerInner[U_CELL_FILE_TEST_REENTRANT_NUM];

    pFileName = (char *) pUPortMalloc(U_CELL_FILE_NAME_MAX_LENGTH + 1);
    U_PORT_TEST_ASSERT(pFileName != NULL);

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Write the files we need to list
    for (size_t x = 0; (x < U_CELL_FILE_TEST_REENTRANT_NUM) &&
         (result == U_CELL_FILE_TEST_REENTRANT_STRING_SIZE); x++) {
        snprintf(buffer, sizeof(buffer), "%s%1d", U_CELL_FILE_TEST_FILE_NAME, x);
        U_TEST_PRINT_LINE("writing file %s...", buffer);
        result = uCellFileWrite(cellHandle, buffer,
                                U_CELL_FILE_TEST_REENTRANT_STRING,
                                U_CELL_FILE_TEST_REENTRANT_STRING_SIZE);
    }
    U_PORT_TEST_ASSERT(result == U_CELL_FILE_TEST_REENTRANT_STRING_SIZE);

    // List the files in two loops, one within the other, making sure
    // that all files are listed in both loops on each run
    U_TEST_PRINT_LINE("listing the files...");
    for (int32_t x = uCellFileListFirst_r(cellHandle, pFileName, &pRentrantOuter);
         x >= 0;
         x = uCellFileListNext_r(pFileName, &pRentrantOuter), y++) {
        U_TEST_PRINT_LINE("outer loop: \"%s\".", pFileName);
        updateTracker(pFileName, trackerOuter, sizeof(trackerOuter) / sizeof(trackerOuter[0]));

        memset(trackerInner, 0, sizeof(trackerInner));
        for (int32_t y = uCellFileListFirst_r(cellHandle, pFileName, &pRentrantInner);
             y >= 0;
             y = uCellFileListNext_r(pFileName, &pRentrantInner)) {
            U_TEST_PRINT_LINE("inner loop: \"%s\".", pFileName);
            updateTracker(pFileName, trackerInner, sizeof(trackerInner) / sizeof(trackerInner[0]));
        }
        U_PORT_TEST_ASSERT(checkTracker(trackerInner, sizeof(trackerInner) / sizeof(trackerInner[0])));
        uCellFileListLast_r(&pRentrantInner);
        U_TEST_PRINT_LINE("inner loop, all files listed on run %d.", y + 1);
    }
    U_PORT_TEST_ASSERT(checkTracker(trackerOuter, sizeof(trackerOuter) / sizeof(trackerOuter[0])));
    uCellFileListLast_r(&pRentrantOuter);
    U_TEST_PRINT_LINE("outer loop, all files listed.");

    // Delete the files again, for tidiness
    for (size_t x = 0; x < U_CELL_FILE_TEST_REENTRANT_NUM; x++) {
        snprintf(buffer, sizeof(buffer), "%s%1d", U_CELL_FILE_TEST_FILE_NAME, x);
        U_TEST_PRINT_LINE("deleting file %s...", buffer);
        U_PORT_TEST_ASSERT(uCellFileDelete(cellHandle, buffer) == 0);
    }

    uPortFree(pFileName);

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Test deleting file.
 */
U_PORT_TEST_FUNCTION("[cellFile]", "cellFileDelete")
{
    int32_t heapUsed;
    uDeviceHandle_t cellHandle;
    const uCellPrivateModule_t *pModule;
    size_t y = 1;

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Get the private module data as we need it for testing
    pModule = pUCellPrivateGetModule(cellHandle);
    U_PORT_TEST_ASSERT(pModule != NULL);
    //lint -esym(613, pModule) Suppress possible use of NULL pointer
    // for pModule from now on

    // Do this twice if tags are supported
    if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_FILE_SYSTEM_TAG)) {
        y = 2;
    }

    for (size_t x = 0; x < y; x++) {
        if (x > 0) {
            U_TEST_PRINT_LINE("repeating with tag...");
            U_PORT_TEST_ASSERT(uCellFileSetTag(cellHandle, "USER") == 0);
        } else {
            U_PORT_TEST_ASSERT(pUCellFileGetTag(cellHandle) == NULL);
        }

        U_TEST_PRINT_LINE("deleting file...");
        U_PORT_TEST_ASSERT(uCellFileDelete(cellHandle, U_CELL_FILE_TEST_FILE_NAME) == 0);

        if (x > 0) {
            U_PORT_TEST_ASSERT(strcmp(pUCellFileGetTag(cellHandle), "USER") == 0);
            U_PORT_TEST_ASSERT(uCellFileSetTag(cellHandle, NULL) == 0);
        } else {
            if (y > 1) {
                // Re-create the file so that we can delete it again
                U_TEST_PRINT_LINE("re-writing file...");
                U_PORT_TEST_ASSERT(uCellFileWrite(cellHandle,
                                                  U_CELL_FILE_TEST_FILE_NAME,
                                                  "some text", 9) == 9);
            }
        }
    }

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[cellFile]", "cellFileCleanUp")
{
    int32_t x;

    uCellTestPrivateCleanup(&gHandles);

    x = uPortTaskStackMinFree(NULL);
    if (x != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_TEST_PRINT_LINE("main task stack had a minimum of %d"
                          " byte(s) free at the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);
    }

    uPortDeinit();

    x = uPortGetHeapMinFree();
    if (x >= 0) {
        U_TEST_PRINT_LINE("heap had a minimum of %d byte(s) free at the"
                          " end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

#endif // #ifdef U_CFG_TEST_CELL_MODULE_TYPE

// End of file
