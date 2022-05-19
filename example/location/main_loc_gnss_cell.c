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

/** @brief This example demonstrates how to bring up a cellular network
 * and then use a GNSS module attached to the cellular module to perform
 * a location fix, i.e. this example ONLY applies if your GNSS module is
 * attached to the cellular module and NOT to this MCU.
 *
 * Also, it is MUCH EASIER in this case to just use Cell Locate in the
 * cellular module (see the example main_loc_cell_locate.c) which will use
 * the GNSS chip for you or will provide a cell-tower-based fix if out
 * of coverage of GNSS.
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

#ifdef U_CFG_TEST_CELL_MODULE_TYPE
// DEVICE i.e. module/chip configuration: in this case a cellular
// module connected via UART
static const uDeviceCfg_t gDeviceCfgCell = {
    .deviceType = U_DEVICE_TYPE_CELL,
    .deviceCfg = {
        .cfgCell = {
            .moduleType = U_CFG_TEST_CELL_MODULE_TYPE,
            .pSimPinCode = NULL, /* SIM pin */
            .pinEnablePower = U_CFG_APP_PIN_CELL_ENABLE_POWER,
            .pinPwrOn = U_CFG_APP_PIN_CELL_PWR_ON,
            .pinVInt = U_CFG_APP_PIN_CELL_VINT
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
};
#else
static const uDeviceCfg_t gDeviceCfgCell = {.deviceType = U_DEVICE_TYPE_NONE};
static const uNetworkCfgCell_t gNetworkCfgCell = {.type = U_NETWORK_TYPE_NONE};
#endif

// GNSS configuration.
// Set U_CFG_TEST_GNSS_MODULE_TYPE to your module type,
// chosen from the values in gnss/api/u_gnss_module_type.h

#ifdef U_CFG_TEST_GNSS_MODULE_TYPE
// DEVICE i.e. module/chip configuration: in this case a GNSS
// module connected via the AT interface of a cellular module
static uDeviceCfg_t gDeviceCfgGnss = {
    .deviceType = U_DEVICE_TYPE_GNSS,
    .deviceCfg = {
        .cfgGnss = {
            .moduleType = U_CFG_TEST_GNSS_MODULE_TYPE,
            .transportType = U_GNSS_TRANSPORT_UBX_AT, // Connection via cellular module's AT interface
            .pinGnssEnablePower = U_CFG_APP_PIN_GNSS_ENABLE_POWER,
            .devHandleAt = NULL, // Handle of the cellular interface, will be filled-in later
            .gnssAtPinPwr = U_CFG_APP_CELL_PIN_GNSS_POWER, // The pins of the *cellular* *module* that are connected
            .gnssAtPinDataReady = U_CFG_APP_CELL_PIN_GNSS_DATA_READY // to the GNSS chip's power and Data Ready lines
        },
    },
    .transportType = U_DEVICE_TRANSPORT_TYPE_NONE
};
// NETWORK configuration for GNSS; nothing to do but the type
static const uNetworkCfgGnss_t gNetworkCfgGnss = {
    .type = U_NETWORK_TYPE_GNSS
};
#else
static uDeviceCfg_t gDeviceCfgGnss = {.deviceType = U_DEVICE_TYPE_NONE};
static const uNetworkCfgGnss_t gNetworkCfgGnss = {.type = U_NETWORK_TYPE_NONE};
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
    uPortLog("I am here: https://maps.google.com/?q=%c%d.%07d/%c%d.%07d\n",
             prefixLat, wholeLat, fractionLat, prefixLong, wholeLong,
             fractionLong);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: THE EXAMPLE
 * -------------------------------------------------------------- */

// The entry point, main(): before this is called the system
// clocks must have been started and the RTOS must be running;
// we are in task space.
U_PORT_TEST_FUNCTION("[example]", "exampleLocGnssCell")
{
    uDeviceHandle_t devHandleCell = NULL;
    uDeviceHandle_t devHandleGnss = NULL;
    uLocation_t location;
    int32_t returnCode;

    // Set an out of range value so that we can test it later
    location.timeUtc = -1;

    // Initialise the APIs we will need
    uPortInit();
    uDeviceInit();

    // Open the cellular device
    returnCode = uDeviceOpen(&gDeviceCfgCell, &devHandleCell);
    uPortLog("Opened cellular device with return code %d.\n", returnCode);

    // Copy the cellular handle into the GNSS configuration
    gDeviceCfgGnss.deviceCfg.cfgGnss.devHandleAt = devHandleCell;

    // Open the GNSS device
    returnCode = uDeviceOpen(&gDeviceCfgGnss, &devHandleGnss);
    uPortLog("Opened GNSS device with return code %d.\n", returnCode);

    // You may configure the networks as required
    // here using any of the GNSS or cell API calls.

    // Bring up the cellular network layer
    uPortLog("Bringing up cellular...\n");
    if (uNetworkInterfaceUp(devHandleCell, U_NETWORK_TYPE_CELL,
                            &gNetworkCfgCell) == 0) {

        // You may use the cellular network, as normal,
        // at any time, for example connect and
        // send data etc.

        // Bring up the GNSS network layer
        uPortLog("Bringing up GNSS...\n");
        if (uNetworkInterfaceUp(devHandleGnss, U_NETWORK_TYPE_GNSS,
                                &gNetworkCfgGnss) == 0) {

            // Here you may use the GNSS API with the network handle
            // if you wish to configure the GNSS chip etc.

            // Now get location
            if (uLocationGet(devHandleGnss, U_LOCATION_TYPE_GNSS,
                             NULL, NULL, &location, NULL) == 0) {
                printLocation(location.latitudeX1e7, location.longitudeX1e7);
            } else {
                uPortLog("Unable to get a location fix!\n");
            }

            // When finished with the GNSS network layer
            uPortLog("Taking down GNSS...\n");
            uNetworkInterfaceDown(devHandleGnss, U_NETWORK_TYPE_GNSS);
        } else {
            uPortLog("Unable to bring up GNSS!\n");
        }

        // When finished with the cellular network layer
        uPortLog("Taking down cellular network...\n");
        uNetworkInterfaceDown(devHandleCell, U_NETWORK_TYPE_CELL);

    } else {
        uPortLog("Unable to bring up the cellular network!\n");
    }

    // Close the devices
    uDeviceClose(devHandleGnss);
    uDeviceClose(devHandleCell);

    // Tidy up
    uDeviceDeinit();
    uPortDeinit();

    uPortLog("Done.\n");
#if defined(U_CFG_TEST_GNSS_MODULE_TYPE) && (U_CFG_APP_GNSS_UART < 0)
    EXAMPLE_FINAL_STATE(location.timeUtc > 0);
#endif
}

// End of file
