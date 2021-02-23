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

/** @brief This example demonstrates how to generate a pre-shared key
 * and associated pre-shared key identity.
 *
 * The choice of module and the choice of platform on which this
 * code runs is made at build time, see the README.md for
 * instructions.
 *
 * IMPORTANT: the module in use must have been security sealed before
 * this example can be used.  Since this is a once-only irreversible
 * procedure this example does not perform a security seal automatically.
 * Please read the code in the \#if 0 section below to see how it would
 * be done.
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

// For the network API
#include "u_network.h"
#include "u_network_config_cell.h"

#include "u_security.h"

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
U_PORT_TEST_FUNCTION("[example]", "exampleSecPsk")
{
    int32_t networkHandle;
    int32_t size = 0;
    char psk[U_SECURITY_PSK_MAX_LENGTH_BYTES];
    char pskId[U_SECURITY_PSK_ID_MAX_LENGTH_BYTES];

    // Initialise the APIs we will need
    uPortInit();
    uNetworkInit();

    // Add a network instance, in this case of type cell
    // since that's what we have configuration information
    // for above.
    networkHandle = uNetworkAdd(U_NETWORK_TYPE_CELL,
                                (void *) &gConfigCell);
    uPortLog("Added network with handle %d.\n", networkHandle);

    // Bring up the network layer
    uPortLog("Bringing up the network...\n");
    if (uNetworkUp(networkHandle) == 0) {

        // The module must have previously been security
        // sealed for this example to work
        if (uSecurityIsSealed(networkHandle)) {
            uPortLog("Device is security sealed.\n");

            uPortLog("Requesting generation of a 32-byte PSK"
                     " and associated PSK ID...\n");
            size = uSecurityPskGenerate(networkHandle, 32,
                                        psk, pskId);
            uPortLog("32 bytes of PSK returned:       ");
            printHex(psk, 32);
            uPortLog("\n");
            uPortLog("%d byte(s) of PSK ID returned:  ", size);
            printHex(pskId, size);
            uPortLog("\n");
            uPortLog("This completes the example.\n");
        } else {
            uPortLog("This device is not security sealed, the PSK"
                     " generation example will not run; see comments"
                     " in the example source code for how to do sealing.\n");
            // The code below would effect a security seal.
#if 0
            // Since sealing is a once-only irreversible process this code
            // is #if 0'ed out.  Should you want to perform security
            // sealing you may compile this code in, maybe move it up to
            // always occur before the end-to-end encryption code runs
            // (if the device is detected to not be already sealed) but if
            // you do so make VERY SURE that the compilation flag discussed
            // below is set correctly each time.

            // There are two inputs to the sealing process: a device profile
            // UID (see the README.md in the directory above for how this
            // is obtained from u-blox) and a serial number of your choosing.

            // To run sealing with this example code, set the value of
            // U_CFG_SECURITY_DEVICE_PROFILE_UID to the device profile UID
            // *without* quotation marks, i.e. something like:
            //
            // U_CFG_SECURITY_DEVICE_PROFILE_UID=AgbCtixjwqLjwV3VWpfPyz

# ifdef U_CFG_SECURITY_DEVICE_PROFILE_UID
            int32_t x;
            char serialNumber[U_SECURITY_SERIAL_NUMBER_MAX_LENGTH_BYTES];

            uPortLog("Waiting for bootstrap status...\n");
            // Before security sealing can be performed the device must
            // have contacted u-blox security services and "bootstrapped"
            // itself (a once-only process): check that this has happened
            for (x = 10; (x > 0) && !uSecurityIsBootstrapped(networkHandle); x--) {
                uPortTaskBlock(5000);
            }

            if (uSecurityIsBootstrapped(networkHandle)) {
                uPortLog("Device is bootstrapped.\n");

                // In this example we obtain the serial number of the
                // device and use that in the sealing process.  You
                // may chose your own serial number instead if you wish.
                x = uSecurityGetSerialNumber(networkHandle, serialNumber);
                if ((x > 0) && x < (int32_t) sizeof(serialNumber)) {
                    uPortLog("Performing security seal with device profile UID"
                             " string \"%s\" and serial number \"%s\"...\n",
                             U_PORT_STRINGIFY_QUOTED(U_CFG_SECURITY_DEVICE_PROFILE_UID),
                             serialNumber);
                    if (uSecuritySealSet(networkHandle,
                                         U_PORT_STRINGIFY_QUOTED(U_CFG_SECURITY_DEVICE_PROFILE_UID),
                                         serialNumber, NULL) == 0) {
                        uPortLog("Device is security sealed with device profile UID string \"%s\""
                                 " and serial number \"%s\".\n",
                                 U_PORT_STRINGIFY_QUOTED(U_CFG_SECURITY_DEVICE_PROFILE_UID),
                                 serialNumber);
                    } else {
                        uPortLog("Unable to security seal device!\n");
                    }
                } else {
                    uPortLog("Unable to obtain a serial number from the device!\n");
                }
            } else {
                uPortLog("This device has not bootstrapped itself!\n");
            }
# else
#  error U_CFG_SECURITY_DEVICE_PROFILE_UID must be set to your device profile UID (without quotation marks) to use this code.
# endif
#endif
        }

        // When finished with the network layer
        uPortLog("Taking down network...\n");
        uNetworkDown(networkHandle);
    } else {
        uPortLog("Unable to bring up the network!\n");
    }

    // For u-blox internal testing only
    EXAMPLE_FINAL_STATE(((size > 0) && (size < sizeof(pskId))) || !uSecurityIsSupported(networkHandle));

    // Calling these will also deallocate the network handle
    uNetworkDeinit();
    uPortDeinit();

    uPortLog("Done.\n");
}

// End of file
