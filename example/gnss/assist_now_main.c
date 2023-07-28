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

/** @brief This example demonstrates how to use the u-blox AssistNow
 * services to improve the time to first fix of your GNSS device; it may
 * be used where you have a GNSS device connected directly to this MCU
 * (so not connected via an intermediate cellular module).
 *
 * The choice of modules and the choice of platform on which this
 * code runs is made at build time, see the README.md for
 * instructions.
 */

#if defined(U_CFG_TEST_GNSS_MODULE_TYPE) && defined(U_CFG_APP_GNSS_ASSIST_NOW_AUTHENTICATION_TOKEN) && defined(U_CFG_TEST_GNSS_ASSIST_NOW)

// Bring in all of the ubxlib public header files
# include "ubxlib.h"

// Bring in the application settings
# include "u_cfg_app_platform_specific.h"

# ifndef U_CFG_DISABLE_TEST_AUTOMATION
// This purely for internal u-blox testing
# include "u_cfg_test_platform_specific.h"
# endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

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

// GNSS configuration.
//
// Set U_CFG_TEST_GNSS_MODULE_TYPE to your module type,
// chosen from the values in gnss/api/u_gnss_module_type.h
//
// Note that the pin numbers are those of the MCU: if you
// are using an MCU inside a u-blox module the IO pin numbering
// for the module is likely different that from the MCU: check
// the data sheet for the module to determine the mapping.

# if ((U_CFG_APP_GNSS_UART >= 0) || (U_CFG_APP_GNSS_I2C >= 0) || (U_CFG_APP_GNSS_SPI >= 0))
// DEVICE i.e. module/chip configuration: in this case a GNSS
// module connected via UART or I2C or SPI
static const uDeviceCfg_t gGnssDeviceCfg = {
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
            .pinSda = U_CFG_APP_PIN_GNSS_SDA,
            .pinScl = U_CFG_APP_PIN_GNSS_SCL
            // There two additional fields here
            // "clockHertz" amd "alreadyOpen", which
            // we do NOT set, we allow the compiler
            // to set them to 0 and all will be fine.
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
        },
    },
# elif (U_CFG_APP_GNSS_SPI >= 0)
    .transportType = U_DEVICE_TRANSPORT_TYPE_SPI,
    .transportCfg = {
        .cfgSpi = {
            .spi = U_CFG_APP_GNSS_SPI,
            .pinMosi = U_CFG_APP_PIN_GNSS_SPI_MOSI,
            .pinMiso = U_CFG_APP_PIN_GNSS_SPI_MISO,
            .pinClk = U_CFG_APP_PIN_GNSS_SPI_CLK,
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
        },
    },
#  else
    .transportType = U_DEVICE_TRANSPORT_TYPE_UART,
    .transportCfg = {
        .cfgUart = {
            .uart = U_CFG_APP_GNSS_UART,
            .baudRate = U_GNSS_UART_BAUD_RATE,
            .pinTxd = U_CFG_APP_PIN_GNSS_TXD,
            .pinRxd = U_CFG_APP_PIN_GNSS_RXD,
            .pinCts = U_CFG_APP_PIN_GNSS_CTS,
            .pinRts = U_CFG_APP_PIN_GNSS_RTS,
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
static const uDeviceCfg_t gGnssDeviceCfg = {.deviceType = U_DEVICE_TYPE_NONE};
# endif

// Configuration of the module to use for HTTP transfers with
// the AssistNow service.
//
# ifdef U_CFG_TEST_CELL_MODULE_TYPE

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
static const uDeviceCfg_t gHttpDeviceCfg = {
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
// NETWORK configuration for cellular
static const uNetworkCfgCell_t gHttpNetworkCfg = {
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
static const uNetworkType_t gHttpNetType = U_NETWORK_TYPE_CELL;
# else
// No module available - set some dummy values to make test system happy
static const uDeviceCfg_t gHttpDeviceCfg = {.deviceType = U_DEVICE_TYPE_NONE};
static const uNetworkCfgCell_t gHttpNetworkCfg = {.type = U_NETWORK_TYPE_NONE};
static const uNetworkType_t gHttpNetType = U_NETWORK_TYPE_CELL;
# endif

// Count of the number of position fixes received
static size_t gPositionCount = 0;

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

// Print out the position contained in a UBX-NAV_PVT message
static void printPosition(const char *pBuffer, size_t length)
{
    char prefix[2] = {0};
    int32_t whole[2] = {0};
    int32_t fraction[2] = {0};
    int32_t longitudeX1e7;
    int32_t latitudeX1e7;

    if ((length >= 32) && (*(pBuffer + 21) & 0x01)) {
        longitudeX1e7 =  uUbxProtocolUint32Decode(pBuffer + 24);
        latitudeX1e7 = uUbxProtocolUint32Decode(pBuffer + 28);
        prefix[0] = latLongToBits(longitudeX1e7, &(whole[0]), &(fraction[0]));
        prefix[1] = latLongToBits(latitudeX1e7, &(whole[1]), &(fraction[1]));
        uPortLog("I am here: https://maps.google.com/?q=%c%d.%07d,%c%d.%07d\n",
                 prefix[1], whole[1], fraction[1], prefix[0], whole[0], fraction[0]);
    }
}

// Callback for position reception.
static void positionCallback(uDeviceHandle_t gnssHandle,
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

// Callback for progress indications when downloading to the GNSS device.
static bool progressCallback(uDeviceHandle_t devHandle, int32_t errorCode,
                             size_t blocksTotal, size_t blocksSent,
                             void *pCallbackParam)
{
    int32_t percentage = blocksSent * 100 / blocksTotal;

    // Not using these, just keep the compiler happy
    (void) devHandle;
    (void) pCallbackParam;

    if (errorCode == 0) {
        uPortLog("Download %d%% complete.\n", percentage);
    }

    return true;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: THE EXAMPLE
 * -------------------------------------------------------------- */

// The entry point, main(): before this is called the system
// clocks must have been started and the RTOS must be running;
// we are in task space.
U_PORT_TEST_FUNCTION("[example]", "exampleGnssAssistNow")
{
    uDeviceHandle_t gnssDevHandle = NULL;
    uDeviceHandle_t httpDevHandle = NULL;
    uGnssMgaOfflineRequest_t request = U_GNSS_MGA_OFFLINE_REQUEST_DEFAULTS;
    uHttpClientContext_t *pHttpContext = NULL;
    uHttpClientConnection_t httpConnection = U_HTTP_CLIENT_CONNECTION_DEFAULT;
    uSecurityTlsSettings_t tlsSettings = U_SECURITY_TLS_SETTINGS_DEFAULT;
    char requestBuffer[128];
    size_t responseBufferSize = 5 * 1024;
    char *pResponseBuffer = (char *) pUPortMalloc(responseBufferSize);
    int64_t timeUtcMilliseconds;
    int32_t returnCode;
    int32_t guardCount = 0;

    // Initialise the APIs we will need
    uPortInit();
    uPortI2cInit(); // You only need this if an I2C interface is used
    uPortSpiInit(); // You only need this if an SPI interface is used
    uDeviceInit();

    // Open the GNSS device
    returnCode = uDeviceOpen(&gGnssDeviceCfg, &gnssDevHandle);
    uPortLog("Opened GNSS device with return code %d.\n", returnCode);

    if ((returnCode == 0) && (pResponseBuffer != NULL)) {
        // Since we are not using the common APIs with the GNSS
        // device we do not need to call uNetworkInteraceUp()/
        // uNetworkInteraceDown().

        // Open the device we will be using for HTTP connectivity
        // with the AssistNow server
        returnCode = uDeviceOpen(&gHttpDeviceCfg, &httpDevHandle);
        uPortLog("Opened HTTP device with return code %d.\n", returnCode);

        if (returnCode == 0) {
            // Bring up the network interface
            uPortLog("Bringing up the network for HTTP...\n");
            if (uNetworkInterfaceUp(httpDevHandle, gHttpNetType,
                                    &gHttpNetworkCfg) == 0) {

                // Set the URL of the AssistNow server; here we use
                // the AssistNow Offline server, for the longer term
                // data you might need if you only had sporadic (e.g.
                // every few days) access to the internet.  If your
                // device has constant connectivity with the internet
                // then you may prefer to use the U_GNSS_MGA_HTTP_SERVER_ONLINE
                // service with uGnssMgaOnlineRequest_t.
                httpConnection.pServerName = U_GNSS_MGA_HTTP_SERVER_OFFLINE;
                // The AssistNow Offline server requires the server name
                // indication field to be set (to exactly the same URL)
                // in the security settings; FYI there is no harm in also
                // setting it for the AssistNow Online case
                tlsSettings.pSni = U_GNSS_MGA_HTTP_SERVER_OFFLINE;

                // Create the HTTP instance
                pHttpContext = pUHttpClientOpen(httpDevHandle, &httpConnection, &tlsSettings);
                if (pHttpContext != NULL) {

                    // We will leave the request at defaults, which will
                    // obtain the satellite data for today, just for the
                    // satellites of GPS
                    // Note also that a complete response is essential or the
                    // GNSS device will reject the data, hence you do not want
                    // to ask for too much (or you may need to increase the size
                    // of pResponseBuffer)

                    // We need to add our authentication token for the service;
                    // an evaluation token may be obtained from
                    // https://www.u-blox.com/en/assistnow-service-evaluation-token-request-form
                    // or from your Thingstream portal
                    // https://portal.thingstream.io/app/location-services
                    request.pTokenStr = U_PORT_STRINGIFY_QUOTED(U_CFG_APP_GNSS_ASSIST_NOW_AUTHENTICATION_TOKEN);

                    // Encode the AssistNow Offline string we will send in the HTTP GET request
                    returnCode = uGnssMgaOfflineRequestEncode(&request, requestBuffer, sizeof(requestBuffer));
                    if (returnCode >= 0) {
                        uPortLog("HTTP GET request will be \"%s\".\n", requestBuffer);
                        // Send the HTTP GET request to the AssistNow server
                        returnCode  = uHttpClientGetRequest(pHttpContext,
                                                            requestBuffer,
                                                            pResponseBuffer,
                                                            &responseBufferSize,
                                                            NULL);
                        if (returnCode == 200) {
                            uPortLog("HTTP GET response received, %d byte(s).\n", responseBufferSize);
                            // A valid response will always begin with the hex character 0xB5; if this
                            // is not the case the server may have been unable to process the request
                            // and it may have returned a string explaining what it didn't like: we
                            // can print that out
                            if ((responseBufferSize > 1) && (*pResponseBuffer != 0xB5)) {
                                uPortLog("Server said \"%*s\".\n", responseBufferSize, pResponseBuffer);
                            } else {
                                // For an AssistNow Offline request to be useful, the GNSS
                                // chip needs to also know the time, which we can get from
                                // the cellular network
                                timeUtcMilliseconds = uCellInfoGetTimeUtc(httpDevHandle);
                                if (timeUtcMilliseconds >= 0) {
                                    uPortLog("UTC timestamp according to cellular is %d.\n", (int32_t) timeUtcMilliseconds);
                                    timeUtcMilliseconds *= 1000;
                                    // Finally, send the data we have received from the server to
                                    // the GNSS device; we only send today's data as that's all
                                    // the GNSS device will need and it saves time
                                    returnCode = uGnssMgaResponseSend(gnssDevHandle, timeUtcMilliseconds,
                                                                      10000,  // Assume cellular time is quite innaccurate
                                                                      U_GNSS_MGA_SEND_OFFLINE_TODAYS,
                                                                      U_GNSS_MGA_FLOW_CONTROL_WAIT,
                                                                      pResponseBuffer, responseBufferSize,
                                                                      progressCallback, NULL);
                                    if (returnCode == 0) {
                                        uPortLog("AssistNow data downloaded to GNSS.\n");
                                        // That's it really; just to finish the example off,
                                        // do some position readings
                                        returnCode = uGnssPosGetStreamedStart(gnssDevHandle,
                                                                              U_GNSS_POS_STREAMED_PERIOD_DEFAULT_MS,
                                                                              positionCallback);
                                        if (returnCode == 0) {
                                            uPortLog("Waiting up to 60 seconds for 5 position fixes.\n");
                                            while ((gPositionCount < 5) && (guardCount < 60)) {
                                                uPortTaskBlock(1000);
                                                guardCount++;
                                            }
                                            // Stop getting position
                                            uGnssPosGetStreamedStop(gnssDevHandle);
                                        } else {
                                            uPortLog("Unable to start position stream (%d)!\n", returnCode);
                                        }
                                    } else {
                                        uPortLog("Unable to download to the GNSS device (%d)!\n", returnCode);
                                    }
                                } else {
                                    uPortLog("Unable to get the time from the cellular network (%d)!\n", timeUtcMilliseconds);
                                }
                            }
                        } else {
                            uPortLog("HTTP GET request failed (%d)!\n", returnCode);
                        }
                    } else {
                        uPortLog("Unable to encode AssistNow Online request (%d)!\n", returnCode);
                    }

                    // Close the HTTP instance again
                    uHttpClientClose(pHttpContext);

                } else {
                    uPortLog("Unable to create HTTP instance!\n");
                }

                // When finished with the network layer
                uPortLog("Taking down network...\n");
                uNetworkInterfaceDown(httpDevHandle, gHttpNetType);
            } else {
                uPortLog("Unable to bring up the network!\n");
            }

            // Close the device we are using for HTTP connectivity
            // Note: we don't power the device down here in order
            // to speed up testing; you may prefer to power it off
            // by setting the second parameter to true.
            uDeviceClose(httpDevHandle, false);

        } else {
            uPortLog("Unable to bring up the device!\n");
        }

        // Close the GNSS device
        // Note: we don't power the device down here in order
        // to speed up testing; you may prefer to power it off
        // by setting the second parameter to true.
        uDeviceClose(gnssDevHandle, false);

    } else {
        uPortLog("Unable to open GNSS!\n");
    }

    // Tidy up
    uDeviceDeinit();
    uPortSpiDeinit(); // You only need this if an SPI interface is used
    uPortI2cDeinit(); // You only need this if an I2C interface is used
    uPortDeinit();

    uPortLog("Done.\n");

    uPortFree(pResponseBuffer);

# if defined(U_CFG_TEST_CELL_MODULE_TYPE) && \
     ((U_CFG_APP_GNSS_UART >= 0) || (U_CFG_APP_GNSS_I2C >= 0) || (U_CFG_APP_GNSS_SPI >= 0))
    // For u-blox internal testing only
    EXAMPLE_FINAL_STATE((gPositionCount > 0) && (returnCode == 0));
# endif
}

#endif // #if defined(U_CFG_TEST_GNSS_MODULE_TYPE) && defined(U_CFG_APP_GNSS_ASSIST_NOW_AUTHENTICATION_TOKEN) && defined(U_CFG_TEST_GNSS_ASSIST_NOW)

// End of file
