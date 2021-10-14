/*
 * Copyright 2020 u-blox
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

#include "stdlib.h"    // malloc()/free()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"   // Required by u_cell_private.h

#include "u_at_client.h"

#include "u_cell_module_type.h"
#include "u_cell.h"
#include "u_cell_net.h"     // Required by u_cell_private.h
#include "u_cell_private.h" // So that we can get at some innards
#include "u_cell_file.h"

#include "u_cell_test_cfg.h"
#include "u_cell_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The name of the file to use when testing.
 */
#define U_CELL_FILE_TEST_FILE_NAME "test.txt"

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
* PUBLIC FUNCTIONS
* -------------------------------------------------------------- */

/** Test writing data into file.
 */
U_PORT_TEST_FUNCTION("[cellFile]", "cellFileWrite")
{
    int32_t heapUsed;
    int32_t cellHandle;
    int32_t result;
    const char *pBuffer = "DEADBEEFDEADBEEF";
    size_t length;

    length = strlen(pBuffer);

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Open file in write mode and write data into the file
    uPortLog("U_CELL_FILE_TEST: writing data into file...\n");
    result = uCellFileWrite(cellHandle, // Cellular Handle
                            U_CELL_FILE_TEST_FILE_NAME, // File name
                            pBuffer, // Data to write into the file
                            length); // Data size
    uPortLog("U_CELL_FILE_TEST: number of bytes written into the file = %d.\n", result);
    U_PORT_TEST_ASSERT(result == length);

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_CELL_FILE_TEST: we have leaked %d byte(s).\n", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Test reading file size.
 */
U_PORT_TEST_FUNCTION("[cellFile]", "cellFileSize")
{
    int32_t heapUsed;
    int32_t cellHandle;
    int32_t fileSize = 0;

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Read size of file
    uPortLog("U_CELL_FILE_TEST: reading file size...\n");
    fileSize = uCellFileSize(cellHandle, // Cellular Handle
                             U_CELL_FILE_TEST_FILE_NAME); // File name
    uPortLog("U_CELL_FILE_TEST: file size = %d.\n", fileSize);
    // This should pass if previous test has passed
    U_PORT_TEST_ASSERT(fileSize > 0);

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_CELL_FILE_TEST: we have leaked %d byte(s).\n", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Test block reading from file.
 */
U_PORT_TEST_FUNCTION("[cellFile]", "cellFileBlockRead")
{
    int32_t heapUsed;
    int32_t cellHandle;
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

    // Block read from file
    length = 8;
    offset = 7;
    uPortLog("U_CELL_FILE_TEST: reading data (block read) from file...\n");
    uPortLog("U_CELL_FILE_TEST: read %d bytes with the offset of %d bytes.\n",
             length, offset);
    memset(buffer, 0xaa, sizeof(buffer));
    result = uCellFileBlockRead(cellHandle, // Cellular Handle
                                U_CELL_FILE_TEST_FILE_NAME, // File name
                                buffer, // Buffer to store file contents
                                offset, // offset from the beginning of file
                                length); // Number of bytes to read
    uPortLog("U_CELL_FILE_TEST: number of bytes read = %d.\n", result);
    uPortLog("U_CELL_FILE_TEST: data read \"%.*s\".\n", length, buffer);
    U_PORT_TEST_ASSERT(result == length);
    U_PORT_TEST_ASSERT(memcmp(buffer, "FDEADBEE", length) == 0);
    U_PORT_TEST_ASSERT(*(buffer + length) == 0xaa);

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_CELL_CFG_TEST: we have leaked %d byte(s).\n", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Test reading whole file.
 */
U_PORT_TEST_FUNCTION("[cellFile]", "cellFileRead")
{
    int32_t heapUsed;
    int32_t cellHandle;
    int32_t length = 0;
    char buffer[50];

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Read contents of file
    uPortLog("U_CELL_FILE_TEST: reading whole file...\n");
    length = uCellFileRead(cellHandle, // Cellular Handle
                           U_CELL_FILE_TEST_FILE_NAME, // File name
                           buffer, sizeof(buffer)); // Buffer to store file contents
    uPortLog("U_CELL_FILE_TEST: number of bytes read = %d.\n", length);
    uPortLog("U_CELL_FILE_TEST: data read \"%.*s\".\n", length, buffer);
    U_PORT_TEST_ASSERT(length > 0);

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_CELL_FILE_TEST: we have leaked %d byte(s).\n", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Test list all files.
 */
U_PORT_TEST_FUNCTION("[cellFile]", "cellFileListAll")
{
    int32_t heapUsed;
    int32_t cellHandle;
    bool found = false;
    char *pFileName;

    pFileName = (char *) malloc(U_CELL_FILE_NAME_MAX_LENGTH);
    U_PORT_TEST_ASSERT(pFileName != NULL);

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    uPortLog("U_CELL_FILE_TEST: listing all the files...\n");
    for (int32_t x = uCellFileListFirst(cellHandle, pFileName);
         x >= 0;
         x = uCellFileListNext(cellHandle, pFileName)) {
        uPortLog("U_CELL_FILE_TEST: \"%s\".\n", pFileName);
        if (strcmp(pFileName, U_CELL_FILE_TEST_FILE_NAME) == 0) {
            found = true;
        }
    }

    free(pFileName);

    U_PORT_TEST_ASSERT(found);

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_CELL_FILE_TEST: we have leaked %d byte(s).\n", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Test deleting file.
 */
U_PORT_TEST_FUNCTION("[cellFile]", "cellFileDelete")
{
    int32_t heapUsed;
    int32_t cellHandle;

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    uPortLog("U_CELL_FILE_TEST: deleting file...\n");
    U_PORT_TEST_ASSERT(uCellFileDelete(cellHandle, U_CELL_FILE_TEST_FILE_NAME) == 0);

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_CELL_FILE_TEST: we have leaked %d byte(s).\n", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

#endif // #ifdef U_CFG_TEST_CELL_MODULE_TYPE

// End of file
