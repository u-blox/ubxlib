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

/** @brief This example demonstrates how to decode messages of your
 * choice, not otherwise decoded by ubxlib, from a GNSS device that
 * is directly connected to this MCU.
 *
 * The choice of module and the choice of platform on which this
 * code runs is made at build time, see the README.md for
 * instructions.
 */

#ifdef U_CFG_TEST_GNSS_MODULE_TYPE

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

/** The size of message buffer we need: enough room for a UBX-NAV-PVT
 * message, which has a body of length 92 bytes.
 */
# define MY_MESSAGE_BUFFER_LENGTH  (92 + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES)

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
# ifdef U_CFG_APP_PIN_GNSS_DATA_READY
            .pinDataReady = U_CFG_APP_PIN_GNSS_DATA_READY,
#  ifndef U_CFG_APP_GNSS_DEVICE_PIO_DATA_READY
#   error U_CFG_APP_GNSS_DEVICE_PIO_DATA_READY must be defined if U_CFG_APP_PIN_GNSS_DATA_READY is defined
#  else
            .devicePioDataReady = U_CFG_APP_GNSS_DEVICE_PIO_DATA_READY
#  endif
# else
            .pinDataReady = -1
# endif
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

// Count of messages decoded
static size_t gDecCount = 0;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Callback for asynchronous message reception and decoding.
static void callback(uDeviceHandle_t devHandle, const uGnssMessageId_t *pMessageId,
                     int32_t errorCodeOrLength, void *pCallbackParam)
{
    char *pBuffer = (char *) pCallbackParam;
    int32_t length;
    uGnssDec_t *pDec;
    uGnssDecUbxNavPvt_t *pUbxNavPvt;
    int64_t utcTimeNanoseconds;

    (void) pMessageId;

    if (errorCodeOrLength >= 0) {
        // Read the message into our buffer
        length = uGnssMsgReceiveCallbackRead(devHandle, pBuffer, errorCodeOrLength);
        if (length >= 0) {
            // Call the uGnssDec() API to decode the message
            pDec = pUGnssDecAlloc(pBuffer, length);
            if ((pDec != NULL) && (pDec->errorCode == 0)) {
                gDecCount++;
                // No need to check pDec->id (or pMessageId) here since we have
                // only asked for UBX-NAV-PVT messages.
                pUbxNavPvt = &(pDec->pBody->ubxNavPvt);
                // Do stuff with the contents
                utcTimeNanoseconds = uGnssDecUbxNavPvtGetTimeUtc(pUbxNavPvt);
                if (utcTimeNanoseconds >= 0) {
                    // This print will only do anything useful if you have
                    // a printf() which supports 64-bit integers
                    uPortLog("UTC time %lld nanoseconds.\n", utcTimeNanoseconds);
                } else {
                    uPortLog("UTC time not available.\n");
                }
            }
            // Must *always* free the memory that pUGnssDecAlloc() allocated
            uGnssDecFree(pDec);
        }
    } else {
        uPortLog("Empty or bad message received.\n");
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: THE EXAMPLE
 * -------------------------------------------------------------- */

// The entry point, main(): before this is called the system
// clocks must have been started and the RTOS must be running;
// we are in task space.
U_PORT_TEST_FUNCTION("[example]", "exampleGnssDec")
{
    uDeviceHandle_t devHandle = NULL;
    uGnssMessageId_t messageId = {0};
    // Enough room for the UBX-NAV-PVT message, which has a body of length 92 bytes
    char *pBuffer = (char *) pUPortMalloc(MY_MESSAGE_BUFFER_LENGTH);
    int32_t returnCode;
    int32_t handle;

    // Initialise the APIs we will need
    uPortInit();
    uPortI2cInit(); // You only need this if an I2C interface is used
    uPortSpiInit(); // You only need this if an SPI interface is used
    uDeviceInit();

    // Open the device
    returnCode = uDeviceOpen(&gDeviceCfg, &devHandle);
    uPortLog("Opened device with return code %d.\n", returnCode);

    if ((returnCode == 0) && (pBuffer != NULL)) {
        // Since we are not using the common APIs we do not need
        // to call uNetworkInteraceUp()/uNetworkInteraceDown().

        // Set up a message receive call-back to capture UBX-NAV-PVT messages.
        // UBX-NAV-PVT messages _are_ decoded by ubxlib, that is how all of
        // the position establishment functions work, but only the position-
        // related fields are returned; the UBX-NAV-PVT message contains other
        // things that may be of interest (e.g. velocity, dead-reckoning
        // information), which the uGnssDec API will decode for you.

        // The other decoder that is currently available in the uGnssDec API,
        // for use with HPG (high precision) GNSS devices, is UBX-NAV-HPPOSLLH
        // (message class/ID 0x0114); you can obtain a list of the available
        // decoders by calling uGnssDecGetIdList().

        // Should you need other message types decoded please let us know;
        // we will add popular/commonly-used ones

        // Otherwise you may call uGnssDecSetCallback() to hook-in your own
        // message decode function.

        messageId.type = U_GNSS_PROTOCOL_UBX;
        messageId.id.ubx = 0x0107; // The message class/ID of a UBX-NAV-PVT message

        // NOTE: of course, you will need to be sure that the GNSS device
        // is actually emitting the message you want to decode: for an M8 or
        // earlier device this is done with:
        // uGnssCfgSetMsgRate(devHandle, &messageId, 1)
        // ...while for an M9 or later device this is done with something like:
        // U_GNSS_CFG_SET_VAL_RAM(devHandle, MSGOUT_UBX_NAV_PVT_I2C_U1, 1)
        // ...or:
        // U_GNSS_CFG_SET_VAL_RAM(devHandle, MSGOUT_UBX_NAV_PVT_SPI_U1, 1)
        // ...etc., depending on which message ID you want to decode and which
        // interface you are using with the GNSS device.

        // As we don't know which GNSS device type or interface this example
        // will be run on, we just do the lot
        if (uGnssCfgSetMsgRate(devHandle, &messageId, 1) < 0) {
            U_GNSS_CFG_SET_VAL_RAM(devHandle, MSGOUT_UBX_NAV_PVT_I2C_U1, 1);
            U_GNSS_CFG_SET_VAL_RAM(devHandle, MSGOUT_UBX_NAV_PVT_SPI_U1, 1);
            U_GNSS_CFG_SET_VAL_RAM(devHandle, MSGOUT_UBX_NAV_PVT_UART1_U1, 1);
            U_GNSS_CFG_SET_VAL_RAM(devHandle, MSGOUT_UBX_NAV_PVT_UART2_U1, 1);
            U_GNSS_CFG_SET_VAL_RAM(devHandle, MSGOUT_UBX_NAV_PVT_USB_U1, 1);
        }

        // We give the message receiver pBuffer so that it can read messages into it
        handle = uGnssMsgReceiveStart(devHandle, &messageId, callback, pBuffer);
        if (handle >= 0) {
            // Wait a while for some messages to arrive; when a wanted
            // message class/ID arrives callback() will be called: see in there
            // for where pUGnssDecAlloc() is called to perform the message decoding
            uPortTaskBlock(5000);
            // Stop the message receiver(s) once more
            uGnssMsgReceiveStopAll(devHandle);
        } else {
            uPortLog("Unable to start message receiver!\n");
        }

        uPortLog("%d UBX-NAV-PVT message(s) decoded.\n", gDecCount);

        // Close the device
        // Note: we don't power the device down here in order
        // to speed up testing; you may prefer to power it off
        // by setting the second parameter to true.
        uDeviceClose(devHandle, false);

    } else {
        uPortLog("Unable to open GNSS!\n");
    }

    // Tidy up
    uDeviceDeinit();
    uPortSpiDeinit(); // You only need this if an SPI interface is used
    uPortI2cDeinit(); // You only need this if an I2C interface is used
    uPortDeinit();

    uPortLog("Done.\n");

    uPortFree(pBuffer);

#if ((U_CFG_APP_GNSS_UART >= 0) || (U_CFG_APP_GNSS_I2C >= 0) || (U_CFG_APP_GNSS_SPI >= 0))
    // For u-blox internal testing only
    EXAMPLE_FINAL_STATE((gDecCount > 0) && (returnCode == 0));
# endif
}

#endif // #ifdef U_CFG_TEST_GNSS_MODULE_TYPE

// End of file
