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
 * @brief Test for the u-blox TLS security API: these should pass on all
 * platforms that support transport security.
 */

#ifndef U_CFG_TEST_TRANSPORT_SECURITY_DISABLE

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "errno.h"
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memcmp(), memset(), strlen()

#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"

#include "u_test_util_resource_check.h"

#include "u_network.h"
#include "u_network_test_shared_cfg.h"

#include "u_sock.h"
#include "u_sock_security.h"
#include "u_sock_test_shared_cfg.h"

#include "u_security_credential.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_SECURITY_TLS_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

/** The hash of the client certificate when it is stored on the
 * module.
 */
#define U_SECURITY_TLS_TEST_CLIENT_CERT_HASH "\x33\x5f\x89\x2f\x59\x84\x58\x80\x93\xcc\xf1\x36\xa3\x65\xe4\x57"

/** The hash of the client private key when it is stored on the
 * module.
 */
#define U_SECURITY_TLS_TEST_CLIENT_KEY_HASH "\x8f\xe6\xdd\xdb\x64\xb8\xf8\x2e\xa2\x52\xb2\xbb\x5e\x38\x08\xe8"

/** The hash of the CA certificate when it is stored on the
 * module.
 */
#define U_SECURITY_TLS_TEST_CA_CERT_HASH "\xa8\x83\xa0\x2d\xe0\xad\x34\x64\x26\xb3\xfb\x8a\x1b\x93\x3d\x84"

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Test data to send.
 */
static const char gData[] =  "_____0000:0123456789012345678901234567890123456789"
                             "01234567890123456789012345678901234567890123456789";

/** This is the client_cert.pem file from the
 * common/sock/test/echo_server/certs directory.
 */
static const char *const gpEchoServerClientCertPem = "-----BEGIN CERTIFICATE-----\r\n"
                                                     "MIICSjCCAdACFD+js1Fht6STx4lF3zGisrnThT4iMAoGCCqGSM49BAMDMIGFMQsw\r\n"
                                                     "CQYDVQQGEwJVUzELMAkGA1UECAwCV0ExEDAOBgNVBAcMB1RoYWx3aWwxDzANBgNV\r\n"
                                                     "BAoMBnUtYmxveDELMAkGA1UECwwCY2ExFzAVBgNVBAMMDnd3dy51LWJsb3guY29t\r\n"
                                                     "MSAwHgYJKoZIhvcNAQkBFhF1YnhsaWJAdS1ibG94LmNvbTAgFw0yMzA3MDkwODI3\r\n"
                                                     "NDBaGA8yMTIzMDYxNTA4Mjc0MFowgYkxCzAJBgNVBAYTAlVTMQswCQYDVQQIDAJX\r\n"
                                                     "QTEQMA4GA1UEBwwHVGhhbHdpbDEPMA0GA1UECgwGdS1ibG94MQ8wDQYDVQQLDAZj\r\n"
                                                     "bGllbnQxFzAVBgNVBAMMDnd3dy51LWJsb3guY29tMSAwHgYJKoZIhvcNAQkBFhF1\r\n"
                                                     "YnhsaWJAdS1ibG94LmNvbTB2MBAGByqGSM49AgEGBSuBBAAiA2IABApmNYLlR8Cr\r\n"
                                                     "S9MAocQX+bUU4+1EkmT61bchs6pf9RVvvbgbLkw2gk/So8vPifo6imJcjWteiIBy\r\n"
                                                     "xYKKFSIyghz/o0hjmpDz1XoYPtGENrz/dyISP35ZFk9sRJZ4pSX1uDAKBggqhkjO\r\n"
                                                     "PQQDAwNoADBlAjEA3scFsQb9Aj+lzC34h+AS6RGHLHr81Txm713MHnXjrpe0jEk8\r\n"
                                                     "bTULtydY8Jyf9c+DAjBMEdAEODaOp5Vn02ZOkKtbm91R6rFS1IZTFJ2MQCALG50C\r\n"
                                                     "GHviROz1O6YfRcRFTks=\r\n"
                                                     "-----END CERTIFICATE-----";

/** This is the client_key.pem file from the
 * common/sock/test/echo_server/certs directory.
 */
static const char *const gpEchoServerClientKeyPem = "-----BEGIN EC PRIVATE KEY-----\r\n"
                                                    "MIGkAgEBBDBxQnFRM8oo6gCjmfNNgTdfUQreohEDs1NFIOq84DO3120rKI4Ypf7h\r\n"
                                                    "xog10lSfhhOgBwYFK4EEACKhZANiAAQKZjWC5UfAq0vTAKHEF/m1FOPtRJJk+tW3\r\n"
                                                    "IbOqX/UVb724Gy5MNoJP0qPLz4n6OopiXI1rXoiAcsWCihUiMoIc/6NIY5qQ89V6\r\n"
                                                    "GD7RhDa8/3ciEj9+WRZPbESWeKUl9bg=\r\n"
                                                    "-----END EC PRIVATE KEY-----";

/** This is the ca_cert.pem file from the
 * common/sock/test/echo_server/certs directory.
 */
static const char *const gpEchoServerCaCertPem = "-----BEGIN CERTIFICATE-----\r\n"
                                                 "MIICoTCCAiagAwIBAgIUXW8iJeCsbA3ygmXIT3wqxqtZla4wCgYIKoZIzj0EAwIw\r\n"
                                                 "gYUxCzAJBgNVBAYTAlVTMQswCQYDVQQIDAJXQTEQMA4GA1UEBwwHVGhhbHdpbDEP\r\n"
                                                 "MA0GA1UECgwGdS1ibG94MQswCQYDVQQLDAJjYTEXMBUGA1UEAwwOd3d3LnUtYmxv\r\n"
                                                 "eC5jb20xIDAeBgkqhkiG9w0BCQEWEXVieGxpYkB1LWJsb3guY29tMCAXDTIzMDcw\r\n"
                                                 "OTA4MjY1NloYDzIxMjMwNjE1MDgyNjU2WjCBhTELMAkGA1UEBhMCVVMxCzAJBgNV\r\n"
                                                 "BAgMAldBMRAwDgYDVQQHDAdUaGFsd2lsMQ8wDQYDVQQKDAZ1LWJsb3gxCzAJBgNV\r\n"
                                                 "BAsMAmNhMRcwFQYDVQQDDA53d3cudS1ibG94LmNvbTEgMB4GCSqGSIb3DQEJARYR\r\n"
                                                 "dWJ4bGliQHUtYmxveC5jb20wdjAQBgcqhkjOPQIBBgUrgQQAIgNiAAS5br7n7+wi\r\n"
                                                 "Mwp5h3BojVn+cH4oZN7ngyfadR961TJZsu/g2arYE8SJTVI+qzQC4KiBb+rTXQIY\r\n"
                                                 "k9sxEo+mTyJ4BWaVxoWOXjvALNRtyrbls6q36ttXoYsU5UAgNWJiH/ejUzBRMB0G\r\n"
                                                 "A1UdDgQWBBRKetSAT3SQ45r2l64eXK1vf8sTzDAfBgNVHSMEGDAWgBRKetSAT3SQ\r\n"
                                                 "45r2l64eXK1vf8sTzDAPBgNVHRMBAf8EBTADAQH/MAoGCCqGSM49BAMCA2kAMGYC\r\n"
                                                 "MQD7WrRzaAxBikIHPuoDZo7tAdA5Zsbg9axBPS+wm3mdKLGwWjdep2IWLmn/uuFE\r\n"
                                                 "VlwCMQDXxDnOuuc6p1nzmtrn9JHVE0/+HdeDj6KdnDWWtZJQsagHDAEmld8oEDlg\r\n"
                                                 "iDO9Bnw=\r\n"
                                                 "-----END CERTIFICATE-----";

/** Hook to hold buffer for test data received.
 */
static char *gpDataReceived = NULL;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Send an entire data buffer until done
static size_t send(uSockDescriptor_t descriptor,
                   const char *pData, size_t sizeBytes)
{
    int32_t x;
    size_t sentSizeBytes = 0;
    int32_t startTimeMs;

    U_TEST_PRINT_LINE("sending %d byte(s) of data...", sizeBytes);
    startTimeMs = uPortGetTickTimeMs();
    while ((sentSizeBytes < sizeBytes) &&
           ((uPortGetTickTimeMs() - startTimeMs) < 10000)) {
        x = uSockWrite(descriptor, (const void *) pData,
                       sizeBytes - sentSizeBytes);
        if (x > 0) {
            sentSizeBytes += x;
            U_TEST_PRINT_LINE("sent %d byte(s) of data @%d ms.",
                              sentSizeBytes, (int32_t) uPortGetTickTimeMs());
        }
    }

    return sentSizeBytes;
}

// Definitely, definitely, close a socket.
static bool closeSock(uSockDescriptor_t descriptor)
{
    bool socketclosed = false;

    if (uSockClose(descriptor) == 0) {
        socketclosed = true;
    }
    uSockCleanUp();
    if (!socketclosed) {
        // If the socket failed to close, clean up
        // here to avoid memory leaks
        uSockDeinit();
        errno = 0;
    }

    return socketclosed;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TESTS
 * -------------------------------------------------------------- */

/** TCP socket over a TLS connection.
 */
U_PORT_TEST_FUNCTION("[securityTls]", "securityTlsSock")
{
    uNetworkTestList_t *pList;
    int32_t errorCode;
    uDeviceHandle_t devHandle = NULL;
    uSockDescriptor_t descriptor = -1;
    uSockAddress_t remoteAddress;
    int32_t startTimeMs;
    size_t sizeBytes;
    size_t offset;
    int32_t y;
    int32_t resourceCount;
    uSecurityTlsSettings_t settings = U_SECURITY_TLS_SETTINGS_DEFAULT;
    char hash[U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES];

    // In case a previous test failed
    uNetworkTestCleanUp();

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    resourceCount = uTestUtilGetDynamicResourceCount();

    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uDeviceInit() == 0);

    // Get a list of things that support secure sockets
    pList = pUNetworkTestListAlloc(uNetworkTestHasSecureSock);
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

    // It is possible for socket closure in an
    // underlying layer to have failed in a previous
    // test, leaving sockets hanging, so just in case,
    // clear them up here
    uSockDeinit();

    // Bring up each network configuration
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        devHandle = *pTmp->pDevHandle;

        U_TEST_PRINT_LINE("bringing up %s...",
                          gpUNetworkTestTypeName[pTmp->networkType]);
        U_PORT_TEST_ASSERT(uNetworkInterfaceUp(devHandle,
                                               pTmp->networkType,
                                               pTmp->pNetworkCfg) == 0);

        // Check if the client certificate is already
        // stored on the module
        if ((uSecurityCredentialGetHash(devHandle,
                                        U_SECURITY_CREDENTIAL_CLIENT_X509,
                                        "ubxlib_test_client_cert",
                                        hash) != 0) ||
            (memcmp(hash, U_SECURITY_TLS_TEST_CLIENT_CERT_HASH, sizeof(hash)) != 0)) {
            // No: load the client certificate into the module
            U_TEST_PRINT_LINE("storing client certificate for the secure echo server...");
            U_PORT_TEST_ASSERT(uSecurityCredentialStore(devHandle,
                                                        U_SECURITY_CREDENTIAL_CLIENT_X509,
                                                        "ubxlib_test_client_cert",
                                                        gpEchoServerClientCertPem,
                                                        strlen(gpEchoServerClientCertPem),
                                                        NULL, NULL) == 0);
        }
        settings.pClientCertificateName = "ubxlib_test_client_cert";

        // Check if the client key is already stored on the module
        if ((uSecurityCredentialGetHash(devHandle,
                                        U_SECURITY_CREDENTIAL_CLIENT_KEY_PRIVATE,
                                        "ubxlib_test_client_key",
                                        hash) != 0) ||
            (memcmp(hash, U_SECURITY_TLS_TEST_CLIENT_KEY_HASH, sizeof(hash)) != 0)) {
            // No: load the client key into the module
            U_TEST_PRINT_LINE("storing client private key for the secure echo server...");
            U_PORT_TEST_ASSERT(uSecurityCredentialStore(devHandle,
                                                        U_SECURITY_CREDENTIAL_CLIENT_KEY_PRIVATE,
                                                        "ubxlib_test_client_key",
                                                        gpEchoServerClientKeyPem,
                                                        strlen(gpEchoServerClientKeyPem),
                                                        NULL, NULL) == 0);
        }
        settings.pClientPrivateKeyName = "ubxlib_test_client_key";

        // Check if the CA certificate is already
        // stored on the module (SARA-R5, for instance, will
        // check against this by default)
        if ((uSecurityCredentialGetHash(devHandle,
                                        U_SECURITY_CREDENTIAL_ROOT_CA_X509,
                                        "ubxlib_test_ca_cert",
                                        hash) != 0) ||
            (memcmp(hash, U_SECURITY_TLS_TEST_CA_CERT_HASH, sizeof(hash)) != 0)) {
            // No: load the CA certificate into the module
            U_TEST_PRINT_LINE("storing CA certificate...");
            U_PORT_TEST_ASSERT(uSecurityCredentialStore(devHandle,
                                                        U_SECURITY_CREDENTIAL_ROOT_CA_X509,
                                                        "ubxlib_test_ca_cert",
                                                        gpEchoServerCaCertPem,
                                                        strlen(gpEchoServerCaCertPem),
                                                        NULL, NULL) == 0);
        }
        settings.pRootCaCertificateName = "ubxlib_test_ca_cert";

        U_TEST_PRINT_LINE("looking up secure TCP echo server \"%s\"...",
                          U_SOCK_TEST_ECHO_SECURE_TCP_SERVER_DOMAIN_NAME);

        // Look up the remoteAddress of the server we use for TCP echo
        // Look up the remoteAddress of the server we use for secure TCP echo
        U_PORT_TEST_ASSERT(uSockGetHostByName(devHandle,
                                              U_SOCK_TEST_ECHO_SECURE_TCP_SERVER_DOMAIN_NAME,
                                              &(remoteAddress.ipAddress)) == 0);

        // Add the port number we will use
        remoteAddress.port = U_SOCK_TEST_ECHO_SECURE_TCP_SERVER_PORT;

        // Connections can fail and, when the secure
        // ones fail the socket often gets closed as well
        // so include uSockCreate() in the loop.
        errorCode = -1;
        for (y = 3; (y > 0) && (errorCode < 0); y--) {
            // Create a TCP socket
            descriptor = uSockCreate(devHandle, U_SOCK_TYPE_STREAM,
                                     U_SOCK_PROTOCOL_TCP);
            // Secure the socket
            U_TEST_PRINT_LINE("securing socket...");
            U_PORT_TEST_ASSERT(uSockSecurity(descriptor, &settings) == 0);

            // Connect the socket
            U_TEST_PRINT_LINE("connect socket to \"%s:%d\"...",
                              U_SOCK_TEST_ECHO_SECURE_TCP_SERVER_DOMAIN_NAME,
                              U_SOCK_TEST_ECHO_SECURE_TCP_SERVER_PORT);
            errorCode = uSockConnect(descriptor, &remoteAddress);
            if (errorCode < 0) {
                U_TEST_PRINT_LINE("*** WARNING *** failed to connect secured socket.");
                U_PORT_TEST_ASSERT(errno != 0);
                closeSock(descriptor);
                uPortTaskBlock(5000);
            }
        }
        U_PORT_TEST_ASSERT(errorCode == 0);

        U_TEST_PRINT_LINE("sending/receiving data over a secure TCP socket...");

        // Throw everything we have up...
        U_PORT_TEST_ASSERT(send(descriptor, gData, sizeof(gData) - 1) == sizeof(gData) - 1);

        U_TEST_PRINT_LINE("%d byte(s) sent via TCP @%d ms, now receiving...",
                          sizeof(gData) - 1, (int32_t) uPortGetTickTimeMs());

        // ...and capture them all again afterwards
        uPortFree(gpDataReceived); // In case the previous test failed
        gpDataReceived = (char *) pUPortMalloc(sizeof(gData) - 1);
        U_PORT_TEST_ASSERT(gpDataReceived != NULL);
        //lint -e(668) Suppress possible use of NULL pointer
        // for gpDataReceived
        memset(gpDataReceived, 0, sizeof(gData) - 1);
        startTimeMs = uPortGetTickTimeMs();
        offset = 0;
        //lint -e{441} Suppress loop variable not found in
        // condition: we're using time instead
        for (y = 0; (offset < sizeof(gData) - 1) &&
             (uPortGetTickTimeMs() - startTimeMs < 20000); y++) {
            sizeBytes = uSockRead(descriptor,
                                  gpDataReceived + offset,
                                  (sizeof(gData) - 1) - offset);
            if (sizeBytes > 0) {
                U_TEST_PRINT_LINE("received %d byte(s) on secure TCP socket.", sizeBytes);
                offset += sizeBytes;
            }
        }
        sizeBytes = offset;
        if (sizeBytes < sizeof(gData) - 1) {
            U_TEST_PRINT_LINE("only %d byte(s) received after %d ms.", sizeBytes,
                              (int32_t) (uPortGetTickTimeMs() - startTimeMs));
        } else {
            U_TEST_PRINT_LINE("all %d byte(s) received back after %d ms, checking"
                              " if they were as expected...", sizeBytes,
                              (int32_t) (uPortGetTickTimeMs() - startTimeMs));
        }

        // Check that we reassembled everything correctly
        U_PORT_TEST_ASSERT(memcmp(gpDataReceived, gData, sizeof(gData) - 1) == 0);

        uPortFree(gpDataReceived);
        gpDataReceived = NULL;

        // Close the socket
        if (!closeSock(descriptor)) {
            // Secure sockets sometimes fail to close with
            // the SARA-R412M-03B we have on the test system.
            U_TEST_PRINT_LINE("*** WARNING *** socket failed to close.");
        }
    }

    // Remove each network type
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        U_TEST_PRINT_LINE("taking down %s...",
                          gpUNetworkTestTypeName[pTmp->networkType]);
        U_PORT_TEST_ASSERT(uNetworkInterfaceDown(*pTmp->pDevHandle,
                                                 pTmp->networkType) == 0);
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

    uSockDeinit();
    uSockCleanUp();
    uSecurityTlsCleanUp();

    uDeviceDeinit();
    uPortDeinit();

    // Check for resource leaks
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
    resourceCount = uTestUtilGetDynamicResourceCount() - resourceCount;
    U_TEST_PRINT_LINE("we have leaked %d resources(s).", resourceCount);
    U_PORT_TEST_ASSERT(resourceCount <= 0);
}

/** UDP socket over a DTLS connection.
 */
U_PORT_TEST_FUNCTION("[securityTls]", "securityTlsUdpSock")
{
    uNetworkTestList_t *pList;
    int32_t errorCode;
    uDeviceHandle_t devHandle = NULL;
    uSockDescriptor_t descriptor = -1;
    uSockAddress_t remoteAddress;
    int32_t startTimeMs;
    size_t sizeBytes;
    size_t offset;
    int32_t y;
    int32_t resourceCount;
    uSecurityTlsSettings_t settings = U_SECURITY_TLS_SETTINGS_DEFAULT;
    char hash[U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES];

    // In case a previous test failed
    uNetworkTestCleanUp();

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    resourceCount = uTestUtilGetDynamicResourceCount();

    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uDeviceInit() == 0);

    // Get a list of things that support secure sockets
    pList = pUNetworkTestListAlloc(uNetworkTestHasSecureSock);
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

    // It is possible for socket closure in an
    // underlying layer to have failed in a previous
    // test, leaving sockets hanging, so just in case,
    // clear them up here
    uSockDeinit();

    // Bring up each network configuration
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        devHandle = *pTmp->pDevHandle;

        U_TEST_PRINT_LINE("bringing up %s...",
                          gpUNetworkTestTypeName[pTmp->networkType]);
        U_PORT_TEST_ASSERT(uNetworkInterfaceUp(devHandle,
                                               pTmp->networkType,
                                               pTmp->pNetworkCfg) == 0);

        // Check if the client certificate is already
        // stored on the module
        if ((uSecurityCredentialGetHash(devHandle,
                                        U_SECURITY_CREDENTIAL_CLIENT_X509,
                                        "ubxlib_test_client_cert",
                                        hash) != 0) ||
            (memcmp(hash, U_SECURITY_TLS_TEST_CLIENT_CERT_HASH, sizeof(hash)) != 0)) {
            // No: load the client certificate into the module
            U_TEST_PRINT_LINE("storing client certificate for the secure echo server...");
            U_PORT_TEST_ASSERT(uSecurityCredentialStore(devHandle,
                                                        U_SECURITY_CREDENTIAL_CLIENT_X509,
                                                        "ubxlib_test_client_cert",
                                                        gpEchoServerClientCertPem,
                                                        strlen(gpEchoServerClientCertPem),
                                                        NULL, NULL) == 0);
        }
        settings.pClientCertificateName = "ubxlib_test_client_cert";

        // Check if the client key is already stored on the module
        if ((uSecurityCredentialGetHash(devHandle,
                                        U_SECURITY_CREDENTIAL_CLIENT_KEY_PRIVATE,
                                        "ubxlib_test_client_key",
                                        hash) != 0) ||
            (memcmp(hash, U_SECURITY_TLS_TEST_CLIENT_KEY_HASH, sizeof(hash)) != 0)) {
            // No: load the client key into the module
            U_TEST_PRINT_LINE("storing client private key for the secure echo server...");
            U_PORT_TEST_ASSERT(uSecurityCredentialStore(devHandle,
                                                        U_SECURITY_CREDENTIAL_CLIENT_KEY_PRIVATE,
                                                        "ubxlib_test_client_key",
                                                        gpEchoServerClientKeyPem,
                                                        strlen(gpEchoServerClientKeyPem),
                                                        NULL, NULL) == 0);
        }
        settings.pClientPrivateKeyName = "ubxlib_test_client_key";

        // Check if the CA certificate is already
        // stored on the module (SARA-R5, for instance, will
        // check against this by default)
        if ((uSecurityCredentialGetHash(devHandle,
                                        U_SECURITY_CREDENTIAL_ROOT_CA_X509,
                                        "ubxlib_test_ca_cert",
                                        hash) != 0) ||
            (memcmp(hash, U_SECURITY_TLS_TEST_CA_CERT_HASH, sizeof(hash)) != 0)) {
            // No: load the CA certificate into the module
            U_TEST_PRINT_LINE("storing CA certificate...");
            U_PORT_TEST_ASSERT(uSecurityCredentialStore(devHandle,
                                                        U_SECURITY_CREDENTIAL_ROOT_CA_X509,
                                                        "ubxlib_test_ca_cert",
                                                        gpEchoServerCaCertPem,
                                                        strlen(gpEchoServerCaCertPem),
                                                        NULL, NULL) == 0);
        }
        settings.pRootCaCertificateName = "ubxlib_test_ca_cert";

        U_TEST_PRINT_LINE("looking up secure UDP echo server \"%s\"...",
                          U_SOCK_TEST_ECHO_SECURE_UDP_SERVER_DOMAIN_NAME);

        // Look up the remoteAddress of the server we use for DTLS echo
        // Look up the remoteAddress of the server we use for secure UDP echo
        U_PORT_TEST_ASSERT(uSockGetHostByName(devHandle,
                                              U_SOCK_TEST_ECHO_SECURE_UDP_SERVER_DOMAIN_NAME,
                                              &(remoteAddress.ipAddress)) == 0);

        // Add the port number we will use
        remoteAddress.port = U_SOCK_TEST_ECHO_SECURE_UDP_SERVER_PORT;

        // Connections can fail and, when the secure
        // ones fail the socket often gets closed as well
        // so include uSockCreate() in the loop.
        errorCode = -1;
        for (y = 3; (y > 0) && (errorCode < 0); y--) {
            // Create a UDP socket
            descriptor = uSockCreate(devHandle, U_SOCK_TYPE_DGRAM,
                                     U_SOCK_PROTOCOL_UDP);
            // Secure the socket
            U_TEST_PRINT_LINE("securing socket...");
            U_PORT_TEST_ASSERT(uSockSecurity(descriptor, &settings) == 0);

            // Connect the socket
            U_TEST_PRINT_LINE("connect socket to \"%s:%d\"...",
                              U_SOCK_TEST_ECHO_SECURE_UDP_SERVER_DOMAIN_NAME,
                              U_SOCK_TEST_ECHO_SECURE_UDP_SERVER_PORT);
            errorCode = uSockConnect(descriptor, &remoteAddress);
            if (errorCode < 0) {
                U_TEST_PRINT_LINE("*** WARNING *** failed to connect secured socket.");
                U_PORT_TEST_ASSERT(errno != 0);
                closeSock(descriptor);
                uPortTaskBlock(5000);
            }
        }
        U_PORT_TEST_ASSERT(errorCode == 0);

        U_TEST_PRINT_LINE("sending/receiving data over a secure UDP socket...");

        // Throw everything we have up...
        U_PORT_TEST_ASSERT(send(descriptor, gData, sizeof(gData) - 1) == sizeof(gData) - 1);

        U_TEST_PRINT_LINE("%d byte(s) sent via UDP @%d ms, now receiving...",
                          sizeof(gData) - 1, (int32_t) uPortGetTickTimeMs());

        // ...and capture them all again afterwards
        uPortFree(gpDataReceived); // In case the previous test failed
        gpDataReceived = (char *) pUPortMalloc(sizeof(gData) - 1);
        U_PORT_TEST_ASSERT(gpDataReceived != NULL);
        //lint -e(668) Suppress possible use of NULL pointer
        // for gpDataReceived
        memset(gpDataReceived, 0, sizeof(gData) - 1);
        startTimeMs = uPortGetTickTimeMs();
        offset = 0;
        //lint -e{441} Suppress loop variable not found in
        // condition: we're using time instead
        for (y = 0; (offset < sizeof(gData) - 1) &&
             (uPortGetTickTimeMs() - startTimeMs < 20000); y++) {
            sizeBytes = uSockRead(descriptor,
                                  gpDataReceived + offset,
                                  (sizeof(gData) - 1) - offset);
            if (sizeBytes > 0) {
                U_TEST_PRINT_LINE("received %d byte(s) on secure UDP socket.", sizeBytes);
                offset += sizeBytes;
            }
        }
        sizeBytes = offset;
        if (sizeBytes < sizeof(gData) - 1) {
            U_TEST_PRINT_LINE("only %d byte(s) received after %d ms.", sizeBytes,
                              (int32_t) (uPortGetTickTimeMs() - startTimeMs));
        } else {
            U_TEST_PRINT_LINE("all %d byte(s) received back after %d ms, checking"
                              " if they were as expected...", sizeBytes,
                              (int32_t) (uPortGetTickTimeMs() - startTimeMs));
        }

        // Check that we reassembled everything correctly
        U_PORT_TEST_ASSERT(memcmp(gpDataReceived, gData, sizeof(gData) - 1) == 0);

        uPortFree(gpDataReceived);
        gpDataReceived = NULL;

        // Close the socket
        if (!closeSock(descriptor)) {
            // Secure sockets sometimes fail to close with
            // the SARA-R412M-03B we have on the test system.
            U_TEST_PRINT_LINE("*** WARNING *** socket failed to close.");
        }
    }

    // Remove each network type
    for (uNetworkTestList_t *pTmp = pList; pTmp != NULL; pTmp = pTmp->pNext) {
        U_TEST_PRINT_LINE("taking down %s...",
                          gpUNetworkTestTypeName[pTmp->networkType]);
        U_PORT_TEST_ASSERT(uNetworkInterfaceDown(*pTmp->pDevHandle,
                                                 pTmp->networkType) == 0);
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

    uSockDeinit();
    uSockCleanUp();
    uSecurityTlsCleanUp();

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
U_PORT_TEST_FUNCTION("[securityTls]", "securityTlsCleanUp")
{
    U_TEST_PRINT_LINE("cleaning up any outstanding resources.\n");

    uSockCleanUp();
    uSockDeinit();

    // Clean-up the TLS security mutex
    uSecurityTlsCleanUp();

    uPortFree(gpDataReceived);

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

#endif // #ifndef U_CFG_TEST_TRANSPORT_SECURITY_DISABLE

// End of file
