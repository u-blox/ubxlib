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

/** @brief This example demonstrates how to bring up a network
 * connection and then perform sockets operations with a server
 * on the public internet using a u-blox module.
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

// For the sockets API
#include "u_sock.h"

#ifndef U_CFG_DISABLE_TEST_AUTOMATION
// This purely for internal u-blox testing
# include "u_cfg_test_platform_specific.h"
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

// Echo server URL and port number
#define MY_SERVER_NAME "ubxlib.it-sgn.u-blox.com"
#define MY_SERVER_PORT 5055

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

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: THE EXAMPLE
 * -------------------------------------------------------------- */

// The entry point, main(): before this is called the system
// clocks must have been started and the RTOS must be running;
// we are in task space.
U_PORT_TEST_FUNCTION("[example]", "exampleSockets")
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

        // Create the socket on the network
        uPortLog("Creating socket...\n");
        sock = uSockCreate(networkHandle,
                           U_SOCK_TYPE_STREAM,
                           U_SOCK_PROTOCOL_TCP);

        // Make a TCP connection to the server using
        // the socket. During our test campaigns, with
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
