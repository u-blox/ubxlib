/*
 * Copyright 2020 u-blox Cambourne Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/** @brief This example demonstrates bringing up a network
 * and performing socket operations over a secured TLS
 * connection with a u-blox module.
 *
 * The choice of module and the choice of platform on which this
 * code runs is made at build time, see the README.md for
 * instructions.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdio.h"
#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"
#include "string.h"

// Required by ubxlib
#include "u_port.h"

// The next two lines will cause uPortLog() output
// to be sent to ubxlib's chosen trace output.
// Comment them out to send the uPortLog() output
// to print() instead.
#include "u_cfg_sw.h"
#include "u_port_debug.h"

// Required for a retry delay
#include "u_port_os.h"

// For default values for U_CFG_APP_xxx
#include "u_cfg_app_platform_specific.h"

// For the cellular module types
#include "u_cell_module_type.h"

// For the network API
#include "u_network.h"
#include "u_network_config_cell.h"
#include "u_network_config_wifi.h"

// For the (secure) sockets API
#include "u_sock.h"
#include "u_sock_security.h"

// For the security credential storage API
#include "u_security_credential.h"

#ifndef U_CFG_DISABLE_TEST_AUTOMATION
// This purely for internal u-blox testing
# include "u_cfg_test_platform_specific.h"
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

// Echo server URL and port number
#define MY_SERVER_NAME "ubxlib.it-sgn.u-blox.com"
#define MY_SERVER_PORT 5060

#ifndef U_CFG_ENABLE_LOGGING
# define uPortLog(format, ...)  print(format, ##__VA_ARGS__)
#endif

// For u-blox internal testing only
#ifdef U_PORT_TEST_ASSERT
# define EXAMPLE_FINAL_STATE(x) U_PORT_TEST_ASSERT(x);
#else
# define EXAMPLE_FINAL_STATE(x)
#endif

#ifndef U_PORT_TEST_FUNCTION
# error if you are not using the unit test framework to run this code you must ensure that the platform clocks/RTOS are set up and either define U_PORT_TEST_FUNCTION yourself or replace it as necessary.
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

// Cellular network configuration:
// Set U_CFG_TEST_CELL_MODULE_TYPE to your module type,
// chosen from the values in cell/api/u_cell_module_type.h
#ifdef U_CFG_TEST_CELL_MODULE_TYPE
static const uNetworkConfigurationCell_t gConfigCell = {U_NETWORK_TYPE_CELL,
                                                        U_CFG_TEST_CELL_MODULE_TYPE,
                                                        NULL, /* SIM pin */
                                                        NULL, /* APN: NULL to accept default.  If using a Thingstream SIM enter "tsiot" here */
                                                        240, /* Connection timeout in seconds */
                                                        U_CFG_APP_CELL_UART,
                                                        /* Note that the pin numbers
                                                           that follow are those of the MCU:
                                                           if you are using an MCU inside
                                                           a u-blox module the IO pin numbering
                                                           for the module is likely different
                                                           to that from the MCU: check the data
                                                           sheet for the module to determine
                                                           the mapping. */
                                                        U_CFG_APP_PIN_CELL_TXD,
                                                        U_CFG_APP_PIN_CELL_RXD,
                                                        U_CFG_APP_PIN_CELL_CTS,
                                                        U_CFG_APP_PIN_CELL_RTS,
                                                        U_CFG_APP_PIN_CELL_ENABLE_POWER,
                                                        U_CFG_APP_PIN_CELL_PWR_ON,
                                                        U_CFG_APP_PIN_CELL_VINT
                                                       };
#else
static const uNetworkConfigurationCell_t gConfigCell = {U_NETWORK_TYPE_NONE};
#endif

// TODO: Wifi network configuration.
// static const uNetworkConfigurationWifi_t gConfigWifi = {U_NETWORK_TYPE_NONE};

// The credentials that may be required to talk to the uxblib echo server.
extern const char *const gpUEchoServerClientCertPem;
extern const char gUEchoServerClientCertHash[];
extern const char *const gpUEchoServerClientKeyPem;
extern const char gUEchoServerClientKeyHash[];
extern const char *const gpUEchoServerServerCertPem;
extern const char gUEchoServerServerCertHash[];

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Print out an address structure.
static void printAddress(const uSockAddress_t *pAddress,
                         bool hasPort)
{
    switch (pAddress->ipAddress.type) {
        case U_SOCK_ADDRESS_TYPE_V4:
            uPortLog("IPV4");
            break;
        case U_SOCK_ADDRESS_TYPE_V6:
            uPortLog("IPV6");
            break;
        case U_SOCK_ADDRESS_TYPE_V4_V6:
            uPortLog("IPV4V6");
            break;
        default:
            uPortLog("unknown type (%d)", pAddress->ipAddress.type);
            break;
    }

    uPortLog(" ");

    if (pAddress->ipAddress.type == U_SOCK_ADDRESS_TYPE_V4) {
        for (int32_t x = 3; x >= 0; x--) {
            uPortLog("%u",
                     (pAddress->ipAddress.address.ipv4 >> (x * 8)) & 0xFF);
            if (x > 0) {
                uPortLog(".");
            }
        }
        if (hasPort) {
            uPortLog(":%u", pAddress->port);
        }
    } else if (pAddress->ipAddress.type == U_SOCK_ADDRESS_TYPE_V6) {
        if (hasPort) {
            uPortLog("[");
        }
        for (int32_t x = 3; x >= 0; x--) {
            uPortLog("%x:%x", pAddress->ipAddress.address.ipv6[x] >> 16,
                     pAddress->ipAddress.address.ipv6[x] & 0xFFFF);
            if (x > 0) {
                uPortLog(":");
            }
        }
        if (hasPort) {
            uPortLog("]:%u", pAddress->port);
        }
    }
}

// Check that the credentials have been loaded.
static void checkCredentials(int32_t networkHandle,
                             uSecurityTlsSettings_t *pSettings)
{
    char hash[U_SECURITY_CREDENTIAL_MD5_LENGTH_BYTES];

    // Check if the client certificate is already
    // stored on the module
    if ((uSecurityCredentialGetHash(networkHandle,
                                    U_SECURITY_CREDENTIAL_CLIENT_X509,
                                    "ubxlib_test_client_cert",
                                    hash) != 0) ||
        (memcmp(hash, gUEchoServerClientCertHash, sizeof(hash)) != 0)) {
        // Either it is not there or the wrong hash has been
        // reported, load the client certificate into the module
        uSecurityCredentialStore(networkHandle,
                                 U_SECURITY_CREDENTIAL_CLIENT_X509,
                                 "ubxlib_test_client_cert",
                                 gpUEchoServerClientCertPem,
                                 strlen(gpUEchoServerClientCertPem),
                                 NULL, NULL);
    }
    pSettings->pClientCertificateName = "ubxlib_test_client_cert";

    // Check if the client key is already stored on the module
    if ((uSecurityCredentialGetHash(networkHandle,
                                    U_SECURITY_CREDENTIAL_CLIENT_KEY_PRIVATE,
                                    "ubxlib_test_client_key",
                                    hash) != 0) ||
        (memcmp(hash, gUEchoServerClientKeyHash, sizeof(hash)) != 0)) {
        // Either it is not there or the wrong hash has been
        // reported, load the client key into the module
        uSecurityCredentialStore(networkHandle,
                                 U_SECURITY_CREDENTIAL_CLIENT_KEY_PRIVATE,
                                 "ubxlib_test_client_key",
                                 gpUEchoServerClientKeyPem,
                                 strlen(gpUEchoServerClientKeyPem),
                                 NULL, NULL);
    }
    pSettings->pClientPrivateKeyName = "ubxlib_test_client_key";

    // Check if the server certificate is already
    // stored on the module
    if ((uSecurityCredentialGetHash(networkHandle,
                                    U_SECURITY_CREDENTIAL_ROOT_CA_X509,
                                    "ubxlib_test_server_cert",
                                    hash) != 0) ||
        (memcmp(hash, gUEchoServerServerCertHash, sizeof(hash)) != 0)) {
        // Either it is not there or the wrong hash has been
        // reported, load the server certificate into the module
        // as a trusted key
        // IMPORTANT: in the real world you would not need to do
        // this, you would have root certificates loaded to do the
        // job.  We are only doing it here because the ubxlib echo
        // server is simply for testing and therefore not part of
        // any chain of trust
        uPortLog("U_SECURITY_TLS_TEST: storing server certificate"
                 " for the secure echo server...\n");
        uSecurityCredentialStore(networkHandle,
                                 U_SECURITY_CREDENTIAL_ROOT_CA_X509,
                                 "ubxlib_test_server_cert",
                                 gpUEchoServerServerCertPem,
                                 strlen(gpUEchoServerServerCertPem),
                                 NULL, NULL);
    }
    pSettings->pRootCaCertificateName = "ubxlib_test_server_cert";
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: THE EXAMPLE
 * -------------------------------------------------------------- */

// The entry point, main(): before this is called the system
// clocks must have been started and the RTOS must be running;
// we are in task space.
U_PORT_TEST_FUNCTION("[example]", "exampleSocketsTls")
{
    int32_t networkHandle;
    int32_t sock;
    int32_t x;
    bool connected;
    uSockAddress_t address;
    const char message[] = "The quick brown fox jumps over the lazy dog.";
    size_t txSize = sizeof(message);
    char buffer[64];
    size_t rxSize = 0;
    uSecurityTlsSettings_t settings = U_SECURITY_TLS_SETTINGS_DEFAULT;

    // Initialise the APIs we will need
    uPortInit();
    uNetworkInit();

    // Add a network instance, in this case of type cell
    // since that's what we have configuration information
    // for above.
    networkHandle = uNetworkAdd(U_NETWORK_TYPE_CELL,
                                (void *) &gConfigCell);
    uPortLog("Added network with handle %d.\n", networkHandle);

    // Bring up the network layer
    uPortLog("Bringing up the network...\n");
    if (uNetworkUp(networkHandle) == 0) {

        // Do things using the network, for
        // example connect and send data to
        // an echo server over a TCP socket
        // as follows

        // Get the server's IP address using
        // the network's DNS resolution facility
        uPortLog("Looking up server address...\n");
        uSockGetHostByName(networkHandle, MY_SERVER_NAME,
                           &(address.ipAddress));
        uPortLog("Address is: ");
        printAddress(&address, false);
        address.port = MY_SERVER_PORT;
        uPortLog("\n");

        // Check that the relevant credentials
        // have been loaded
        checkCredentials(networkHandle, &settings);

        // Create the socket on the network
        uPortLog("Creating socket...\n");
        sock = uSockCreate(networkHandle,
                           U_SOCK_TYPE_STREAM,
                           U_SOCK_PROTOCOL_TCP);

        // Secure the socket.  Before calling this
        // you would make any changes to settings
        // that you wished.  By default only
        // end to end encryption will be performed
        // but, having loaded the credentials above,
        // we will pass the client certificate to
        // the server on request and some modules
        // (e.g. SARA-R5) will also by default confirm
        // the server's authenticity
        if (uSockSecurity(sock, &settings) == 0) {
            // Make a TCP connection to the server
            // over TLS. During our test campaigns, with
            // many boards running at once, this can fail
            // due to collisions, so try a few times with
            // a little delay
            for (x = 3; !(connected = (uSockConnect(sock, &address) >= 0)) &&
                 (x > 0); x--) {
                uPortTaskBlock(5000);
            }

            if (connected) {
                // Send the data over the socket
                // and print the echo that comes back
                uPortLog("Sending data...\n");
                x = 0;
                while ((x >= 0) && (txSize > 0)) {
                    x = uSockWrite(sock, message, txSize);
                    if (x > 0) {
                        txSize -= x;
                    }
                }
                uPortLog("Sent %d byte(s) to echo server.\n", sizeof(message) - txSize);
                while ((x >= 0) && (rxSize < sizeof(message))) {
                    x = uSockRead(sock, buffer + rxSize, sizeof(buffer) - rxSize);
                    if (x > 0) {
                        rxSize += x;
                    }
                }
                if (rxSize > 0) {
                    uPortLog("\nReceived echo back (%d byte(s)): %s\n", rxSize, buffer);
                } else {
                    uPortLog("\nNo reply received!\n");
                }
            } else {
                uPortLog("Unable to connect to server!\n");
            }

            // Note: since networkHandle is a cellular
            // handle any of the `cell` API calls
            // could be made here using it.
            // If the configuration used were Wifi
            // then the `wifi` API calls could be
            // used

            // Close the socket
            uPortLog("Closing socket...\n");
            uSockShutdown(sock, U_SOCK_SHUTDOWN_READ_WRITE);
            uSockClose(sock);
            uSockCleanUp(sock);

            // When finished with the network layer
            uPortLog("Taking down network...\n");
            uNetworkDown(networkHandle);
        } else {
            uPortLog("Unable to secure socket!\n");
        }
    } else {
        uPortLog("Unable to bring up the network!\n");
    }

    // Calling these will also deallocate the network handle
    uNetworkDeinit();
    uPortDeinit();

    uPortLog("Done.\n");

#ifdef U_CFG_TEST_CELL_MODULE_TYPE
    // For u-blox internal testing only
    EXAMPLE_FINAL_STATE((txSize == 0) && (rxSize == sizeof(message)));
#endif
}

// End of file
