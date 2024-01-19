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

/** @brief This example demonstrates how to bring up a network
 * connection and then perform sockets operations using the native
 * IP stack of the Zephyr platform.
 *
 * For this example to run you must define U_CFG_PPP_ENABLE when
 * building ubxlib and you must set the following in your prj.conf file:
 *
 * CONFIG_NETWORKING=y
 * CONFIG_NET_DRIVERS=y
 * CONFIG_NET_IPV6=n
 * CONFIG_NET_IPV4=y
 * CONFIG_PPP_NET_IF_NO_AUTO_START=y
 * CONFIG_NET_PPP=y
 * CONFIG_NET_PPP_ASYNC_UART=y
 * CONFIG_NET_L2_PPP=y
 * CONFIG_NET_L2_PPP_PAP=y
 * CONFIG_NET_L2_PPP_TIMEOUT=10000
 * CONFIG_NET_PPP_UART_BUF_LEN=512 (suggested buffer size)
 * CONFIG_NET_PPP_ASYNC_UART_TX_BUF_LEN=512 (suggested buffer size)
 *
 * Depending on how much data you expect to receive, you may want to increase
 * CONFIG_NET_PPP_RINGBUF_SIZE from the default of 256 (during testing we
 * use 1024).
 *
 * For this example to work you must also enable sockets and TCP with:
 *
 * CONFIG_NET_TCP=y
 * CONFIG_NET_TCP_MAX_SEND_WINDOW_SIZE=256 (since the PPP link is relatively slow, keep the window size small)
 * CONFIG_NET_TCP_MAX_RECV_WINDOW_SIZE=256
 * CONFIG_NET_SOCKETS=y
 *
 * In addition to all of the above, you must add the following to
 * your `.dts` or `.overlay` file:
 *
 *
 * / {
 *    chosen {
 *        zephyr,ppp-uart = &uart99;
 *    };
 *
 *    uart99: uart-ppp@8000 {
 *        compatible = "u-blox,uart-ppp";
 *        reg = <0x8000 0x100>;
 *        status = "okay";
 *    };
 *};
 *
 * Note that if your network operator requires a user name and password
 * along with the APN then you must edit the username/password that
 * is hard-coded in Zephyr ppp.c; Zephyr does not offer a way to set
 * this at run-time.  Also note that Zephyr does not support CHAP
 * authentication.
 *
 * The choice of [cellular] module is made at build time, see the
 * README.md for instructions.
 */

// This example is only for the Zephyr platform
#ifdef __ZEPHYR__

// Bring in all of the ubxlib public header files
#include "ubxlib.h"

#include <version.h>

#if KERNEL_VERSION_NUMBER >= ZEPHYR_VERSION(3,1,0)
#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#else
#include <kernel.h>
#include <net/socket.h>
#endif

#if defined(CONFIG_NET_PPP) && defined(CONFIG_NET_TCP) &&    \
    defined(CONFIG_NET_SOCKETS) && defined(U_CFG_PPP_ENABLE)

#include "errno.h"

// Bring in the application settings
#include "u_cfg_app_platform_specific.h"

#ifndef U_CFG_DISABLE_TEST_AUTOMATION
// This purely for internal u-blox testing
# include "u_cfg_test_platform_specific.h"
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

// Echo server IP address and port number; see the body of the code
// for why we use the IP address rather than the domain name
#define MY_SERVER_IP_ADDRESS "18.133.144.142"
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

#ifdef U_CFG_TEST_CELL_MODULE_TYPE
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
            .pinRts = U_CFG_APP_PIN_CELL_RTS,
#ifdef U_CFG_APP_UART_PREFIX
            .pPrefix = U_PORT_STRINGIFY_QUOTED(U_CFG_APP_UART_PREFIX) // Relevant for Linux only
#else
            .pPrefix = NULL
#endif
        },
    },
};
// NETWORK configuration
static const uNetworkCfgCell_t gNetworkCfg = {
    .type = U_NETWORK_TYPE_CELL,
    .pApn = NULL, /* APN: NULL to accept default.  If using a Thingstream SIM enter "tsiot" here */
    .timeoutSeconds = 240 /* Connection timeout in seconds */
    // There are four additional fields here which we do NOT set,
    // we allow the compiler to set them to 0 and all will be fine.
    // The fields are:
    //
    // - "pKeepGoingCallback": you may set this field to a function
    //   of the form "bool keepGoingCallback(uDeviceHandle_t devHandle)",
    //   e.g.:
    //
    //   .pKeepGoingCallback = keepGoingCallback;
    //
    //   ...and your function will be called periodically during an
    //   abortable network operation such as connect/disconnect;
    //   if it returns true the operation will continue else it
    //   will be aborted, allowing you immediate control.  If this
    //   field is set, timeoutSeconds will be ignored.
    //
    // - "pUsername" and "pPassword": if you are required to set a
    //   user name and password to go with the APN value that you
    //   were given by your service provider, set them here.
    //
    // - "authenticationMode": if you MUST give a user name and
    //   password then you must populate this field with the
    //   authentication mode that should be used, see
    //   #uCellNetAuthenticationMode_t in u_cell_net.h, and noting
    //   that automatic authentication mode will NOT work with PPP.
    //   You ONLY NEED TO WORRY ABOUT THIS if you were given a user name
    //   name and password with the APN (which is thankfully not usual).
    //
    // - "pMccMnc": ONLY required if you wish to connect to a specific
    //   MCC/MNC rather than to the best available network; should point
    //   to the null-terminated string giving the MCC and MNC of the PLMN
    //   to use (for example "23410").
};
#else
// No module available - set some dummy values to make test system happy
static const uDeviceCfg_t gDeviceCfg = {.deviceType = U_DEVICE_TYPE_NONE};
static const uNetworkCfgCell_t gNetworkCfg = {.type = U_NETWORK_TYPE_NONE};
#endif

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: THE EXAMPLE
 * -------------------------------------------------------------- */

// The entry point, main(): before this is called the system
// clocks must have been started and the RTOS must be running;
// we are in task space.
U_PORT_TEST_FUNCTION("[example]", "examplePppZephyrSockets")
{
    uDeviceHandle_t devHandle = NULL;
    struct sockaddr_in destinationAddress;
    int32_t sock;
    const char message[] = "The quick brown zephyr-fox jumps over the lazy dog.";
    size_t txSize = sizeof(message);
    char buffer[128];
    size_t rxSize = 0;
    int32_t returnCode;

    // Initialise the APIs we will need
    uPortInit();
    uDeviceInit();

    // Open the device
    returnCode = uDeviceOpen(&gDeviceCfg, &devHandle);
    uPortLog("Opened device with return code %d.\n", returnCode);

    if (returnCode == 0) {
        // Bring up the network interface
        uPortLog("Bringing up the network...\n");
        if (uNetworkInterfaceUp(devHandle, U_NETWORK_TYPE_CELL,
                                &gNetworkCfg) == 0) {

            // Zephyr's IP stack is now connected to the
            // internet via the cellular module

            // Call the native sockets APIs of the
            // Zephyr platform to send data; you could
            // equally use any of the Zephyr native protocol
            // entities (MQTT, HTTP, etc.)

            // Note: normally you would do something like:
            //
            // struct addrinfo hints = {0};
            // struct addrinfo *pAddrs = NULL;
            // hints.ai_family = AF_INET;
            // hints.ai_socktype = SOCK_STREAM;
            // if (zsock_getaddrinfo(MY_SERVER_NAME, NULL, &hints, &pAddrs) == 0) {
            //     ...
            //
            // ...here to get the IP address of MY_SERVER_NAME (e.g.
            // "ubxlib.com") into the pointer pAddrs, calling
            // zsock_freeaddrinfo(pAddrs) to free memory again at the end.
            //
            // However, zsock_getaddrinfo() does not work with the Zephyr
            // minimal lib C (it requires an implementation of calloc(),
            // which we don't bring in), hence in this example we use the
            // known IP address of the server instead.
            zsock_inet_pton(AF_INET, MY_SERVER_IP_ADDRESS, &destinationAddress.sin_addr);
            destinationAddress.sin_family = AF_INET;
            destinationAddress.sin_port = htons(MY_SERVER_PORT);
            errno = 0;
            sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
            if (sock >= 0) {
                if (zsock_connect(sock, (struct sockaddr *) &destinationAddress, sizeof(destinationAddress)) == 0) {
                    if (zsock_send(sock, message, txSize, 0) == txSize) {
                        uPortLog("Sent %d byte(s) to echo server.\n", txSize);
                        rxSize = zsock_recv(sock, buffer, sizeof(buffer), 0);
                        if (rxSize > 0) {
                            uPortLog("\nReceived echo back (%d byte(s)): %s\n", rxSize, buffer);
                        } else {
                            uPortLog("\nNo reply received!\n");
                        }
                    } else {
                        uPortLog("Unable to send to server (errno %d)!\n", errno);
                    }
                } else {
                    uPortLog("Unable to connect to server (errno %d)!\n", errno);
                }
            } else {
                uPortLog("Unable to create socket (errno %d)!\n", errno);
            }

            // Close the socket
            uPortLog("Closing socket...\n");
            zsock_shutdown(sock, 0);
            zsock_close(sock);

            // When finished with the network layer
            uPortLog("Taking down network...\n");
            uNetworkInterfaceDown(devHandle, U_NETWORK_TYPE_CELL);
        } else {
            uPortLog("Unable to bring up the network!\n");
        }

        // Close the device
        // Note: we don't power the device down here in order
        // to speed up testing; you may prefer to power it off
        // by setting the second parameter to true.
        uDeviceClose(devHandle, false);

    } else {
        uPortLog("Unable to bring up the device!\n");
    }

    // Tidy up
    uDeviceDeinit();
    uPortDeinit();

    uPortLog("Done.\n");

#ifdef U_CFG_TEST_CELL_MODULE_TYPE
    // For u-blox internal testing only
    EXAMPLE_FINAL_STATE(rxSize == sizeof(message));
#endif
}

#endif // #if defined(CONFIG_NET_PPP) && defined(CONFIG_NET_TCP) &&
// defined(CONFIG_NET_SOCKETS) && defined(U_CFG_PPP_ENABLE)
#endif // #ifdef __ZEPHYR__

// End of file
