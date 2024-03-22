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

/** @brief This example demonstrates how to use the common uGeofence
 * API with a GNSS chip  The same common uGeofence API may be used
 * with cellular (using CellLocate for position) or with Wi-Fi
 * (using Google, Skyhook or Here for position).
 *
 * IMPORTANT: you MUST pass the conditional compilation flag
 * U_CFG_GEOFENCE into your build for this example to do anything
 * useful.
 *
 * The choice of module and the choice of platform on which this
 * code runs is made at build time, see the README.md for
 * instructions.
 */

#ifdef U_CFG_TEST_GNSS_MODULE_TYPE

// For LLONG_MIN
# include "limits.h"

// Bring in all of the ubxlib public header files
# include "ubxlib.h"

// Bring in the application settings
# include "u_cfg_app_platform_specific.h"

# ifndef U_CFG_DISABLE_TEST_AUTOMATION
// This purely for internal u-blox testing
#  include "u_cfg_test_platform_specific.h"
# endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

// The latitude of our test geofence in degrees times ten to the
// power nine (which are the units the geofence API uses)
#define MY_LATITUDE_X1E9 52222565519LL

// The longitude of our test geofence in degrees times ten to the
// power nine (which are the units the geofence API uses)
#define MY_LONGITUDE_X1E9 -74404134LL

// For u-blox internal testing only
# ifdef U_PORT_TEST_ASSERT
#  define EXAMPLE_FINAL_STATE(x) U_PORT_TEST_ASSERT(x);
# else
#  define EXAMPLE_FINAL_STATE(x)
# endif

# ifndef U_PORT_TEST_FUNCTION
#  error if you are not using the unit test framework to run this code you must ensure that the platform clocks/RTOS are set up and either define U_PORT_TEST_FUNCTION yourself or replace it as necessary.
# endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

// ZEPHYR USERS may prefer to set the device and network
// configuration from their device tree, rather than in this C
// code: see /port/platform/zephyr/README.md for instructions on
// how to do that.

// GNSS configuration.
// Set U_CFG_TEST_GNSS_MODULE_TYPE to your module type,
// chosen from the values in gnss/api/u_gnss_module_type.h
//
// Note that the pin numbers are those of the MCU: if you
// are using an MCU inside a u-blox module the IO pin numbering
// for the module is likely different to that of the MCU: check
// the data sheet for the module to determine the mapping.

#if ((U_CFG_APP_GNSS_UART >= 0) || (U_CFG_APP_GNSS_I2C >= 0) || (U_CFG_APP_GNSS_SPI >= 0))
// DEVICE i.e. module/chip configuration: in this case a GNSS
// module connected via UART or I2C or SPI
static const uDeviceCfg_t gDeviceCfg = {
    .deviceType = U_DEVICE_TYPE_GNSS,
    .deviceCfg = {
        .cfgGnss = {
            .moduleType = U_CFG_TEST_GNSS_MODULE_TYPE,
            .pinEnablePower = U_CFG_APP_PIN_GNSS_ENABLE_POWER,
            .pinDataReady = -1, // Not used
            // There is an additional field here:
            // "i2cAddress", which we do NOT set,
            // we allow the compiler to set it to 0
            // and all will be fine. You may set the
            // field to the I2C address of your GNSS
            // device if you have modified the I2C
            // address of your GNSS device to something
            // other than the default value of 0x42,
            // for example:
            // .i2cAddress = 0x43
        },
    },
#  if (U_CFG_APP_GNSS_I2C >= 0)
    .transportType = U_DEVICE_TRANSPORT_TYPE_I2C,
    .transportCfg = {
        .cfgI2c = {
            .i2c = U_CFG_APP_GNSS_I2C,
            .pinSda = U_CFG_APP_PIN_GNSS_SDA,  // Use -1 if on Zephyr or Linux
            .pinScl = U_CFG_APP_PIN_GNSS_SCL   // Use -1 if on Zephyr or Linux
            // There are three additional fields here,
            // "clockHertz", "alreadyOpen" and
            // "maxSegmentSize", which we do not set,
            // we allow the compiler to set them to 0
            // and all will be fine.
            // You may set clockHertz if you want the
            // I2C bus to use a different clock frequency
            // to the default of
            // #U_PORT_I2C_CLOCK_FREQUENCY_HERTZ, for
            // example:
            // .clockHertz = 400000
            // You may set alreadyOpen to true if you
            // are already using this I2C HW block,
            // with the native platform APIs,
            // elsewhere in your application code,
            // and you would like the ubxlib code
            // to use the I2C HW block WITHOUT
            // [re]configuring it, for example:
            // .alreadyOpen = true
            // if alreadyOpen is set to true then
            // pinSda, pinScl and clockHertz will
            // be ignored.
            // You may set maxSegmentSize if the I2C
            // HW you are using has a size limitation
            // (e.g. nRF52832 does); any I2C transfer
            // greater than this size will be split
            // into N transfers smaller than this size.
        },
    },
# elif (U_CFG_APP_GNSS_SPI >= 0)
    .transportType = U_DEVICE_TRANSPORT_TYPE_SPI,
    .transportCfg = {
        .cfgSpi = {
            .spi = U_CFG_APP_GNSS_SPI,
            .pinMosi = U_CFG_APP_PIN_GNSS_SPI_MOSI,  // Use -1 if on Zephyr or Linux
            .pinMiso = U_CFG_APP_PIN_GNSS_SPI_MISO,  // Use -1 if on Zephyr or Linux
            .pinClk = U_CFG_APP_PIN_GNSS_SPI_CLK,    // Use -1 if on Zephyr or Linux
            // Note: Zephyr users may find it more natural to use
            // .device = U_COMMON_SPI_CONTROLLER_DEVICE_INDEX_DEFAULTS(x)
            // instead of the below, where x is the index of a `cs-gpios`
            // entry that has already been defined for this SPI block in
            // their Zephyr device tree.  For instance, if this SPI block
            // in the device tree contained:
            //     cs-gpios = <&gpio0 2 GPIO_ACTIVE_LOW>,
            //                <&gpio1 14 GPIO_ACTIVE_LOW>;
            // then:
            // .device = U_COMMON_SPI_CONTROLLER_DEVICE_INDEX_DEFAULTS(1)
            // would use pin 14 of port GPIO 1 as the chip select.
            .device = U_COMMON_SPI_CONTROLLER_DEVICE_DEFAULTS(U_CFG_APP_PIN_GNSS_SPI_SELECT)
            // There is an additional field here,
            // "maxSegmentSize", which we do not set,
            // we allow the compiler to set it to 0
            // and all will be fine.
            // You may set maxSegmentSize if the SPI
            // HW you are using has a size limitation
            // (e.g. nRF52832 does); any SPI transfer
            // greater than this size will be split
            // into N transfers smaller than this size.
        },
    },
#  else
    .transportType = U_DEVICE_TRANSPORT_TYPE_UART,
    .transportCfg = {
        .cfgUart = {
            .uart = U_CFG_APP_GNSS_UART,
            .baudRate = U_GNSS_UART_BAUD_RATE, /* Use 0 to try all possible baud rates
                                                  and find the correct one. */
            .pinTxd = U_CFG_APP_PIN_GNSS_TXD,  // Use -1 if on Zephyr or Linux or Windows
            .pinRxd = U_CFG_APP_PIN_GNSS_RXD,  // Use -1 if on Zephyr or Linux or Windows
            .pinCts = U_CFG_APP_PIN_GNSS_CTS,  // Use -1 if on Zephyr
            .pinRts = U_CFG_APP_PIN_GNSS_RTS,  // Use -1 if on Zephyr
#ifdef U_CFG_APP_UART_PREFIX
            .pPrefix = U_PORT_STRINGIFY_QUOTED(U_CFG_APP_UART_PREFIX) // Relevant for Linux only
#else
            .pPrefix = NULL
#endif
        },
    },
#  endif
};
# else
static const uDeviceCfg_t gDeviceCfg = {.deviceType = U_DEVICE_TYPE_NONE};
# endif

// Count of the number of position fixes received
static size_t gPositionCount = 0;

// Count of the number of times the geofence callback is called
static size_t gGoefenceCount = 0;

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

// Callback for position.
static void posCallback(uDeviceHandle_t gnssHandle,
                        int32_t errorCode,
                        int32_t latitudeX1e7,
                        int32_t longitudeX1e7,
                        int32_t altitudeMillimetres,
                        int32_t radiusMillimetres,
                        int32_t speedMillimetresPerSecond,
                        int32_t svs,
                        int64_t timeUtc)
{
    char prefix[2] = {0};
    int32_t whole[2] = {0};
    int32_t fraction[2] = {0};

    // Not using these, just keep the compiler happy
    (void) gnssHandle;
    (void) altitudeMillimetres;
    (void) radiusMillimetres;
    (void) speedMillimetresPerSecond;
    (void) svs;
    (void) timeUtc;

    if (errorCode == 0) {
        prefix[0] = latLongToBits(longitudeX1e7, &(whole[0]), &(fraction[0]));
        prefix[1] = latLongToBits(latitudeX1e7, &(whole[1]), &(fraction[1]));
        uPortLog("I am here: https://maps.google.com/?q=%c%d.%07d,%c%d.%07d\n",
                 prefix[1], whole[1], fraction[1], prefix[0], whole[0], fraction[0]);
        gPositionCount++;
    }
}

// Callback for the geofence.
static void geofenceCallback(uDeviceHandle_t gnssHandle,
                             const void *pFence, const char *pNameStr,
                             uGeofencePositionState_t positionState,
                             int64_t latitudeX1e9,
                             int64_t longitudeX1e9,
                             int32_t altitudeMillimetres,
                             int32_t radiusMillimetres,
                             int32_t altitudeUncertaintyMillimetres,
                             int64_t distanceMillimetres,
                             void *pCallbackParam)
{
    char prefix[2] = {0};
    int32_t whole[2] = {0};
    int32_t fraction[2] = {0};

    // Not using these, just keep the compiler happy
    (void) gnssHandle;
    (void) pFence;
    (void) altitudeMillimetres;
    (void) radiusMillimetres;
    (void) altitudeUncertaintyMillimetres;
    (void) distanceMillimetres;
    (void) pCallbackParam;

    if (positionState != U_GEOFENCE_POSITION_STATE_NONE) {
        prefix[0] = latLongToBits((int32_t) (longitudeX1e9 / 100), &(whole[0]), &(fraction[0]));
        prefix[1] = latLongToBits((int32_t) (latitudeX1e9 / 100), &(whole[1]), &(fraction[1]));
        uPortLog("https://maps.google.com/?q=%c%d.%07d,%c%d.%07d is %s \"%s\".\n",
                 prefix[1], whole[1], fraction[1], prefix[0], whole[0], fraction[0],
                 positionState == U_GEOFENCE_POSITION_STATE_INSIDE ? "inside" : "outside",
                 pNameStr != NULL ? pNameStr : "NULL");
        gGoefenceCount++;
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: THE EXAMPLE
 * -------------------------------------------------------------- */

// The entry point, main(): before this is called the system
// clocks must have been started and the RTOS must be running;
// we are in task space.
U_PORT_TEST_FUNCTION("[example]", "exampleGnssGeofence")
{
    uDeviceHandle_t devHandle = NULL;
    int32_t returnCode;
    uGeofence_t *pFence = NULL;
    int32_t guardCount = 0;

    // Initialise the APIs we will need
    uPortInit();
    uPortI2cInit(); // You only need this if an I2C interface is used
    uPortSpiInit(); // You only need this if an SPI interface is used
    uDeviceInit();

    // Create a geofence: a polygon centred around MY_LATITUDE_X1E9/
    // MY_LONGITUDE_X1E9 with each side about 100 metres in length;
    // 0.00044 degrees latitude (so 440000 when multiplied by ten
    // to the power nine) is about 50 metres and, at this
    // latitude, 0.00075 degrees longitude (so 750000 when multiplied
    // by ten to the power nine) is about 50 metres.
    pFence = pUGeofenceCreate("my test geofence");
    // Top right-hand corner
    uGeofenceAddVertex(pFence, MY_LATITUDE_X1E9 + 440000,
                       MY_LONGITUDE_X1E9 + 750000, false);
    // Bottom right-hand corner
    uGeofenceAddVertex(pFence, MY_LATITUDE_X1E9 - 440000,
                       MY_LONGITUDE_X1E9 + 750000, false);
    // Bottom left-hand corner
    uGeofenceAddVertex(pFence, MY_LATITUDE_X1E9 - 440000,
                       MY_LONGITUDE_X1E9 - 750000, false);
    // Top left-hand corner
    uGeofenceAddVertex(pFence, MY_LATITUDE_X1E9 + 440000,
                       MY_LONGITUDE_X1E9 - 750000, false);

    // It is also possible to add circles, additional polygons,
    // and add altitude limits to the geofence

    // If you like, you can test the geofence now by calling
    // uGeofenceTest() with a position to see the outcome

    // With our geofence ready to go, we can open the device.
    // In this case we are applying it to a GNSS device but the
    // geofence API is common and so the same geofence could be
    // applied to a cellular device (see the uCellGeofence API)
    // or a Wi-Fi device (see the uWifiGeofence API).
    returnCode = uDeviceOpen(&gDeviceCfg, &devHandle);
    uPortLog("Opened device with return code %d.\n", returnCode);

    if (returnCode == 0) {
        // Since we are not using the other common APIs with
        // this GNSS device we do not need to call
        // uNetworkInteraceUp()/uNetworkInteraceDown(); you WOULD
        // call those if this were a cellular or Wi-Fi device.

        // Apply the geofence to the device: you may create
        // multiple fences and apply them to the same GNSS
        // instance, and you may apply the same geofence to
        // as many instances (GNSS, cellular or Wifi) as you
        // like.  The only limit is heap memory and processing
        // power (since trigonometric maths (cos(), sin(), etc.)
        // is required)
        uGnssGeofenceApply(devHandle, pFence);

        // When we set the callback we set the type of test
        // it is to make against the geofence: in this case we
        // do an "inside" check with "pessimistic" set to
        // true.  This means that if the radius of position
        // (i.e. the horizontal uncertainty of the position) is,
        // say, 10 metres, and we are within the geofence but by
        // only, say, 9 metres, then geofenceCallback() will
        // be called with the result "outside", because we
        // are being pessimistic about the "inside" check.
        uGnssGeofenceSetCallback(devHandle, U_GEOFENCE_TEST_TYPE_INSIDE,
                                 true, geofenceCallback, NULL);

        // Start to get position
        uPortLog("Starting position stream.\n");
        returnCode = uGnssPosGetStreamedStart(devHandle,
                                              U_GNSS_POS_STREAMED_PERIOD_DEFAULT_MS,
                                              posCallback);
        if (returnCode == 0) {
            // geofenceCallback() will now be called with
            // the outcome of our test for each position fix
            uPortLog("Waiting for a few position fixes.\n");
            while ((gPositionCount < 5) && (guardCount < 60)) {
                uPortTaskBlock(1000);
                guardCount++;
            }
            // Stop getting position
            uGnssPosGetStreamedStop(devHandle);

        } else {
            uPortLog("Unable to start position stream!\n");
        }

        // Remove [all] geofences from the GNSS instance
        uGnssGeofenceRemove(devHandle, NULL);

        // Close the device
        // Note: we don't power the device down here in order
        // to speed up testing; you may prefer to power it off
        // by setting the second parameter to true.
        uDeviceClose(devHandle, false);

    } else {
        uPortLog("Unable to open GNSS!\n");
    }

    // Free the geofence once more
    uGeofenceFree(pFence);

    // Tidy up
    uDeviceDeinit();
    uPortSpiDeinit(); // You only need this if an SPI interface is used
    uPortI2cDeinit(); // You only need this if an I2C interface is used
    uPortDeinit();

    uPortLog("Done.\n");

#if defined(U_CFG_GEOFENCE) && ((U_CFG_APP_GNSS_UART >= 0) || (U_CFG_APP_GNSS_I2C >= 0) || (U_CFG_APP_GNSS_SPI >= 0))
    // For u-blox internal testing only
    // This clears up a mutex that would NOT normally be cleared up
    // (for thread-safety reasons); we only do it during testing
    // so that the memory sums add up
    uGeofenceCleanUp();
    EXAMPLE_FINAL_STATE(((gPositionCount > 0) && (gGoefenceCount == gPositionCount) &&
                         (returnCode == 0)) || (returnCode == U_ERROR_COMMON_NOT_SUPPORTED));
# endif
}

#endif // #ifdef U_CFG_TEST_GNSS_MODULE_TYPE

// End of file
