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
 * @brief Test for the port API: these should pass on all platforms.
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
#include "string.h"    // strlen(), memcmp()
#include "ctype.h"     // isprint()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_port_clib_platform_specific.h" /* Integer stdio, must be included
                                              before the other port files if
                                              any print or scan function is used. */
#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_uart.h"

#include "u_lib.h"
#include "u_lib_common_test.h"

#include "string.h"

#include "u_lib.h"
#include "lib_fibonacci.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#define U_LIB_COMMON_TEST_ERROR_NUMBER 65535

#define FIB_102 1020930517

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

// incorporated from libfibonacci.cmake
extern const unsigned char __libfibonacci_blob[];

// this is where we will relocate the library code to
uint8_t *reloc_buf;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// super simple malloc, but enough as libfib only allocates on open
static void *simpleMalloc(uint32_t len)
{
    static uint32_t someRam[8];
    if (len > sizeof(someRam)) {
        return 0;
    }
    uPortLogF("mallocing %d bytes @ %p\n", len, &someRam[0]);
    return &someRam[0];
}

// super simple free, but enough as libfib only frees on close
static void simpleFree(void *p)
{
    (void)p;
    uPortLogF("freeing %p\n", p);
}

// The task within which the example run.
static int32_t runLibTest(void)
{
    // our library handle
    static uLibHdl_t libHdl;

    // utility functions for libfibonacci
    // libfibonacci needs a malloc and a free
    static uLibLibc_t libc = {
        .fnmalloc = simpleMalloc,
        .fnfree = simpleFree,
        .fnvprintf = 0
    };

    int res;

    // probe the library address to see if there is a proper library
    uLibHdr_t libHdr; // if the probe succeeds, we will get library information in here
    uPortLogF("\nProbing lib\n");
    res = uLibProbe(&libHdl, &libHdr, &__libfibonacci_blob[0]);
    U_PORT_TEST_ASSERT(res == U_ERROR_COMMON_SUCCESS);

    uPortLogF("name:    %s\n", libHdr.name);
    uPortLogF("version: %08x\n", libHdr.version);
    uPortLogF("flags:   %08x (arch id %02x)\n\n", libHdr.flags, U_LIB_HDR_FLAG_GET_ARCH(libHdr.flags));

    U_PORT_TEST_ASSERT(strcmp(libHdr.name,
                              "fibonacci") == 0);  // Swap for something "common" to test and lib build
    U_PORT_TEST_ASSERT(libHdr.version == U_COMMON_LIB_TEST_VERSION);
    U_PORT_TEST_ASSERT(U_LIB_HDR_FLAG_GET_ARCH(libHdr.flags) == U_LIB_ARCH);
    U_PORT_TEST_ASSERT((libHdr.flags & U_LIB_HDR_FLAG_NEEDS_MALLOC) == U_LIB_HDR_FLAG_NEEDS_MALLOC);

    // depending on the flags, one can for instance validate the
    // library here so we don't start running malicious code

    // try opening the library
    uPortLogF("Opening lib\n");
    res = uLibOpen(&libHdl, &__libfibonacci_blob[0], &libc, 0, NULL);
    U_PORT_TEST_ASSERT(res == U_ERROR_COMMON_SUCCESS);

    // declare the library api functions, see lib_fibonacci.h
    int (*libFibTestCalc)(void *ctx, int series);
    int (*libFibTestLastRes)(void *ctx);
    const char *(*libFibTestHelloWorld)(void *ctx);

    // look up addresses to library functions
    libFibTestCalc = uLibSym(&libHdl, "libFibTestCalc");
    U_PORT_TEST_ASSERT(libFibTestCalc != NULL);
    libFibTestLastRes = uLibSym(&libHdl, "libFibTestLastRes");
    U_PORT_TEST_ASSERT(libFibTestLastRes != NULL);
    libFibTestHelloWorld = uLibSym(&libHdl, "libFibTestHelloWorld");
    U_PORT_TEST_ASSERT(libFibTestHelloWorld != NULL);

    uPortLogF("@libFibTestCalc:      %p\n", libFibTestCalc);
    uPortLogF("@libFibTestLastRes:   %p\n", libFibTestLastRes);
    uPortLogF("@libFibTestHelloWorld:%p\n\n", libFibTestHelloWorld);

    // start calling the library
    int libFibTestResult = libFibTestCalc(libHdl.ictx, 102);
    U_PORT_TEST_ASSERT(libFibTestResult == FIB_102);
    uPortLogF("libFibTestCalc(102):  %d\n", libFibTestResult);

    int libFibTestLastResult = libFibTestLastRes(libHdl.ictx);
    U_PORT_TEST_ASSERT(libFibTestResult == libFibTestLastResult);
    uPortLogF("libFibTestLastRes:    %d\n", libFibTestLastResult);

    const char *pBuffer = libFibTestHelloWorld(libHdl.ictx);
    U_PORT_TEST_ASSERT(strcmp(pBuffer, U_PORT_STRINGIFY_QUOTED(U_COMMON_LIB_TEST_STRING)) == 0);
    uPortLogF("libFibTestHelloWorld: %s (%p)\n", pBuffer, libFibTestHelloWorld(libHdl.ictx));

    // try relocate the library to ram instead
    uPortLogF("\nRelocate library code to ram\n");

    // get address and length of the library code blob
    const void *pCode;
    uint32_t codeLen;
    size_t chunkSize;
    res = uLibGetCode(&libHdl, &pCode, &codeLen);
    U_PORT_TEST_ASSERT(res == U_ERROR_COMMON_SUCCESS);
    reloc_buf = uPortAcquireExecutableChunk(NULL,
                                            &chunkSize,
                                            U_PORT_EXECUTABLE_CHUNK_NO_FLAGS,
                                            U_PORT_EXECUTABLE_CHUNK_INDEX_0);

    U_PORT_TEST_ASSERT(reloc_buf != NULL);


    uPortLog("Code currently resides @ %p, %d bytes\n", pCode, codeLen);
    uPortLog("Moving code to %p\n", reloc_buf);
    if (codeLen >= chunkSize) {
        uPortLog("Reloc_buf to small to fit code, need %d. Cannot continue...", codeLen);
        U_PORT_TEST_ASSERT(-1);
    }
    // instead of memcpy here, one could for instance decrypt the code
    memcpy(reloc_buf, pCode, codeLen);
    res = uLibRelocate(&libHdl, reloc_buf);
    U_PORT_TEST_ASSERT(res == U_ERROR_COMMON_SUCCESS);

    // after relocating, we need to update symbols
    libFibTestCalc = uLibSym(&libHdl, "libFibTestCalc");
    U_PORT_TEST_ASSERT(libFibTestCalc != NULL);
    libFibTestLastRes = uLibSym(&libHdl, "libFibTestLastRes");
    U_PORT_TEST_ASSERT(libFibTestLastRes != NULL);
    libFibTestHelloWorld = uLibSym(&libHdl, "libFibTestHelloWorld");
    U_PORT_TEST_ASSERT(libFibTestHelloWorld != NULL);

    uPortLogF("@libFibTestCalc:      %p\n", libFibTestCalc);
    uPortLogF("@libFibTestLastRes:   %p\n", libFibTestLastRes);
    uPortLogF("@libFibTestHelloWorld:%p\n\n", libFibTestHelloWorld);

    // call the library again, now we will execute from ram
    int libFibTestLastResultReloc = libFibTestLastRes(libHdl.ictx);
    U_PORT_TEST_ASSERT(libFibTestLastResultReloc == libFibTestLastResult);
    uPortLogF("libFibTestLastRes:    %d\n", libFibTestLastResultReloc);

    int libFibTestResultReloc = libFibTestCalc(libHdl.ictx, 102);
    U_PORT_TEST_ASSERT(libFibTestResultReloc == FIB_102);
    uPortLogF("libFibTestCalc(102):  %d\n", libFibTestResultReloc);


    libFibTestLastResultReloc = libFibTestLastRes(libHdl.ictx);
    U_PORT_TEST_ASSERT(libFibTestLastResultReloc == libFibTestResultReloc);
    uPortLogF("libFibTestLastRes:    %d\n", libFibTestLastResultReloc);

    pBuffer = libFibTestHelloWorld(libHdl.ictx);
    U_PORT_TEST_ASSERT(strcmp(pBuffer, U_PORT_STRINGIFY_QUOTED(U_COMMON_LIB_TEST_STRING)) == 0);
    uPortLogF("libFibTestHelloWorld: %s (%p)\n", pBuffer, libFibTestHelloWorld(libHdl.ictx));

    // close library
    uPortLogF("\nClosing lib\n");
    res = uLibClose(&libHdl);
    U_PORT_TEST_ASSERT(res == U_ERROR_COMMON_SUCCESS);

    return U_ERROR_COMMON_SUCCESS;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: MISC
 * -------------------------------------------------------------- */


/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TESTS
 * -------------------------------------------------------------- */

/** Basic test: initialise and then de-initialise libCommon.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */
U_PORT_TEST_FUNCTION("[libCommon]", "libCommonRunLib")
{
    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(runLibTest() == 0);
    uPortDeinit();
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[libCommon]", "libCommonCleanUp")
{
    int32_t minFreeStackBytes;

    minFreeStackBytes = uPortTaskStackMinFree(NULL);
    if (minFreeStackBytes != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        uPortLog("U_LIB_COMMON_TEST: main task stack had a minimum of"
                 " %d byte(s) free at the end of these tests.\n",
                 minFreeStackBytes);
        U_PORT_TEST_ASSERT(minFreeStackBytes >=
                           U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);
    }

    uPortDeinit();
}

// End of file
