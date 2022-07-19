/*
 * Copyright 2022 u-blox
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

/** @brief This is an Arduino version of main.c, demonstrating how to
 * bring up a network connection and then perform sockets operations
 * with a server on the public internet using a u-blox module.
 */

// Include the ubxlib library
#include <ubxlib.h>

// Bring in the U_CFG_APP_xxx application settings; you ONLY need
// this for the U_CFG_APP_xxx values below, it is not required by
// the ubxlib code
#include <u_cfg_app_platform_specific.h>

// Required, just for this example file, to bring in U_CFG_OS_xxx;
// you would not need this in your application.
#include <u_cfg_os_platform_specific.h>

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

// Cellular network configuration.

// IMPORTANT: YOU must #define U_CFG_TEST_CELL_MODULE_TYPE to the
// type of cellular module you are using, chosen from the values in
// cell/api/u_cell_module_type.h in the ubxlib library.
// For instance, if you are using a SARA-R5 module you might
// uncomment the following line:
//
// #define U_CFG_TEST_CELL_MODULE_TYPE U_CELL_MODULE_TYPE_SARA_R5

#ifdef U_CFG_TEST_CELL_MODULE_TYPE
// The default values for U_CFG_xxx are set in the file
// u_cfg_app_platform_specific.h under the ESP-IDF platform; you may
// edit the values in that file or you may replace the values
// here with the appropriate pin numbers (where -1 means "not
// connected/used"), it is up to you.
//
// Note that the pin numbers are those of the MCU: if you
// are using an MCU inside a u-blox module the IO pin numbering
// for the module is likely different from that of the MCU: check
// the data sheet for the module to determine the mapping.
//
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
};
#else
// No module available - set some dummy values to make test system happy
static const uDeviceCfg_t gDeviceCfg = {.deviceType = U_DEVICE_TYPE_NONE};
static const uNetworkCfgCell_t gNetworkCfg = {.type = U_NETWORK_TYPE_NONE};
#endif

// The network handle
static uDeviceHandle_t devHandle = NULL;

// A global error code
static int32_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;

/* ----------------------------------------------------------------
 * UTILITY FUNCTIONS
 * -------------------------------------------------------------- */

// Print out an address structure.
static void printAddress(const uSockAddress_t *pAddress, bool hasPort)
{
    switch (pAddress->ipAddress.type) {
        case U_SOCK_ADDRESS_TYPE_V4:
            printf("IPV4");
            break;
        case U_SOCK_ADDRESS_TYPE_V6:
            printf("IPV6");
            break;
        case U_SOCK_ADDRESS_TYPE_V4_V6:
            printf("IPV4V6");
            break;
        default:
            printf("unknown type (%d)", pAddress->ipAddress.type);
            break;
    }

    printf(" ");

    if (pAddress->ipAddress.type == U_SOCK_ADDRESS_TYPE_V4) {
        for (int32_t x = 3; x >= 0; x--) {
            printf("%u", (pAddress->ipAddress.address.ipv4 >> (x * 8)) & 0xFF);
            if (x > 0) {
                printf(".");
            }
        }
        if (hasPort) {
            printf(":%u", pAddress->port);
        }
    } else if (pAddress->ipAddress.type == U_SOCK_ADDRESS_TYPE_V6) {
        if (hasPort) {
            printf("[");
        }
        for (int32_t x = 3; x >= 0; x--) {
            printf("%x:%x", pAddress->ipAddress.address.ipv6[x] >> 16,
                     pAddress->ipAddress.address.ipv6[x] & 0xFFFF);
            if (x > 0) {
                printf(":");
            }
        }
        if (hasPort) {
            printf("]:%u", pAddress->port);
        }
    }
}

/* ----------------------------------------------------------------
 * THE EXAMPLE
 * -------------------------------------------------------------- */

void setup() {
    int32_t returnCode;
    // Initialise the APIs we will need
    uPortInit();
    uDeviceInit();

    // Open the device.  Once this function has returned a
    // non-negative value then the transport is powered-up,
    // can be configured etc. but is not yet connected.
    returnCode = uDeviceOpen(&gDeviceCfg, &devHandle);
    if (returnCode >= 0) {
        printf("Added network with handle %d.\n", devHandle);
        // Bring up the network layer, i.e. connect it so that
        // after this point it may be used to transfer data.
        printf("Bringing up the network...\n");
        errorCode = uNetworkInterfaceUp(devHandle, U_NETWORK_TYPE_CELL,
                                        &gNetworkCfg);
    } else {
        printf("Unable to add network, error %d!\n", devHandle);
    }

    // An errorCode of zero indicates success
}

void loop() {
    int32_t sock;
    int32_t x = 0;
    uSockAddress_t address;
    const char message[] = "The quick brown fox jumps over the lazy dog.";
    size_t txSize = sizeof(message);
    char buffer[64];
    size_t rxSize = 0;

    if (errorCode == 0) {
        // Do things using the network, for
        // example connect and send data to
        // an echo server over a TCP socket
        // as follows

        // Get the IP address of the echo server using
        // the network's DNS resolution facility
        printf("Looking up server address...\n");
        uSockGetHostByName(devHandle, "ubxlib.it-sgn.u-blox.com",
                           &(address.ipAddress));
        printf("Address is: ");
        printAddress(&address, false);
        // The echo server is configured to echo TCP
        // packets on port 5055
        address.port = 5055;
        printf("\n");

        // Create the socket on the network
        printf("Creating socket...\n");
        sock = uSockCreate(devHandle,
                           U_SOCK_TYPE_STREAM,
                           U_SOCK_PROTOCOL_TCP);

        // Make a TCP connection to the server using
        // the socket
        if (uSockConnect(sock, &address) == 0) {
            // Send the data over the socket
            // and print the echo that comes back
            printf("Sending data...\n");
            while ((x >= 0) && (txSize > 0)) {
                x = uSockWrite(sock, message, txSize);
                if (x > 0) {
                    txSize -= x;
                }
            }
            printf("Sent %d byte(s) to echo server.\n", sizeof(message) - txSize);
            while ((x >= 0) && (rxSize < sizeof(message))) {
                x = uSockRead(sock, buffer + rxSize, sizeof(buffer) - rxSize);
                if (x > 0) {
                    rxSize += x;
                }
            }
            if (rxSize > 0) {
                printf("\nReceived echo back (%d byte(s)): %s\n", rxSize, buffer);
            } else {
                printf("\nNo reply received!\n");
            }
        } else {
            printf("Unable to connect to server!\n");
        }

        // Note: since devHandle is a cellular
        // handle any of the `cell` API calls
        // could be made here using it.
        // If the configuration used were Wifi
        // then the `wifi` API calls could be
        // used

        // Close the socket
        printf("Closing socket...\n");
        uSockShutdown(sock, U_SOCK_SHUTDOWN_READ_WRITE);
        uSockClose(sock);
        uSockCleanUp();
    } else {
        printf("Network is not available!\n");
    }

    // The remainder of this function is for u-blox internal
    // testing only, you can happily delete it
    int32_t failures = 0;
    int32_t tests = 0;
#ifdef U_CFG_TEST_CELL_MODULE_TYPE
    tests = 1;
    if ((txSize != 0) || (rxSize != sizeof(message))) {
        failures = 1;
    }
#endif
    printf("%d Tests %d Failures 0 Ignored\n", tests, failures);

    // Close the device
    // Note: we don't power the device down here in order
    // to speed up testing; you may prefer to power it off
    // by setting the second parameter to true.
    uDeviceClose(devHandle, false);

    // Tidy up
    uDeviceDeinit();
    uPortDeinit();

    // We make the loop stop here as otherwise it will
    // send data again and you could run-up a large bill
    while (1) {
        // Can't just be an infinite loop as there might
        // be a watchdog timer running
        delay(1000);
    }
}
