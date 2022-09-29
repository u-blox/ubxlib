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

/** @brief This example demonstrates how to configure the settings
 * in a u-blox cellular module related to getting network service.
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
# include "u_cell_test_cfg.h"
# include "u_cfg_test_platform_specific.h"
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef MY_MNO_PROFILE
// Replace U_CELL_TEST_CFG_MNO_PROFILE with the MNO profile number
// you require: consult the u-blox AT command manual for your module
// to find out the possible values; 100, for example, is "Europe",
// 90 is "global".
# define MY_MNO_PROFILE U_CELL_TEST_CFG_MNO_PROFILE
#endif

// The RATs you want the module to use, in priority order.
// Set the value of MY_RAT0 to the RAT you want to use
// first (see the definition of uCellNetRat_t in cell/api/u_cell_net.h
// for the possibilities); for SARA-U201 you might chose
// U_CELL_NET_RAT_UTRAN or U_CELL_NET_RAT_GSM_GPRS_EGPRS, for
// SARA-R41x you might chose U_CELL_NET_RAT_CATM1, for
// for SARA-R412M you might chose U_CELL_NET_RAT_CATM1 or
// U_CELL_NET_RAT_GSM_GPRS_EGPRS and for SARA-R5 you might
// chose U_CELL_NET_RAT_CATM1.
// If your module supports more than one RAT at the same time
// (consult the data sheet for your module to find out how many
// it supports at the same time), add secondary and tertiary
// RATs by setting the values for MY_RAT1 and MY_RAT2 as
// required.
#ifndef MY_RAT0
# define MY_RAT0 U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED
#endif
#ifndef MY_RAT1
# define MY_RAT1 U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED
#endif
#ifndef MY_RAT2
# define MY_RAT2 U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED
#endif

// Set the values of MY_xxx_BANDMASKx to your chosen band masks
// for the Cat M1 and NB1 RATs; see cell/api/u_cell_cfg.h for some
// examples.  This is definitely the ADVANCED class: not all
// modules support all bands and a module will reject a band mask
// if one bit in one bit-position is not supported.  If you make a
// band selection that does not include a band that the network
// broadcasts at your location you will never obtain coverage,
// so take care.
// When in doubt, set an MNO profile and rely on that to configure
// the bands that your modules _does_ support.
#ifndef MY_CATM1_BANDMASK1
# define MY_CATM1_BANDMASK1 U_CELL_CFG_BAND_MASK_1_NORTH_AMERICA_CATM1_DEFAULT
#endif
#ifndef MY_CATM1_BANDMASK2
# define MY_CATM1_BANDMASK2 U_CELL_CFG_BAND_MASK_2_NORTH_AMERICA_CATM1_DEFAULT
#endif
#ifndef MY_NB1_BANDMASK1
# define MY_NB1_BANDMASK1   U_CELL_CFG_BAND_MASK_1_EUROPE_NB1_DEFAULT
#endif
#ifndef MY_NB1_BANDMASK2
# define MY_NB1_BANDMASK2   U_CELL_CFG_BAND_MASK_2_EUROPE_NB1_DEFAULT
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

// The RATs as an array.
uCellNetRat_t gMyRatList[] = {MY_RAT0, MY_RAT1, MY_RAT2};

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
// NETWORK configuration for cellular
static const uNetworkCfgCell_t gNetworkCfg = {
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
#else
static const uDeviceCfg_t gDeviceCfg = {.deviceType = U_DEVICE_TYPE_NONE};
static const uNetworkCfgCell_t gNetworkCfg = {.type = U_NETWORK_TYPE_NONE};
#endif

// The names for each RAT, for debug purposes
static const char *const gpRatStr[] = {"unknown or not used",
                                       "GSM/GPRS/EGPRS",
                                       "GSM Compact",
                                       "UTRAN",
                                       "EGPRS",
                                       "HSDPA",
                                       "HSUPA",
                                       "HSDPA/HSUPA",
                                       "LTE",
                                       "EC GSM",
                                       "CAT-M1",
                                       "NB1"
                                      };

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Print out an address structure.
static void printAddress(const uSockAddress_t *pAddress,
                         bool hasPort)
{
    switch (pAddress->ipAddress.type) {
        case U_SOCK_ADDRESS_TYPE_V4:
            uPortLog("IPV4");
            break;
        case U_SOCK_ADDRESS_TYPE_V6:
            uPortLog("IPV6");
            break;
        case U_SOCK_ADDRESS_TYPE_V4_V6:
            uPortLog("IPV4V6");
            break;
        default:
            uPortLog("unknown type (%d)", pAddress->ipAddress.type);
            break;
    }

    uPortLog(" ");

    if (pAddress->ipAddress.type == U_SOCK_ADDRESS_TYPE_V4) {
        for (int32_t x = 3; x >= 0; x--) {
            uPortLog("%u",
                     (pAddress->ipAddress.address.ipv4 >> (x * 8)) & 0xFF);
            if (x > 0) {
                uPortLog(".");
            }
        }
        if (hasPort) {
            uPortLog(":%u", pAddress->port);
        }
    } else if (pAddress->ipAddress.type == U_SOCK_ADDRESS_TYPE_V6) {
        if (hasPort) {
            uPortLog("[");
        }
        for (int32_t x = 3; x >= 0; x--) {
            uPortLog("%x:%x", pAddress->ipAddress.address.ipv6[x] >> 16,
                     pAddress->ipAddress.address.ipv6[x] & 0xFFFF);
            if (x > 0) {
                uPortLog(":");
            }
        }
        if (hasPort) {
            uPortLog("]:%u", pAddress->port);
        }
    }
}

// Read and then set the band mask for a given RAT.
static void readAndSetBand(uDeviceHandle_t devHandle, uCellNetRat_t rat,
                           uint64_t bandMask1, uint64_t bandMask2)
{
    uint64_t readBandMask1;
    uint64_t readBandMask2;

    // Read the current band mask for information
    if (uCellCfgGetBandMask(devHandle, rat,
                            &readBandMask1, &readBandMask2) == 0) {
        uPortLog("### Band mask for RAT %s is 0x%08x%08x %08x%08x.\n", gpRatStr[rat],
                 (uint32_t) (readBandMask2 >> 32), (uint32_t) readBandMask2,
                 (uint32_t) (readBandMask1 >> 32), (uint32_t) readBandMask1);
        if ((readBandMask1 != bandMask1) || (readBandMask2 != bandMask2)) {
            // Set the band mask
            uPortLog("### Setting band mask for RAT %s to 0x%08x%08x %08x%08x...\n",
                     gpRatStr[rat],
                     (uint32_t) (bandMask2 >> 32), (uint32_t) (bandMask2),
                     (uint32_t) (bandMask1 >> 32), (uint32_t) (bandMask1));
            if (uCellCfgSetBandMask(devHandle, rat,
                                    bandMask1, bandMask2) != 0) {
                uPortLog("### Unable to change band mask for RAT %s, it is"
                         " likely your module does not support one of those"
                         " bands.\n", gpRatStr[rat]);
            }
        }
    } else {
        uPortLog("### Unable to read band mask for RAT %s.\n", gpRatStr[rat]);
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: THE EXAMPLE
 * -------------------------------------------------------------- */

// The entry point, main(): before this is called the system
// clocks must have been started and the RTOS must be running;
// we are in task space.
U_PORT_TEST_FUNCTION("[example]", "exampleCellLteCfg")
{
    uDeviceHandle_t devHandle = NULL;
    uSockAddress_t address;
    int32_t x;
    char buffer[32];
    uCellNetRat_t rat[3];

    // Set to an out-of-range value so that we can
    // check it later
    address.ipAddress.type = (uSockIpAddressType_t) 0xFF;

    // Initialise the APIs we will need
    uPortInit();
    uDeviceInit();

    // Open the device
    x = uDeviceOpen(&gDeviceCfg, &devHandle);
    uPortLog("## Opened device with return code %d.\n", x);

    //---------------- CONFIGURATION BEGINS -----------------

    // Before we bring up the network,
    // configure it just how we want it to be.

    //--------------------- MNO profile ---------------------
    // Configure the MNO profile, do this first as it can alter
    // the RF band settings which we may want to change
    // ourselves later
    x = uCellCfgGetMnoProfile(devHandle);
    if (x >= 0) {
        if (x != MY_MNO_PROFILE) {
            if (uCellCfgSetMnoProfile(devHandle,
                                      MY_MNO_PROFILE) == 0) {
                uPortLog("### MNO profile has been changed from %d to %d.\n",
                         x, MY_MNO_PROFILE);
            }
        } else {
            uPortLog("### The MNO profile is already set to %d.\n", x);
        }
    } else {
        uPortLog("### This module does not support setting an MNO profile.\n");
    }
    // Reboot the module if required
    if (uCellPwrRebootIsRequired(devHandle)) {
        uPortLog("### Re-booting the module to apply MNO profile change...\n");
        uCellPwrReboot(devHandle, NULL);
    }

    //------------------------- RAT -------------------------
    // Read out the existing RAT list and set the new ones
    for (x = 0; x < 3; x++) {
        // Get the RAT at rank x
        rat[x] = uCellCfgGetRat(devHandle, x);
        if (rat[x] >= 0) {
            uPortLog("### RAT[%d] is %s.\n", x, gpRatStr[rat[x]]);
            // The effect of this code is to set MY_RAT0
            // if it is specified and then to set MY_RAT1 and
            // MY_RAT2 in all cases; hence if MY_RAT1 and MY_RAT2 are
            // left at U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED they will be
            // removed, leaving just MY_RAT0 as the sole RAT.
            if (((gMyRatList[x] > U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED) || (x > 0)) &&
                (gMyRatList[x] != rat[x])) {
                // The RAT at this rank is not what we wanted,
                // so set it
                uPortLog("### Setting RAT[%d] to %s...\n", x,
                         gpRatStr[gMyRatList[x]]);
                if (uCellCfgSetRatRank(devHandle, gMyRatList[x], x) != 0) {
                    uPortLog("### Unable to set RAT[%d] to %s.\n", x,
                             gpRatStr[gMyRatList[x]]);
                }
            }
        }
    }
    // Reboot the module if required
    if (uCellPwrRebootIsRequired(devHandle)) {
        uPortLog("### Re-booting the module to apply RAT changes...\n");
        uCellPwrReboot(devHandle, NULL);
    }

    //----------------------- RF BANDS ----------------------
    // If any of our chosen RATs are cat-M1 or NB1, set the
    // RF bands as required
    for (x = 0; x < 3; x++) {
        if (gMyRatList[x] == U_CELL_NET_RAT_CATM1) {
            readAndSetBand(devHandle, gMyRatList[x],
                           MY_CATM1_BANDMASK1, MY_CATM1_BANDMASK2);
        } else if (gMyRatList[x] == U_CELL_NET_RAT_NB1) {
            readAndSetBand(devHandle, gMyRatList[x],
                           MY_NB1_BANDMASK1, MY_NB1_BANDMASK2);
        }
    }
    // Reboot the module if required
    if (uCellPwrRebootIsRequired(devHandle)) {
        uPortLog("### Re-booting the module to apply RF band changes...\n");
        uCellPwrReboot(devHandle, NULL);
    }

    //------------------ CONFIGURATION ENDS -----------------

    uint64_t readBandMask1;
    uint64_t readBandMask2;
    if (uCellCfgGetBandMask(devHandle, U_CELL_NET_RAT_CATM1,
                            &readBandMask1, &readBandMask2) == 0) {
        uPortLog("### Band mask for RAT %s is 0x%08x%08x %08x%08x.\n",
                 gpRatStr[U_CELL_NET_RAT_CATM1],
                 (uint32_t) (readBandMask2 >> 32), (uint32_t) readBandMask2,
                 (uint32_t) (readBandMask1 >> 32), (uint32_t) readBandMask1);
    } else {
        uPortLog("### unable to read bandmask!\n");
    }
    for (x = uCellNetScanGetFirst(devHandle, NULL, 0,
                                  buffer, NULL, NULL);
         x >= 0;
         x = uCellNetScanGetNext(devHandle, NULL, 0, buffer, NULL)) {
        uPortLog("### %d: network: %s\n", x, buffer);
    }

    // Now that the module is configured, bring up the network
    if (uNetworkInterfaceUp(devHandle, U_NETWORK_TYPE_CELL,
                            &gNetworkCfg) == 0) {

        // Read the APN we have ended up with
        x = uCellNetGetApnStr(devHandle, buffer, sizeof(buffer));
        if (x >= 0) {
            uPortLog("### The APN is \"%.*s\".\n", x, buffer);
        } else {
            uPortLog("### Unable to read the APN!\n");
        }

        // Prove that we have a data connection
        // by performing a DNS look-up
        uPortLog("### Looking up server address...\n");
        if (uSockGetHostByName(devHandle, "www.google.com",
                               &(address.ipAddress)) == 0) {
            uPortLog("### www.google.com is: ");
            printAddress(&address, false);
            uPortLog("\n");
        } else {
            uPortLog("### Unable to perform DNS lookup!\n");
        }

        // When finished with the network layer
        uPortLog("### Taking down network...\n");
        uNetworkInterfaceDown(devHandle, U_NETWORK_TYPE_CELL);
    } else {
        uPortLog("### Unable to bring up the network!\n");
    }

    // Close the device
    // Note: we don't power the device down here in order
    // to speed up testing; you may prefer to power it off
    // by setting the second parameter to true.
    uDeviceClose(devHandle, false);

    // Tidy up
    uDeviceDeinit();
    uPortDeinit();

    uPortLog("### Done.\n");

#ifdef U_CFG_TEST_CELL_MODULE_TYPE
    // For u-blox internal testing only
    EXAMPLE_FINAL_STATE((address.ipAddress.type == U_SOCK_ADDRESS_TYPE_V4) ||
                        (address.ipAddress.type == U_SOCK_ADDRESS_TYPE_V6));
#endif
}

// End of file
