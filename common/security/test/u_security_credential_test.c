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

#include "stdlib.h"    // malloc(), free()
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
    uNetworkTestCfg_t *pNetworkCfg = NULL;
    uDeviceHandle_t devHandle = NULL;

    uPortInit();
    uNetworkInit();

    uPortLog("U_SECURITY_CREDENTIAL_TEST: checking which storage"
             " formats are supported.\n");

    // Add each network type
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        gUNetworkTestCfg[x].devHandle = -1;
        if (*((const uNetworkType_t *) (gUNetworkTestCfg[x].pConfiguration)) != U_NETWORK_TYPE_NONE) {
            int32_t returnCode;
            uPortLog("U_SECURITY_CREDENTIAL_TEST: adding %s network...\n",
                     gpUNetworkTestTypeName[gUNetworkTestCfg[x].type]);
#if (U_CFG_APP_GNSS_UART < 0)
            // If there is no GNSS UART then any GNSS chip must
            // be connected via the cellular module's AT interface
            // hence we capture the cellular network handle here and
            // modify the GNSS configuration to use it before we add
            // the GNSS network
            uNetworkTestGnssAtConfiguration(devHandle,
                                            gUNetworkTestCfg[x].pConfiguration);
#endif
            returnCode = uNetworkAdd(gUNetworkTestCfg[x].type,
                                     gUNetworkTestCfg[x].pConfiguration,
                                     &gUNetworkTestCfg[x].devHandle);
            U_PORT_TEST_ASSERT(returnCode >= 0);
            if (gUNetworkTestCfg[x].type == U_NETWORK_TYPE_CELL) {
                devHandle = gUNetworkTestCfg[x].devHandle;
            }
        }
    }

    // Power up each network type
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {

        if (gUNetworkTestCfg[x].devHandle != NULL) {
            pNetworkCfg = &(gUNetworkTestCfg[x]);
            devHandle = pNetworkCfg->devHandle;

            for (size_t y = 0; y < gUSecurityCredentialTestFormatSize; y++) {
                // Store the security credential
                uPortLog("U_SECURITY_CREDENTIAL_TEST: storing credential %s...\n",
                         gUSecurityCredentialTestFormat[y].pDescription);
                if (uSecurityCredentialStore(devHandle,
                                             gUSecurityCredentialTestFormat[y].type,
                                             "ubxlib_test",
                                             (const char *) gUSecurityCredentialTestFormat[y].contents,
                                             gUSecurityCredentialTestFormat[y].size,
                                             gUSecurityCredentialTestFormat[y].pPassword,
                                             NULL) == 0) {
                    uPortLog("U_SECURITY_CREDENTIAL_TEST: %s format is supported.\n",
                             gUSecurityCredentialTestFormat[y].pDescription);
                    // Delete the credential
                    uPortLog("U_SECURITY_CREDENTIAL_TEST: deleting credential...\n");
                    uSecurityCredentialRemove(devHandle,
                                              gUSecurityCredentialTestFormat[y].type,
                                              "ubxlib_test");
                } else {
                    uPortLog("U_SECURITY_CREDENTIAL_TEST: %s format is NOT supported.\n",
                             gUSecurityCredentialTestFormat[y].pDescription);
                }

                // Give the module a rest in case we've upset it
                uPortTaskBlock(1000);
            }

        }
    }

    // Remove each network type, in reverse order so
    // that GNSS is taken down before cellular
    for (int32_t x = (int32_t) gUNetworkTestCfgSize - 1; x >= 0; x--) {
        if (gUNetworkTestCfg[x].devHandle != NULL) {
            uPortLog("U_SECURITY_CREDENTIAL_TEST: taking down %s...\n",
                     gpUNetworkTestTypeName[gUNetworkTestCfg[x].type]);
            U_PORT_TEST_ASSERT(uNetworkDown(gUNetworkTestCfg[x].devHandle) == 0);
        }
    }

    uNetworkDeinit();
    uPortDeinit();
}
#endif

/** Test everything; there isn't much.
 */
U_PORT_TEST_FUNCTION("[securityCredential]", "securityCredentialTest")
{
    uNetworkTestCfg_t *pNetworkCfg = NULL;
    uDeviceHandle_t devHandle = NULL;
    int32_t heapUsed;
    uSecurityCredential_t credential;
    int32_t otherCredentialCount;
    int32_t z;
    int32_t errorCode;
    char hash[U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES];
    char buffer[U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES];

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uNetworkInit() == 0);

    // Add each network type
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        gUNetworkTestCfg[x].devHandle = NULL;
        if ((*((const uNetworkType_t *) (gUNetworkTestCfg[x].pConfiguration)) != U_NETWORK_TYPE_NONE) &&
            U_NETWORK_TEST_TYPE_HAS_CREDENTIAL_STORAGE(gUNetworkTestCfg[x].type,
                                                       ((const uNetworkConfigurationBle_t *) gUNetworkTestCfg[x].pConfiguration)->module)) {
            uPortLog("U_SECURITY_CREDENTIAL_TEST: adding %s network...\n",
                     gpUNetworkTestTypeName[gUNetworkTestCfg[x].type]);
#if (U_CFG_APP_GNSS_UART < 0)
            // If there is no GNSS UART then any GNSS chip must
            // be connected via the cellular module's AT interface
            // hence we capture the cellular network handle here and
            // modify the GNSS configuration to use it before we add
            // the GNSS network
            uNetworkTestGnssAtConfiguration(devHandle,
                                            gUNetworkTestCfg[x].pConfiguration);
#endif
            errorCode = uNetworkAdd(gUNetworkTestCfg[x].type,
                                    gUNetworkTestCfg[x].pConfiguration,
                                    &gUNetworkTestCfg[x].devHandle);
            U_PORT_TEST_ASSERT_EQUAL((int32_t) U_ERROR_COMMON_SUCCESS, errorCode);
            if (gUNetworkTestCfg[x].type == U_NETWORK_TYPE_CELL) {
                devHandle = gUNetworkTestCfg[x].devHandle;
            }
        }
    }

    // Test each network type
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {

        if (gUNetworkTestCfg[x].devHandle != NULL) {
            pNetworkCfg = &(gUNetworkTestCfg[x]);
            devHandle = pNetworkCfg->devHandle;

            uPortLog("U_SECURITY_CREDENTIAL_TEST_%d: testing %s.\n", x,
                     gpUNetworkTestTypeName[pNetworkCfg->type]);

            // List the credentials at start of day
            uPortLog("U_SECURITY_CREDENTIAL_TEST_%d: listing credentials...\n", x);
            z = 0;
            otherCredentialCount = 0;
            for (int32_t y = uSecurityCredentialListFirst(devHandle, &credential);
                 y >= 0;
                 y = uSecurityCredentialListNext(devHandle, &credential)) {
                z++;
                uPortLog("U_SECURITY_CREDENTIAL_TEST_%d_%d: credential name \"%s\".\n", x, z,
                         credential.name);
                uPortLog("U_SECURITY_CREDENTIAL_TEST_%d_%d: type %d.\n", x, z,
                         credential.type);
                uPortLog("U_SECURITY_CREDENTIAL_TEST_%d_%d: subject \"%s\".\n", x, z,
                         credential.subject);
                uPortLog("U_SECURITY_CREDENTIAL_TEST_%d_%d: expiration %d UTC.\n", x, z,
                         credential.expirationUtc);
                if ((strcmp(credential.name, "ubxlib_test_cert") != 0) &&
                    (strcmp(credential.name, "ubxlib_test_key") != 0)) {
                    otherCredentialCount++;
                }
            }
            uPortLog("U_SECURITY_CREDENTIAL_TEST_%d: %d original credential(s) listed.\n",
                     x, otherCredentialCount);

            // Store the test certificate
            uPortLog("U_SECURITY_CREDENTIAL_TEST_%d: storing certificate...\n", x);
            U_PORT_TEST_ASSERT(uSecurityCredentialStore(devHandle,
                                                        U_SECURITY_CREDENTIAL_CLIENT_X509,
                                                        "ubxlib_test_cert",
                                                        (const char *) gUSecurityCredentialTestClientX509Pem,
                                                        gUSecurityCredentialTestClientX509PemSize,
                                                        NULL, hash) == 0);

            // Read MD5 hash and compare with expected
            uPortLog("U_SECURITY_CREDENTIAL_TEST_%d: reading MD5 hash of certificate...\n", x);
            U_PORT_TEST_ASSERT(uSecurityCredentialGetHash(devHandle,
                                                          U_SECURITY_CREDENTIAL_CLIENT_X509,
                                                          "ubxlib_test_cert",
                                                          buffer) == 0);
            // Compare
            uPortLog("U_SECURITY_CREDENTIAL_TEST_%d: checking MD5 hash of certificate...\n", x);
            for (size_t y = 0; y < sizeof(buffer); y++) {
                U_PORT_TEST_ASSERT((uint8_t) buffer[y] == hash[y]);
            }

            // Check that the certificate is listed
            uPortLog("U_SECURITY_CREDENTIAL_TEST_%d: listing credentials...\n", x, z);
            z = 0;
            for (int32_t y = uSecurityCredentialListFirst(devHandle, &credential);
                 y >= 0;
                 y = uSecurityCredentialListNext(devHandle, &credential)) {
                if (strcmp(credential.name, "ubxlib_test_key") != 0) {
                    // Do the check above in case there's a ubxlib_test_key
                    // left in the system from a previous test
                    z++;
                }
                uPortLog("U_SECURITY_CREDENTIAL_TEST_%d_%d: credential name \"%s\".\n", x, z,
                         credential.name);
                uPortLog("U_SECURITY_CREDENTIAL_TEST_%d_%d: type %d.\n", x, z,
                         credential.type);
                uPortLog("U_SECURITY_CREDENTIAL_TEST_%d_%d: subject \"%s\".\n", x, z,
                         credential.subject);
                uPortLog("U_SECURITY_CREDENTIAL_TEST_%d_%d: expiration %d UTC.\n", x, z,
                         credential.expirationUtc);
                if (strcmp(credential.name, "ubxlib_test_cert") == 0) {
                    U_PORT_TEST_ASSERT(credential.type == U_SECURITY_CREDENTIAL_CLIENT_X509);
                    if (strcmp(credential.subject, "") != 0) {
                        U_PORT_TEST_ASSERT(strcmp(credential.subject, U_SECURITY_CREDENTIAL_TEST_X509_SUBJECT) == 0);
                    }
                    if (credential.expirationUtc != 0) {
                        U_PORT_TEST_ASSERT(credential.expirationUtc == U_SECURITY_CREDENTIAL_TEST_X509_EXPIRATION_UTC);
                    }
                }
            }
            U_PORT_TEST_ASSERT(z == otherCredentialCount + 1);
            uPortLog("U_SECURITY_CREDENTIAL_TEST_%d: %d credential(s) listed.\n", x, z);

            if (pNetworkCfg->type == U_NETWORK_TYPE_CELL) {
#ifdef U_CFG_TEST_CELL_MODULE_TYPE
                //lint -e506 -e774 Suppress const value Boolean and always true
                if (U_SECURITY_CREDENTIAL_TEST_CELL_PASSWORD_SUPPORTED) {
                    // Store the security key
                    uPortLog("U_SECURITY_CREDENTIAL_TEST_%d: storing private key...\n", x);
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
                    uPortLog("U_SECURITY_CREDENTIAL_TEST_%d: storing unprotected private key...\n", x);
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
                uPortLog("U_SECURITY_CREDENTIAL_TEST_%d: storing private key...\n", x);
                U_PORT_TEST_ASSERT(uSecurityCredentialStore(devHandle,
                                                            U_SECURITY_CREDENTIAL_CLIENT_KEY_PRIVATE,
                                                            "ubxlib_test_key",
                                                            (const char *) gUSecurityCredentialTestKey1024Pkcs1Pem,
                                                            gUSecurityCredentialTestKey1024Pkcs1PemSize,
                                                            U_SECURITY_CREDENTIAL_TEST_PASSPHRASE,
                                                            hash) == 0);
            }

            // Check that both credentials are listed
            uPortLog("U_SECURITY_CREDENTIAL_TEST_%d: listing credentials...\n", x);
            z = 0;
            for (int32_t y = uSecurityCredentialListFirst(devHandle, &credential);
                 y >= 0;
                 y = uSecurityCredentialListNext(devHandle, &credential)) {
                z++;
                uPortLog("U_SECURITY_CREDENTIAL_TEST_%d_%d: credential name \"%s\".\n", x, z,
                         credential.name);
                uPortLog("U_SECURITY_CREDENTIAL_TEST_%d_%d: type %d.\n", x, z,
                         credential.type);
                uPortLog("U_SECURITY_CREDENTIAL_TEST_%d_%d: subject \"%s\".\n", x, z,
                         credential.subject);
                uPortLog("U_SECURITY_CREDENTIAL_TEST_%d_%d: expiration %d UTC.\n", x, z,
                         credential.expirationUtc);
                if (strcmp(credential.name, "ubxlib_test_cert") == 0) {
                    U_PORT_TEST_ASSERT(credential.type == U_SECURITY_CREDENTIAL_CLIENT_X509);
                    if (strcmp(credential.subject, "") != 0) {
                        U_PORT_TEST_ASSERT(strcmp(credential.subject, U_SECURITY_CREDENTIAL_TEST_X509_SUBJECT) == 0);
                    }
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
            uPortLog("U_SECURITY_CREDENTIAL_TEST_%d: %d credential(s) listed.\n", x, z);

            // Read MD5 hash and compare with expected
            uPortLog("U_SECURITY_CREDENTIAL_TEST_%d: reading MD5 hash of key...\n", x);
            U_PORT_TEST_ASSERT(uSecurityCredentialGetHash(devHandle,
                                                          U_SECURITY_CREDENTIAL_CLIENT_KEY_PRIVATE,
                                                          "ubxlib_test_key",
                                                          buffer) == 0);
            // Compare
            uPortLog("U_SECURITY_CREDENTIAL_TEST_%d: checking MD5 hash of key...\n", x);
            for (size_t y = 0; y < sizeof(buffer); y++) {
                U_PORT_TEST_ASSERT((uint8_t) buffer[y] == hash[y]);
            }

            // Delete the certificate
            uPortLog("U_SECURITY_CREDENTIAL_TEST_%d: deleting certificate...\n", x);
            U_PORT_TEST_ASSERT(uSecurityCredentialRemove(devHandle,
                                                         U_SECURITY_CREDENTIAL_CLIENT_X509,
                                                         "ubxlib_test_cert") == 0);

            // Check that it is no longer listed
            uPortLog("U_SECURITY_CREDENTIAL_TEST_%d: listing credentials...\n", x);
            z = 0;
            for (int32_t y = uSecurityCredentialListFirst(devHandle, &credential);
                 y >= 0;
                 y = uSecurityCredentialListNext(devHandle, &credential)) {
                z++;
                uPortLog("U_SECURITY_CREDENTIAL_TEST_%d_%d: credential name \"%s\".\n", x, z,
                         credential.name);
                uPortLog("U_SECURITY_CREDENTIAL_TEST_%d_%d: type %d.\n", x, z,
                         credential.type);
                U_PORT_TEST_ASSERT(strcmp(credential.name, "ubxlib_test_cert") != 0);
                if (strcmp(credential.name, "ubxlib_test_key") == 0) {
                    U_PORT_TEST_ASSERT(credential.type == U_SECURITY_CREDENTIAL_CLIENT_KEY_PRIVATE);
                    U_PORT_TEST_ASSERT(strcmp(credential.subject, "") == 0);
                    U_PORT_TEST_ASSERT(credential.expirationUtc == 0);
                }
            }
            U_PORT_TEST_ASSERT(z == otherCredentialCount + 1);
            uPortLog("U_SECURITY_CREDENTIAL_TEST_%d: %d credential(s) listed.\n", x, z);

            // Delete the security key with a bad name
            uPortLog("U_SECURITY_CREDENTIAL_TEST_%d: deleting private key with bad name...\n", x);
            U_PORT_TEST_ASSERT(uSecurityCredentialRemove(devHandle,
                                                         U_SECURITY_CREDENTIAL_CLIENT_KEY_PRIVATE,
                                                         "xubxlib_test_key") < 0);
            // Delete the security key properly
            uPortLog("U_SECURITY_CREDENTIAL_TEST_%d: deleting private key...\n", x);
            U_PORT_TEST_ASSERT(uSecurityCredentialRemove(devHandle,
                                                         U_SECURITY_CREDENTIAL_CLIENT_KEY_PRIVATE,
                                                         "ubxlib_test_key") == 0);

            // Check that none of ours are listed
            uPortLog("U_SECURITY_CREDENTIAL_TEST_%d: listing credentials (should be"
                     " none of ours)...\n", x);
            z = 0;
            for (int32_t y = uSecurityCredentialListFirst(devHandle, &credential);
                 y >= 0;
                 y = uSecurityCredentialListNext(devHandle, &credential)) {
                z++;
                uPortLog("U_SECURITY_CREDENTIAL_TEST_%d: name \"%s\".\n", x, credential.name);
                U_PORT_TEST_ASSERT(strcmp(credential.name, "ubxlib_test_key") != 0);
            }
            U_PORT_TEST_ASSERT(z == otherCredentialCount);
            uPortLog("U_SECURITY_CREDENTIAL_TEST_%d: %d credential(s) listed.\n", x, z);
        }
    }

    // Remove each network type, in reverse order so
    // that GNSS (which might be connected via a cellular
    // module) is taken down before cellular
    for (int32_t x = (int32_t) gUNetworkTestCfgSize - 1; x >= 0; x--) {
        if (gUNetworkTestCfg[x].devHandle != NULL) {
            uPortLog("U_SECURITY_CREDENTIAL_TEST: taking down %s...\n",
                     gpUNetworkTestTypeName[gUNetworkTestCfg[x].type]);
            U_PORT_TEST_ASSERT(uNetworkDown(gUNetworkTestCfg[x].devHandle) == 0);
        }
    }

    uNetworkDeinit();
    uPortDeinit();

#ifndef __XTENSA__
    // Check for memory leaks
    // TODO: this if'ed out for ESP32 (xtensa compiler) at
    // the moment as there is an issue with ESP32 hanging
    // on to memory in the UART drivers that can't easily be
    // accounted for.
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_SECURITY_CREDENTIAL_TEST: during this test we have"
             " leaked %d byte(s).\n", heapUsed);
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
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        gUNetworkTestCfg[x].devHandle = NULL;
    }
    uNetworkDeinit();

    y = uPortTaskStackMinFree(NULL);
    if (y != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        uPortLog("U_SECURITY_CREDENTIAL_TEST: main task stack had a minimum of %d"
                 " byte(s) free at the end of these tests.\n", y);
        U_PORT_TEST_ASSERT(y >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);
    }

    uPortDeinit();

    y = uPortGetHeapMinFree();
    if (y >= 0) {
        uPortLog("U_SECURITY_CREDENTIAL_TEST: heap had a minimum of %d"
                 " byte(s) free at the end of these tests.\n", y);
        U_PORT_TEST_ASSERT(y >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

// End of file
