/*
 * Copyright 2019-2024 u-blox
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
 * @brief Test for the u-blox security API: these should pass on all
 * platforms that include the appropriate communications hardware,
 * i.e. currently cellular SARA-R5.
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the U_PORT_TEST_FUNCTION()
 * macro.
 */

#ifndef U_CFG_TEST_SECURITY_DISABLE

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // rand()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset(), strcmp()

#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"
#include "u_cfg_os_platform_specific.h"

#include "u_error_common.h" // For U_ERROR_COMMON_NOT_SUPPORTED

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"

#include "u_test_util_resource_check.h"

#ifdef U_CFG_TEST_CELL_MODULE_TYPE
#include "u_cell_module_type.h"
#include "u_cell_test_cfg.h" // For the cellular test macros
#endif

#include "u_network.h"
#include "u_network_test_shared_cfg.h"

#include "u_security.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_SECURITY_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#ifdef U_CFG_SECURITY_DEVICE_PROFILE_UID
/** Timeout for the security sealing operation.
 */
# define U_SECURITY_TEST_SEAL_TIMEOUT_SECONDS (60 * 4)
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

#ifdef U_CFG_SECURITY_DEVICE_PROFILE_UID
/** Used for keepGoingCallback() timeout.
 */
static int64_t gStopTimeMs;
#endif

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

#ifdef U_CFG_SECURITY_DEVICE_PROFILE_UID
// Callback function for the security sealing processes.
static bool keepGoingCallback()
{
    bool keepGoing = true;

    if (uPortGetTickTimeMs() > gStopTimeMs) {
        keepGoing = false;
    }

    return keepGoing;
}
#endif

// Standard preamble for all security tests
static uNetworkTestList_t *pStdPreamble()
{
    uNetworkTestList_t *pList;

    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uDeviceInit() == 0);

    // Add the devices for each network configuration
    // if not already added
    pList = pUNetworkTestListAlloc(uNetworkTestHasSecurity);
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

    // Bring up each network type
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        U_TEST_PRINT_LINE("bringing up %s...", gpUNetworkTestTypeName[pTmp->networkType]);
        U_PORT_TEST_ASSERT(uNetworkInterfaceUp(*pTmp->pDevHandle,
                                               pTmp->networkType,
                                               pTmp->pNetworkCfg) == 0);
    }

    return pList;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TESTS
 * -------------------------------------------------------------- */

/** Test security sealing, requires a network connection.
 * Note: this test will *only* attempt a seal if
 * U_CFG_SECURITY_DEVICE_PROFILE_UID is defined to contain
 * a valid device profile UID string (without quotes).
 */
U_PORT_TEST_FUNCTION("[security]", "securitySeal")
{
    uNetworkTestList_t *pList;
    uDeviceHandle_t devHandle;
    int32_t resourceCount;
    int32_t z;
    char serialNumber[U_SECURITY_SERIAL_NUMBER_MAX_LENGTH_BYTES];
    char rotUid[U_SECURITY_ROOT_OF_TRUST_UID_LENGTH_BYTES];

    // In case a previous test failed
    uNetworkTestCleanUp();

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    resourceCount = uTestUtilGetDynamicResourceCount();

    // Do the standard preamble to make sure there is
    // a network underneath us
    pList = pStdPreamble();
    // Repeat for all bearers
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        devHandle = *pTmp->pDevHandle;

        U_TEST_PRINT_LINE("checking if u-blox security is supported by handle"
                          " 0x%08x...", devHandle);
        if (uSecurityIsSupported(devHandle)) {
            U_TEST_PRINT_LINE("security is supported.");

            // Get the serial number
            z = uSecurityGetSerialNumber(devHandle, serialNumber);
            U_PORT_TEST_ASSERT((z > 0) && (z < (int32_t) sizeof(serialNumber)));
            U_TEST_PRINT_LINE("module serial number is \"%s\".", serialNumber);

            // Get the root of trust UID with NULL rotUid
            U_PORT_TEST_ASSERT(uSecurityGetRootOfTrustUid(devHandle, NULL) >= 0);
            // Get the root of trust UID properly
            U_PORT_TEST_ASSERT(uSecurityGetRootOfTrustUid(devHandle,
                                                          rotUid) == sizeof(rotUid));
            uPortLog( U_TEST_PREFIX "root of trust UID is 0x");
            for (size_t y = 0; y < sizeof(rotUid); y++) {
                uPortLog("%02x", rotUid[y]);
            }
            uPortLog(".\n");

            U_TEST_PRINT_LINE("waiting for bootstrap status...");
            // Try 10 times with a wait in-between to get bootstrapped
            // status
            for (size_t y = 10; (y > 0) &&
                 !uSecurityIsBootstrapped(devHandle); y--) {
                uPortTaskBlock(5000);
            }
            if (uSecurityIsBootstrapped(devHandle)) {
                U_TEST_PRINT_LINE("device is bootstrapped.");
                if (!uSecurityIsSealed(devHandle)) {
#ifdef U_CFG_SECURITY_DEVICE_PROFILE_UID
                    U_TEST_PRINT_LINE("device is bootstrapped, performing security seal"
                                      " with device profile UID string \"%s\" and serial"
                                      " number \"%s\"...",
                                      U_PORT_STRINGIFY_QUOTED(U_CFG_SECURITY_DEVICE_PROFILE_UID),
                                      serialNumber);
                    gStopTimeMs = uPortGetTickTimeMs() +
                                  (U_SECURITY_TEST_SEAL_TIMEOUT_SECONDS * 1000);
                    if (uSecuritySealSet(devHandle,
                                         U_PORT_STRINGIFY_QUOTED(U_CFG_SECURITY_DEVICE_PROFILE_UID),
                                         serialNumber, keepGoingCallback) == 0) {
                        U_TEST_PRINT_LINE("device is security sealed with device profile"
                                          " UID string \"%s\" and serial number \"%s\".",
                                          U_PORT_STRINGIFY_QUOTED(U_CFG_SECURITY_DEVICE_PROFILE_UID),
                                          serialNumber);
                        U_PORT_TEST_ASSERT(uSecurityIsSealed(devHandle));
                    } else {
                        U_TEST_PRINT_LINE("unable to security seal device.");
                        U_PORT_TEST_ASSERT(!uSecurityIsSealed(devHandle));
                        //lint -e(774) Suppress always evaluates to false
                        U_PORT_TEST_ASSERT(false);
                    }
#else
                    U_TEST_PRINT_LINE("device is bootstrapped but U_CFG_SECURITY_DEVICE_PROFILE_UID"
                                      " is not defined so no test of security sealing"
                                      " will be performed.");
#endif
                } else {
                    U_TEST_PRINT_LINE("this device supports u-blox security and is already"
                                      " security sealed, no test of security sealing will"
                                      " be carried out.");
                }
            } else {
                U_TEST_PRINT_LINE("this device supports u-blox security but will not bootstrap.");
                U_PORT_TEST_ASSERT(!uSecurityIsSealed(devHandle));
                //lint -e(506, 774) Suppress constant Boolean always evaluates to false
                U_PORT_TEST_ASSERT(false);
            }
        }
    }

    // Close the devices once more and free the list
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        if (*pTmp->pDevHandle != NULL) {
            U_TEST_PRINT_LINE("taking down %s...",
                              gpUNetworkTestTypeName[pTmp->networkType]);
            U_PORT_TEST_ASSERT(uNetworkInterfaceDown(*pTmp->pDevHandle,
                                                     pTmp->networkType) == 0);
            U_TEST_PRINT_LINE("closing device %s...",
                              gpUNetworkTestDeviceTypeName[pTmp->pDeviceCfg->deviceType]);
            U_PORT_TEST_ASSERT(uDeviceClose(*pTmp->pDevHandle, false) == 0);
            *pTmp->pDevHandle = NULL;
        }
    }
    uNetworkTestListFree();

    uDeviceDeinit();
    uPortDeinit();

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}

/** Test PSK generation.
 */
U_PORT_TEST_FUNCTION("[security]", "securityPskGeneration")
{
    uNetworkTestList_t *pList;
    uDeviceHandle_t devHandle;
    //lint -esym(838, z) Suppress not used, which will be true
    // if logging is compiled out
    int32_t z;
    int32_t pskIdSize;
    int32_t resourceCount;
    char psk[U_SECURITY_PSK_MAX_LENGTH_BYTES];
    char pskId[U_SECURITY_PSK_ID_MAX_LENGTH_BYTES];

    // In case a previous test failed
    uNetworkTestCleanUp();

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    resourceCount = uTestUtilGetDynamicResourceCount();

    // Do the standard preamble to make sure there is
    // a network underneath us
    pList = pStdPreamble();
    // Repeat for all bearers
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        devHandle = *pTmp->pDevHandle;

        U_TEST_PRINT_LINE("checking if u-blox security is supported by handle"
                          " 0x%08x...", devHandle);
        if (uSecurityIsSupported(devHandle)) {
            U_TEST_PRINT_LINE("security is supported.");
            U_TEST_PRINT_LINE("waiting for seal status...");
            if (uSecurityIsSealed(devHandle)) {
                U_TEST_PRINT_LINE("device is sealed.");

                // Ask for a security heartbeat to be triggered:
                // this very likely won't be permitted since
                // it is quite severely rate limited (e.g. just once
                // in 24 hours) so we're really only checking that it
                // doesn't crash here
                // TODO: temporarily remove the security heartbeat
                // call here.  One of the test instances is misbehaving
                // in this function (taking too long to return), will
                // disable while the problem is investiated.
                //z = uSecurityHeartbeatTrigger(devHandle);
                //U_TEST_PRINT_LINE("uSecurityHeartbeatTrigger() returned %d.", z);
                U_TEST_PRINT_LINE("testing PSK generation...");
                memset(psk, 0, sizeof(psk));
                memset(pskId, 0, sizeof(pskId));
                pskIdSize = uSecurityPskGenerate(devHandle, 16,
                                                 psk, pskId);
                U_PORT_TEST_ASSERT(pskIdSize > 0);
                U_PORT_TEST_ASSERT(pskIdSize < (int32_t) sizeof(pskId));
                // Check that the PSK ID isn't still all zeroes
                // expect beyond pskIdSize
                z = 0;
                for (size_t y = 0; y < sizeof(pskId); y++) {
                    if ((int32_t) y < pskIdSize) {
                        if (pskId[y] == 0) {
                            z++;
                        }
                    } else {
                        U_PORT_TEST_ASSERT(pskId[y] == 0);
                    }
                }
                U_PORT_TEST_ASSERT(z < pskIdSize);
                // Check that the first 16 bytes of the PSK aren't still
                // all zero but that the remainder are
                z = 0;
                for (size_t y = 0; y < sizeof(psk); y++) {
                    if (y < 16) {
                        if (psk[y] == 0) {
                            z++;
                        }
                    } else {
                        U_PORT_TEST_ASSERT(psk[y] == 0);
                    }
                }
                U_PORT_TEST_ASSERT(z < 16);
                memset(psk, 0, sizeof(psk));
                memset(pskId, 0, sizeof(pskId));
                pskIdSize = uSecurityPskGenerate(devHandle, 32,
                                                 psk, pskId);
                U_PORT_TEST_ASSERT(pskIdSize > 0);
                U_PORT_TEST_ASSERT(pskIdSize < (int32_t) sizeof(pskId));
                // Check that the PSK ID isn't still all zeroes
                // expect beyond pskIdSize
                z = 0;
                for (size_t y = 0; y < sizeof(pskId); y++) {
                    if ((int32_t) y < pskIdSize) {
                        if (pskId[y] == 0) {
                            z++;
                        }
                    } else {
                        U_PORT_TEST_ASSERT(pskId[y] == 0);
                    }
                }
                U_PORT_TEST_ASSERT(z < pskIdSize);
                // Check that the PSK isn't still all zeroes
                z = 0;
                for (size_t y = 0; y < sizeof(psk); y++) {
                    if (psk[y] == 0) {
                        z++;
                    }
                }
                U_PORT_TEST_ASSERT(z < (int32_t) sizeof(psk));
            } else {
                U_TEST_PRINT_LINE("this device supports u-blox security but"
                                  " has not been security sealed, no testing"
                                  " of PSK generation will be carried out.");
            }
        }
    }

    // Close the devices once more and free the list
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        if (*pTmp->pDevHandle != NULL) {
            U_TEST_PRINT_LINE("taking down %s...",
                              gpUNetworkTestTypeName[pTmp->networkType]);
            U_PORT_TEST_ASSERT(uNetworkInterfaceDown(*pTmp->pDevHandle,
                                                     pTmp->networkType) == 0);
            U_TEST_PRINT_LINE("closing device %s...",
                              gpUNetworkTestDeviceTypeName[pTmp->pDeviceCfg->deviceType]);
            U_PORT_TEST_ASSERT(uDeviceClose(*pTmp->pDevHandle, false) == 0);
            *pTmp->pDevHandle = NULL;
        }
    }
    uNetworkTestListFree();

    uDeviceDeinit();
    uPortDeinit();

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}

/** Test reading the certificate/key/authorities from sealing.
 */
U_PORT_TEST_FUNCTION("[security]", "securityZtp")
{
    uNetworkTestList_t *pList;
    uDeviceHandle_t devHandle;
    int32_t y;
    int32_t z;
    int32_t resourceCount;
    char *pData;

    // In case a previous test failed
    uNetworkTestCleanUp();

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    resourceCount = uTestUtilGetDynamicResourceCount();

    // Do the standard preamble to make sure there is
    // a network underneath us
    pList = pStdPreamble();
    // Repeat for all bearers
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        devHandle = *pTmp->pDevHandle;

        U_TEST_PRINT_LINE("checking if u-blox security is supported by handle"
                          " 0x%08x...", devHandle);
        if (uSecurityIsSupported(devHandle)) {
            U_TEST_PRINT_LINE("security is supported.");
            U_TEST_PRINT_LINE("waiting for seal status...");
            if (uSecurityIsSealed(devHandle)) {
                U_TEST_PRINT_LINE("device is sealed.");

                // First get the size of the device public certificate
                y = uSecurityZtpGetDeviceCertificate(devHandle, NULL, 0);
                U_TEST_PRINT_LINE("device public X.509 certificate is %d bytes.", y);
                U_PORT_TEST_ASSERT((y > 0) || (y == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED));
                if (y > 0) {
                    // Allocate memory to receive into and zero it for good measure
                    pData = (char *) pUPortMalloc(y);
                    U_PORT_TEST_ASSERT(pData != NULL);
                    //lint -e(668) Suppress possible use of NULL pointer for pData
                    memset(pData, 0, y);
                    U_TEST_PRINT_LINE("getting device public X.509 certificate...", y);
                    z = uSecurityZtpGetDeviceCertificate(devHandle, pData, y);
                    U_PORT_TEST_ASSERT(z == y);
                    // Can't really check the data but can check that it is
                    // of the correct length
                    U_PORT_TEST_ASSERT((int32_t)strlen(pData) == z - 1);
                    uPortFree(pData);
                } else {
                    U_TEST_PRINT_LINE("module does not support reading device public certificate.");
                }

                // Get the size of the device private certificate
                y = uSecurityZtpGetPrivateKey(devHandle, NULL, 0);
                U_TEST_PRINT_LINE("private key is %d bytes.", y);
                U_PORT_TEST_ASSERT((y > 0) || (y == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED));
                if (y > 0) {
                    // Allocate memory to receive into and zero it for good measure
                    pData = (char *) pUPortMalloc(y);
                    U_PORT_TEST_ASSERT(pData != NULL);
                    //lint -e(668) Suppress possible use of NULL pointer for pData
                    memset(pData, 0, y);
                    U_TEST_PRINT_LINE("getting private key...", y);
                    z = uSecurityZtpGetPrivateKey(devHandle, pData, y);
                    U_PORT_TEST_ASSERT(z == y);
                    // Can't really check the data but can check that it is
                    // of the correct length
                    U_PORT_TEST_ASSERT((int32_t)strlen(pData) == z - 1);
                    uPortFree(pData);
                } else {
                    U_TEST_PRINT_LINE("module does not support reading device private key.");
                }

                // Get the size of the certificate authorities
                y = uSecurityZtpGetCertificateAuthorities(devHandle, NULL, 0);
                U_TEST_PRINT_LINE("X.509 certificate authorities are %d bytes.", y);
                U_PORT_TEST_ASSERT((y > 0) || (y == (int32_t) U_ERROR_COMMON_NOT_SUPPORTED));
                if (y > 0) {
                    // Allocate memory to receive into and zero it for good measure
                    pData = (char *) pUPortMalloc(y);
                    U_PORT_TEST_ASSERT(pData != NULL);
                    //lint -e(668) Suppress possible use of NULL pointer for pData
                    memset(pData, 0, y);
                    U_TEST_PRINT_LINE("getting X.509 certificate authorities...", y);
                    z = uSecurityZtpGetCertificateAuthorities(devHandle, pData, y);
                    U_PORT_TEST_ASSERT(z == y);
                    // Can't really check the data but can check that it is
                    // of the correct length
                    U_PORT_TEST_ASSERT((int32_t)strlen(pData) == z - 1);
                    uPortFree(pData);
                } else {
                    U_TEST_PRINT_LINE("module does not support reading certificate authorities.");
                }
            } else {
                U_TEST_PRINT_LINE("this device supports u-blox security but has"
                                  " not been security sealed, no testing of reading"
                                  " ZTP items can be carried out.");
            }
        }
    }

    // Close the devices once more and free the list
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        if (*pTmp->pDevHandle != NULL) {
            U_TEST_PRINT_LINE("taking down %s...",
                              gpUNetworkTestTypeName[pTmp->networkType]);
            U_PORT_TEST_ASSERT(uNetworkInterfaceDown(*pTmp->pDevHandle,
                                                     pTmp->networkType) == 0);
            U_TEST_PRINT_LINE("closing device %s...",
                              gpUNetworkTestDeviceTypeName[pTmp->pDeviceCfg->deviceType]);
            U_PORT_TEST_ASSERT(uDeviceClose(*pTmp->pDevHandle, false) == 0);
            *pTmp->pDevHandle = NULL;
        }
    }
    uNetworkTestListFree();

    uDeviceDeinit();
    uPortDeinit();

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[security]", "securityCleanUp")
{
    // The network test configuration is shared between
    // the network, sockets, security and location tests
    // so must reset the handles here in case the
    // tests of one of the other APIs are coming next.
    uNetworkTestCleanUp();
    uDeviceDeinit();
    uPortDeinit();
    // Printed for information: asserting happens in the postamble
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
}

#endif // #ifndef U_CFG_TEST_SECURITY_DISABLE

// End of file
