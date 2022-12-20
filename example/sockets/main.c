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

/** @brief This example demonstrates how to bring up a network
 * connection and then perform sockets operations with a server
 * on the public internet using a u-blox module.
 *
 * The choice of module and the choice of platform on which this
 * code runs is made at build time, see the README.md for
 * instructions.
 */

// Bring in all of the ubxlib public header files
#include "ubxlib.h"

// Bring in the application settings
#include "u_cfg_app_platform_specific.h"

// For U_SHORT_RANGE_TEST_WIFI()
#include "u_short_range_test_selector.h"

#ifndef U_CFG_DISABLE_TEST_AUTOMATION
// This purely for internal u-blox testing
# include "u_cfg_test_platform_specific.h"
# include "u_wifi_test_cfg.h"
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

// Echo server URL and port number
#define MY_SERVER_NAME "ubxlib.redirectme.net"
#define MY_SERVER_PORT 5055

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

// Below is the module configuration
// When U_CFG_TEST_CELL_MODULE_TYPE is set this example will setup a cellular
// link using uNetworkConfigurationCell_t.
// When U_CFG_TEST_SHORT_RANGE_MODULE_TYPE is set this example will instead use
// uNetworkConfigurationWifi_t config to setup a Wifi connection.

#if U_SHORT_RANGE_TEST_WIFI()

// Set U_CFG_TEST_SHORT_RANGE_MODULE_TYPE to your module type,
// chosen from the values in common/short_range/api/u_short_range_module_type.h

// DEVICE i.e. module/chip configuration: in this case a short-range
// module connected via UART
static const uDeviceCfg_t gDeviceCfg = {
    .deviceType = U_DEVICE_TYPE_SHORT_RANGE,
    .deviceCfg = {
        .cfgSho = {
            .moduleType = U_CFG_TEST_SHORT_RANGE_MODULE_TYPE
        },
    },
    .transportType = U_DEVICE_TRANSPORT_TYPE_UART,
    .transportCfg = {
        .cfgUart = {
            .uart = U_CFG_APP_SHORT_RANGE_UART,
            .baudRate = U_SHORT_RANGE_UART_BAUD_RATE,
            .pinTxd = U_CFG_APP_PIN_SHORT_RANGE_TXD,
            .pinRxd = U_CFG_APP_PIN_SHORT_RANGE_RXD,
            .pinCts = U_CFG_APP_PIN_SHORT_RANGE_CTS,
            .pinRts = U_CFG_APP_PIN_SHORT_RANGE_RTS
        },
    },
};
// NETWORK configuration for Wi-Fi
static const uNetworkCfgWifi_t gNetworkCfg = {
    .type = U_NETWORK_TYPE_WIFI,
    .pSsid = U_PORT_STRINGIFY_QUOTED(U_WIFI_TEST_CFG_SSID), /* Wifi SSID - replace with your SSID */
    .authentication = U_WIFI_TEST_CFG_AUTHENTICATION, /* Authentication mode (see uWifiAuth_t in wifi/api/u_wifi.h) */
    .pPassPhrase = U_PORT_STRINGIFY_QUOTED(U_WIFI_TEST_CFG_WPA2_PASSPHRASE) /* WPA2 passphrase */
};
static const uNetworkType_t gNetType = U_NETWORK_TYPE_WIFI;

#elif defined(U_CFG_TEST_CELL_MODULE_TYPE)

// Cellular configuration.
// Set U_CFG_TEST_CELL_MODULE_TYPE to your module type,
// chosen from the values in cell/api/u_cell_module_type.h
//
// Note that the pin numbers are those of the MCU: if you
// are using an MCU inside a u-blox module the IO pin numbering
// for the module is likely different that from the MCU: check
// the data sheet for the module to determine the mapping.

// DEVICE i.e. module/chip configuration: in this case a cellular
// module connected via UART
static const uDeviceCfg_t gDeviceCfg = {
    .deviceType = U_DEVICE_TYPE_CELL,
    .deviceCfg = {
        .cfgCell = {
            .moduleType = U_CFG_TEST_CELL_MODULE_TYPE,
            .pSimPinCode = NULL, /* SIM pin */
            .pinEnablePower = U_CFG_APP_PIN_CELL_ENABLE_POWER,
            .pinPwrOn = U_CFG_APP_PIN_CELL_PWR_ON,
            .pinVInt = U_CFG_APP_PIN_CELL_VINT,
            .pinDtrPowerSaving = U_CFG_APP_PIN_CELL_DTR
        },
    },
    .transportType = U_DEVICE_TRANSPORT_TYPE_UART,
    .transportCfg = {
        .cfgUart = {
            .uart = U_CFG_APP_CELL_UART,
            .baudRate = U_CELL_UART_BAUD_RATE,
            .pinTxd = U_CFG_APP_PIN_CELL_TXD,
            .pinRxd = U_CFG_APP_PIN_CELL_RXD,
            .pinCts = U_CFG_APP_PIN_CELL_CTS,
            .pinRts = U_CFG_APP_PIN_CELL_RTS
        },
    },
};
// NETWORK configuration for cellular
static const uNetworkCfgCell_t gNetworkCfg = {
    .type = U_NETWORK_TYPE_CELL,
    .pApn = NULL, /* APN: NULL to accept default.  If using a Thingstream SIM enter "tsiot" here */
    .timeoutSeconds = 240 /* Connection timeout in seconds */
    // There is an additional field here "pKeepGoingCallback",
    // which we do NOT set, we allow the compiler to set it to 0
    // and all will be fine. You may set the field to a function
    // of the form "bool keepGoingCallback(uDeviceHandle_t devHandle)",
    // e.g.:
    // .pKeepGoingCallback = keepGoingCallback
    // ...and your function will be called periodically during an
    // abortable network operation such as connect/disconnect;
    // if it returns true the operation will continue else it
    // will be aborted, allowing you immediate control.  If this
    // field is set, timeoutSeconds will be ignored.
};
static const uNetworkType_t gNetType = U_NETWORK_TYPE_CELL;
#else
// No module available - set some dummy values to make test system happy
static const uDeviceCfg_t gDeviceCfg = {.deviceType = U_DEVICE_TYPE_NONE};
static const uNetworkCfgCell_t gNetworkCfg = {.type = U_NETWORK_TYPE_NONE};
static const uNetworkType_t gNetType = U_NETWORK_TYPE_CELL;
#endif

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
    uDeviceHandle_t devHandle = NULL;
    int32_t sock;
    int32_t x = 0;
    uSockAddress_t address;
    const char message[] = "The quick brown fox jumps over the lazy dog.";
    size_t txSize = sizeof(message);
    char buffer[64];
    size_t rxSize = 0;
    int32_t returnCode;

    // Initialise the APIs we will need
    uPortInit();
    uDeviceInit();

    // Open the device
    returnCode = uDeviceOpen(&gDeviceCfg, &devHandle);
    uPortLog("Opened device with return code %d.\n", returnCode);

    // Bring up the network interface
    uPortLog("Bringing up the network...\n");
    if (uNetworkInterfaceUp(devHandle, gNetType,
                            &gNetworkCfg) == 0) {

        // Do things using the network, for
        // example connect and send data to
        // an echo server over a TCP socket
        // as follows

        // Get the server's IP address using
        // the network's DNS resolution facility
        uPortLog("Looking up server address...\n");
        uSockGetHostByName(devHandle, MY_SERVER_NAME,
                           &(address.ipAddress));
        uPortLog("Address is: ");
        printAddress(&address, false);
        address.port = MY_SERVER_PORT;
        uPortLog("\n");

        // Create the socket on the network
        uPortLog("Creating socket...\n");
        sock = uSockCreate(devHandle,
                           U_SOCK_TYPE_STREAM,
                           U_SOCK_PROTOCOL_TCP);

        // Make a TCP connection to the server using
        // the socket
        if (uSockConnect(sock, &address) == 0) {
            // Send the data over the socket
            // and print the echo that comes back
            uPortLog("Sending data...\n");
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

        // Note: if devHandle is a cellular
        // handle any of the `cell` API calls
        // could be made here using it.
        // If the configuration used were Wifi
        // then the `wifi` API calls could be
        // used

        // Close the socket
        uPortLog("Closing socket...\n");
        uSockShutdown(sock, U_SOCK_SHUTDOWN_READ_WRITE);
        uSockClose(sock);
        uSockCleanUp();

        // When finished with the network layer
        uPortLog("Taking down network...\n");
        uNetworkInterfaceDown(devHandle, gNetType);
    } else {
        uPortLog("Unable to bring up the network!\n");
    }

    // Close the device
    // Note: we don't power the device down here in order
    // to speed up testing; you may prefer to power it off
    // by setting the second parameter to true.
    uDeviceClose(devHandle, false);

    // Tidy up
    uDeviceDeinit();
    uPortDeinit();

    uPortLog("Done.\n");

#ifdef U_CFG_TEST_CELL_MODULE_TYPE
    // For u-blox internal testing only
    EXAMPLE_FINAL_STATE((txSize == 0) && (rxSize == sizeof(message)));
#endif
}

// End of file
