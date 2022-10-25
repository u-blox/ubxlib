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
 * @brief Test for the u-blox TLS security API: these should pass on all
 * platforms.
 */

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
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_os.h"

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
#define U_SECURITY_TLS_TEST_CLIENT_CERT_HASH "\x76\x1a\x0c\xa6\x95\xd0\x88\x58\x30\xe9\x21\x02\xce\x2b\x4b\x08"

/** The hash of the client private key when it is stored on the
 * module.
 */
#define U_SECURITY_TLS_TEST_CLIENT_KEY_HASH "\x89\xed\xba\xc2\x3d\x6a\xd9\xa9\x7d\xa4\x08\x4a\x1d\x28\x01\x05"

/** The hash of the server certificate when it is stored on the
 * module.
 */
#define U_SECURITY_TLS_TEST_SERVER_CERT_HASH "\xf9\x5c\xbf\xe7\x5c\x1a\xa7\xd5\x82\x22\x31\xc8\x17\xff\xf3\x95"

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
static const char *const gpEchoServerClientCertPem = "-----BEGIN CERTIFICATE-----\n"
                                                     "MIIDiDCCAnACCQC8IOP+9fCfSTANBgkqhkiG9w0BAQsFADCBhTELMAkGA1UEBhMC"
                                                     "VVMxCzAJBgNVBAgMAldBMRAwDgYDVQQHDAdUaGFsd2lsMQ8wDQYDVQQKDAZ1LWJs"
                                                     "b3gxCzAJBgNVBAsMAklUMRcwFQYDVQQDDA53d3cudS1ibG94LmNvbTEgMB4GCSqG"
                                                     "SIb3DQEJARYRdWJ4bGliQHUtYmxveC5jb20wHhcNMjAxMDI2MjMzNDI3WhcNMjEx"
                                                     "MDI2MjMzNDI3WjCBhTELMAkGA1UEBhMCVVMxCzAJBgNVBAgMAldBMRAwDgYDVQQH"
                                                     "DAdUaGFsd2lsMQ8wDQYDVQQKDAZ1LWJsb3gxCzAJBgNVBAsMAklUMRcwFQYDVQQD"
                                                     "DA53d3cudS1ibG94LmNvbTEgMB4GCSqGSIb3DQEJARYRdWJ4bGliQHUtYmxveC5j"
                                                     "b20wggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQD5W8MnZEA9Dzl9/dGE"
                                                     "ObXC13ZnKZVdjiR4/pyDEGzzMzS8JdJKMNE7GhczNoLY8pZYzWdfLidwgJm59hZ2"
                                                     "oj4ddqqeXsa5wMY2zZXICygNpRdI8HJ/59tyoIYy4FR8yxUHDOfM2B+fqbw00+ER"
                                                     "UBGEvbkF4F3xxheQ0jE77QPsEq0xj+rXeicGkZNISTocOWzwOgj9hD3NvIpNGG34"
                                                     "quX5rT3V/ELsXHh15cTkH0isAa4uM89qKhIQQfKKQBh12hzK0rI5PbJaM3KrxjN3"
                                                     "UYq+kRslNMNJHYtYDIowTAGgB8QfqydYtWxjk5Cfi9igfEirgxqQUM3IzC3XsDFM"
                                                     "11MJAgMBAAEwDQYJKoZIhvcNAQELBQADggEBABxmy0IVEbppUaCxSbW0YrREbqWk"
                                                     "k2VT8erUyPhAlbmyLbD+VrwYNSedWQlkVbIeU+N7OJ/RtJT3Nno84clczTe1pB7p"
                                                     "t7vXGM1EG1t/EBrEreyMXJKmLItnuO1btxhQXcU619x1SY65NeqX4Gv7X2r14Ij5"
                                                     "1IwueTuzXT+iWD89eIxrNWPFI+6Xwxcm05smdukuX1Hiq2VVoqDbJKRN4FfPowFy"
                                                     "2MLsDlw0bYZGNyaIBweb2NJH2zU/qPHLVKrMf3LAx35sv+nq4vDnZS/Nn/vI2MD7"
                                                     "mVbRICHB7Zg8UoNnPToqy1o8xqqUB4h3KKIgHtw4tLtPZlaM3AiDSdkZQ6g=\n"
                                                     "-----END CERTIFICATE-----\n";

/** This is the client_key.pem file from the
 * common/sock/test/echo_server/certs directory.
 */
static const char *const gpEchoServerClientKeyPem = "-----BEGIN RSA PRIVATE KEY-----\n"
                                                    "MIIEpQIBAAKCAQEA+VvDJ2RAPQ85ff3RhDm1wtd2ZymVXY4keP6cgxBs8zM0vCXS"
                                                    "SjDROxoXMzaC2PKWWM1nXy4ncICZufYWdqI+HXaqnl7GucDGNs2VyAsoDaUXSPBy"
                                                    "f+fbcqCGMuBUfMsVBwznzNgfn6m8NNPhEVARhL25BeBd8cYXkNIxO+0D7BKtMY/q"
                                                    "13onBpGTSEk6HDls8DoI/YQ9zbyKTRht+Krl+a091fxC7Fx4deXE5B9IrAGuLjPP"
                                                    "aioSEEHyikAYddocytKyOT2yWjNyq8Yzd1GKvpEbJTTDSR2LWAyKMEwBoAfEH6sn"
                                                    "WLVsY5OQn4vYoHxIq4MakFDNyMwt17AxTNdTCQIDAQABAoIBADluLvZFmp31gbJI"
                                                    "4RZpDDnB0h1UcHhJopDTY0y0XcNtibnDpDk+IRJRogJDjcNVq9bsB+DeCmtY0w8H"
                                                    "ZIkSOOgkSouLHI3vnjdFBjg6iZEK8t/zsQtQZTRzUDUrgYn0Y/VpvYFqTW5Cc3xf"
                                                    "SDjqjf5ai+CUmk5y5z6NipVYs0yNVD9W+8fQUfs3wSrQzmmXSy87P1qaOePp5JxA"
                                                    "D6dDlWSXPRgHfj+mgk/l7BoCRvwWrRGgf62PDD8j21dESyr3KOqSSshns64Hbdd4"
                                                    "2rl4+i5Ylxy6aDtaT5hWb+1G6aXjogiNu/8s5hJFEBwBEY8OgdPDrSD495+yTDN1"
                                                    "fxwqp+ECgYEA/uQKnX5WWbZGzWmuykBq1M+YlRSxjS6ICv2vezHSa98JKHzX3u4K"
                                                    "Iv6UzX6YGIHZA6d6YlP5hHD8IcpezsUE5mykzhTx2Sqc81WMsN4wc2CkbIg3vVr5"
                                                    "kJhx3NGzp/vDYwJkI3TiiRUmRAru2fdnvyxLd7vo0uldxuvkHvAoQIUCgYEA+nGO"
                                                    "vz2YMAfo3v7Ctse7HE7KHSSH2jTVToea8v3gxsMUq/gEXVwIJlfeJQVsk9FTfQiL"
                                                    "4d/40x9jwmHKP4+ebq5QQ4yPfCVTM2goLF20hExFzRXsjij73ntA2b4iuy8modT/"
                                                    "BqfwPf2bw5/SMJ+mLYh8cjXTMYIDynLf8Ix7cbUCgYEAiWFp40cjzYi0EqTig7pC"
                                                    "ml8l0zxrEjhBNQNUoKbSzjdRXVQkmdBdAE2M8FFKMvNRf2m2SecO9nZbPu8vOGzy"
                                                    "XiuyjCy3yZ/xJio3AWFQZe9xz9l/iXzOREQWIrmYBnNo9SVlycKHEvGmRUhLQonZ"
                                                    "ji2Wo3tRWtRTKhMcShyQ5W0CgYEAt9oLb+sYwRHda27cpG/ltXdFurUpog+tE9RK"
                                                    "9N1ZWLC3iTMuiRbZyMQyiTz9I1rFDoHqpqvUL7DYfEdrwNN+/EOtGpmicAG6nX92"
                                                    "FnPH5GNVzqOsoAQIOqCC0BZbysxncOA7Q7ifjfKSmb7G//kDdmO+790BqFOI0uMX"
                                                    "8LBAow0CgYEA3qNhRhNxrg25kM+wqlkjJ+fo3jQ68r5VB8u/KLQ/Di6WnzNwOQlx"
                                                    "QuSxkmMtDPNzhxMhm+IMMwzT1Z8ZyTcWhacMptXXcKrO0gboBIknRlVzSykUqpf/"
                                                    "YH1TkviZaurGrrpZHWXN4/z91wqISl/B6SPoom/4ribwGB7+c3e398M=\n"
                                                    "-----END RSA PRIVATE KEY-----\n";

/** This is the server_cert.pem file from the
 * common/sock/test/echo_server/certs directory.
 */
static const char *const gpEchoServerServerCertPem = "-----BEGIN CERTIFICATE-----\n"
                                                     "MIID3zCCAsegAwIBAgIJAINm5Mhtj3MbMA0GCSqGSIb3DQEBCwUAMIGFMQswCQYD"
                                                     "VQQGEwJVUzELMAkGA1UECAwCV0ExEDAOBgNVBAcMB1RoYWx3aWwxDzANBgNVBAoM"
                                                     "BnUtYmxveDELMAkGA1UECwwCSVQxFzAVBgNVBAMMDnd3dy51LWJsb3guY29tMSAw"
                                                     "HgYJKoZIhvcNAQkBFhF1YnhsaWJAdS1ibG94LmNvbTAeFw0yMDEwMjYyMzMyNTNa"
                                                     "Fw0yMTEwMjYyMzMyNTNaMIGFMQswCQYDVQQGEwJVUzELMAkGA1UECAwCV0ExEDAO"
                                                     "BgNVBAcMB1RoYWx3aWwxDzANBgNVBAoMBnUtYmxveDELMAkGA1UECwwCSVQxFzAV"
                                                     "BgNVBAMMDnd3dy51LWJsb3guY29tMSAwHgYJKoZIhvcNAQkBFhF1YnhsaWJAdS1i"
                                                     "bG94LmNvbTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALVsmrHdFsS3"
                                                     "Zzc25sjLLnEsHANvDRQXn0J1zbDP/o5yJD77AawkAlfmapZX5wlSkr6owOfQ5LKE"
                                                     "xWP/xoJiw5EgpZGMuUgqU6diZ2TSh+TACA08KGmb1pfXq6XzmMo23B+pxK0HNEpk"
                                                     "cYJlmylpkiRMzI0VTzX0u7aGNLvdEqizryzzDZDtn5yK6z/UFf3RngYsvGoPz5n0"
                                                     "eoSV5m8VPc9lHaihgxU0SSyXi/kV27236dnPSE6RknY7QeySVnzx9H2wiaL/A0pE"
                                                     "Sg36Gh5BXvyLf+SCgTrvziyCl5EQetd4LYWcBb1MgyFdgSWs+2eiKyv6V4bCdUpG"
                                                     "e1YJQ/bUD2UCAwEAAaNQME4wHQYDVR0OBBYEFPwmrNbS0hPKnfqyDUcT1wJBhosm"
                                                     "MB8GA1UdIwQYMBaAFPwmrNbS0hPKnfqyDUcT1wJBhosmMAwGA1UdEwQFMAMBAf8w"
                                                     "DQYJKoZIhvcNAQELBQADggEBAEMwacs1g/yH1vNQAlBFKQ8aAy+b0eNONcOMGI/0"
                                                     "tPyPFDM+gX3H3Htjo8HJ/6pUWN0etLCwEd55NPkI1kfHjZjScMlPRjsToS+cSHvq"
                                                     "nVIhRK/ZJIrc1z6ni0qoFFtsbY82qYCVHKqxuqhV7eyZ+drPfESqoSoFWH9Wex5H"
                                                     "u1VLJeTXrhc1MqH+bUTOoRR+2qEDBSBULfw3HqXIWOAu3CLoIWr/5PGjPN6ycooD"
                                                     "0UR0BU1vwQCPdntMFY6C3mgL60ZO5DjmtXI/msh+4bGgXBY8Pl1sBlhk+ya6eKJ5"
                                                     "jYvoxIWq4c8DlH1+jW5/gf8QOMdZA/3CNcvs6w2ccDhh9l4=\n"
                                                     "-----END CERTIFICATE-----\n";

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Send an entire TCP data buffer until done
static size_t sendTcp(uSockDescriptor_t descriptor,
                      const char *pData, size_t sizeBytes)
{
    int32_t x;
    size_t sentSizeBytes = 0;
    int32_t startTimeMs;

    U_TEST_PRINT_LINE("sending %d byte(s) of TCP data...", sizeBytes);
    startTimeMs = uPortGetTickTimeMs();
    while ((sentSizeBytes < sizeBytes) &&
           ((uPortGetTickTimeMs() - startTimeMs) < 10000)) {
        x = uSockWrite(descriptor, (const void *) pData,
                       sizeBytes - sentSizeBytes);
        if (x > 0) {
            sentSizeBytes += x;
            U_TEST_PRINT_LINE("sent %d byte(s) of TCP data @%d ms.",
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
    char *pDataReceived;
    int32_t startTimeMs;
    size_t sizeBytes;
    size_t offset;
    int32_t y;
    int32_t heapUsed;
    int32_t heapSockInitLoss = 0;
    int32_t heapXxxSockInitLoss = 0;
    uSecurityTlsSettings_t settings = U_SECURITY_TLS_SETTINGS_DEFAULT;
    char hash[U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES];

    // In case a previous test failed
    uNetworkTestCleanUp();

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

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

        // Check if the server certificate is already
        // stored on the module (SARA-R5, for instance, will
        // check against this by default)
        if ((uSecurityCredentialGetHash(devHandle,
                                        U_SECURITY_CREDENTIAL_ROOT_CA_X509,
                                        "ubxlib_test_server_cert",
                                        hash) != 0) ||
            (memcmp(hash, U_SECURITY_TLS_TEST_SERVER_CERT_HASH, sizeof(hash)) != 0)) {
            // No: load the server certificate into the module
            U_TEST_PRINT_LINE("storing server certificate for the secure echo server...");
            U_PORT_TEST_ASSERT(uSecurityCredentialStore(devHandle,
                                                        U_SECURITY_CREDENTIAL_ROOT_CA_X509,
                                                        "ubxlib_test_server_cert",
                                                        gpEchoServerServerCertPem,
                                                        strlen(gpEchoServerServerCertPem),
                                                        NULL, NULL) == 0);
        }
        settings.pRootCaCertificateName = "ubxlib_test_server_cert";

        U_TEST_PRINT_LINE("looking up secure TCP echo server \"%s\"...",
                          U_SOCK_TEST_ECHO_SECURE_TCP_SERVER_DOMAIN_NAME);

        // Look up the remoteAddress of the server we use for TCP echo
        // The first call to a sockets API needs to
        // initialise the underlying sockets layer; take
        // account of that initialisation heap cost here.
        heapSockInitLoss = uPortGetHeapFree();
        // Look up the remoteAddress of the server we use for secure TCP echo
        U_PORT_TEST_ASSERT(uSockGetHostByName(devHandle,
                                              U_SOCK_TEST_ECHO_SECURE_TCP_SERVER_DOMAIN_NAME,
                                              &(remoteAddress.ipAddress)) == 0);
        heapSockInitLoss -= uPortGetHeapFree();

        // Add the port number we will use
        remoteAddress.port = U_SOCK_TEST_ECHO_SECURE_TCP_SERVER_PORT;

        // Connections can fail and, when the secure
        // ones fail the socket often gets closed as well
        // so include uSockCreate() in the loop.
        errorCode = -1;
        for (y = 3; (y > 0) && (errorCode < 0); y--) {
            // Create a TCP socket
            // Creating a secure socket may use heap in the underlying
            // network layer which will be reclaimed when the
            // network layer is closed but we don't do that here
            // to save time so need to allow for it in the heap loss
            // calculation
            heapXxxSockInitLoss += uPortGetHeapFree();
            descriptor = uSockCreate(devHandle, U_SOCK_TYPE_STREAM,
                                     U_SOCK_PROTOCOL_TCP);

            // Secure the socket
            U_TEST_PRINT_LINE("securing socket...");
            U_PORT_TEST_ASSERT(uSockSecurity(descriptor, &settings) == 0);
            heapXxxSockInitLoss -= uPortGetHeapFree();

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
        U_PORT_TEST_ASSERT(sendTcp(descriptor, gData, sizeof(gData) - 1) == sizeof(gData) - 1);

        U_TEST_PRINT_LINE("%d byte(s) sent via TCP @%d ms, now receiving...",
                          sizeof(gData) - 1, (int32_t) uPortGetTickTimeMs());

        // ...and capture them all again afterwards
        pDataReceived = (char *) pUPortMalloc(sizeof(gData) - 1);
        U_PORT_TEST_ASSERT(pDataReceived != NULL);
        //lint -e(668) Suppress possible use of NULL pointer
        // for pDataReceived
        memset(pDataReceived, 0, sizeof(gData) - 1);
        startTimeMs = uPortGetTickTimeMs();
        offset = 0;
        //lint -e{441} Suppress loop variable not found in
        // condition: we're using time instead
        for (y = 0; (offset < sizeof(gData) - 1) &&
             (uPortGetTickTimeMs() - startTimeMs < 20000); y++) {
            sizeBytes = uSockRead(descriptor,
                                  pDataReceived + offset,
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
        U_PORT_TEST_ASSERT(memcmp(pDataReceived, gData, sizeof(gData) - 1) == 0);

        uPortFree(pDataReceived);

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

    uDeviceDeinit();
    uPortDeinit();

#if !defined(__XTENSA__) && !defined(ARDUINO)
    // Check for memory leaks
    // TODO: this if'ed out for ESP32 (xtensa compiler or Arduino)
    // at the moment as there is an issue with ESP32 hanging
    // on to memory in the UART drivers that can't easily be
    // accounted for.
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("%d byte(s) were lost to secure sockets initialisation;"
                      " we have leaked %d byte(s).",
                      heapSockInitLoss + heapXxxSockInitLoss,
                      heapUsed - (heapSockInitLoss + heapXxxSockInitLoss));
    U_PORT_TEST_ASSERT(heapUsed <= heapSockInitLoss + heapXxxSockInitLoss);
#else
    (void) heapSockInitLoss;
    (void) heapXxxSockInitLoss;
    (void) heapUsed;
#endif
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[securityTls]", "securityTlsCleanUp")
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
