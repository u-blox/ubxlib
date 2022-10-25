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
 * @brief Tests for the cellular TLS security API. These should pass
 * on all platforms that have a cellular module connected to them.
 * They are only compiled if U_CFG_TEST_CELL_MODULE_TYPE is defined.
 */

#ifdef U_CFG_TEST_CELL_MODULE_TYPE

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // strcmp(), memset()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_os.h"   // Required by u_cell_private.h
#include "u_port_uart.h"

#include "u_at_client.h"

#include "u_cell_module_type.h"
#include "u_cell.h"
#include "u_cell_file.h"
#include "u_cell_net.h"     // Required by u_cell_private.h
#include "u_cell_private.h" // So that we can get at some innards
#include "u_cell_pwr.h"
#include "u_cell_sec_tls.h"

#include "u_cell_test_cfg.h"
#include "u_cell_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_CELL_SEC_TLS_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

/** All the "name" strings used in this test are of the same form
 * ("test_name_x") and hence the same length and this is the length
 * (not including the null terminator).
 */
#define U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES 11

#ifndef U_CELL_SEC_TLS_TEST_CIPHER_1
/** A cipher we know all cellular modules support:
 * TLS_RSA_WITH_3DES_EDE_CBC_SHA. */
# define U_CELL_SEC_TLS_TEST_CIPHER_1 0x000a
#endif

#ifndef U_CELL_SEC_TLS_TEST_CIPHER_2
/** A cipher we know all cellular modules support:
 * TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA. */
# define U_CELL_SEC_TLS_TEST_CIPHER_2 0xC003
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Handles.
 */
static uCellTestPrivate_t gHandles = U_CELL_TEST_PRIVATE_DEFAULTS;

/** All the possible TLS versions, deliberately in reverse
 * order so that when testing we don't have the default
 * first in the list (when it should already be at the default).
 */
static int32_t gTlsVersions[] = {12, 11, 10, 0};

/** All the possible checking levels, again in reverse order
 * so that the default isn't at the start.
 */
static uCellSecTlsCertficateCheck_t gChecks[] = {U_CELL_SEC_TLS_CERTIFICATE_CHECK_ROOT_CA_URL_DATE,
                                                 U_CELL_SEC_TLS_CERTIFICATE_CHECK_ROOT_CA_URL,
                                                 U_CELL_SEC_TLS_CERTIFICATE_CHECK_ROOT_CA,
                                                 U_CELL_SEC_TLS_CERTIFICATE_CHECK_NONE
                                                };

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Test all of the settings.
 */
U_PORT_TEST_FUNCTION("[cellSecTls]", "cellSecTlsSettings")
{
    uDeviceHandle_t cellHandle;
    int32_t heapUsed;
    const uCellPrivateModule_t *pModule;
    uCellSecTlsContext_t *pContext;
    char *pBuffer;
    size_t numCiphers;
    bool good;
    int32_t y;
    int32_t z;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    // malloc a buffer to put names in
    pBuffer = (char *) pUPortMalloc(U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES + 1);
    U_PORT_TEST_ASSERT(pBuffer != NULL);

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Get the module data, we will need it later
    pModule = pUCellPrivateGetModule(cellHandle);
    U_PORT_TEST_ASSERT(pModule != NULL);

    // Add a security context
    U_TEST_PRINT_LINE("adding a security context...");
    pContext = pUCellSecSecTlsAdd(cellHandle);
    U_PORT_TEST_ASSERT(pContext != NULL);

    // Check that last error returns zero
    U_PORT_TEST_ASSERT(uCellSecTlsResetLastError() == 0);

    // Check for defaults
    U_TEST_PRINT_LINE("checking defaults...");
    U_PORT_TEST_ASSERT(uCellSecTlsRootCaCertificateNameGet(pContext, pBuffer,
                                                           U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES + 1) == 0);
    U_PORT_TEST_ASSERT(strcmp(pBuffer, "") == 0);
    U_PORT_TEST_ASSERT(uCellSecTlsClientCertificateNameGet(pContext, pBuffer,
                                                           U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES + 1) == 0);
    U_PORT_TEST_ASSERT(strcmp(pBuffer, "") == 0);
    U_PORT_TEST_ASSERT(uCellSecTlsClientPrivateKeyNameGet(pContext, pBuffer,
                                                          U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES + 1) == 0);
    U_PORT_TEST_ASSERT(strcmp(pBuffer, "") == 0);
    U_TEST_PRINT_LINE("default ciphers are:");
    numCiphers = 0;
    for (int32_t x = uCellSecTlsCipherSuiteListFirst(pContext);
         x >= 0;
         x = uCellSecTlsCipherSuiteListNext(pContext)) {
        numCiphers++;
        U_TEST_PRINT_LINE("    0x%04x", x);
    }
    U_TEST_PRINT_LINE("%d cipher(s) found.", numCiphers);
    U_PORT_TEST_ASSERT(numCiphers == 0);
    // SARA-R5 and SARA-R422 have the default of 1.2
    U_PORT_TEST_ASSERT((uCellSecTlsVersionGet(pContext) == 0) ||
                       (uCellSecTlsVersionGet(pContext) == 12));
    // SARA-R5, SARA-R422 and LARA-R6 have the default of root CA checking
    if ((pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R5) ||
        (pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R422) ||
        (pModule->moduleType == U_CELL_MODULE_TYPE_LARA_R6)) {
        U_PORT_TEST_ASSERT(uCellSecTlsCertificateCheckGet(pContext, NULL, 0) ==
                           (int32_t) U_CELL_SEC_TLS_CERTIFICATE_CHECK_ROOT_CA);
    } else {
        U_PORT_TEST_ASSERT(uCellSecTlsCertificateCheckGet(pContext, NULL, 0) ==
                           (int32_t) U_CELL_SEC_TLS_CERTIFICATE_CHECK_NONE);
    }
    if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_SECURITY_TLS_SERVER_NAME_INDICATION)) {
        U_PORT_TEST_ASSERT(uCellSecTlsSniGet(pContext, pBuffer,
                                             U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES + 1) == 0);
        U_PORT_TEST_ASSERT(strcmp(pBuffer, "") == 0);
    } else {
        U_PORT_TEST_ASSERT(uCellSecTlsSniGet(pContext, pBuffer,
                                             U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES + 1) < 0);
    }

    // Check that the root/CA certificate name can be set/got
    U_TEST_PRINT_LINE("checking root/CA certificate name...");
    U_PORT_TEST_ASSERT(uCellSecTlsRootCaCertificateNameSet(pContext,
                                                           "test_name_1") == 0);
    U_PORT_TEST_ASSERT(uCellSecTlsRootCaCertificateNameGet(pContext, pBuffer,
                                                           U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES + 1) ==
                       U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(strcmp(pBuffer, "test_name_1") == 0);
    U_PORT_TEST_ASSERT(uCellSecTlsRootCaCertificateNameSet(pContext,
                                                           "test_name_x") == 0);
    U_PORT_TEST_ASSERT(uCellSecTlsRootCaCertificateNameGet(pContext, pBuffer,
                                                           U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES + 1) ==
                       U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(strcmp(pBuffer, "test_name_x") == 0);

    // Check that the client certificate name can be set/got
    U_TEST_PRINT_LINE("checking client certificate name...");
    U_PORT_TEST_ASSERT(uCellSecTlsClientCertificateNameSet(pContext,
                                                           "test_name_2") == 0);
    U_PORT_TEST_ASSERT(uCellSecTlsClientCertificateNameGet(pContext, pBuffer,
                                                           U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES + 1) ==
                       U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(strcmp(pBuffer, "test_name_2") == 0);
    U_PORT_TEST_ASSERT(uCellSecTlsClientCertificateNameSet(pContext,
                                                           "test_name_x") == 0);
    U_PORT_TEST_ASSERT(uCellSecTlsClientCertificateNameGet(pContext, pBuffer,
                                                           U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES + 1) ==
                       U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(strcmp(pBuffer, "test_name_x") == 0);

    // Check that the client private key name can be set/got
    U_TEST_PRINT_LINE("checking client private key name...");
    U_PORT_TEST_ASSERT(uCellSecTlsClientPrivateKeyNameSet(pContext,
                                                          "test_name_3",
                                                          NULL) == 0);
    U_PORT_TEST_ASSERT(uCellSecTlsClientPrivateKeyNameGet(pContext, pBuffer,
                                                          U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES + 1) ==
                       U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(strcmp(pBuffer, "test_name_3") == 0);
    U_PORT_TEST_ASSERT(uCellSecTlsClientPrivateKeyNameSet(pContext,
                                                          "test_name_x",
                                                          NULL) == 0);
    U_PORT_TEST_ASSERT(uCellSecTlsClientPrivateKeyNameGet(pContext, pBuffer,
                                                          U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES + 1) ==
                       U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(strcmp(pBuffer, "test_name_x") == 0);

    // Check that the Psk/PskId can be set
    U_TEST_PRINT_LINE("checking PSK and PSK ID...");
    U_PORT_TEST_ASSERT(uCellSecTlsClientPskSet(pContext, "this_is_a_password", 18,
                                               "this_is_the_id_for_the_password", 31,
                                               false) == 0);
    U_TEST_PRINT_LINE("checking fail cases...");
    // Try with ID missing
    U_PORT_TEST_ASSERT(uCellSecTlsClientPskSet(pContext, "this_is_a_password_again",
                                               24, NULL, 0, false) < 0);

    // Check that last error returns negative and then is reset
    U_PORT_TEST_ASSERT(uCellSecTlsResetLastError() < 0);
    U_PORT_TEST_ASSERT(uCellSecTlsResetLastError() == 0);

    // Try with password missing
    U_PORT_TEST_ASSERT(uCellSecTlsClientPskSet(pContext, NULL, 0,
                                               "this_is_the_id_for_the_password_again",
                                               37, false) < 0);

    // Check that the Psk/PskId can be set once more
    U_TEST_PRINT_LINE("checking PSK and PSK ID again...");
    U_PORT_TEST_ASSERT(uCellSecTlsClientPskSet(pContext, "this_is_a_password_final", 24,
                                               "this_is_the_id_for_the_password_final", 37,
                                               false) == 0);

    // If root of trust is supported, check that it can be requested to
    // do the PSK stuff
    if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_ROOT_OF_TRUST)) {
        U_PORT_TEST_ASSERT(uCellSecTlsClientPskSet(pContext, NULL, 0, NULL, 0,
                                                   true) == 0);
    }

    // Check cipher management
    U_TEST_PRINT_LINE("checking manipulation of cipher list...");

    if (U_CELL_PRIVATE_HAS(pModule,
                           U_CELL_PRIVATE_FEATURE_SECURITY_TLS_CIPHER_LIST)) {
        // For modules which support a list of ciphers, add a cipher that we
        // know all cellular modules support
        U_PORT_TEST_ASSERT(uCellSecTlsCipherSuiteAdd(pContext,
                                                     U_CELL_SEC_TLS_TEST_CIPHER_1) == 0);
        z = 0;
        good = false;
        for (int32_t x = uCellSecTlsCipherSuiteListFirst(pContext);
             (x >= 0);
             x = uCellSecTlsCipherSuiteListNext(pContext)) {
            z++;
            if (x == U_CELL_SEC_TLS_TEST_CIPHER_1) {
                good = true;
            }
        }
        U_PORT_TEST_ASSERT(good);
        U_PORT_TEST_ASSERT(z == numCiphers + 1);

        // Add another
        U_PORT_TEST_ASSERT(uCellSecTlsCipherSuiteAdd(pContext,
                                                     U_CELL_SEC_TLS_TEST_CIPHER_2) == 0);
        z = 0;
        good = false;
        for (int32_t x = uCellSecTlsCipherSuiteListFirst(pContext);
             (x >= 0);
             x = uCellSecTlsCipherSuiteListNext(pContext)) {
            z++;
            if (x == U_CELL_SEC_TLS_TEST_CIPHER_2) {
                good = true;
            }
        }
        U_PORT_TEST_ASSERT(good);
        U_PORT_TEST_ASSERT(z == numCiphers + 2);

        // Remove the first and check that it's gone
        U_PORT_TEST_ASSERT(uCellSecTlsCipherSuiteRemove(pContext, U_CELL_SEC_TLS_TEST_CIPHER_1) == 0);
        z = 0;
        for (int32_t x = uCellSecTlsCipherSuiteListFirst(pContext);
             x >= 0;
             x = uCellSecTlsCipherSuiteListNext(pContext)) {
            z++;
            U_PORT_TEST_ASSERT(x != U_CELL_SEC_TLS_TEST_CIPHER_1);
        }
        U_PORT_TEST_ASSERT(z == numCiphers + 1);

        // Remove the last and check that it's gone
        U_PORT_TEST_ASSERT(uCellSecTlsCipherSuiteRemove(pContext, U_CELL_SEC_TLS_TEST_CIPHER_2) == 0);
        z = 0;
        for (int32_t x = uCellSecTlsCipherSuiteListFirst(pContext);
             x >= 0;
             x = uCellSecTlsCipherSuiteListNext(pContext)) {
            z++;
            U_PORT_TEST_ASSERT(x != U_CELL_SEC_TLS_TEST_CIPHER_2);
        }
        U_PORT_TEST_ASSERT(z == numCiphers);
    } else {
        // Should still be able to add and remove one cipher
        U_PORT_TEST_ASSERT(uCellSecTlsCipherSuiteAdd(pContext,
                                                     U_CELL_SEC_TLS_TEST_CIPHER_1) == 0);
        U_PORT_TEST_ASSERT(uCellSecTlsCipherSuiteRemove(pContext,
                                                        U_CELL_SEC_TLS_TEST_CIPHER_1) == 0);
        U_PORT_TEST_ASSERT(uCellSecTlsCipherSuiteListFirst(pContext) < 0);
        U_PORT_TEST_ASSERT(uCellSecTlsCipherSuiteListNext(pContext) < 0);
    }

    // Check that all the TLS versions can be set
    U_TEST_PRINT_LINE("checking setting TLS version...");
    for (size_t x = 0; x < sizeof(gTlsVersions) / sizeof(gTlsVersions[0]); x++) {
        U_PORT_TEST_ASSERT(uCellSecTlsVersionSet(pContext, gTlsVersions[x]) == 0);
        U_PORT_TEST_ASSERT(uCellSecTlsVersionGet(pContext) == gTlsVersions[x]);
    }

    // Check that all the checking levels can be set
    U_TEST_PRINT_LINE("checking setting validation level...");
    for (size_t x = 0; x < sizeof(gChecks) / sizeof(gChecks[0]); x++) {
        if (gChecks[x] < U_CELL_SEC_TLS_CERTIFICATE_CHECK_ROOT_CA_URL) {
            U_PORT_TEST_ASSERT(uCellSecTlsCertificateCheckSet(pContext, gChecks[x], NULL) == 0);
            U_PORT_TEST_ASSERT(uCellSecTlsCertificateCheckGet(pContext, NULL, 0) == (int32_t) gChecks[x]);
        } else {
            memset(pBuffer, 0, U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES + 1);
            U_PORT_TEST_ASSERT(uCellSecTlsCertificateCheckSet(pContext, gChecks[x], "test_name_4") == 0);
            U_PORT_TEST_ASSERT(uCellSecTlsCertificateCheckGet(pContext, pBuffer,
                                                              U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES + 1) == (int32_t) gChecks[x]);
            U_PORT_TEST_ASSERT(strcmp(pBuffer, "test_name_4") == 0);
            U_PORT_TEST_ASSERT(uCellSecTlsCertificateCheckSet(pContext, gChecks[x], "test_name_x") == 0);
            U_PORT_TEST_ASSERT(uCellSecTlsCertificateCheckGet(pContext, NULL, 0) == (int32_t) gChecks[x]);
        }
    }

    if (U_CELL_PRIVATE_HAS(pModule,
                           U_CELL_PRIVATE_FEATURE_SECURITY_TLS_SERVER_NAME_INDICATION)) {
        // Check that SNI can be set
        U_TEST_PRINT_LINE("checking SNI...");
        U_PORT_TEST_ASSERT(uCellSecTlsSniSet(pContext, "test_name_5") == 0);
        U_PORT_TEST_ASSERT(uCellSecTlsSniGet(pContext, pBuffer,
                                             U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES + 1) ==
                           U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES);
        U_PORT_TEST_ASSERT(strcmp(pBuffer, "test_name_5") == 0);
        U_PORT_TEST_ASSERT(uCellSecTlsSniSet(pContext,
                                             "test_name_x") == 0);
        U_PORT_TEST_ASSERT(uCellSecTlsSniGet(pContext, pBuffer,
                                             U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES + 1) ==
                           U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES);
        U_PORT_TEST_ASSERT(strcmp(pBuffer, "test_name_x") == 0);
    } else {
        U_PORT_TEST_ASSERT(uCellSecTlsSniSet(pContext, "test_name_5") < 0);
        U_PORT_TEST_ASSERT(uCellSecTlsSniGet(pContext, pBuffer,
                                             U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES + 1) < 0);
    }

    // TODO currently there are no automated tests of
    // uCellSecTlsUseDeviceCertificateSet() and uCellSecTlsIsUsingDeviceCertificate()
    // since none of the FW versions we have on the modules of the
    // automated test system support it.  The functions have instead
    // been tested manually.  Come back to this when we have replaced
    // the module HW so that a later module FW can be used.

    // Remove the security context
    U_TEST_PRINT_LINE("removing security context...");
    uCellSecTlsRemove(pContext);

    // Add it again and re-check for defaults
    U_TEST_PRINT_LINE("re-adding security context...");
    pContext = pUCellSecSecTlsAdd(cellHandle);
    U_PORT_TEST_ASSERT(pContext != NULL);

    // Check for defaults
    U_TEST_PRINT_LINE("re-checking defaults...");
    U_PORT_TEST_ASSERT(uCellSecTlsRootCaCertificateNameGet(pContext, pBuffer,
                                                           U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES + 1) == 0);
    U_PORT_TEST_ASSERT(strcmp(pBuffer, "") == 0);
    U_PORT_TEST_ASSERT(uCellSecTlsClientCertificateNameGet(pContext, pBuffer,
                                                           U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES + 1) == 0);
    U_PORT_TEST_ASSERT(strcmp(pBuffer, "") == 0);
    U_PORT_TEST_ASSERT(uCellSecTlsClientPrivateKeyNameGet(pContext, pBuffer,
                                                          U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES + 1) == 0);
    U_PORT_TEST_ASSERT(strcmp(pBuffer, "") == 0);
    U_TEST_PRINT_LINE("default ciphers are:");
    y = 0;
    for (int32_t x = uCellSecTlsCipherSuiteListFirst(pContext);
         x >= 0;
         x = uCellSecTlsCipherSuiteListNext(pContext)) {
        y++;
        U_TEST_PRINT_LINE("    0x%04x", x);
    }
    U_TEST_PRINT_LINE("%d cipher(s) found.", y);
    U_PORT_TEST_ASSERT(y == numCiphers);
    // SARA-R5 and SARA-R422 have the default of 1.2
    U_PORT_TEST_ASSERT((uCellSecTlsVersionGet(pContext) == 0) ||
                       (uCellSecTlsVersionGet(pContext) == 12));
    // SARA-R5, SARA-R422 and LARA-R6 have the default of root CA checking
    if ((pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R5) ||
        (pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R422) ||
        (pModule->moduleType == U_CELL_MODULE_TYPE_LARA_R6)) {
        U_PORT_TEST_ASSERT(uCellSecTlsCertificateCheckGet(pContext, NULL, 0) ==
                           (int32_t) U_CELL_SEC_TLS_CERTIFICATE_CHECK_ROOT_CA);
    } else {
        U_PORT_TEST_ASSERT(uCellSecTlsCertificateCheckGet(pContext, NULL, 0) ==
                           (int32_t) U_CELL_SEC_TLS_CERTIFICATE_CHECK_NONE);
    }
    if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_SECURITY_TLS_SERVER_NAME_INDICATION)) {
        U_PORT_TEST_ASSERT(uCellSecTlsSniGet(pContext, pBuffer,
                                             U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES + 1) == 0);
        U_PORT_TEST_ASSERT(strcmp(pBuffer, "") == 0);
    } else {
        U_PORT_TEST_ASSERT(uCellSecTlsSniGet(pContext, pBuffer,
                                             U_CELL_SEC_TLS_TEST_NAME_LENGTH_BYTES + 1) < 0);
    }

    // Remove the security context again
    U_TEST_PRINT_LINE("removing security context again...");
    uCellSecTlsRemove(pContext);

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Release memory
    uPortFree(pBuffer);

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
U_PORT_TEST_FUNCTION("[cellSecTls]", "cellSecTlsCleanUp")
{
    int32_t minFreeStackBytes;

    uCellTestPrivateCleanup(&gHandles);

    minFreeStackBytes = uPortTaskStackMinFree(NULL);
    if (minFreeStackBytes != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_TEST_PRINT_LINE("main task stack had a minimum of %d byte(s)"
                          " free at the end of these tests.", minFreeStackBytes);
        U_PORT_TEST_ASSERT(minFreeStackBytes >=
                           U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);
    }

    uPortDeinit();
}

#endif // #ifdef U_CFG_TEST_CELL_MODULE_TYPE

// End of file
