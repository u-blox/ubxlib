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

/** @brief This example demonstrates how to bring up a GNSS network
 * and then perform a location fix.
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

// For the GNSS module and interface types
#include "u_gnss_module_type.h"
#include "u_gnss_type.h"

// For the network API
#include "u_network.h"
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

// GNSS network configuration:
// Set U_CFG_TEST_GNSS_MODULE_TYPE to your module type,
// chosen from the values in gnss/api/u_gnss_module_type.h
#ifdef U_CFG_TEST_GNSS_MODULE_TYPE
static const uNetworkConfigurationGnss_t gConfig = {U_NETWORK_TYPE_GNSS,
                                                    U_GNSS_MODULE_TYPE_M8,
                                                    /* Note that the pin numbers
                                                       used here are those of the MCU:
                                                       if you are using an MCU inside
                                                       a u-blox module the IO pin numbering
                                                       for the module is likely different
                                                       to that from the MCU: check the data
                                                       sheet for the module to determine
                                                       the mapping. */
                                                    U_CFG_APP_PIN_GNSS_ENABLE_POWER,
                                                    /* Connecton is UART. */
                                                    U_GNSS_TRANSPORT_UBX_UART,
                                                    U_CFG_APP_GNSS_UART,
                                                    U_CFG_APP_PIN_GNSS_TXD,
                                                    U_CFG_APP_PIN_GNSS_RXD,
                                                    U_CFG_APP_PIN_GNSS_CTS,
                                                    U_CFG_APP_PIN_GNSS_RTS,
                                                    0, -1, -1
                                                   };
#else
static const uNetworkConfigurationGnss_t gConfig = {U_NETWORK_TYPE_NONE};
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

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: THE EXAMPLE
 * -------------------------------------------------------------- */

// The entry point, main(): before this is called the system
// clocks must have been started and the RTOS must be running;
// we are in task space.
U_PORT_TEST_FUNCTION("[example]", "exampleLocGnss")
{
    int32_t networkHandle;
    uLocation_t location;
    // Set an out of range value so that we can test it later
    location.tickTimeMs = -1;
    int32_t whole;
    int32_t fraction;

    // Initialise the APIs we will need
    uPortInit();
    uNetworkInit();

    // Add a network instance of type GNSS
    networkHandle = uNetworkAdd(U_NETWORK_TYPE_GNSS,
                                (void *) &gConfig);
    uPortLog("Added network with handle %d.\n", networkHandle);

    // You may configure GNSS as required here
    // here using any of the GNSS API calls.

    // Bring up the GNSS network layer
    uPortLog("Bringing up GNSS...\n");
    if (uNetworkUp(networkHandle) == 0) {

        // Get location
        if (uLocationGet(networkHandle, U_LOCATION_TYPE_GNSS,
                         NULL, NULL, &location, NULL) == 0) {
            uPortLog("I am here: https://maps.google.com/?q=%c%d.%07d/%c%d.%07d\n",
                     latLongToBits(location.latitudeX1e7, &whole, &fraction),
                     whole, fraction,
                     latLongToBits(location.longitudeX1e7, &whole, &fraction),
                     whole, fraction);
        } else {
            uPortLog("Unable to get a location fix!\n");
        }

        // When finished with the GNSS network layer
        uPortLog("Taking down GNSS...\n");
        uNetworkDown(networkHandle);
    } else {
        uPortLog("Unable to bring up GNSS!\n");
    }

    // Calling these will also deallocate the network handle
    uNetworkDeinit();
    uPortDeinit();

    uPortLog("Done.\n");

#ifdef U_CFG_TEST_GNSS_MODULE_TYPE
# if 0 // Commenting out for now as the location API is not done yet
    // For u-blox internal testing only
    EXAMPLE_FINAL_STATE(location.tickTimeMs > 0);
# endif
#endif
}

// End of file
