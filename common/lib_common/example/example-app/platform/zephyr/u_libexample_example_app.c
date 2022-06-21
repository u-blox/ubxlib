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

/** @file
 * @brief Zephyr simple example of how to use a library.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_error_common.h"
#include "u_assert.h"
#include "u_port_clib_platform_specific.h"
#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_uart.h"

#include "string.h"

#include "zephyr.h"
#include "u_lib.h"
#include "lib_fibonacci.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

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
static void appTask(void *pParam)
{
    (void) pParam;
    // our library handle
    static uLibHdl_t libHdl;

    // utility functions for libfibonacci
    // libfibonacci needs a malloc and a free
    static uLibLibc_t libc = {
        .fnmalloc = simpleMalloc,
        .fnfree = simpleFree,
        .fnvprintf = 0
    };

    uPortInit();
    uPortLog("\n\nU_APP: application task started.\n");
    int res;

    // probe the library address to see if there is a proper library
    uLibHdr_t libHdr; // if the probe succeeds, we will get library information in here
    uPortLogF("\nProbing lib\n");
    res = uLibProbe(&libHdr, &__libfibonacci_blob[0]);
    if (res) {
        uPortLogF("error %d\nhalt\n", res);
        while (1);
    }

    uPortLogF("name:    %s\n", libHdr.name);
    uPortLogF("version: %08x\n", libHdr.version);
    uPortLogF("flags:   %08x (arch id %02x)\n\n", libHdr.flags, U_LIB_HDR_FLAG_GET_ARCH(libHdr.flags));
    // depending on the flags, one can for instance validate the
    // library here so we don't start running malicious code

    // try opening the library
    uPortLogF("Opening lib\n");
    res = uLibOpen(&libHdl, &__libfibonacci_blob[0], &libc, 0);
    if (res) {
        uPortLogF("error %d\nhalt\n", res);
        while (1);
    }

    // declare the library api functions, see lib_fibonacci.h
    int (*libFibCalc)(void *ctx, int series);
    int (*libFibLastRes)(void *ctx);
    const char *(*libFibHelloWorld)(void *ctx);

    // look up addresses to library functions
    libFibCalc = uLibSym(&libHdl, "libFibCalc");
    libFibLastRes = uLibSym(&libHdl, "libFibLastRes");
    libFibHelloWorld = uLibSym(&libHdl, "libFibHelloWorld");

    uPortLogF("@libFibCalc:      %p\n", libFibCalc);
    uPortLogF("@libFibLastRes:   %p\n", libFibLastRes);
    uPortLogF("@libFibHelloWorld:%p\n\n", libFibHelloWorld);

    // start calling the library
    uPortLogF("libFibCalc(102):  %d\n", libFibCalc(libHdl.ictx, 102));
    uPortLogF("libFibLastRes:    %d\n", libFibLastRes(libHdl.ictx));
    uPortLogF("libFibHelloWorld: %s (%p)\n", libFibHelloWorld(libHdl.ictx),
              libFibHelloWorld(libHdl.ictx));

    for (int i = 10; i < 20; i++) {
        uPortLogF("libFibCalc(%d):   %d\n", i, libFibCalc(libHdl.ictx, i));
    }

    // try relocate the library to ram instead
    uPortLogF("\nRelocate library code to ram\n");
    // get address and length of the library code blob
    const void *pCode;
    uint32_t codeLen;
    size_t chunkSize;
    res = uLibGetCode(&libHdl, &pCode, &codeLen);
    reloc_buf = uPortAcquireExecutableChunk(NULL,
                                            &chunkSize,
                                            U_PORT_EXECUTABLE_CHUNK_NO_FLAGS,
                                            U_PORT_EXECUTABLE_CHUNK_INDEX_0);
    if (res) {
        uPortLogF("error %d\nhalt\n", res);
        while (1);
    }

    uPortLog("Code currently resides @ %p, %d bytes\n", pCode, codeLen);
    uPortLog("Moving code to %p\n", reloc_buf);
    if (codeLen >= chunkSize) {
        uPortLog("Reloc_buf to small to fit code, need %d. Cannot continue...", codeLen);
        while (1);
    }
    // instead of memcpy here, one could for instance decrypt the code
    memcpy(reloc_buf, pCode, codeLen);
    res = uLibRelocate(&libHdl, reloc_buf);
    if (res) {
        uPortLogF("error %d\nhalt\n", res);
        while (1);
    }

    // after relocating, we need to update symbols
    libFibCalc = uLibSym(&libHdl, "libFibCalc");
    libFibLastRes = uLibSym(&libHdl, "libFibLastRes");
    libFibHelloWorld = uLibSym(&libHdl, "libFibHelloWorld");

    uPortLogF("@libFibCalc:      %p\n", libFibCalc);
    uPortLogF("@libFibLastRes:   %p\n", libFibLastRes);
    uPortLogF("@libFibHelloWorld:%p\n\n", libFibHelloWorld);

    // call the library again, now we will execute from ram
    uPortLogF("libFibLastRes:    %d\n", libFibLastRes(libHdl.ictx));
    uPortLogF("libFibCalc(102):  %d\n", libFibCalc(libHdl.ictx, 102));
    for (int i = 10; i < 20; i++) {
        uPortLogF("libFibCalc(%d):   %d\n", i, libFibCalc(libHdl.ictx, i));
    }
    uPortLogF("libFibLastRes:    %d\n", libFibLastRes(libHdl.ictx));
    uPortLogF("libFibHelloWorld: %s (%p)\n", libFibHelloWorld(libHdl.ictx),
              libFibHelloWorld(libHdl.ictx));

    // close library
    uPortLogF("\nClosing lib\n");
    res = uLibClose(&libHdl);
    if (res) {
        uPortLogF("error %d\nhalt\n", res);
        while (1);
    }

    uPortLog("\n\nU_APP: application task ended.\n");
    uPortDeinit();

    while (1) {}
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Entry point
int main(void)
{
    // Start the platform to run the tests
    uPortPlatformStart(appTask, NULL, 0, 0);

    // Should never get here
    U_ASSERT(false);

    return 0;
}

// End of file
