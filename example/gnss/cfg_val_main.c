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

/** @brief This example demonstrates how to configure a GNSS device
 * that is directly connected to this MCU.
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

// GNSS configuration.
//
// Set U_CFG_TEST_GNSS_MODULE_TYPE to your module type,
// chosen from the values in gnss/api/u_gnss_module_type.h and must
// be M9 or later for this example to work.
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
            .pinDataReady = -1 // Not used
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

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Print a single configuration value nicely.
static void printCfgVal(uGnssCfgVal_t *pCfgVal)
{
    uGnssCfgValKeySize_t encodedSize = U_GNSS_CFG_VAL_KEY_GET_SIZE(pCfgVal->keyId);

    switch (encodedSize) {
        case U_GNSS_CFG_VAL_KEY_SIZE_ONE_BIT:
            if (pCfgVal->value) {
                uPortLog("true");
            } else {
                uPortLog("false");
            }
            break;
        case U_GNSS_CFG_VAL_KEY_SIZE_ONE_BYTE:
            uPortLog("0x%02x", (uint8_t) pCfgVal->value);
            break;
        case U_GNSS_CFG_VAL_KEY_SIZE_TWO_BYTES:
            uPortLog("0x%04x", (uint16_t) pCfgVal->value);
            break;
        case U_GNSS_CFG_VAL_KEY_SIZE_FOUR_BYTES:
            uPortLog("0x%08x", (uint32_t) pCfgVal->value);
            break;
        case U_GNSS_CFG_VAL_KEY_SIZE_EIGHT_BYTES:
            // Print in two halves as 64-bit integers are not supported
            // by some printf() functions
            uPortLog("0x%08x%08x", (uint32_t) (pCfgVal->value >> 32),
                     (uint32_t) (pCfgVal->value));
            break;
        default:
            break;
    }
}

// Print an array of uGnssCfgVal_t.
static void printCfgValList(uGnssCfgVal_t *pCfgValList, size_t numItems)
{
    for (size_t x = 0; x < numItems; x++) {
        uPortLog("%5d keyId 0x%08x = ", x + 1, pCfgValList->keyId);
        printCfgVal(pCfgValList);
        uPortLog("\n");
        pCfgValList++;
        // Pause every few lines so as not to overload logging
        // on some platforms
        if (x % 10 == 9) {
            uPortTaskBlock(20);
        }
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: THE EXAMPLE
 * -------------------------------------------------------------- */

// The entry point, main(): before this is called the system
// clocks must have been started and the RTOS must be running;
// we are in task space.
U_PORT_TEST_FUNCTION("[example]", "exampleGnssCfgVal")
{
    uDeviceHandle_t devHandle = NULL;
    int32_t returnCode;
    bool boolValue = false;
    uint32_t keyId;
    uGnssCfgVal_t *pCfgValList = NULL;
    int32_t numValues = 0;

    if (gDeviceCfg.deviceCfg.cfgGnss.moduleType >= U_GNSS_MODULE_TYPE_M9) {
        // Initialise the APIs we will need
        uPortInit();
        uPortI2cInit(); // You only need this if an I2C interface is used
        uDeviceInit();

        // Open the device
        returnCode = uDeviceOpen(&gDeviceCfg, &devHandle);
        uPortLog("Opened device with return code %d.\n", returnCode);
        if (returnCode == 0) {
            // Since we are not using the common APIs we do not need
            // to call uNetworkInteraceUp()/uNetworkInteraceDown().

            // Now we can set any configuration we wish in the GNSS device.

            // For instance, to tell the GNSS chip to enable the first GEOFENCE
            // field, using one of the macros from the top of the file
            // u_gnss_cfg.h and the U_GNSS_CFG_VAL_KEY_ID_ items from the
            // file u_gnss_cfg_val_key.h, we would do as follows:

            if ((U_GNSS_CFG_SET_VAL_RAM(devHandle, GEOFENCE_FENCE1_LAT_I4, 522227594) == 0) &&
                (U_GNSS_CFG_SET_VAL_RAM(devHandle, GEOFENCE_FENCE1_LON_I4, -748057) == 0) &&
                (U_GNSS_CFG_SET_VAL_RAM(devHandle, GEOFENCE_FENCE1_RAD_U4, 10000) == 0) &&
                (U_GNSS_CFG_SET_VAL_RAM(devHandle, GEOFENCE_PIN_U1, 1) == 0) &&
                (U_GNSS_CFG_SET_VAL_RAM(devHandle, GEOFENCE_USE_FENCE1_L, true) == 0)) {
                uPortLog("Set GEOFENCE1.\n");
            } else {
                uPortLog("Unable to set GEOFENCE1!\n");
            }

            // You can do the above for any value listed in u_gnss_cfg_val_key.h.
            // If you find that the particular key ID you want is not listed
            // in u_gnss_cfg_val_key.h, you can instead use the 32-bit key ID
            // as listed in the GNSS interface description as follows:
            if (uGnssCfgValSet(devHandle, 0x10240020, 1, U_GNSS_CFG_VAL_TRANSACTION_NONE,
                               U_GNSS_CFG_VAL_LAYER_RAM) == 0) {
                uPortLog("Set 0x10240020 (AKA GEOFENCE_USE_FENCE1_L) to true.\n");
            } else {
                uPortLog("Unable to set 0x10240020!\n");
            }

            // Reading a single value from the configuration settings works
            // in the same way, but this time you must either specify the full
            // U_GNSS_CFG_VAL_KEY_ID_XXX value (or write in the 32-bit key ID from
            // the GNSS interface description):
            if (uGnssCfgValGet(devHandle, U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_USE_FENCE1_L,
                               (void *) &boolValue, sizeof(boolValue),
                               U_GNSS_CFG_VAL_LAYER_RAM) == 0) {
                uPortLog("U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_USE_FENCE1_L is %s.\n", boolValue ? "true" : "false");
            } else {
                uPortLog("Unable to get U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_USE_FENCE1_L!\n");
            }

            // You may read multiple values at once by using the wildcard
            // U_GNSS_CFG_VAL_KEY_ITEM_ID_ALL as the item ID of the group:
            keyId = U_GNSS_CFG_VAL_KEY(U_GNSS_CFG_VAL_KEY_GROUP_ID_GEOFENCE,
                                       U_GNSS_CFG_VAL_KEY_ITEM_ID_ALL, 0);
            numValues = uGnssCfgValGetAlloc(devHandle, keyId, &pCfgValList, U_GNSS_CFG_VAL_LAYER_RAM);
            if (numValues > 0) {
                printCfgValList(pCfgValList, numValues);
                // The function uGnssCfgValGetAlloc(), as implied by its name,
                // will have allocated memory for pCfgValList; we must free it.
                uPortFree(pCfgValList);
            } else {
                uPortLog("Unable to get all of group ID GEOFENCE!\n");
            }

            // There are other more advanced things you can do: set lists
            // of values, read lists of values, delete values, delete lists
            // of values, write to different storage layers (battery-backed RAM
            // and flash, where fitted) and use transactions; see the
            // functions described in u_gnss_cfg.h for more details.

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

        // For u-blox internal testing only
        EXAMPLE_FINAL_STATE((numValues > 0) && (returnCode == 0) && boolValue);
    } else {
        uPortLog("The CFGVALXXX API is only supported on M9 modules and later.\n");
    }
}

// End of file
