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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Tests for the GNSS configuratoin API: these should pass on all
 * platforms that have a GNSS module connected to them.  They
 * are only compiled if U_CFG_TEST_GNSS_MODULE_TYPE is defined.
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the U_PORT_TEST_FUNCTION()
 * macro.
 */

#ifdef U_CFG_TEST_GNSS_MODULE_TYPE

# ifdef U_CFG_OVERRIDE
#  include "u_cfg_override.h" // For a customer's configuration override
# endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_os.h"   // Required by u_gnss_private.h
#include "u_port_uart.h"

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss.h"
#include "u_gnss_msg.h" // uGnssMsgReceiveStatStreamLoss()
#include "u_gnss_cfg.h"
#include "u_gnss_cfg_val_key.h"
#include "u_gnss_private.h"

#include "u_gnss_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_GNSS_CFG_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#ifndef U_GNSS_CFG_TEST_MIN_HEAP_TO_READ_ALL_BYTES
/** The minimum amount of heap required to read all of the configuration
 * data from a GNSS chip at once.
 */
# define U_GNSS_CFG_TEST_MIN_HEAP_TO_READ_ALL_BYTES (1024 * 16)
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Handles.
 */
static uGnssTestPrivate_t gHandles = U_GNSS_TEST_PRIVATE_DEFAULTS;

/** The initial dynamic setting.
*/
static int32_t gDynamic = -1;

/** The initial fix mode.
*/
static int32_t gFixMode = -1;

/** The initial UTC standard.
*/
static int32_t gUtcStandard = -1;

/** Array of UTC standard values to check (ones that are supported by
 * all module types).
 */
static const uGnssUtcStandard_t gUtcStandardValues[] = {U_GNSS_UTC_STANDARD_AUTOMATIC,
                                                        U_GNSS_UTC_STANDARD_USNO,
                                                        U_GNSS_UTC_STANDARD_GALILEO,
                                                        U_GNSS_UTC_STANDARD_GLONASS,
                                                        U_GNSS_UTC_STANDARD_NTSC
                                                       };

/** The keyId's associated with GEOFENCE.
 */
static const uint32_t gKeyIdGeofence[] = {U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_CONFLVL_E1,
                                          U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_USE_PIO_L,
                                          U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_PINPOL_E1,
                                          U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_PIN_U1,
                                          U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_USE_FENCE1_L,
                                          U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_FENCE1_LAT_I4,
                                          U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_FENCE1_LON_I4,
                                          U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_FENCE1_RAD_U4,
                                          U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_USE_FENCE2_L,
                                          U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_FENCE2_LAT_I4,
                                          U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_FENCE2_LON_I4,
                                          U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_FENCE2_RAD_U4,
                                          U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_USE_FENCE3_L,
                                          U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_FENCE3_LAT_I4,
                                          U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_FENCE3_LON_I4,
                                          U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_FENCE3_RAD_U4,
                                          U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_USE_FENCE4_L,
                                          U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_FENCE4_LAT_I4,
                                          U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_FENCE4_LON_I4,
                                          U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_FENCE4_RAD_U4
                                         };

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Get the size in bytes required to store the value of the given key.
static size_t storageSizeBytes(uint32_t keyId)
{
    uGnssCfgValKeySize_t storageSize = U_GNSS_CFG_VAL_KEY_GET_SIZE(keyId);
    size_t sizeBytes = 0;

    switch (storageSize) {
        case U_GNSS_CFG_VAL_KEY_SIZE_ONE_BIT:
        //lint -fallthrough
        case U_GNSS_CFG_VAL_KEY_SIZE_ONE_BYTE:
            sizeBytes = 1;
            break;
        case U_GNSS_CFG_VAL_KEY_SIZE_TWO_BYTES:
            sizeBytes = 2;
            break;
        case U_GNSS_CFG_VAL_KEY_SIZE_FOUR_BYTES:
            sizeBytes = 4;
            break;
        case U_GNSS_CFG_VAL_KEY_SIZE_EIGHT_BYTES:
            sizeBytes = 8;
            break;
        default:
            break;
    }

    return sizeBytes;
}

// Print a single value nicely.
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
            U_PORT_TEST_ASSERT(false);
            break;
    }
}

// Print an array of uGnssCfgVal_t.
static void printCfgValList(uGnssCfgVal_t *pCfgValList, size_t numItems,
                            uint16_t *pGroupId)
{
    for (size_t x = 0; x < numItems; x++) {
        uPortLog(U_TEST_PREFIX "%5d keyId 0x%08x = ", x + 1, pCfgValList->keyId);
        if (pGroupId != NULL) {
            U_PORT_TEST_ASSERT(U_GNSS_CFG_VAL_KEY_GET_GROUP_ID(pCfgValList->keyId) == *pGroupId);
        }
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

// Modify all of the values in a list in a defined way.
static void modValues(uGnssCfgVal_t *pCfgValList, size_t numValues)
{
    for (size_t x = 0; x < numValues; x++) {
        uPortLog(U_TEST_PREFIX "value for 0x%08x changed to ", pCfgValList->keyId);
        // Values are changed to 1 if 0 or 0 if 1, can't safely
        // do much more than that as the permitted range for
        // different fields can be limited and we'd just
        // get a Nack
        if (pCfgValList->value == 0) {
            pCfgValList->value = 1;
        } else {
            pCfgValList->value = 0;
        }
        printCfgVal(pCfgValList);
        uPortLog("\n");
        pCfgValList++;
        // Don't overload logging
        uPortTaskBlock(10);
    }
}

// Check that a value is as expected after modification.
static bool valueMatches(uint32_t keyId, uint64_t value, uGnssCfgVal_t *pCfgValList,
                         size_t numValues)
{
    bool identical = false;
    uGnssCfgVal_t *pCfgVal = NULL;

    // Find this key ID in the list
    for (size_t x = 0; (x < numValues) && (pCfgVal == NULL); x++) {
        if (keyId == (pCfgValList + x)->keyId) {
            pCfgVal = pCfgValList + x;
        }
    }
    U_PORT_TEST_ASSERT(pCfgVal != NULL);

    if (pCfgVal != NULL) { // This just to shut clang analyzer up
        identical = (value == pCfgVal->value);
    }

    return identical;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Test the basic GNSS configuration functions.
 */
U_PORT_TEST_FUNCTION("[gnssCfg]", "gnssCfgBasic")
{
    uDeviceHandle_t gnssHandle;
    int32_t heapUsed;
    size_t iterations;
    int32_t y;
    int32_t w;
    bool onNotOff;
    uGnssTransportType_t transportTypes[U_GNSS_TRANSPORT_MAX_NUM_WITH_UBX];

    // In case a previous test failed
    uGnssTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Repeat for all transport types
    iterations = uGnssTestPrivateTransportTypesSet(transportTypes, U_CFG_APP_GNSS_UART,
                                                   U_CFG_APP_GNSS_I2C);
    for (size_t x = 0; x < iterations; x++) {
        // Do the standard preamble
        U_TEST_PRINT_LINE("testing on transport %s...",
                          pGnssTestPrivateTransportTypeName(transportTypes[x]));
        U_PORT_TEST_ASSERT(uGnssTestPrivatePreamble(U_CFG_TEST_GNSS_MODULE_TYPE,
                                                    transportTypes[x], &gHandles, true,
                                                    U_CFG_APP_CELL_PIN_GNSS_POWER,
                                                    U_CFG_APP_CELL_PIN_GNSS_DATA_READY) == 0);
        gnssHandle = gHandles.gnssHandle;

        // So that we can see what we're doing
        uGnssSetUbxMessagePrint(gnssHandle, true);

        // Get the initial dynamic setting
        gDynamic = uGnssCfgGetDynamic(gnssHandle);
        U_TEST_PRINT_LINE("initial dynamic setting is %d.", gDynamic);
        U_PORT_TEST_ASSERT((gDynamic >= (int32_t) U_GNSS_DYNAMIC_PORTABLE) &&
                           (gDynamic <= (int32_t) U_GNSS_DYNAMIC_BIKE));

        // Get the initial fix mode
        gFixMode = uGnssCfgGetFixMode(gnssHandle);
        U_TEST_PRINT_LINE("initial fix mode is %d.", gFixMode);
        U_PORT_TEST_ASSERT((gFixMode >= (int32_t) U_GNSS_FIX_MODE_2D) &&
                           (gFixMode <= (int32_t) U_GNSS_FIX_MODE_AUTO));

        // Get the initial UTC standard
        gUtcStandard = uGnssCfgGetUtcStandard(gnssHandle);
        U_TEST_PRINT_LINE("initial UTC standard is %d.", gUtcStandard);
        U_PORT_TEST_ASSERT((gUtcStandard >= (int32_t) U_GNSS_UTC_STANDARD_AUTOMATIC) &&
                           (gUtcStandard <= (int32_t) U_GNSS_UTC_STANDARD_NPLI));

        // Set all the dynamic types except for U_GNSS_DYNAMIC_BIKE
        // since that is only supported on a specific protocol version
        // which might not be on the chip we're using
        for (int32_t z = (int32_t) U_GNSS_DYNAMIC_PORTABLE; z <= (int32_t) U_GNSS_DYNAMIC_WRIST; z++) {
            U_TEST_PRINT_LINE("setting dynamic %d.", z);
            U_PORT_TEST_ASSERT(uGnssCfgSetDynamic(gnssHandle, (uGnssDynamic_t) z) == 0);
            y = uGnssCfgGetDynamic(gnssHandle);
            U_TEST_PRINT_LINE("dynamic setting is now %d.", y);
            U_PORT_TEST_ASSERT(y == z);
            // Check that the fix mode and UTC standard haven't been changed
            U_PORT_TEST_ASSERT(uGnssCfgGetFixMode(gnssHandle) == gFixMode);
            U_PORT_TEST_ASSERT(uGnssCfgGetUtcStandard(gnssHandle) == gUtcStandard);
        }
        // Put the initial dynamic setting back
        U_PORT_TEST_ASSERT(uGnssCfgSetDynamic(gnssHandle, (uGnssDynamic_t) gDynamic) == 0);

        // Set all the fix modes
        for (int32_t z = (int32_t) U_GNSS_FIX_MODE_2D; z <= (int32_t) U_GNSS_FIX_MODE_AUTO; z++) {
            U_TEST_PRINT_LINE("setting fix mode %d.", z);
            U_PORT_TEST_ASSERT(uGnssCfgSetFixMode(gnssHandle, (uGnssFixMode_t) z) == 0);
            y = uGnssCfgGetFixMode(gnssHandle);
            U_TEST_PRINT_LINE("fix mode is now %d.", y);
            U_PORT_TEST_ASSERT(y == z);
            // Check that the dynamic setting and UTC standard haven't been changed
            U_PORT_TEST_ASSERT(uGnssCfgGetDynamic(gnssHandle) == gDynamic);
            U_PORT_TEST_ASSERT(uGnssCfgGetUtcStandard(gnssHandle) == gUtcStandard);
        }
        // Put the initial fix mode back
        U_PORT_TEST_ASSERT(uGnssCfgSetFixMode(gnssHandle, (uGnssFixMode_t) gFixMode) == 0);

        // Set all the UTC standards
        for (size_t z = 0; z < sizeof(gUtcStandardValues) / sizeof(gUtcStandardValues[0]); z++) {
            U_TEST_PRINT_LINE("setting UTC standard %d.", gUtcStandardValues[z]);
            U_PORT_TEST_ASSERT(uGnssCfgSetUtcStandard(gnssHandle, gUtcStandardValues[z]) == 0);
            y = uGnssCfgGetUtcStandard(gnssHandle);
            U_TEST_PRINT_LINE("UTC standard is now %d.", y);
            U_PORT_TEST_ASSERT(y == (int32_t) gUtcStandardValues[z]);
            // Check that the fix mode and dynamic setting haven't been changed
            U_PORT_TEST_ASSERT(uGnssCfgGetFixMode(gnssHandle) == gFixMode);
            U_PORT_TEST_ASSERT(uGnssCfgGetDynamic(gnssHandle) == gDynamic);
        }
        // Put the initial UTC standard back
        U_PORT_TEST_ASSERT(uGnssCfgSetUtcStandard(gnssHandle, (uGnssUtcStandard_t) gUtcStandard) == 0);

        U_TEST_PRINT_LINE("getting/setting output protocols.");
        if (transportTypes[x] == U_GNSS_TRANSPORT_AT) {
            // Can't do protocol output control when there's an AT interface in the way
            U_PORT_TEST_ASSERT(uGnssCfgGetProtocolOut(gnssHandle) < 0);
            U_PORT_TEST_ASSERT(uGnssCfgSetProtocolOut(gnssHandle, U_GNSS_PROTOCOL_NMEA, true) < 0);
        } else {
            // Get the current output protocol bit-map
            y = uGnssCfgGetProtocolOut(gnssHandle);
            U_TEST_PRINT_LINE("output protocols are 0x%04x.", y);
            U_PORT_TEST_ASSERT(y > 0);
            // Try to set them all off; should fail
            U_PORT_TEST_ASSERT(uGnssCfgSetProtocolOut(gnssHandle, U_GNSS_PROTOCOL_ALL, false) < 0);
            // Try to set UBX protocol off; should fail
            U_PORT_TEST_ASSERT(uGnssCfgSetProtocolOut(gnssHandle, U_GNSS_PROTOCOL_UBX, false) < 0);
            // Set NMEA to the opposite of what it was before
            onNotOff = true;
            if (((uint32_t) y) & (1 << U_GNSS_PROTOCOL_NMEA)) {
                onNotOff = false;
            }
            U_PORT_TEST_ASSERT(uGnssCfgSetProtocolOut(gnssHandle, U_GNSS_PROTOCOL_NMEA, onNotOff) == 0);
            w = uGnssCfgGetProtocolOut(gnssHandle);
            U_TEST_PRINT_LINE("output protocols are now 0x%04x.", w);
            U_PORT_TEST_ASSERT(w > 0);
            if (onNotOff) {
                U_PORT_TEST_ASSERT(((uint32_t) w) & (1 << U_GNSS_PROTOCOL_NMEA));
            } else {
                U_PORT_TEST_ASSERT((((uint32_t) w) & (1 << U_GNSS_PROTOCOL_NMEA)) == 0);
            }
            // Put things back to where they were
            U_PORT_TEST_ASSERT(uGnssCfgSetProtocolOut(gnssHandle, U_GNSS_PROTOCOL_NMEA, !onNotOff) == 0);
        }

        // Check that we haven't dropped any incoming data
        y = uGnssMsgReceiveStatStreamLoss(gnssHandle);
        U_TEST_PRINT_LINE("%d byte(s) lost at the input to the ring-buffer during that test.", y);
        U_PORT_TEST_ASSERT(y == 0);

        // Do the standard postamble, leaving the module on for the next
        // test to speed things up
        uGnssTestPrivatePostamble(&gHandles, false);
    }

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Test the GNSS VALXXX generic configuration functions.
 */
U_PORT_TEST_FUNCTION("[gnssCfg]", "gnssCfgValBasic")
{
    uDeviceHandle_t gnssHandle;
    const uGnssPrivateModule_t *pModule;
    int32_t heapUsed;
    int32_t y;
    uint16_t groupId;
    uint32_t keyId;
    uint64_t value;
    uint64_t savedValue;
    uGnssCfgVal_t *pCfgValList = NULL;
    int32_t numValues;
    size_t iterations;
    uGnssTransportType_t transportTypes[U_GNSS_TRANSPORT_MAX_NUM_WITH_UBX];

    // In case a previous test failed
    uGnssTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Repeat for all transport types
    iterations = uGnssTestPrivateTransportTypesSet(transportTypes, U_CFG_APP_GNSS_UART,
                                                   U_CFG_APP_GNSS_I2C);
    for (size_t x = 0; x < iterations; x++) {
        // Do the standard preamble
        U_TEST_PRINT_LINE("testing on transport %s...",
                          pGnssTestPrivateTransportTypeName(transportTypes[x]));
        U_PORT_TEST_ASSERT(uGnssTestPrivatePreamble(U_CFG_TEST_GNSS_MODULE_TYPE,
                                                    transportTypes[x], &gHandles, true,
                                                    U_CFG_APP_CELL_PIN_GNSS_POWER,
                                                    U_CFG_APP_CELL_PIN_GNSS_DATA_READY) == 0);
        gnssHandle = gHandles.gnssHandle;

        // Get the private module data and only proceeed if it supports
        // VALXXX-style configuation
        pModule = pUGnssPrivateGetModule(gnssHandle);
        U_PORT_TEST_ASSERT(pModule != NULL);
        if (U_GNSS_PRIVATE_HAS(pModule, U_GNSS_PRIVATE_FEATURE_CFGVALXXX)) {
            // So that we can see what we're doing
            uGnssSetUbxMessagePrint(gnssHandle, true);

#ifndef U_CFG_TEST_USING_NRF5SDK // NRF5 SDK's heap doesn't seem to be able to cope with such a huge malloc
            y = uPortGetHeapFree();
            // y < 0 below because reading the amount of heap free is not
            // supported on all platforms
            if ((y >= U_GNSS_CFG_TEST_MIN_HEAP_TO_READ_ALL_BYTES) || (y < 0)) {
                // Not to be under-ambitious, first try asking for everything;
                // this may well run out of memory on some platforms as it
                // requires a very large malloc
                U_TEST_PRINT_LINE("reading the entire device configuration with VALGET.");
                keyId = U_GNSS_CFG_VAL_KEY(U_GNSS_CFG_VAL_KEY_GROUP_ID_ALL,
                                           U_GNSS_CFG_VAL_KEY_ITEM_ID_ALL,
                                           U_GNSS_CFG_VAL_KEY_SIZE_EIGHT_BYTES);
                numValues = uGnssCfgValGetAlloc(gnssHandle, keyId, &pCfgValList, U_GNSS_CFG_VAL_LAYER_RAM);
                U_PORT_TEST_ASSERT ((numValues > 0) ||
                                    (numValues == (int32_t) U_ERROR_COMMON_NO_MEMORY));
                if (numValues >= 0) {
                    U_TEST_PRINT_LINE("VALGET returned %d item(s):", numValues);
                    printCfgValList(pCfgValList, numValues, NULL);
                    if (y >= 0) {
                        U_TEST_PRINT_LINE("...and that required %d byte(s) of heap.", y - uPortGetHeapFree());
                    }
                    // Free memory
                    uPortFree(pCfgValList);
                } else {
                    U_TEST_PRINT_LINE("not enough memory to VALGET everything");
                }
            } else {
                U_TEST_PRINT_LINE("not enough heap left to VALGET everything");
            }
#endif

            // Enough showing off: do the rest of the testing on the GeoFence
            // configuration as it has a nice range of values (except an 8-byte
            // one, which we test separately below) and changing it won't screw
            // anything up
            U_TEST_PRINT_LINE("reading the GEOFENCE configuration with VALGET.");
            groupId = U_GNSS_CFG_VAL_KEY_GROUP_ID_GEOFENCE;
            keyId = U_GNSS_CFG_VAL_KEY(groupId, U_GNSS_CFG_VAL_KEY_ITEM_ID_ALL,
                                       U_GNSS_CFG_VAL_KEY_SIZE_EIGHT_BYTES);
            numValues = uGnssCfgValGetAlloc(gnssHandle, keyId, &pCfgValList, U_GNSS_CFG_VAL_LAYER_RAM);
            // For the rest of this test to work, we need the number of
            // entries in GEOFENCE to be as expected
            U_PORT_TEST_ASSERT(numValues == sizeof(gKeyIdGeofence) / sizeof(gKeyIdGeofence[0]));
            U_TEST_PRINT_LINE("GEOFENCE (0x%04x) contains %d item(s):",
                              U_GNSS_CFG_VAL_KEY_GROUP_ID_GEOFENCE, numValues);
            printCfgValList(pCfgValList, numValues, &groupId);

            // Modify every value
            U_TEST_PRINT_LINE("modifying all the GEOFENCE values.");
            modValues(pCfgValList, numValues);

            // Write the new values back, list-style
            // Note that we don't test transactions here since they are
            // handled entirely inside the GNSS chip
            U_TEST_PRINT_LINE("writing GEOFENCE values.");
            U_PORT_TEST_ASSERT(uGnssCfgValSetList(gnssHandle, pCfgValList, numValues,
                                                  U_GNSS_CFG_VAL_TRANSACTION_NONE,
                                                  U_GNSS_CFG_VAL_LAYER_RAM) == 0);
            U_TEST_PRINT_LINE("reading back the modified GEOFENCE values.");
            for (int32_t x = 0; x < numValues; x++) {
                // Read the new values, entry by entry this time, and check
                // that they have been modified
                value = 0;
                U_PORT_TEST_ASSERT(uGnssCfgValGet(gnssHandle, gKeyIdGeofence[x],
                                                  &value, storageSizeBytes(gKeyIdGeofence[x]),
                                                  U_GNSS_CFG_VAL_LAYER_RAM) == 0);
                U_TEST_PRINT_LINE("0x%08x value read is 0x%08x.", gKeyIdGeofence[x], (int) value);
                U_PORT_TEST_ASSERT(valueMatches(gKeyIdGeofence[x], value,  pCfgValList, numValues));
                // Don't overload logging
                uPortTaskBlock(10);
            }

            // Now modify one value, non-list style, using the helper macro
            value = 0xFFFFFFFF;
            U_TEST_PRINT_LINE("modifying one GEOFENCE value 0x%08x to 0x%08x.",
                              U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_FENCE4_RAD_U4, value);
            U_PORT_TEST_ASSERT(U_GNSS_CFG_SET_VAL_RAM(gnssHandle, GEOFENCE_FENCE4_RAD_U4, value) == 0);
            savedValue = value;
            value = 0;
            for (int32_t x = 0; x < numValues; x++) {
                // Read the values again and check that only the one has changed
                U_PORT_TEST_ASSERT(uGnssCfgValGet(gnssHandle, gKeyIdGeofence[x],
                                                  &value, storageSizeBytes(gKeyIdGeofence[x]),
                                                  U_GNSS_CFG_VAL_LAYER_RAM) == 0);
                U_TEST_PRINT_LINE("value read back for 0x%08x is 0x%08x.",
                                  gKeyIdGeofence[x], value);
                if (gKeyIdGeofence[x] != U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_FENCE4_RAD_U4) {
                    U_PORT_TEST_ASSERT(valueMatches(gKeyIdGeofence[x], value,  pCfgValList, numValues));
                } else {
                    U_PORT_TEST_ASSERT(value == savedValue);
                }
                // Don't overload logging
                uPortTaskBlock(10);
            }

            // To test a 64-bit value, use one of the USB entries as that's pretty harmless
            U_TEST_PRINT_LINE("modifying 0x%08x (a 64-bit value).", U_GNSS_CFG_VAL_KEY_ID_USB_VENDOR_STR0_X8);
            U_PORT_TEST_ASSERT(uGnssCfgValGet(gnssHandle, U_GNSS_CFG_VAL_KEY_ID_USB_VENDOR_STR0_X8,
                                              (void *) &value, 8, U_GNSS_CFG_VAL_LAYER_RAM) == 0);
            U_TEST_PRINT_LINE("original value 0x%08x%08x", (int) (value >> 32), (int) value);
            value = ~value;
            U_TEST_PRINT_LINE("setting new value 0x%08x%08x", (int) (value >> 32), (int) value);
            savedValue = value;
            U_PORT_TEST_ASSERT(uGnssCfgValSet(gnssHandle,
                                              U_GNSS_CFG_VAL_KEY_ID_USB_VENDOR_STR0_X8,
                                              value, U_GNSS_CFG_VAL_TRANSACTION_NONE,
                                              U_GNSS_CFG_VAL_LAYER_RAM) == 0);
            value = ~value;
            U_PORT_TEST_ASSERT(uGnssCfgValGet(gnssHandle, U_GNSS_CFG_VAL_KEY_ID_USB_VENDOR_STR0_X8,
                                              (void *) &value, 8, U_GNSS_CFG_VAL_LAYER_RAM) == 0);
            U_TEST_PRINT_LINE("value read back is 0x%08x%08x", (int) (value >> 32), (int) value);
            U_PORT_TEST_ASSERT(value == savedValue);

            // And finally, deleting, using a different USB field for variety
            // First a single value
            value = 0;
            U_TEST_PRINT_LINE("reading 0x%08x (a 64-bit value) from BBRAM.",
                              U_GNSS_CFG_VAL_KEY_ID_USB_PRODUCT_STR3_X8);
            if (uGnssCfgValGet(gnssHandle, U_GNSS_CFG_VAL_KEY_ID_USB_PRODUCT_STR3_X8,
                               (void *) &value, 8, U_GNSS_CFG_VAL_LAYER_BBRAM) < 0) {
                U_TEST_PRINT_LINE("no value in BBRAM currently");
            }
            U_TEST_PRINT_LINE("value 0x%08x%08x", (int) (value >> 32), (int) value);
            value = ~value;
            U_TEST_PRINT_LINE("setting new value 0x%08x%08x in BBRAM", (int) (value >> 32), (int) value);
            savedValue = value;
            U_PORT_TEST_ASSERT(uGnssCfgValSet(gnssHandle,
                                              U_GNSS_CFG_VAL_KEY_ID_USB_PRODUCT_STR3_X8,
                                              value, U_GNSS_CFG_VAL_TRANSACTION_NONE,
                                              U_GNSS_CFG_VAL_LAYER_BBRAM) == 0);
            value = ~value;
            U_PORT_TEST_ASSERT(uGnssCfgValGet(gnssHandle, U_GNSS_CFG_VAL_KEY_ID_USB_PRODUCT_STR3_X8,
                                              (void *) &value, 8, U_GNSS_CFG_VAL_LAYER_BBRAM) == 0);
            U_TEST_PRINT_LINE("value read back from BBRAM is 0x%08x%08x", (int) (value >> 32), (int) value);
            U_PORT_TEST_ASSERT(value == savedValue);
            U_TEST_PRINT_LINE("deleting value for 0x%08x from BBRAM.",
                              U_GNSS_CFG_VAL_KEY_ID_USB_PRODUCT_STR3_X8);
            U_PORT_TEST_ASSERT(uGnssCfgValDel(gnssHandle, U_GNSS_CFG_VAL_KEY_ID_USB_PRODUCT_STR3_X8,
                                              U_GNSS_CFG_VAL_TRANSACTION_NONE,
                                              U_GNSS_CFG_VAL_LAYER_BBRAM) == 0);
            U_PORT_TEST_ASSERT(uGnssCfgValGet(gnssHandle, U_GNSS_CFG_VAL_KEY_ID_USB_PRODUCT_STR3_X8,
                                              (void *) &value, 8, U_GNSS_CFG_VAL_LAYER_BBRAM) < 0);

            // Now a list of key IDs, so back to using GEOFENCE
            U_TEST_PRINT_LINE("deleting current GEOFENCE values in BBRAM.");
            groupId = U_GNSS_CFG_VAL_KEY_GROUP_ID_GEOFENCE;
            keyId = U_GNSS_CFG_VAL_KEY(groupId, U_GNSS_CFG_VAL_KEY_ITEM_ID_ALL,
                                       U_GNSS_CFG_VAL_KEY_SIZE_EIGHT_BYTES);
            uGnssCfgValDel(gnssHandle, keyId, U_GNSS_CFG_VAL_TRANSACTION_NONE, U_GNSS_CFG_VAL_LAYER_BBRAM);
            // Getting the values from BBRAM should fail for all GEOFENCE entries
            U_TEST_PRINT_LINE("checking that no GEOFENCE values can be read from BBRAM.");
            for (size_t x = 0; x < sizeof(gKeyIdGeofence) / sizeof(gKeyIdGeofence[0]); x++) {
                U_PORT_TEST_ASSERT(uGnssCfgValGet(gnssHandle, gKeyIdGeofence[x],
                                                  &value, storageSizeBytes(gKeyIdGeofence[x]),
                                                  U_GNSS_CFG_VAL_LAYER_BBRAM) < 0);
                // Don't overload logging
                uPortTaskBlock(10);
            }
            // Write the values we already have to BBRAM
            U_TEST_PRINT_LINE("writing GEOFENCE values to BBRAM.");
            U_PORT_TEST_ASSERT(uGnssCfgValSetList(gnssHandle, pCfgValList,
                                                  sizeof(gKeyIdGeofence) / sizeof(gKeyIdGeofence[0]),
                                                  U_GNSS_CFG_VAL_TRANSACTION_NONE,
                                                  U_GNSS_CFG_VAL_LAYER_BBRAM) == 0);
            U_TEST_PRINT_LINE("checking that GEOFENCE values can now be read from BBRAM.");
            for (size_t x = 0; x < sizeof(gKeyIdGeofence) / sizeof(gKeyIdGeofence[0]); x++) {
                U_PORT_TEST_ASSERT(uGnssCfgValGet(gnssHandle, gKeyIdGeofence[x],
                                                  &value, storageSizeBytes(gKeyIdGeofence[x]),
                                                  U_GNSS_CFG_VAL_LAYER_BBRAM) == 0);
                // Don't overload logging
                uPortTaskBlock(10);
            }
            U_TEST_PRINT_LINE("deleting GEOFENCE values from BBRAM once more.");
            U_PORT_TEST_ASSERT(uGnssCfgValDelList(gnssHandle, gKeyIdGeofence,
                                                  sizeof(gKeyIdGeofence) / sizeof(gKeyIdGeofence[0]),
                                                  U_GNSS_CFG_VAL_TRANSACTION_NONE,
                                                  U_GNSS_CFG_VAL_LAYER_BBRAM) == 0);
            U_TEST_PRINT_LINE("checking that GEOFENCE values cannot be read from BBRAM again.");
            for (size_t x = 0; x < sizeof(gKeyIdGeofence) / sizeof(gKeyIdGeofence[0]); x++) {
                U_PORT_TEST_ASSERT(uGnssCfgValGet(gnssHandle, gKeyIdGeofence[x],
                                                  &value, storageSizeBytes(gKeyIdGeofence[x]),
                                                  U_GNSS_CFG_VAL_LAYER_BBRAM) < 0);
            }

            // Last of the last, delete using a configuration item array
            U_TEST_PRINT_LINE("writing GEOFENCE values to BBRAM.");
            U_PORT_TEST_ASSERT(uGnssCfgValSetList(gnssHandle, pCfgValList,
                                                  sizeof(gKeyIdGeofence) / sizeof(gKeyIdGeofence[0]),
                                                  U_GNSS_CFG_VAL_TRANSACTION_NONE,
                                                  U_GNSS_CFG_VAL_LAYER_BBRAM) == 0);
            U_TEST_PRINT_LINE("checking that GEOFENCE values can now be read from BBRAM.");
            for (size_t x = 0; x < sizeof(gKeyIdGeofence) / sizeof(gKeyIdGeofence[0]); x++) {
                U_PORT_TEST_ASSERT(uGnssCfgValGet(gnssHandle, gKeyIdGeofence[x],
                                                  &value, storageSizeBytes(gKeyIdGeofence[x]),
                                                  U_GNSS_CFG_VAL_LAYER_BBRAM) == 0);
                // Don't overload logging
                uPortTaskBlock(10);
            }
            U_TEST_PRINT_LINE("deleting GEOFENCE values from BBRAM using a configuration"
                              " item list this time.");
            U_PORT_TEST_ASSERT(uGnssCfgValDelListX(gnssHandle, pCfgValList,
                                                   sizeof(gKeyIdGeofence) / sizeof(gKeyIdGeofence[0]),
                                                   U_GNSS_CFG_VAL_TRANSACTION_NONE,
                                                   U_GNSS_CFG_VAL_LAYER_BBRAM) == 0);
            U_TEST_PRINT_LINE("checking that GEOFENCE values cannot be read from BBRAM again.");
            for (size_t x = 0; x < sizeof(gKeyIdGeofence) / sizeof(gKeyIdGeofence[0]); x++) {
                U_PORT_TEST_ASSERT(uGnssCfgValGet(gnssHandle, gKeyIdGeofence[x],
                                                  &value, storageSizeBytes(gKeyIdGeofence[x]),
                                                  U_GNSS_CFG_VAL_LAYER_BBRAM) < 0);
                // Don't overload logging
                uPortTaskBlock(10);
            }

            // Free memory
            uPortFree(pCfgValList);

            // Check that we haven't dropped any incoming data
            y = uGnssMsgReceiveStatStreamLoss(gnssHandle);
            U_TEST_PRINT_LINE("%d byte(s) lost at the input to the ring-buffer during that test.", y);
            U_PORT_TEST_ASSERT(y == 0);
        } else {
            U_TEST_PRINT_LINE("this module does not support VALXXX messages, not testing them.");
        }

        // Do the standard postamble, leaving the module on for the next
        // test to speed things up
        uGnssTestPrivatePostamble(&gHandles, false);
    }

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[gnssCfg]", "gnssCfgCleanUp")
{
    int32_t x;

    if ((gDynamic >= 0) && (gHandles.gnssHandle != NULL)) {
        // Put the initial dynamic setting back
        uGnssCfgSetDynamic(gHandles.gnssHandle, (uGnssDynamic_t) gDynamic);
    }

    if ((gFixMode >= 0) && (gHandles.gnssHandle != NULL)) {
        // Put the initial fix mode back
        uGnssCfgSetFixMode(gHandles.gnssHandle, (uGnssFixMode_t) gFixMode);
    }

    uGnssTestPrivateCleanup(&gHandles);

    x = uPortTaskStackMinFree(NULL);
    if (x != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_TEST_PRINT_LINE("main task stack had a minimum of %d byte(s)"
                          " free at the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);
    }

    uPortDeinit();

    x = uPortGetHeapMinFree();
    if (x >= 0) {
        U_TEST_PRINT_LINE("heap had a minimum of %d byte(s) free at the"
                          " end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

#endif // #ifdef U_CFG_TEST_GNSS_MODULE_TYPE

// End of file
