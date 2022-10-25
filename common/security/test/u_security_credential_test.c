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
 * @brief Test for the u-blox security credential API: these should
 * pass on all platforms.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // strcmp()

#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"

#ifdef U_CFG_TEST_CELL_MODULE_TYPE
#include "u_cell_module_type.h"
#include "u_cell_test_cfg.h" // For the cellular test macros
#endif

#include "u_network.h"
#include "u_network_config_ble.h"
#include "u_network_test_shared_cfg.h"

#include "u_security_credential.h"
#include "u_security_credential_test_data.h"
#include "u_short_range_module_type.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The base string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX_BASE "U_SECURITY_CREDENTIAL_TEST"

/** The string to put at the start of all prints from this test
 * that do not require any iterations on the end.
 */
#define U_TEST_PREFIX U_TEST_PREFIX_BASE ": "

/** Print a whole line, with terminator, prefixed for this test
 * file, no iteration(s) version.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

/** The string to put at the start of all prints from this test
 * where an interation is required on the end.
 */
#define U_TEST_PREFIX_X U_TEST_PREFIX_BASE "_%d: "

/** Print a whole line, with terminator and an iteration on the end,
 * prefixed for this test file.
 */
#define U_TEST_PRINT_LINE_X(format, ...) uPortLog(U_TEST_PREFIX_X format "\n", ##__VA_ARGS__)

/** The string to put at the start of all prints from this test
 * where two interations are required on the end.
 */
#define U_TEST_PREFIX_X_Y U_TEST_PREFIX_BASE "_%d_%d: "

/** Print a whole line, with terminator and iterations on the end,
 * prefixed for this test file.
 */
#define U_TEST_PRINT_LINE_X_Y(format, ...) uPortLog(U_TEST_PREFIX_X_Y format "\n", ##__VA_ARGS__)

/** Some cellular modules don't support use of a password when
 * storing a security key.
 */
#ifdef U_CFG_TEST_CELL_MODULE_TYPE
# define U_SECURITY_CREDENTIAL_TEST_CELL_PASSWORD_SUPPORTED                \
    ((U_CFG_TEST_CELL_MODULE_TYPE != U_CELL_MODULE_TYPE_SARA_U201) &&      \
     (U_CFG_TEST_CELL_MODULE_TYPE != U_CELL_MODULE_TYPE_SARA_R410M_02B) && \
     (U_CFG_TEST_CELL_MODULE_TYPE != U_CELL_MODULE_TYPE_SARA_R412M_02B) && \
     (U_CFG_TEST_CELL_MODULE_TYPE != U_CELL_MODULE_TYPE_SARA_R412M_03B))
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

#ifdef U_SECURITY_CREDENTIAL_TEST_FORMATS
/** Not a test, since it doesn't have any test asserts in it, but
 * a function to try all the possible credential formats/encodings
 * and hence determine what a given module supports.
 */
U_PORT_TEST_FUNCTION("[securityCredential]", "securityCredentialFormats")
{
    uNetworkTestList_t *pList;
    uDeviceHandle_t devHandle = NULL;

    // In case a previous test failed
    uNetworkTestCleanUp();

    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uDeviceInit() == 0);

    U_TEST_PRINT_LINE("checking which storage formats are supported.");

    // Get a list of things that support credential storage
    pList = pUNetworkTestListAlloc(uNetworkTestHasCredentialStorage);
    if (pList == NULL) {
        U_TEST_PRINT_LINE("*** WARNING *** nothing to do.");
    }
    // Open the devices that are not already open
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        if (*pTmp->pDevHandle == NULL) {
            U_TEST_PRINT_LINE("adding device %s for network %s...",
                              gpUNetworkTestDeviceTypeName[pTmp->pDeviceCfg->deviceType],
                              gpUNetworkTestTypeName[pTmp->networkType]);
            U_PORT_TEST_ASSERT(uDeviceOpen(pTmp->pDeviceCfg, pTmp->pDevHandle) == 0);
        }
    }

    // Test each device type
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext, x++) {
        devHandle = *pTmp->pDevHandle;
        for (size_t y = 0; y < gUSecurityCredentialTestFormatSize; y++) {
            // Store the security credential
            U_TEST_PRINT_LINE("storing credential %s...",
                              gUSecurityCredentialTestFormat[y].pDescription);
            if (uSecurityCredentialStore(devHandle,
                                         gUSecurityCredentialTestFormat[y].type,
                                         "ubxlib_test",
                                         (const char *) gUSecurityCredentialTestFormat[y].contents,
                                         gUSecurityCredentialTestFormat[y].size,
                                         gUSecurityCredentialTestFormat[y].pPassword,
                                         NULL) == 0) {
                U_TEST_PRINT_LINE("%s format is supported.",
                                  gUSecurityCredentialTestFormat[y].pDescription);
                // Delete the credential
                U_TEST_PRINT_LINE("deleting credential...");
                uSecurityCredentialRemove(devHandle,
                                          gUSecurityCredentialTestFormat[y].type,
                                          "ubxlib_test");
            } else {
                U_TEST_PRINT_LINE("%s format is NOT supported.",
                                  gUSecurityCredentialTestFormat[y].pDescription);
            }

            // Give the module a rest in case we've upset it
            uPortTaskBlock(1000);
        }
    }

    // Close the devices once more and free the list
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        if (*pTmp->pDevHandle != NULL) {
            U_TEST_PRINT_LINE("closing device %s...",
                              gpUNetworkTestDeviceTypeName[pTmp->pDeviceCfg->deviceType]);
            U_PORT_TEST_ASSERT(uDeviceClose(*pTmp->pDevHandle, false) == 0);
            *pTmp->pDevHandle = NULL;
        }
    }
    uNetworkTestListFree();

    uDeviceDeinit();
    uPortDeinit();
}
#endif

/** Test everything; there isn't much.
 */
U_PORT_TEST_FUNCTION("[securityCredential]", "securityCredentialTest")
{
    uNetworkTestList_t *pList;
    uDeviceHandle_t devHandle = NULL;
    int32_t heapUsed;
    uSecurityCredential_t credential;
    int32_t otherCredentialCount;
    int32_t z;
    char hash[U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES];
    char buffer[U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES];

    // In case a previous test failed
    uNetworkTestCleanUp();

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uDeviceInit() == 0);

    // Get a list of things that support credential storage
    pList = pUNetworkTestListAlloc(uNetworkTestHasCredentialStorage);
    if (pList == NULL) {
        U_TEST_PRINT_LINE("*** WARNING *** nothing to do.");
    }
    // Open the devices that are not already open
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        if (*pTmp->pDevHandle == NULL) {
            U_TEST_PRINT_LINE("adding device %s for network %s...",
                              gpUNetworkTestDeviceTypeName[pTmp->pDeviceCfg->deviceType],
                              gpUNetworkTestTypeName[pTmp->networkType]);
            U_PORT_TEST_ASSERT(uDeviceOpen(pTmp->pDeviceCfg, pTmp->pDevHandle) == 0);
        }
    }

    // Test each device type, noting that there's no need to bring
    // any networks up for this test, whether credential storage is
    // possible or not is actually more a property of the device
    int32_t x = 0;
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext, x++) {
        devHandle = *pTmp->pDevHandle;

        U_TEST_PRINT_LINE_X("testing %s.", x, gpUNetworkTestTypeName[pTmp->networkType]);

        // List the credentials at start of day
        U_TEST_PRINT_LINE_X(": listing credentials...", x);
        z = 0;
        otherCredentialCount = 0;
        for (int32_t y = uSecurityCredentialListFirst(devHandle, &credential);
             y >= 0;
             y = uSecurityCredentialListNext(devHandle, &credential)) {
            z++;
            U_TEST_PRINT_LINE_X_Y("credential name \"%s\".", x, z, credential.name);
            U_TEST_PRINT_LINE_X_Y("type %d.", x, z, credential.type);
            U_TEST_PRINT_LINE_X_Y("subject \"%s\".", x, z, credential.subject);
            U_TEST_PRINT_LINE_X_Y("expiration %d UTC.", x, z, credential.expirationUtc);
            if ((strcmp(credential.name, "ubxlib_test_cert") != 0) &&
                (strcmp(credential.name, "ubxlib_test_key") != 0)) {
                otherCredentialCount++;
            }
        }
        U_TEST_PRINT_LINE_X("%d original credential(s) listed.", x, otherCredentialCount);

        // Store the test certificate
        U_TEST_PRINT_LINE_X("storing certificate...", x);
        U_PORT_TEST_ASSERT(uSecurityCredentialStore(devHandle,
                                                    U_SECURITY_CREDENTIAL_CLIENT_X509,
                                                    "ubxlib_test_cert",
                                                    (const char *) gUSecurityCredentialTestClientX509Pem,
                                                    gUSecurityCredentialTestClientX509PemSize,
                                                    NULL, hash) == 0);

        // Read MD5 hash and compare with expected
        U_TEST_PRINT_LINE_X("reading MD5 hash of certificate...", x);
        U_PORT_TEST_ASSERT(uSecurityCredentialGetHash(devHandle,
                                                      U_SECURITY_CREDENTIAL_CLIENT_X509,
                                                      "ubxlib_test_cert",
                                                      buffer) == 0);
        // Compare
        U_TEST_PRINT_LINE_X("checking MD5 hash of certificate...", x);
        for (size_t y = 0; y < sizeof(buffer); y++) {
            U_PORT_TEST_ASSERT((uint8_t) buffer[y] == hash[y]);
        }

        // Check that the certificate is listed
        U_TEST_PRINT_LINE_X("listing credentials...", x);
        z = 0;
        for (int32_t y = uSecurityCredentialListFirst(devHandle, &credential);
             y >= 0;
             y = uSecurityCredentialListNext(devHandle, &credential)) {
            if (strcmp(credential.name, "ubxlib_test_key") != 0) {
                // Do the check above in case there's a ubxlib_test_key
                // left in the system from a previous test
                z++;
            }
            U_TEST_PRINT_LINE_X_Y("credential name \"%s\".", x, z, credential.name);
            U_TEST_PRINT_LINE_X_Y("type %d.", x, z, credential.type);
            U_TEST_PRINT_LINE_X_Y("subject \"%s\".", x, z, credential.subject);
            U_TEST_PRINT_LINE_X_Y("expiration %d UTC.", x, z, credential.expirationUtc);
            if (strcmp(credential.name, "ubxlib_test_cert") == 0) {
                U_PORT_TEST_ASSERT(credential.type == U_SECURITY_CREDENTIAL_CLIENT_X509);
                // Used to check the subject here but V5 uConnectExpress doesn't
                // give what we would expect (the subject of ubxlib_test_cert should
                // be "ubxlib client" but uConnectExpress V5 has it as "CN=ubxlib ca",
                // while earlier version of uConnectExpress don't report it at all),
                // so we can't check it
                if (credential.expirationUtc != 0) {
                    U_PORT_TEST_ASSERT(credential.expirationUtc == U_SECURITY_CREDENTIAL_TEST_X509_EXPIRATION_UTC);
                }
            }
        }
        U_PORT_TEST_ASSERT(z == otherCredentialCount + 1);
        U_TEST_PRINT_LINE_X("%d credential(s) listed.", x, z);

        if (pTmp->networkType == U_NETWORK_TYPE_CELL) {
#ifdef U_CFG_TEST_CELL_MODULE_TYPE
            //lint -e506 -e774 Suppress const value Boolean and always true
            if (U_SECURITY_CREDENTIAL_TEST_CELL_PASSWORD_SUPPORTED) {
                // Store the security key
                U_TEST_PRINT_LINE_X("storing private key...", x);
                U_PORT_TEST_ASSERT(uSecurityCredentialStore(devHandle,
                                                            U_SECURITY_CREDENTIAL_CLIENT_KEY_PRIVATE,
                                                            "ubxlib_test_key",
                                                            (const char *) gUSecurityCredentialTestKey1024Pkcs8Pem,
                                                            gUSecurityCredentialTestKey1024Pkcs8PemSize,
                                                            U_SECURITY_CREDENTIAL_TEST_PASSPHRASE,
                                                            hash) == 0);
            } else {
                // Have to store the unprotected security key,
                // so that SARA-U201 can cope
                U_TEST_PRINT_LINE_X("storing unprotected private key...", x);
                U_PORT_TEST_ASSERT(uSecurityCredentialStore(devHandle,
                                                            U_SECURITY_CREDENTIAL_CLIENT_KEY_PRIVATE,
                                                            "ubxlib_test_key",
                                                            (const char *) gUSecurityCredentialTestKey1024Pkcs1PemNoPass,
                                                            gUSecurityCredentialTestKey1024Pkcs1PemNoPassSize,
                                                            NULL, hash) == 0);
            }
#endif
        } else {
            // Store the security key
            U_TEST_PRINT_LINE_X("storing private key...", x);
            U_PORT_TEST_ASSERT(uSecurityCredentialStore(devHandle,
                                                        U_SECURITY_CREDENTIAL_CLIENT_KEY_PRIVATE,
                                                        "ubxlib_test_key",
                                                        (const char *) gUSecurityCredentialTestKey1024Pkcs1Pem,
                                                        gUSecurityCredentialTestKey1024Pkcs1PemSize,
                                                        U_SECURITY_CREDENTIAL_TEST_PASSPHRASE,
                                                        hash) == 0);
        }

        // Check that both credentials are listed
        U_TEST_PRINT_LINE_X("listing credentials...", x);
        z = 0;
        for (int32_t y = uSecurityCredentialListFirst(devHandle, &credential);
             y >= 0;
             y = uSecurityCredentialListNext(devHandle, &credential)) {
            z++;
            U_TEST_PRINT_LINE_X_Y("credential name \"%s\".", x, z, credential.name);
            U_TEST_PRINT_LINE_X_Y("type %d.", x, z, credential.type);
            U_TEST_PRINT_LINE_X_Y("subject \"%s\".", x, z, credential.subject);
            U_TEST_PRINT_LINE_X_Y("expiration %d UTC.", x, z, credential.expirationUtc);
            if (strcmp(credential.name, "ubxlib_test_cert") == 0) {
                U_PORT_TEST_ASSERT(credential.type == U_SECURITY_CREDENTIAL_CLIENT_X509);
                if (credential.expirationUtc != 0) {
                    U_PORT_TEST_ASSERT(credential.expirationUtc == U_SECURITY_CREDENTIAL_TEST_X509_EXPIRATION_UTC);
                }
            } else if (strcmp(credential.name, "ubxlib_test_key") == 0) {
                U_PORT_TEST_ASSERT(credential.type == U_SECURITY_CREDENTIAL_CLIENT_KEY_PRIVATE);
                U_PORT_TEST_ASSERT(strcmp(credential.subject, "") == 0);
                U_PORT_TEST_ASSERT(credential.expirationUtc == 0);
            }
        }
        U_PORT_TEST_ASSERT(z == otherCredentialCount + 2);
        U_TEST_PRINT_LINE_X("%d credential(s) listed.", x, z);

        // Read MD5 hash and compare with expected
        U_TEST_PRINT_LINE_X("reading MD5 hash of key...", x);
        U_PORT_TEST_ASSERT(uSecurityCredentialGetHash(devHandle,
                                                      U_SECURITY_CREDENTIAL_CLIENT_KEY_PRIVATE,
                                                      "ubxlib_test_key",
                                                      buffer) == 0);
        // Compare
        U_TEST_PRINT_LINE_X("checking MD5 hash of key...", x);
        for (size_t y = 0; y < sizeof(buffer); y++) {
            U_PORT_TEST_ASSERT((uint8_t) buffer[y] == hash[y]);
        }

        // Delete the certificate
        U_TEST_PRINT_LINE_X("deleting certificate...", x);
        U_PORT_TEST_ASSERT(uSecurityCredentialRemove(devHandle,
                                                     U_SECURITY_CREDENTIAL_CLIENT_X509,
                                                     "ubxlib_test_cert") == 0);

        // Check that it is no longer listed
        U_TEST_PRINT_LINE_X("listing credentials...", x);
        z = 0;
        for (int32_t y = uSecurityCredentialListFirst(devHandle, &credential);
             y >= 0;
             y = uSecurityCredentialListNext(devHandle, &credential)) {
            z++;
            U_TEST_PRINT_LINE_X_Y("credential name \"%s\".", x, z, credential.name);
            U_TEST_PRINT_LINE_X_Y("type %d.", x, z, credential.type);
            U_PORT_TEST_ASSERT(strcmp(credential.name, "ubxlib_test_cert") != 0);
            if (strcmp(credential.name, "ubxlib_test_key") == 0) {
                U_PORT_TEST_ASSERT(credential.type == U_SECURITY_CREDENTIAL_CLIENT_KEY_PRIVATE);
                U_PORT_TEST_ASSERT(strcmp(credential.subject, "") == 0);
                U_PORT_TEST_ASSERT(credential.expirationUtc == 0);
            }
        }
        U_PORT_TEST_ASSERT(z == otherCredentialCount + 1);
        U_TEST_PRINT_LINE_X("%d credential(s) listed.", x, z);

        // Delete the security key with a bad name
        U_TEST_PRINT_LINE_X("deleting private key with bad name...", x);
        U_PORT_TEST_ASSERT(uSecurityCredentialRemove(devHandle,
                                                     U_SECURITY_CREDENTIAL_CLIENT_KEY_PRIVATE,
                                                     "xubxlib_test_key") < 0);
        // Delete the security key properly
        U_TEST_PRINT_LINE_X("deleting private key...", x);
        U_PORT_TEST_ASSERT(uSecurityCredentialRemove(devHandle,
                                                     U_SECURITY_CREDENTIAL_CLIENT_KEY_PRIVATE,
                                                     "ubxlib_test_key") == 0);

        // Check that none of ours are listed
        U_TEST_PRINT_LINE_X("listing credentials (should be none of ours)...", x);
        z = 0;
        for (int32_t y = uSecurityCredentialListFirst(devHandle, &credential);
             y >= 0;
             y = uSecurityCredentialListNext(devHandle, &credential)) {
            z++;
            U_TEST_PRINT_LINE_X("name \"%s\".", x, credential.name);
            U_PORT_TEST_ASSERT(strcmp(credential.name, "ubxlib_test_key") != 0);
        }
        U_PORT_TEST_ASSERT(z == otherCredentialCount);
        U_TEST_PRINT_LINE_X("%d credential(s) listed.", x, z);
    }

    // Close the devices once more and free the list
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        if (*pTmp->pDevHandle != NULL) {
            U_TEST_PRINT_LINE("closing device %s...",
                              gpUNetworkTestDeviceTypeName[pTmp->pDeviceCfg->deviceType]);
            U_PORT_TEST_ASSERT(uDeviceClose(*pTmp->pDevHandle, false) == 0);
            *pTmp->pDevHandle = NULL;
        }
    }
    uNetworkTestListFree();

    uDeviceDeinit();
    uPortDeinit();

#ifndef __XTENSA__
    // Check for memory leaks
    // TODO: this if'ed out for ESP32 (xtensa compiler) at
    // the moment as there is an issue with ESP32 hanging
    // on to memory in the UART drivers that can't easily be
    // accounted for.
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("during this test we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
#else
    (void) heapUsed;
#endif
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[securityCredential]", "securityCredentialCleanUp")
{
    int32_t y;

    // The network test configuration is shared between
    // the network, sockets, security and location tests
    // so must reset the handles here in case the
    // tests of one of the other APIs are coming next.
    uNetworkTestCleanUp();
    uDeviceDeinit();

    y = uPortTaskStackMinFree(NULL);
    if (y != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_TEST_PRINT_LINE("main task stack had a minimum of %d byte(s)"
                          " free at the end of these tests.", y);
        U_PORT_TEST_ASSERT(y >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);
    }

    uPortDeinit();

    y = uPortGetHeapMinFree();
    if (y >= 0) {
        U_TEST_PRINT_LINE("heap had a minimum of %d byte(s) free"
                          " at the end of these tests.", y);
        U_PORT_TEST_ASSERT(y >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

// End of file
