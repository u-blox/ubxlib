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

/** @brief This example demonstrates how to use the common HTTP API.
 *
 * The choice of module and the choice of platform on which this
 * code runs is made at build time, see the README.md for
 * instructions.
 */

#include "string.h" // For strlen()
#include "stdio.h"  // For snprintf()

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

// HTTPS server URL: this is a test server that accepts PUT/POST
// requests and GET/HEAD/DELETE requests on port 8081; there is
// also an HTTP server on port 8080.
#define MY_SERVER_NAME "ubxlib.it-sgn.u-blox.com:8081"

// Some data to PUT and GET with the server.
const char *gpMyData = "Hello world!";

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
// When U_CFG_TEST_CELL_MODULE_TYPE is set this example will setup a
// cellular link using uNetworkCfgCell_t.
// When U_CFG_TEST_SHORT_RANGE_MODULE_TYPE is set this example will
// instead use uNetworkCfgWifi_t config to setup a Wifi connection.

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

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: THE EXAMPLE
 * -------------------------------------------------------------- */

// The entry point, main(): before this is called the system
// clocks must have been started and the RTOS must be running;
// we are in task space.
U_PORT_TEST_FUNCTION("[example]", "exampleHttpClient")
{
    uDeviceHandle_t devHandle = NULL;
    uHttpClientContext_t *pContext = NULL;
    uHttpClientConnection_t connection = U_HTTP_CLIENT_CONNECTION_DEFAULT;
    uSecurityTlsSettings_t tlsSettings = U_SECURITY_TLS_SETTINGS_DEFAULT;
    char serialNumber[U_SECURITY_SERIAL_NUMBER_MAX_LENGTH_BYTES];
    char path[U_SECURITY_SERIAL_NUMBER_MAX_LENGTH_BYTES + 10];
    char buffer[32] = {0};
    size_t size = sizeof(buffer);
    int32_t statusCode = 0;
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

        // Note: since devHandle is a cellular
        // handle, any of the `cell` API calls could
        // be made here using it.

        // In order to create a unique path on the public
        // server which won't collide with anyone else,
        // we make the path the serial number of the
        // module
        uSecurityGetSerialNumber(devHandle, serialNumber);
        snprintf(path, sizeof(path), "/%s.html", serialNumber);

        // Set the URL of the server; each instance
        // is associated with a single server -
        // you may create more than one insance,
        // for different servers, or close and
        // open instances to access more than one
        // server.  There are other settings in
        // uHttpClientConnection_t, but for the
        // purposes of this example they can be
        // left at defaults
        connection.pServerName = MY_SERVER_NAME;

        // Create an HTTPS instance for the server; to
        // create an HTTP instance instead you would
        // replace &tlsSettings with NULL (and of
        // course use port 8080 on the test HTTP server).
        pContext = pUHttpClientOpen(devHandle, &connection, &tlsSettings);
        if (pContext != NULL) {

            // POST some data to the server; doesn't have to be text,
            // can be anything, including binary data, though obviously
            // you must give the appropriate content-type
            statusCode = uHttpClientPostRequest(pContext, path, gpMyData, strlen(gpMyData), "text/plain");
            if (statusCode == 200) {

                uPortLog("POST some data to the file \"%s\" on %s.\n", path, MY_SERVER_NAME);

                // GET it back again
                statusCode = uHttpClientGetRequest(pContext, path, buffer, &size, NULL);
                if (statusCode == 200) {

                    uPortLog("GET the data: it was \"%s\" (%d byte(s)).\n", buffer, size);

                } else {
                    uPortLog("Unable to GET file \"%s\" from %s; status code was %d!\n",
                             path, MY_SERVER_NAME, statusCode);
                }
            } else {
                uPortLog("Unable to POST file \"%s\" to %s; status code was %d!\n",
                         path, MY_SERVER_NAME, statusCode);
            }

            // Close the HTTP instance again
            uHttpClientClose(pContext);

        } else {
            uPortLog("Unable to create HTTP instance!\n");
        }

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
    EXAMPLE_FINAL_STATE((pContext == NULL) || ((statusCode == 200) &&
                                               (memcmp(buffer, gpMyData, strlen(gpMyData)) == 0)));
#endif
}

// End of file
