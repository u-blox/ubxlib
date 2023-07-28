/*
 * Copyright 2019-2023 u-blox
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

/** @brief This example demonstrates how to bring up a cellular module
 * and then use a GNSS module attached to the cellular module to perform
 * a location fix continuously, i.e. this example ONLY applies if your
 * GNSS module is attached to the cellular module and NOT to this MCU.
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

#if defined(U_CFG_TEST_CELL_MODULE_TYPE) && defined(U_CFG_TEST_GNSS_MODULE_TYPE) && (U_CFG_APP_GNSS_UART < 0) && (U_CFG_APP_GNSS_I2C < 0) && (U_CFG_APP_GNSS_SPI < 0)
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
// NETWORK configuration for GNSS
static const uNetworkCfgGnss_t gNetworkCfg = {
    .type = U_NETWORK_TYPE_GNSS,
    .moduleType = U_CFG_TEST_GNSS_MODULE_TYPE,
    .devicePinPwr = U_CFG_APP_CELL_PIN_GNSS_POWER, // The pins of the *cellular* *module* that are connected
    .devicePinDataReady = U_CFG_APP_CELL_PIN_GNSS_DATA_READY // to the GNSS chip's power and Data Ready lines
};
#else
static const uDeviceCfg_t gDeviceCfg = {.deviceType = U_DEVICE_TYPE_NONE};
static const uNetworkCfgGnss_t gNetworkCfg = {.type = U_NETWORK_TYPE_NONE};
#endif

// Count of the number of location fixes received
static size_t gLocationCount = 0;

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

// Callback function to receive location.
static void callback(uDeviceHandle_t devHandle,
                     int32_t errorCode,
                     const uLocation_t *pLocation)
{
    char prefix[2] = {0};
    int32_t whole[2] = {0};
    int32_t fraction[2] = {0};

    // Not used
    (void) devHandle;

    if (errorCode == 0) {
        prefix[0] = latLongToBits(pLocation->longitudeX1e7, &(whole[0]), &(fraction[0]));
        prefix[1] = latLongToBits(pLocation->latitudeX1e7, &(whole[1]), &(fraction[1]));
        uPortLog("I am here: https://maps.google.com/?q=%c%d.%07d,%c%d.%07d\n",
                 prefix[1], whole[1], fraction[1], prefix[0], whole[0], fraction[0]);
        gLocationCount++;
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: THE EXAMPLE
 * -------------------------------------------------------------- */

// The entry point, main(): before this is called the system
// clocks must have been started and the RTOS must be running;
// we are in task space.
U_PORT_TEST_FUNCTION("[example]", "exampleLocGnssCellContinuous")
{
    uDeviceHandle_t devHandle = NULL;
    int32_t returnCode;
    int32_t guardCount = 0;

    // Initialise the APIs we will need
    uPortInit();
    uDeviceInit();

    // Open the cellular device
    returnCode = uDeviceOpen(&gDeviceCfg, &devHandle);
    uPortLog("Opened cellular device with return code %d.\n", returnCode);

    if (returnCode == 0) {
        // You may configure the cellular device as required
        // here using any of the cell API calls.

        // Note that in this example we don't bring up the cellular
        // network interface on the cellular device as we don't need
        // it; you may choose to do so of course.

        // Bring up the GNSS network layer on the cellular device
        uPortLog("Bringing up GNSS...\n");
        if (uNetworkInterfaceUp(devHandle, U_NETWORK_TYPE_GNSS,
                                &gNetworkCfg) == 0) {

            // Here you may use the GNSS API with the device handle
            // if you wish to configure the GNSS chip etc.

            // Start to get location
            uPortLog("Starting continuous location.\n");
            returnCode = uLocationGetContinuousStart(devHandle,
                                                     U_GNSS_POS_STREAMED_PERIOD_DEFAULT_MS,
                                                     U_LOCATION_TYPE_GNSS,
                                                     NULL, NULL, callback);
            if (returnCode == 0) {

                uPortLog("Waiting up to 60 seconds for 5 location fixes.\n");
                while ((gLocationCount < 5) && (guardCount < 60)) {
                    uPortTaskBlock(1000);
                    guardCount++;
                }
                // Stop getting location
                uLocationGetStop(devHandle);

            } else {
                uPortLog("Unable to start continuous location!\n");
            }

            // When finished with the GNSS network layer
            uPortLog("Taking down GNSS...\n");
            uNetworkInterfaceDown(devHandle, U_NETWORK_TYPE_GNSS);
        } else {
            uPortLog("Unable to bring up GNSS!\n");
        }

        // Close the device
        uDeviceClose(devHandle, true);
    } else {
        uPortLog("Unable to bring up the cellular device!\n");
    }

    // Tidy up
    uDeviceDeinit();
    uPortDeinit();

    uPortLog("Done.\n");
#if defined(U_CFG_TEST_CELL_MODULE_TYPE) && defined(U_CFG_TEST_GNSS_MODULE_TYPE) && (U_CFG_APP_GNSS_UART < 0) && (U_CFG_APP_GNSS_I2C < 0) && (U_CFG_APP_GNSS_SPI < 0)
    // For u-blox internal testing only
    EXAMPLE_FINAL_STATE(((gLocationCount > 0) && (returnCode == 0)) ||
                        (returnCode == U_ERROR_COMMON_NOT_SUPPORTED));
#endif
}

// End of file
