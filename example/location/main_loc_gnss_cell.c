/*
 * Copyright 2020 u-blox
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

// For default values for U_CFG_APP_xxx
#include "u_cfg_app_platform_specific.h"

// For the cellular module types
#include "u_cell_module_type.h"

// For the GNSS module and interface types
#include "u_gnss_module_type.h"
#include "u_gnss_type.h"

// For the network API
#include "u_network.h"
#include "u_network_config_cell.h"
#include "u_network_config_gnss.h"

// For the location API
#include "u_location.h"

#ifndef U_CFG_DISABLE_TEST_AUTOMATION
// This purely for internal u-blox testing
# include "u_cfg_test_platform_specific.h"
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

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

// GNSS network configuration:
// Set U_CFG_TEST_GNSS_MODULE_TYPE to your module type,
// chosen from the values in gnss/api/u_gnss_module_type.h
#ifdef U_CFG_TEST_GNSS_MODULE_TYPE
static uNetworkConfigurationGnss_t gConfigGnss = {U_NETWORK_TYPE_GNSS,
                                                  U_CFG_TEST_GNSS_MODULE_TYPE,
                                                  /* Note that the pin numbers
                                                     used here are those of the MCU:
                                                     if you are using an MCU inside
                                                     a u-blox module the IO pin numbering
                                                     for the module is likely different
                                                     to that from the MCU: check the data
                                                     sheet for the module to determine
                                                     the mapping. */
                                                  U_CFG_APP_PIN_GNSS_ENABLE_POWER,
                                                  /* Connection is via the cellular
                                                     module's AT interface. */
                                                  U_GNSS_TRANSPORT_UBX_AT,
                                                  /* The GNSS UART number and pins are
                                                     all irrelevant since the GNSS chip
                                                     is not connected to this MCU; it is
                                                     connected to the cellular module. */
                                                  -1, -1, -1, -1, -1,
                                                  /* The handle of the cellular interface will
                                                     be filled in later. */
                                                  0,
                                                  /* The pins of the *cellular* *module*
                                                     that are connected to the GNSS chip's
                                                     power and Data Ready lines. */
                                                  U_CFG_APP_CELL_PIN_GNSS_POWER,
                                                  U_CFG_APP_CELL_PIN_GNSS_DATA_READY
                                                  };
#else
static uNetworkConfigurationGnss_t gConfigGnss = {U_NETWORK_TYPE_NONE};
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
    int32_t networkHandleCell;
    int32_t networkHandleGnss;
    uLocation_t location;

    // Set an out of range value so that we can test it later
    location.timeUtc = -1;

    // Initialise the APIs we will need
    uPortInit();
    uNetworkInit();

    // Add a cellular network instance
    networkHandleCell = uNetworkAdd(U_NETWORK_TYPE_CELL,
                                    (void *) &gConfigCell);
    uPortLog("Added cellular network with handle %d.\n", networkHandleCell);

    // Copy the cellular handle into the GNSS configuration
    gConfigGnss.networkHandleAt = networkHandleCell;

    // Add a GNSS network instance
    networkHandleGnss = uNetworkAdd(U_NETWORK_TYPE_GNSS,
                                    (void *) &gConfigGnss);
    uPortLog("Added GNSS network with handle %d.\n", networkHandleGnss);

    // You may configure the networks as required
    // here using any of the GNSS or cell API calls.

    // Bring up the cellular network layer
    uPortLog("Bringing up cellular...\n");
    if (uNetworkUp(networkHandleCell) == 0) {

        // You may use the cellular network, as normal,
        // at any time, for example connect and
        // send data etc.

        // Bring up the GNSS network layer
        uPortLog("Bringing up GNSS...\n");
        if (uNetworkUp(networkHandleGnss) == 0) {

            // Here you may use the GNSS API with the network handle
            // if you wish to configure the GNSS chip etc.

            // Now get location
            if (uLocationGet(networkHandleGnss, U_LOCATION_TYPE_GNSS,
                             NULL, NULL, &location, NULL) == 0) {
                printLocation(location.latitudeX1e7, location.longitudeX1e7);
            } else {
                uPortLog("Unable to get a location fix!\n");
            }

            // When finished with the GNSS network layer
            uPortLog("Taking down GNSS...\n");
            uNetworkDown(networkHandleGnss);
        } else {
            uPortLog("Unable to bring up GNSS!\n");
        }

        // When finished with the cellular network layer
        uPortLog("Taking down cellular network...\n");
        uNetworkDown(networkHandleCell);

    } else {
        uPortLog("Unable to bring up the cellular network!\n");
    }

    // Calling these will also deallocate the network handles
    uNetworkDeinit();
    uPortDeinit();

    uPortLog("Done.\n");
#if defined(U_CFG_TEST_GNSS_MODULE_TYPE) && (U_CFG_APP_GNSS_UART < 0)
    EXAMPLE_FINAL_STATE(location.timeUtc > 0);
#endif
}

// End of file
