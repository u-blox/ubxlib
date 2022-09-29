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

/** @brief This example demonstrates how to exchange message of your
 * choice with a GNSS device that is directly connected to this MCU;
 * this mechanism does not currently work if your GNSS device is
 * connected via an intermediate [cellular] module.
 *
 * The choice of module and the choice of platform on which this
 * code runs is made at build time, see the README.md for
 * instructions.
 */

#include "stdlib.h" // For malloc()/free()

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

/** The size of message buffer we need: enough room for a UBX-NAV-PVT
 * message, which has a body of length 92 bytes, and any NMEA message,
 * which have a maximum size of 82 bytes.
 */
#define MY_MESSAGE_BUFFER_LENGTH  (92 + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES)

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

// GNSS configuration.
// Set U_CFG_TEST_GNSS_MODULE_TYPE to your module type,
// chosen from the values in gnss/api/u_gnss_module_type.h
//
// Note that the pin numbers are those of the MCU: if you
// are using an MCU inside a u-blox module the IO pin numbering
// for the module is likely different that from the MCU: check
// the data sheet for the module to determine the mapping.

#if defined(U_CFG_TEST_GNSS_MODULE_TYPE) && ((U_CFG_APP_GNSS_UART >= 0) || (U_CFG_APP_GNSS_I2C >= 0))
// DEVICE i.e. module/chip configuration: in this case a GNSS
// module connected via UART or I2C
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
# if (U_CFG_APP_GNSS_I2C >= 0)
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
# else
    .transportType = U_DEVICE_TRANSPORT_TYPE_UART,
    .transportCfg = {
        .cfgUart = {
            .uart = U_CFG_APP_GNSS_UART,
            .baudRate = U_GNSS_UART_BAUD_RATE,
            .pinTxd = U_CFG_APP_PIN_GNSS_TXD,
            .pinRxd = U_CFG_APP_PIN_GNSS_RXD,
            .pinCts = U_CFG_APP_PIN_GNSS_CTS,
            .pinRts = U_CFG_APP_PIN_GNSS_RTS
        },
    },
# endif
};
#else
static const uDeviceCfg_t gDeviceCfg = {.deviceType = U_DEVICE_TYPE_NONE};
#endif

// Count of messages received
static size_t gMessageCount = 0;

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

// Callback for asynchronous message reception.
static void callback(uDeviceHandle_t devHandle, const uGnssMessageId_t *pMessageId,
                     int32_t errorCodeOrLength, void *pCallbackParam)
{
    char *pBuffer = (char *) pCallbackParam;
    int32_t length;

    (void) pMessageId;

    if (errorCodeOrLength >= 0) {
        // Read the message into our buffer and print it
        length = uGnssMsgReceiveCallbackRead(devHandle, pBuffer, errorCodeOrLength);
        if (length >= 0) {
            gMessageCount++;
#if !U_CFG_OS_CLIB_LEAKS && !defined(U_CFG_TEST_USING_NRF5SDK) // NRF52 goes a bit crazy if you print here
            uPortLog("%.*s", length, pBuffer);
        } else {
            uPortLog("Empty or bad message received.\n");
#endif
        }
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: THE EXAMPLE
 * -------------------------------------------------------------- */

// The entry point, main(): before this is called the system
// clocks must have been started and the RTOS must be running;
// we are in task space.
U_PORT_TEST_FUNCTION("[example]", "exampleGnssMsg")
{
    uDeviceHandle_t devHandle = NULL;
    uGnssMessageId_t messageId = {0};
    // Enough room for the UBX-NAV-PVT message, which has a body of length 92 bytes,
    // and any NMEA message (which have a maximum size of 82 bytes)
    char *pBuffer = (char *) malloc(MY_MESSAGE_BUFFER_LENGTH);
    int32_t length = 0;
    int32_t returnCode;
    int32_t handle;

    // Initialise the APIs we will need
    uPortInit();
    uPortI2cInit(); // You only need this if an I2C interface is used
    uDeviceInit();

    // Open the device
    returnCode = uDeviceOpen(&gDeviceCfg, &devHandle);
    uPortLog("Opened device with return code %d.\n", returnCode);

    if ((returnCode == 0) && (pBuffer != NULL)) {
        // Since we are not using the common APIs we do not need
        // to call uNetworkInteraceUp()/uNetworkInteraceDown().

        // Just for when this test is running on the ubxlib test system
        // with other tests that may have switched NMEA messages off
        // (we need them a little lower down).
        uGnssCfgSetProtocolOut(devHandle, U_GNSS_PROTOCOL_NMEA, true);

        // Begin by sending a single UBX-format message to the GNSS
        // device and picking up the answer; the message does not have
        // to be a UBX-format message, it can be anything you think the
        // GNSS chip will understand (NMEA, SPARTN etc.), we are just
        // using a UBX-format message to demonstrate uUbxProtocolEncode().

        // First encode the message into pBuffer; we just send the message
        // class and ID of the UBX-NAV-PVT message (values read from the
        // GNSS interface manual - we will enumerate these at some point)
        // with an empty body: this "polls" the GNSS device for a
        // UBX-NAV-PVT message.
        length = uUbxProtocolEncode(0x01, 0x07, NULL, 0, pBuffer);
        if (uGnssMsgSend(devHandle, pBuffer, length) == length) {
            // Wait for the UBX-NAV-PVT response to come back
            messageId.type = U_GNSS_PROTOCOL_UBX;
            messageId.id.ubx = 0x0107; // This could be any UBX message ID/class
            length = uGnssMsgReceive(devHandle, &messageId, &pBuffer, MY_MESSAGE_BUFFER_LENGTH, 30000, NULL);
            if (length > 0) {
                printPosition(pBuffer, length);
            } else {
                uPortLog("Did not receive a response!\n");
            }
        } else {
            uPortLog("Unable to send message!\n");
        }

        // Alternatively, we can set up one or more message receive call-backs
        // We will set one up to capture all NMEA messages
        messageId.type = U_GNSS_PROTOCOL_NMEA;
        messageId.id.pNmea = NULL; // This means all, but could be "GPGSV", etc.
        // We give the message receiver pBuffer so that it can read messages into it
        handle = uGnssMsgReceiveStart(devHandle, &messageId, callback, pBuffer);
        if (handle >= 0) {
            // Wait a while for some messages to arrive
            uPortTaskBlock(5000);
            // Stop the message receiver(s) once more
            uGnssMsgReceiveStopAll(devHandle);
        } else {
            uPortLog("Unable to start message receiver!\n");
        }

        uPortLog("%d NMEA message(s) received.\n", gMessageCount);

    } else {
        uPortLog("Unable to open GNSS!\n");
    }

    // Close the device
    // Note: we don't power the device down here in order
    // to speed up testing; you may prefer to power it off
    // by setting the second parameter to true.
    uDeviceClose(devHandle, false);

    // Tidy up
    uDeviceDeinit();
    uPortI2cDeinit(); // You only need this if an I2C interface is used
    uPortDeinit();

    uPortLog("Done.\n");

    free(pBuffer);

#if defined(U_CFG_TEST_GNSS_MODULE_TYPE) && ((U_CFG_APP_GNSS_UART >= 0) || (U_CFG_APP_GNSS_I2C >= 0))
    // For u-blox internal testing only
    EXAMPLE_FINAL_STATE((length > 0) && (gMessageCount > 0) && (returnCode == 0));
#endif
}

// End of file
