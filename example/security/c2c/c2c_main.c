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

/** @brief This example demonstrates how to use u-blox chip to chip
 * security.  This example will only run if a value is defined for
 * U_CFG_TEST_SECURITY_C2C_TE_SECRET (with no quotation marks around the
 * value) and, once it has run, the module it was run against CANNOT
 * be C2C-paired again except by arrangement with u-blox (see below
 * for an explanation).  In other words, this is a once-only and
 * irreversible process unless you arrange otherwise by contacting
 * u-blox.
 *
 * The choice of module and the choice of platform on which this
 * code runs is made at build time, see the README.md for
 * instructions.
 *
 * IMPORTANT: it is intended that the pairing process that enables
 * chip to chip security is carried out in a secure environment,
 * e.g. in your factory.  To ensure that is the case the module will
 * ONLY allow chip to chip security pairing to be performed BEFORE
 * the module has been security boot-strapped, something the module
 * will do THE MOMENT it contacts the cellular network for the first
 * time.  In other words, the sequence must be:
 *
 * 1. Complete the C2C pairing process between your MCU and the module;
 *    your MCU must store the pairing keys that are used to switch C2C
 *    security on and off later as desired.
 * 2. Allow the module to contact the network for the first time: it
 *    will perform security-bootstrapping with the u-blox security
 *    servers.
 * 3. Complete the security sealing process.
 *
 * Steps 1 to 3 must be performed in the order given and should be
 * performed in a secure environment.  With that done C2C security can
 * be started and stopped by your MCU at any time.
 *
 * Note: in order to test this example code, we have enabled a
 * special permission, LocalC2CKeyPairing, on our test devices which
 * DOES permit C2C pairing to be performed on a security
 * bootstrapped/sealed module.
 */

#ifdef U_CFG_TEST_SECURITY_C2C_TE_SECRET

// Bring in all of the ubxlib public header files
#include "ubxlib.h"

// Bring in the application settings
#include "u_cfg_app_platform_specific.h"

#ifndef U_CFG_DISABLE_TEST_AUTOMATION
// This purely for internal u-blox testing
# include "u_cfg_test_platform_specific.h"
#endif

#include "string.h" // For memcmp()

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
#else
static const uDeviceCfg_t gDeviceCfg = {.deviceType = U_DEVICE_TYPE_NONE};
#endif

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Print out binary.
static void printHex(const char *pStr, size_t length)
{
    char c;

    for (size_t x = 0; x < length; x++) {
        c = *pStr++;
        uPortLog("%02x", c);
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: THE EXAMPLE
 * -------------------------------------------------------------- */

// The entry point, main(): before this is called the system
// clocks must have been started and the RTOS must be running;
// we are in task space.
U_PORT_TEST_FUNCTION("[example]", "exampleSecC2c")
{
    uDeviceHandle_t devHandle = NULL;
    char rotUid[U_SECURITY_ROOT_OF_TRUST_UID_LENGTH_BYTES];
    char key[U_SECURITY_C2C_ENCRYPTION_KEY_LENGTH_BYTES];
    char hmac[U_SECURITY_C2C_HMAC_TAG_LENGTH_BYTES];
    char serialNumber1[U_SECURITY_SERIAL_NUMBER_MAX_LENGTH_BYTES];
    char serialNumber2[U_SECURITY_SERIAL_NUMBER_MAX_LENGTH_BYTES];
    int32_t x;
    int32_t y;
    bool same = false;

    // Initialise the APIs we will need
    uPortInit();
    uDeviceInit();

    // Open the device
    x = uDeviceOpen(&gDeviceCfg, &devHandle);
    uPortLog("Opened device with return code %d.\n", x);

    // Remember: at this point the module must NEVER have
    // been able to contact the u-blox security servers,
    // must never have been connected to cellular, hence
    // no "uNetworkInterfaceUp()" here.

    if (uSecurityIsSupported(devHandle)) {

        // This simply a mechanism to ensure
        // that the module has had time to
        // wake-up the u-blox security features
        // completely, since there's no point in
        // wasting time checking for device status
        uSecurityGetRootOfTrustUid(devHandle, rotUid);

        // Your MCU or factory test system would have
        // generated the 16-byte U_CFG_TEST_SECURITY_C2C_TE_SECRET
        uPortLog("Performing C2c pairing...\n");
        if (uSecurityC2cPair(devHandle,
                             U_PORT_STRINGIFY_QUOTED(U_CFG_TEST_SECURITY_C2C_TE_SECRET),
                             key, hmac) == 0) {
            uPortLog("Pairing completed, the values:");
            uPortLog("\nC2C TE secret: ");
            printHex(U_PORT_STRINGIFY_QUOTED(U_CFG_TEST_SECURITY_C2C_TE_SECRET),
                     sizeof(U_PORT_STRINGIFY_QUOTED(U_CFG_TEST_SECURITY_C2C_TE_SECRET)) - 1);
            uPortLog("\nC2C key:       ");
            printHex(key, sizeof(key));
            uPortLog("\nC2C HMAC:      ");
            printHex(hmac, sizeof(hmac));
            uPortLog("\n...should be stored securely by your MCU"
                     " as they are required to switch on C2C"
                     " protection when you need it.\n");
            uPortLog("Note: HMAC will be zero for v1 C2C but"
                     " must still be provided to uSecurityC2cOpen().\n");

            // The pairing process above is now NEVER EVER
            // run again: C2C sessions are simply opened
            // and closed using the stored keys.

            uPortLog("A C2C session is not yet open, the following"
                     " AT transaction will be in plain text.\n");
            x = uSecurityGetSerialNumber(devHandle, serialNumber1);
            uPortLog("Module returned serial number %.*s.\n",
                     x, serialNumber1);

            uPortLog("Opening a secure session using the stored"
                     " keys...\n");
            if (uSecurityC2cOpen(devHandle,
                                 U_PORT_STRINGIFY_QUOTED(U_CFG_TEST_SECURITY_C2C_TE_SECRET),
                                 key, hmac) == 0) {
                uPortLog("With a C2C session open AT comms are"
                         " now scrambled; please connect a logic"
                         " probe to the serial lines between"
                         " the MCU and the module to see the"
                         " effect.\n");
                y = uSecurityGetSerialNumber(devHandle, serialNumber2);
                uPortLog("Module returned serial number %.*s.\n",
                         y, serialNumber2);
                same = memcmp(serialNumber1, serialNumber2, x) == 0;
                if (!same) {
                    uPortLog("There's a problem- those should have"
                             " been the same!\n");
                }

                // Perform any other operations you wish with
                // C2C enabled.

                uPortLog("Closing the C2C session...\n");
                if (uSecurityC2cClose(devHandle) == 0) {
                    uPortLog("With the C2C session closed AT"
                             " communications are in plain text"
                             " once more.\n");
                    y = uSecurityGetSerialNumber(devHandle, serialNumber2);
                    uPortLog("Module returned serial number %.*s.\n",
                             y, serialNumber2);
                } else {
                    uPortLog("Unable to close the C2C security session!\n");
                }
            } else {
                uPortLog("Unable to open a C2C security session!\n");
            }
        } else {
            uPortLog("Unable to perform C2C pairing!\n");
        }
    } else {
        uPortLog("This device does not support u-blox security.\n");
    }

    // For u-blox internal testing only
    EXAMPLE_FINAL_STATE((same) || !uSecurityIsSupported(devHandle));

    // Close the device
    // Note: we don't power the device down here in order
    // to speed up testing; you may prefer to power it off
    // by setting the second parameter to true.
    uDeviceClose(devHandle, false);

    // Tidy up
    uDeviceDeinit();
    uPortDeinit();

    uPortLog("Done.\n");
}

#endif // U_CFG_TEST_SECURITY_C2C_TE_SECRET

// End of file
