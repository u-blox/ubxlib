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

/** @brief This example demonstrates how to use Cloud Locate.  It
 * employs a GNSS module that is connected via or is inside (the
 * SARA-R510M8S/SARA-R422M8S case) a cellular module.
 *
 * The choice of module and the choice of platform on which this
 * code runs is made at build time, see the README.md for
 * instructions.
 */

// Bring in all of the ubxlib public header files
#include "ubxlib.h"

// Bring in the application settings
#include "u_cfg_app_platform_specific.h"

#ifndef U_CFG_DISABLE_TEST_AUTOMATION
// This purely for internal u-blox testing
# include "u_cfg_test_platform_specific.h"
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

// Set the Thingstream client ID of your thing below,
// something like "device:521b5a33-2374-4547-8edc-50743c144509"
#define MY_THINGSTREAM_CLIENT_ID "TBC"

// Set the Thingstream user name of your thing below,
// something like "WF592TTWUQ18512KLU6L"
#define MY_THINGSTREAM_USERNAME "TBC"

// Set the Thingstream password of your thing below,
// something like "nsd8hsK/NSDFdgdblfmbQVXbx7jeZ/8vnsiltgty"
#define MY_THINGSTREAM_PASSWORD "TBC"

// The minimum number of satellites we need to be able to see to
// include a GNSS measurement in the data sent to Cloud Locate
#define SATELLITES_MIN 6

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

// Cellular configuration.
// Set U_CFG_TEST_CELL_MODULE_TYPE to your module type,
// chosen from the values in cell/api/u_cell_module_type.h
//
// Note that the pin numbers are those of the MCU: if you
// are using an MCU inside a u-blox module the IO pin numbering
// for the module is likely different that from the MCU: check
// the data sheet for the module to determine the mapping.

#if defined(U_CFG_TEST_CELL_MODULE_TYPE) && defined(U_CFG_TEST_GNSS_MODULE_TYPE)
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
static const uNetworkCfgCell_t gNetworkCfgCell = {
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
// NETWORK configuration for GNSS
static const uNetworkCfgGnss_t gNetworkCfgGnss = {
    .type = U_NETWORK_TYPE_GNSS,
    .moduleType = U_CFG_TEST_GNSS_MODULE_TYPE,
    .devicePinPwr = U_CFG_APP_CELL_PIN_GNSS_POWER, // The pins of the *cellular* *module* that are connected
    .devicePinDataReady = U_CFG_APP_CELL_PIN_GNSS_DATA_READY // to the GNSS chip's power and Data Ready lines
};
#else
static const uDeviceCfg_t gDeviceCfg = {.deviceType = U_DEVICE_TYPE_NONE};
static const uNetworkCfgCell_t gNetworkCfgCell = {.type = U_NETWORK_TYPE_NONE};
static const uNetworkCfgGnss_t gNetworkCfgGnss = {.type = U_NETWORK_TYPE_NONE};
#endif

#if !defined(U_CFG_APP_CLOUD_LOCATE_MQTT_CLIENT_ID) || !defined(U_CFG_TEST_CLOUD_LOCATE)
const char *gpMyThingstreamClientId = MY_THINGSTREAM_CLIENT_ID;
#else
// For u-blox internal testing only
const char *gpMyThingstreamClientId = U_PORT_STRINGIFY_QUOTED(
                                          U_CFG_APP_CLOUD_LOCATE_MQTT_CLIENT_ID);
#endif
#if !defined(U_CFG_APP_CLOUD_LOCATE_MQTT_USERNAME) || !defined(U_CFG_TEST_CLOUD_LOCATE)
const char *gpMyThingstreamUsername = MY_THINGSTREAM_USERNAME;
#else
// For u-blox internal testing only
const char *gpMyThingstreamUsername = U_PORT_STRINGIFY_QUOTED(
                                          U_CFG_APP_CLOUD_LOCATE_MQTT_USERNAME);
#endif
#if !defined(U_CFG_APP_CLOUD_LOCATE_MQTT_PASSWORD) || !defined(U_CFG_TEST_CLOUD_LOCATE)
const char *gpMyThingstreamPassword = MY_THINGSTREAM_PASSWORD;
#else
// For u-blox internal testing only
const char *gpMyThingstreamPassword = U_PORT_STRINGIFY_QUOTED(
                                          U_CFG_APP_CLOUD_LOCATE_MQTT_PASSWORD);
#endif

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Convert a lat/long into a whole number and a bit-after-the-decimal-point
// that can be printed by a version of printf() that does not support
// floating point operations, returning the prefix (either "+" or "-").
// The result should be printed with printf() format specifiers
// %c%d.%07d, e.g. something like:
//
// int32_t whole;
// int32_t fraction;
//
// printf("%c%d.%07d/%c%d.%07d", latLongToBits(latitudeX1e7, &whole, &fraction),
//                               whole, fraction,
//                               latLongToBits(longitudeX1e7, &whole, &fraction),
//                               whole, fraction);
static char latLongToBits(int32_t thingX1e7,
                          int32_t *pWhole,
                          int32_t *pFraction)
{
    char prefix = '+';

    // Deal with the sign
    if (thingX1e7 < 0) {
        thingX1e7 = -thingX1e7;
        prefix = '-';
    }
    *pWhole = thingX1e7 / 10000000;
    *pFraction = thingX1e7 % 10000000;

    return prefix;
}

// Print lat/long location as a clickable link.
static void printLocation(int32_t latitudeX1e7, int32_t longitudeX1e7)
{
    char prefixLat;
    char prefixLong;
    int32_t wholeLat;
    int32_t wholeLong;
    int32_t fractionLat;
    int32_t fractionLong;

    prefixLat = latLongToBits(latitudeX1e7, &wholeLat, &fractionLat);
    prefixLong = latLongToBits(longitudeX1e7, &wholeLong, &fractionLong);
    uPortLog("I am here: https://maps.google.com/?q=%c%d.%07d,%c%d.%07d\n",
             prefixLat, wholeLat, fractionLat, prefixLong, wholeLong,
             fractionLong);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: THE EXAMPLE
 * -------------------------------------------------------------- */

// The entry point, main(): before this is called the system
// clocks must have been started and the RTOS must be running;
// we are in task space.
U_PORT_TEST_FUNCTION("[example]", "exampleLocGnssCloudLocate")
{
    uDeviceHandle_t devHandle = NULL;
    uLocationAssist_t locationAssist = U_LOCATION_ASSIST_DEFAULTS;
    uMqttClientConnection_t mqttConnection = U_MQTT_CLIENT_CONNECTION_DEFAULT;
    uLocation_t location;
    int32_t returnCode;

    // Set an out of range value so that we can test it later
    location.timeUtc = -1;

    // Initialise the APIs we will need
    uPortInit();
    uDeviceInit();

    // Open the cellular device
    returnCode = uDeviceOpen(&gDeviceCfg, &devHandle);
    uPortLog("Opened cellular device with return code %d.\n", returnCode);

    // You may configure the cellular device as required
    // here using any of the cell API calls.

    // Bring up the cellular network layer
    uPortLog("Bringing up cellular...\n");
    if (uNetworkInterfaceUp(devHandle, U_NETWORK_TYPE_CELL,
                            &gNetworkCfgCell) == 0) {

        // You may use the cellular network, as normal,
        // at any time, for example connect and
        // send data etc.

        // Bring up the GNSS network layer
        uPortLog("Bringing up GNSS...\n");
        if (uNetworkInterfaceUp(devHandle, U_NETWORK_TYPE_GNSS,
                                &gNetworkCfgGnss) == 0) {

            // Here you may use the GNSS API with the device handle
            // if you wish to configure the GNSS chip etc.

            // To use Cloud Locate we need to populate the
            // locationAssist structure passed to the location
            // API to tell it what to do.

            // Set the number of satellites that GNSS must be able
            // to see before it is worth including that measurement
            // in the estimate
            locationAssist.svsThreshold = SATELLITES_MIN;
            // Cloud Locate requires an MQTT Now connection to a thing
            // in your Thingstream account that is enabled for the
            // u-blox Cloud Locate service
            locationAssist.pMqttClientContext = pUMqttClientOpen(devHandle, NULL);
            if (locationAssist.pMqttClientContext != NULL) {
                // Populate the MQTT connection structure with the
                // credentials of your thing
                mqttConnection.pBrokerNameStr = "mqtt.thingstream.io";
                mqttConnection.pClientIdStr = gpMyThingstreamClientId;
                mqttConnection.pUserNameStr = gpMyThingstreamUsername;
                mqttConnection.pPasswordStr = gpMyThingstreamPassword;

                // Make the MQTT connection to Thingstream
                uPortLog("Connecting to Thingstream MQTT broker \"%s\"...\n",
                         mqttConnection.pBrokerNameStr);
                if (uMqttClientConnect((uMqttClientContext_t *) locationAssist.pMqttClientContext,
                                       &mqttConnection) == 0) {
                    // Note: in order to make this a self-contained example we
                    // read back our location from the Cloud Locate service by
                    // setting the locationAssist.pClientIdStr field to the client
                    // ID of your Thingstream account and passing a location structure
                    // to the uLocationGet() call; normally with Cloud Locate you
                    // would not bother with this as the point is that the cloud-side
                    // knows where the device is, the device itself does not care
                    locationAssist.pClientIdStr = gpMyThingstreamClientId;

                    // Now put the lot together by running the Cloud Locate service,
                    // giving it the location assist structure
                    if (uLocationGet(devHandle,
                                     U_LOCATION_TYPE_CLOUD_CLOUD_LOCATE,
                                     &locationAssist, NULL,
                                     &location, NULL) == 0) {
                        printLocation(location.latitudeX1e7, location.longitudeX1e7);
                    } else {
                        uPortLog("Unable to establish location!\n");
                    }

                    // When finished with the MQTT client
                    uMqttClientDisconnect((uMqttClientContext_t *) locationAssist.pMqttClientContext);
                } else {
                    uPortLog("Unable to connect to the Thingstream MQTT broker!\n");
                }

                // When finished with the MQTT context
                uMqttClientClose((uMqttClientContext_t *) locationAssist.pMqttClientContext);
            } else {
                uPortLog("Unable to create an MQTT context!\n");
            }

            // When finished with the GNSS network layer
            uPortLog("Taking down GNSS...\n");
            uNetworkInterfaceDown(devHandle, U_NETWORK_TYPE_GNSS);
        } else {
            uPortLog("Unable to bring up GNSS!\n");
        }

        // When finished with the cellular network layer
        uPortLog("Taking down cellular network...\n");
        uNetworkInterfaceDown(devHandle, U_NETWORK_TYPE_CELL);

    } else {
        uPortLog("Unable to bring up the cellular network!\n");
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
#if defined(U_CFG_TEST_CLOUD_LOCATE) && defined(U_CFG_TEST_GNSS_MODULE_TYPE) && (U_CFG_APP_GNSS_UART < 0)
    // For u-blox internal testing only
    EXAMPLE_FINAL_STATE(location.timeUtc > 0);
#endif
}

// End of file
