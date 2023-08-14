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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Tests for the GNSS power API: these should pass on all
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
#include "string.h"    // strstr()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"   // Required by u_gnss_private.h
#include "u_port_uart.h"

#include "u_at_client.h" // Required by u_gnss_private.h

#include "u_cell_module_type.h"

#include "u_ubx_protocol.h"

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss.h"
#include "u_gnss_pwr.h"
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
#define U_TEST_PREFIX "U_GNSS_PWR_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#ifndef U_GNSS_PWR_TEST_FLAG_COMBINATION
/** A combination of power-saving flags to test, ones that are
 * supported by all modules.
 */
# define U_GNSS_PWR_TEST_FLAG_COMBINATION ((1UL << U_GNSS_PWR_FLAG_ACQUISITION_RETRY_IMMEDIATELY_ENABLE) | \
                                           (1UL << U_GNSS_PWR_FLAG_EXTINT_WAKE_ENABLE))
#endif

#ifndef U_GNSS_PWR_TEST_SET_WAIT_MS
/** A short delay between setting a flag/timer and reading it back as
 * some modules (e.g. NEO-M9) can become upset if you hammer
 * the CFG-VAL interface.
 */
# define U_GNSS_PWR_TEST_SET_WAIT_MS 100
#endif

#ifndef U_GNSS_PWR_TEST_FLAG_SET_RETRIES
/** Quite infrequently, but often enough to be problematic, a
 * GNSS device may acknowledge the a flag has been set but then
 * not actually do the job, hence we allow a retry.
 */
# define U_GNSS_PWR_TEST_FLAG_SET_RETRIES 1
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

/** The original number of message retries, so that we can put it back.
 */
static int32_t gOriginalRetries = -1;

/** The original power-saving mode, so that we can put it back.
 */
static int32_t gOriginalMode = -1;

/** The original power-saving flags, so that we can put them back.
 */
static int32_t gOriginalFlags = -1;

/** The original acquisition period, so that we can put it back.
 */
static int32_t gOriginalAcquisitionPeriodSeconds = -1;

/** The original acquisition retry period, so that we can put it back.
 */
static int32_t gOriginalAcquisitionRetryPeriodSeconds = -1;

/** The original on-time, so that we can put it back.
 */
static int32_t gOriginalOnTimeSeconds = -1;

/** The original maximum acquisition time, so that we can put it back.
 */
static int32_t gOriginalMaxAcquisitionTimeSeconds = -1;

/** The original minimum acquisition time, so that we can put it back.
 */
static int32_t gOriginalMinAcquisitionTimeSeconds = -1;

/** The original grid offset, so that we can put it back.
 */
static int32_t gOriginalOffsetSeconds = -1;

/** The original EXTINT timeout, so that we can put it back.
 */
static int32_t gOriginalTimeoutMs = -1;

#ifndef U_CFG_TEST_GNSS_POWER_SAVING_NOT_SUPPORTED

/** The power saving flags that are only supported by M8 modules.
 */
static const uGnssPwrFlag_t gFlagM8[] = {U_GNSS_PWR_FLAG_CYCLIC_TRACKING_OPTIMISE_FOR_POWER_ENABLE,
                                         U_GNSS_PWR_FLAG_RTC_WAKE_ENABLE
                                        };

/** The power saving flags that are only supported by M8 and M9 modules.
 */
static const uGnssPwrFlag_t gFlagM8M9[] = {U_GNSS_PWR_FLAG_EXTINT_PIN_1_NOT_0};

/** The power saving flags that all modules in the test system
 * support; MUST have the same number of elements as gFlagAllKeyId.
 *
 * Note: #U_GNSS_PWR_FLAG_EXTINT_BACKUP_ENABLE and
 * #U_GNSS_PWR_FLAG_EXTINT_INACTIVITY_ENABLE are commented out since
 * modules can be come unresponsive if they get set.
 */
static const uGnssPwrFlag_t gFlagAll[] = {U_GNSS_PWR_FLAG_EXTINT_WAKE_ENABLE, // AKA key ID U_GNSS_CFG_VAL_KEY_ID_PM_EXTINTWAKE_L
                                          // U_GNSS_PWR_FLAG_EXTINT_BACKUP_ENABLE, // AKA key ID U_GNSS_CFG_VAL_KEY_ID_PM_EXTINTBACKUP_L
                                          // U_GNSS_PWR_FLAG_EXTINT_INACTIVITY_ENABLE, // AKA key ID U_GNSS_CFG_VAL_KEY_ID_PM_EXTINTINACTIVE_L
                                          U_GNSS_PWR_FLAG_LIMIT_PEAK_CURRENT_ENABLE, // AKA key ID U_GNSS_CFG_VAL_KEY_ID_PM_LIMITPEAKCURR_L
                                          U_GNSS_PWR_FLAG_WAIT_FOR_TIME_FIX_ENABLE, // AKA key ID U_GNSS_CFG_VAL_KEY_ID_PM_WAITTIMEFIX_L
                                          U_GNSS_PWR_FLAG_EPHEMERIS_WAKE_ENABLE, // AKA key ID #U_GNSS_CFG_VAL_KEY_ID_PM_UPDATEEPH_L
                                          U_GNSS_PWR_FLAG_ACQUISITION_RETRY_IMMEDIATELY_ENABLE // AKA key ID U_GNSS_CFG_VAL_KEY_ID_PM_DONOTENTEROFF_L
                                         };

/** The CFG-VAL key IDs corresponding to gFlagAll; MUST have the same number
 * of elements as gFlagAll.
 *
 * Note: #U_GNSS_CFG_VAL_KEY_ID_PM_EXTINTBACKUP_L and
 * #U_GNSS_CFG_VAL_KEY_ID_PM_EXTINTINACTIVE_L are commented out since
 * modules can be come unresponsive if they get set.
 */
static const uint32_t gFlagAllKeyId[] = {U_GNSS_CFG_VAL_KEY_ID_PM_EXTINTWAKE_L,
                                         // U_GNSS_CFG_VAL_KEY_ID_PM_EXTINTBACKUP_L,
                                         // U_GNSS_CFG_VAL_KEY_ID_PM_EXTINTINACTIVE_L,
                                         U_GNSS_CFG_VAL_KEY_ID_PM_LIMITPEAKCURR_L,
                                         U_GNSS_CFG_VAL_KEY_ID_PM_WAITTIMEFIX_L,
                                         U_GNSS_CFG_VAL_KEY_ID_PM_UPDATEEPH_L,
                                         U_GNSS_CFG_VAL_KEY_ID_PM_DONOTENTEROFF_L
                                        };

#endif

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Set or clear power saving flags, returning the new setting.
// Note: each operation has to return success but we may allow
// a retry if the GNSS device did not action the request for
// some reason.
static int32_t setOrClearPwrSavingFlag(uDeviceHandle_t gnssHandle,
                                       int32_t flagBitMap,
                                       bool setNotClear)
{
    int32_t y = 0;
    bool keepGoing = true;

    for (size_t x = 0; keepGoing && (x < U_GNSS_PWR_TEST_FLAG_SET_RETRIES + 1); x++) {
        if (setNotClear) {
            U_PORT_TEST_ASSERT(uGnssPwrSetFlag(gnssHandle, flagBitMap) == 0);
        } else {
            U_PORT_TEST_ASSERT(uGnssPwrClearFlag(gnssHandle, flagBitMap) == 0);
        }
        // Allow a short delay to avoid hammering the interface
        uPortTaskBlock(U_GNSS_PWR_TEST_SET_WAIT_MS);
        y = uGnssPwrGetFlag(gnssHandle);
        U_PORT_TEST_ASSERT(y >= 0);
        if (setNotClear) {
            keepGoing = ((y & flagBitMap) != flagBitMap);
        } else {
            keepGoing = ((y & flagBitMap) != 0);
        }
        if (keepGoing) {
            U_TEST_PRINT_LINE("*** WARNING asked to %s flag(s) 0x%08x but on read"
                              " flags were still 0x%08x.", setNotClear ? "set" : "clear",
                              flagBitMap, y);
            if (x < U_GNSS_PWR_TEST_FLAG_SET_RETRIES) {
                U_TEST_PRINT_LINE("*** trying again...");
            }
        }
    }

    return y;
}


/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Power up and down a GNSS chip.
 */
U_PORT_TEST_FUNCTION("[gnssPwr]", "gnssPwrBasic")
{
    uDeviceHandle_t gnssHandle;
    int32_t heapUsed;
    size_t iterations;
    uGnssTransportType_t transportTypes[U_GNSS_TRANSPORT_MAX_NUM];
    int32_t y;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    // Repeat for all transport types
    iterations = uGnssTestPrivateTransportTypesSet(transportTypes, U_CFG_APP_GNSS_UART,
                                                   U_CFG_APP_GNSS_I2C, U_CFG_APP_GNSS_SPI);
    for (size_t x = 0; x < iterations; x++) {
        // Do the standard preamble
        U_TEST_PRINT_LINE("testing on transport %s...",
                          pGnssTestPrivateTransportTypeName(transportTypes[x]));
        U_PORT_TEST_ASSERT(uGnssTestPrivatePreamble(U_CFG_TEST_GNSS_MODULE_TYPE,
                                                    transportTypes[x], &gHandles, false,
                                                    U_CFG_APP_CELL_PIN_GNSS_POWER,
                                                    U_CFG_APP_CELL_PIN_GNSS_DATA_READY) == 0);
        gnssHandle = gHandles.gnssHandle;

        U_TEST_PRINT_LINE("powering on GNSS...");
        U_PORT_TEST_ASSERT(uGnssPwrOn(gnssHandle) == 0);

        U_TEST_PRINT_LINE("checking that GNSS is alive...");
        U_PORT_TEST_ASSERT(uGnssPwrIsAlive(gnssHandle));

        U_TEST_PRINT_LINE("powering off GNSS...");
        U_PORT_TEST_ASSERT(uGnssPwrOff(gnssHandle) == 0);

        switch (transportTypes[x]) {
            case U_GNSS_TRANSPORT_UART:
            //lint -fallthrough
            case U_GNSS_TRANSPORT_UART_2:
            //lint -fallthrough
            case U_GNSS_TRANSPORT_SPI:
                // If we are communicating via UART or SPI we can
                // also test the power-off-to-back-up version
                U_TEST_PRINT_LINE("powering on GNSS...");
                U_PORT_TEST_ASSERT(uGnssPwrOn(gnssHandle) == 0);

                U_TEST_PRINT_LINE("powering off GNSS to back-up mode...");
                U_PORT_TEST_ASSERT(uGnssPwrOffBackup(gnssHandle) == 0);
                break;
            case U_GNSS_TRANSPORT_I2C:
                U_TEST_PRINT_LINE("not testing uGnssPwrOffBackup() 'cos we're on I2C...");
                break;
            case U_GNSS_TRANSPORT_VIRTUAL_SERIAL:
                U_TEST_PRINT_LINE("not testing uGnssPwrOffBackup() 'cos we're on Virtual Serial...");
                break;
            case U_GNSS_TRANSPORT_AT:
                U_PORT_TEST_ASSERT(uGnssPwrOffBackup(gnssHandle) == U_ERROR_COMMON_NOT_SUPPORTED);
                break;
            default:
                U_PORT_TEST_ASSERT(false);
                break;
        }

#if U_CFG_APP_PIN_GNSS_ENABLE_POWER >= 0
        U_TEST_PRINT_LINE("checking that GNSS is no longer alive...");
        U_PORT_TEST_ASSERT(!uGnssPwrIsAlive(gnssHandle));
#endif

        // Check that we haven't dropped any incoming data
        y = uGnssMsgReceiveStatStreamLoss(gnssHandle);
        U_TEST_PRINT_LINE("%d byte(s) lost from the message stream during that test.", y);
        U_PORT_TEST_ASSERT(y == 0);

        // Do the standard postamble
        uGnssTestPrivatePostamble(&gHandles, false);
    }

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

#ifndef U_CFG_TEST_GNSS_POWER_SAVING_NOT_SUPPORTED

/** Test power-saving configurations.
 */
U_PORT_TEST_FUNCTION("[gnssPwr]", "gnssPwrSaving")
{
    uDeviceHandle_t gnssHandle;
    const uGnssPrivateModule_t *pModule;
    int32_t heapUsed;
    size_t iterations;
    uGnssTransportType_t transportTypes[U_GNSS_TRANSPORT_MAX_NUM];
    int32_t z;
    uint64_t value;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    // Repeat for all transport types
    iterations = uGnssTestPrivateTransportTypesSet(transportTypes, U_CFG_APP_GNSS_UART,
                                                   U_CFG_APP_GNSS_I2C, U_CFG_APP_GNSS_SPI);
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

        // Get the private module data so that we can check if it supports CFG-VAL
        pModule = pUGnssPrivateGetModule(gnssHandle);
        U_PORT_TEST_ASSERT(pModule != NULL);

        // During testing of power-saving the module may go to sleep and so
        // make sure we retry any comms
        gOriginalRetries = uGnssGetRetries(gnssHandle);
        U_PORT_TEST_ASSERT(gOriginalRetries >= 0);
        uGnssSetRetries(gnssHandle, 3);
        U_PORT_TEST_ASSERT(uGnssGetRetries(gnssHandle) == 3);

        // Set each of the power-saving modes and check that they have been set
        gOriginalMode = uGnssPwrGetMode(gnssHandle);
        U_TEST_PRINT_LINE("power-saving mode was %d.", gOriginalMode);
        U_PORT_TEST_ASSERT(gOriginalMode >= 0);
        U_PORT_TEST_ASSERT(gOriginalMode < U_GNSS_PWR_SAVING_MODE_MAX_NUM);
        for (int32_t y = U_GNSS_PWR_SAVING_MODE_MAX_NUM - 1; y >= 0; y--) {
            // M8 doesn't support setting no power-saving and the manual
            // says that protocol versions 23 to 23.01 don't support
            // on/off power-saving but I've yet to find an M8 version
            // which supports on/off power-saving (e.g. the version 18.00
            // one inside SARA-R5/SARA-R422 NACKs an attempt to set on/off
            // power-saving) so don't do that for M8 either
            if ((pModule->moduleType == U_GNSS_MODULE_TYPE_M8) &&
                ((y == (int32_t) U_GNSS_PWR_SAVING_MODE_NONE) ||
                 (y == (int32_t) U_GNSS_PWR_SAVING_MODE_ON_OFF))) {
                U_TEST_PRINT_LINE("skipping setting power-saving mode %d.", y);
            } else {
                U_TEST_PRINT_LINE("setting power-saving mode %d...", y);
                U_PORT_TEST_ASSERT(uGnssPwrSetMode(gnssHandle, (uGnssPwrSavingMode_t) y) == 0);
                U_PORT_TEST_ASSERT(uGnssPwrGetMode(gnssHandle) == y);
            }
        }
        // Put the original power-saving mode back
        U_PORT_TEST_ASSERT(uGnssPwrSetMode(gnssHandle, (uGnssPwrSavingMode_t) gOriginalMode) == 0);
        gOriginalMode = -1;

        // Set all of the power saving flags that all modules support
        // and, where possible, check that they have been set
        // by reading the corresponding CFG-VAL key as well as by
        // reading them back
        gOriginalFlags = uGnssPwrGetFlag(gnssHandle);
        U_PORT_TEST_ASSERT(gOriginalFlags >= 0);
        U_TEST_PRINT_LINE("power-saving flags were 0x%08x.", gOriginalFlags);
        for (size_t y = 0; y < sizeof(gFlagAll) / sizeof(gFlagAll[0]); y++) {
            if ((gFlagAll[y] == (int32_t) U_GNSS_PWR_FLAG_EXTINT_INACTIVITY_ENABLE) &&
                ((pModule->moduleType == U_GNSS_MODULE_TYPE_M8) ||
                 (transportTypes[x] == U_GNSS_TRANSPORT_I2C))) {
                // Don't enable EXTINT inactivity if we're on M8 (where it is not supported)
                // or if we're on I2C ('cos we will go to sleep and never be able
                // to wake up as the I2C pins aren't in the wake-up set).
                U_TEST_PRINT_LINE("skipping setting flag %d.", gFlagAll[y]);
            } else {
                U_TEST_PRINT_LINE("setting flag %d (0x%08x)...", gFlagAll[y], 1UL << gFlagAll[y]);
                z = setOrClearPwrSavingFlag(gnssHandle, 1UL << gFlagAll[y], true);
                U_TEST_PRINT_LINE("flags are now 0x%08x.", z);
                U_PORT_TEST_ASSERT((z & (1UL << gFlagAll[y])) == (1UL << gFlagAll[y]));
                U_PORT_TEST_ASSERT((z & ~(1UL << gFlagAll[y])) == (gOriginalFlags & ~(1UL << gFlagAll[y])));
                value = 0;
                if (U_GNSS_PRIVATE_HAS(pModule, U_GNSS_PRIVATE_FEATURE_CFGVALXXX)) {
                    U_PORT_TEST_ASSERT(uGnssCfgValGet(gnssHandle, gFlagAllKeyId[y],
                                                      &value, 1, U_GNSS_CFG_VAL_LAYER_RAM) == 0);
                    U_PORT_TEST_ASSERT(value);
                }
                U_TEST_PRINT_LINE("clearing flag %d (0x%08x)...", gFlagAll[y], 1UL << gFlagAll[y]);
                z = setOrClearPwrSavingFlag(gnssHandle, 1UL << gFlagAll[y], false);
                U_TEST_PRINT_LINE("flags are now 0x%08x.", z);
                U_PORT_TEST_ASSERT((z & (1UL << gFlagAll[y])) == 0);
                U_PORT_TEST_ASSERT((z & ~(1UL << gFlagAll[y])) == (gOriginalFlags & ~(1UL << gFlagAll[y])));
                if (U_GNSS_PRIVATE_HAS(pModule, U_GNSS_PRIVATE_FEATURE_CFGVALXXX)) {
                    U_PORT_TEST_ASSERT(uGnssCfgValGet(gnssHandle, gFlagAllKeyId[y],
                                                      &value, 1, U_GNSS_CFG_VAL_LAYER_RAM) == 0);
                    U_PORT_TEST_ASSERT(!value);
                }
                if (gOriginalFlags & (1UL << gFlagAll[y])) {
                    // Set the flag again if it was set originally
                    setOrClearPwrSavingFlag(gnssHandle, 1UL << gFlagAll[y], true);
                }
            }
        }
        // Try setting the flags only supported by M8 (these can't be read back with CFG-VAL)
        if (pModule->moduleType == U_GNSS_MODULE_TYPE_M8) {
            for (size_t y = 0; y < sizeof(gFlagM8) / sizeof(gFlagM8[0]); y++) {
                U_TEST_PRINT_LINE("setting flag %d (0x%08x)...", gFlagM8[y], 1UL << gFlagM8[y]);
                z = setOrClearPwrSavingFlag(gnssHandle, 1UL << gFlagM8[y], true);
                U_TEST_PRINT_LINE("flags are now 0x%08x.", z);
                U_PORT_TEST_ASSERT((z & (1UL << gFlagM8[y])) == (1UL << gFlagM8[y]));
                U_PORT_TEST_ASSERT((z & ~(1UL << gFlagM8[y])) == (gOriginalFlags & ~(1UL << gFlagM8[y])));
                U_TEST_PRINT_LINE("clearing flag %d (0x%08x)...", gFlagM8[y], 1UL << gFlagM8[y]);
                z = setOrClearPwrSavingFlag(gnssHandle, 1UL << gFlagM8[y], false);
                U_TEST_PRINT_LINE("flags are now 0x%08x.", z);
                U_PORT_TEST_ASSERT((z & (1UL << gFlagM8[y])) == 0);
                U_PORT_TEST_ASSERT((z & ~(1UL << gFlagM8[y])) == (gOriginalFlags & ~(1UL << gFlagM8[y])));
                if (gOriginalFlags & (1UL << gFlagM8[y])) {
                    // Set the flag again if it was set originally
                    setOrClearPwrSavingFlag(gnssHandle, 1UL << gFlagM8[y], true);
                }
            }
        }
        // Try setting the flags only supported by M8 and M9 modules
        if ((pModule->moduleType == U_GNSS_MODULE_TYPE_M8) ||
            (pModule->moduleType == U_GNSS_MODULE_TYPE_M9)) {
            for (size_t y = 0; y < sizeof(gFlagM8M9) / sizeof(gFlagM8M9[0]); y++) {
                U_TEST_PRINT_LINE("setting flag %d (0x%08x)...", gFlagM8M9[y], 1UL << gFlagM8M9[y]);
                z = setOrClearPwrSavingFlag(gnssHandle, 1UL << gFlagM8M9[y], true);
                U_TEST_PRINT_LINE("flags are now 0x%08x.", z);
                U_PORT_TEST_ASSERT((z & (1UL << gFlagM8M9[y])) == (1UL << gFlagM8M9[y]));
                U_PORT_TEST_ASSERT((z & ~(1UL << gFlagM8M9[y])) == (gOriginalFlags & ~(1UL << gFlagM8M9[y])));
                U_TEST_PRINT_LINE("clearing flag %d (0x%08x)...", gFlagM8M9[y], 1UL << gFlagM8M9[y]);
                z = setOrClearPwrSavingFlag(gnssHandle, 1UL << gFlagM8M9[y], false);
                U_TEST_PRINT_LINE("flags are now 0x%08x.", z);
                U_PORT_TEST_ASSERT((z & (1UL << gFlagM8M9[y])) == 0);
                U_PORT_TEST_ASSERT((z & ~(1UL << gFlagM8M9[y])) == (gOriginalFlags & ~(1UL << gFlagM8M9[y])));
                if (gOriginalFlags & (1UL << gFlagM8M9[y])) {
                    // Set the flag again if it was set originally
                    setOrClearPwrSavingFlag(gnssHandle, 1UL << gFlagM8M9[y], true);
                }
            }
        }

        // Try setting more than one flag at a time
        U_TEST_PRINT_LINE("setting flags 0x%08x...", U_GNSS_PWR_TEST_FLAG_COMBINATION);
        z = setOrClearPwrSavingFlag(gnssHandle, U_GNSS_PWR_TEST_FLAG_COMBINATION, true);
        U_PORT_TEST_ASSERT((z & U_GNSS_PWR_TEST_FLAG_COMBINATION) == U_GNSS_PWR_TEST_FLAG_COMBINATION);
        U_PORT_TEST_ASSERT((z & ~U_GNSS_PWR_TEST_FLAG_COMBINATION) == (gOriginalFlags &
                                                                       ~U_GNSS_PWR_TEST_FLAG_COMBINATION));
        U_TEST_PRINT_LINE("clearing flags 0x%08x...", U_GNSS_PWR_TEST_FLAG_COMBINATION);
        z = setOrClearPwrSavingFlag(gnssHandle, U_GNSS_PWR_TEST_FLAG_COMBINATION, false);
        U_PORT_TEST_ASSERT((z & U_GNSS_PWR_TEST_FLAG_COMBINATION) == 0);
        U_PORT_TEST_ASSERT((z & ~U_GNSS_PWR_TEST_FLAG_COMBINATION) == (gOriginalFlags &
                                                                       ~U_GNSS_PWR_TEST_FLAG_COMBINATION));

        // Put the original flag values back
        setOrClearPwrSavingFlag(gnssHandle, (uint32_t) gOriginalFlags, true);
        gOriginalFlags = -1;

        // Set each of the main timings, doing them all at once
        U_PORT_TEST_ASSERT(uGnssPwrGetTiming(gnssHandle,
                                             &gOriginalAcquisitionPeriodSeconds,
                                             &gOriginalAcquisitionRetryPeriodSeconds,
                                             &gOriginalOnTimeSeconds,
                                             &gOriginalMaxAcquisitionTimeSeconds,
                                             &gOriginalMinAcquisitionTimeSeconds) == 0);
        U_TEST_PRINT_LINE("original acquisition period %d second(s).", gOriginalAcquisitionPeriodSeconds);
        U_TEST_PRINT_LINE("original acquisition retry period %d second(s).",
                          gOriginalAcquisitionRetryPeriodSeconds);
        U_TEST_PRINT_LINE("original on time %d second(s).", gOriginalOnTimeSeconds);
        U_TEST_PRINT_LINE("original max acquisition time %d second(s).",
                          gOriginalMaxAcquisitionTimeSeconds);
        U_TEST_PRINT_LINE("original min acquisition time %d second(s).",
                          gOriginalMinAcquisitionTimeSeconds);
        z = uGnssPwrSetTiming(gnssHandle, gOriginalAcquisitionPeriodSeconds + 1,
                              gOriginalAcquisitionRetryPeriodSeconds + 2,
                              gOriginalOnTimeSeconds + 3,
                              gOriginalMaxAcquisitionTimeSeconds + 4,
                              gOriginalMinAcquisitionTimeSeconds + 5);
        uPortTaskBlock(U_GNSS_PWR_TEST_SET_WAIT_MS);
        U_TEST_PRINT_LINE("uGnssPwrSetTiming() returned %d.", z);
        U_PORT_TEST_ASSERT(z == 0);
        U_PORT_TEST_ASSERT(uGnssPwrGetTiming(gnssHandle, &z, NULL, NULL, NULL, NULL) == 0);
        U_TEST_PRINT_LINE("new acquisition period %d second(s).", z);
        U_PORT_TEST_ASSERT(z == gOriginalAcquisitionPeriodSeconds + 1);
        z = 0;
        U_PORT_TEST_ASSERT(uGnssPwrGetTiming(gnssHandle, NULL, &z, NULL, NULL, NULL) == 0);
        U_TEST_PRINT_LINE("new acquisition retry period %d second(s).", z);
        U_PORT_TEST_ASSERT(z == gOriginalAcquisitionRetryPeriodSeconds + 2);
        z = 0;
        U_PORT_TEST_ASSERT(uGnssPwrGetTiming(gnssHandle, NULL, NULL, &z, NULL, NULL) == 0);
        U_TEST_PRINT_LINE("new on time %d second(s).", z);
        U_PORT_TEST_ASSERT(z == gOriginalOnTimeSeconds + 3);
        z = 0;
        U_PORT_TEST_ASSERT(uGnssPwrGetTiming(gnssHandle, NULL, NULL, NULL, &z, NULL) == 0);
        U_TEST_PRINT_LINE("new max acquisition time %d second(s).", z);
        U_PORT_TEST_ASSERT(z == gOriginalMaxAcquisitionTimeSeconds + 4);
        z = 0;
        U_PORT_TEST_ASSERT(uGnssPwrGetTiming(gnssHandle, NULL, NULL, NULL, NULL, &z) == 0);
        U_TEST_PRINT_LINE("new min acquisition time %d second(s).", z);
        U_PORT_TEST_ASSERT(z == gOriginalMinAcquisitionTimeSeconds + 5);

        // Spot-check leaving some out
        U_PORT_TEST_ASSERT(uGnssPwrSetTiming(gnssHandle, gOriginalAcquisitionPeriodSeconds + 6,
                                             -1, -1, gOriginalMaxAcquisitionTimeSeconds + 7, -1) == 0);
        uPortTaskBlock(U_GNSS_PWR_TEST_SET_WAIT_MS);
        U_PORT_TEST_ASSERT(uGnssPwrGetTiming(gnssHandle, &z, NULL, NULL, NULL, NULL) == 0);
        U_TEST_PRINT_LINE("new acquisition period %d second(s).", z);
        U_PORT_TEST_ASSERT(z == gOriginalAcquisitionPeriodSeconds + 6);
        z = 0;
        U_PORT_TEST_ASSERT(uGnssPwrGetTiming(gnssHandle, NULL, &z, NULL, NULL, NULL) == 0);
        U_TEST_PRINT_LINE("acquisition retry period %d second(s).", z);
        U_PORT_TEST_ASSERT(z == gOriginalAcquisitionRetryPeriodSeconds + 2);
        z = 0;
        U_PORT_TEST_ASSERT(uGnssPwrGetTiming(gnssHandle, NULL, NULL, &z, NULL, NULL) == 0);
        U_TEST_PRINT_LINE("on time %d second(s).", z);
        U_PORT_TEST_ASSERT(z == gOriginalOnTimeSeconds + 3);
        z = 0;
        U_PORT_TEST_ASSERT(uGnssPwrGetTiming(gnssHandle, NULL, NULL, NULL, &z, NULL) == 0);
        U_TEST_PRINT_LINE("new max acquisition time %d second(s).", z);
        U_PORT_TEST_ASSERT(z == gOriginalMaxAcquisitionTimeSeconds + 7);
        z = 0;
        U_PORT_TEST_ASSERT(uGnssPwrGetTiming(gnssHandle, NULL, NULL, NULL, NULL, &z) == 0);
        U_TEST_PRINT_LINE("min acquisition time %d second(s).", z);
        U_PORT_TEST_ASSERT(z == gOriginalMinAcquisitionTimeSeconds + 5);

        // Out of range values
        U_PORT_TEST_ASSERT(uGnssPwrSetTiming(gnssHandle,
                                             gOriginalAcquisitionPeriodSeconds,
                                             gOriginalAcquisitionRetryPeriodSeconds,
                                             UINT16_MAX + 1,
                                             gOriginalMaxAcquisitionTimeSeconds,
                                             gOriginalMinAcquisitionTimeSeconds) < 0);
        U_PORT_TEST_ASSERT(uGnssPwrSetTiming(gnssHandle,
                                             gOriginalAcquisitionPeriodSeconds,
                                             gOriginalAcquisitionRetryPeriodSeconds,
                                             gOriginalOnTimeSeconds,
                                             UINT8_MAX + 1,
                                             gOriginalMinAcquisitionTimeSeconds) < 0);
        z = UINT8_MAX + 1;
        if (U_GNSS_PRIVATE_HAS(pModule, U_GNSS_PRIVATE_FEATURE_OLD_CFG_API)) {
            z = UINT16_MAX + 1;
        }
        U_PORT_TEST_ASSERT(uGnssPwrSetTiming(gnssHandle,
                                             gOriginalAcquisitionPeriodSeconds,
                                             gOriginalAcquisitionRetryPeriodSeconds,
                                             gOriginalOnTimeSeconds,
                                             gOriginalMaxAcquisitionTimeSeconds, z) < 0);

        // Put the original timings back
        U_PORT_TEST_ASSERT(uGnssPwrSetTiming(gnssHandle,
                                             gOriginalAcquisitionPeriodSeconds,
                                             gOriginalAcquisitionRetryPeriodSeconds,
                                             gOriginalOnTimeSeconds,
                                             gOriginalMaxAcquisitionTimeSeconds,
                                             gOriginalMinAcquisitionTimeSeconds) == 0);
        gOriginalAcquisitionPeriodSeconds = -1;
        gOriginalAcquisitionRetryPeriodSeconds = -1;
        gOriginalOnTimeSeconds = -1;
        gOriginalMaxAcquisitionTimeSeconds = -1;
        gOriginalMinAcquisitionTimeSeconds = -1;

        // Set the timing offset
        gOriginalOffsetSeconds = uGnssPwrGetTimingOffset(gnssHandle);
        U_TEST_PRINT_LINE("original timing offset %d second(s).", gOriginalOffsetSeconds);
        U_PORT_TEST_ASSERT(uGnssPwrSetTimingOffset(gnssHandle, gOriginalOffsetSeconds + 1) == 0);
        uPortTaskBlock(U_GNSS_PWR_TEST_SET_WAIT_MS);
        z = uGnssPwrGetTimingOffset(gnssHandle);
        U_TEST_PRINT_LINE("new timing offset %d second(s).", z);
        U_PORT_TEST_ASSERT(z ==  gOriginalOffsetSeconds + 1);
        U_PORT_TEST_ASSERT(uGnssPwrSetTimingOffset(gnssHandle, gOriginalOffsetSeconds) == 0);
        uPortTaskBlock(U_GNSS_PWR_TEST_SET_WAIT_MS);
        gOriginalOffsetSeconds = -1;

        // Set the EXTINT inactivity timeout
        gOriginalTimeoutMs = uGnssPwrGetExtintInactivityTimeout(gnssHandle);
        U_TEST_PRINT_LINE("original EXTINT inactivity timeout %d ms.", gOriginalTimeoutMs);
        U_PORT_TEST_ASSERT(uGnssPwrSetExtintInactivityTimeout(gnssHandle, gOriginalTimeoutMs + 10) == 0);
        uPortTaskBlock(U_GNSS_PWR_TEST_SET_WAIT_MS);
        z = uGnssPwrGetExtintInactivityTimeout(gnssHandle);
        U_TEST_PRINT_LINE("new EXTINT inactivity timeout %d ms.", z);
        U_PORT_TEST_ASSERT(z ==  gOriginalTimeoutMs + 10);
        U_PORT_TEST_ASSERT(uGnssPwrSetExtintInactivityTimeout(gnssHandle, gOriginalTimeoutMs) == 0);
        uPortTaskBlock(U_GNSS_PWR_TEST_SET_WAIT_MS);
        gOriginalTimeoutMs = -1;

        // And finally, put the number of retries back as it was
        uGnssSetRetries(gnssHandle, gOriginalRetries);
        gOriginalRetries = -1;

        // Check that we haven't dropped any incoming data
        z = uGnssMsgReceiveStatStreamLoss(gnssHandle);
        U_TEST_PRINT_LINE("%d byte(s) lost from the message stream during that test.", z);
        U_PORT_TEST_ASSERT(z == 0);

        // Do the standard postamble
        uGnssTestPrivatePostamble(&gHandles, false);
    }

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

#endif // #ifndef U_CFG_TEST_GNSS_POWER_SAVING_NOT_SUPPORTED

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[gnssPwr]", "gnssPwrCleanUp")
{
    int32_t x;

    if (gOriginalMode >= 0) {
        uGnssPwrSetMode(gHandles.gnssHandle, (uGnssPwrSavingMode_t) gOriginalMode);
    }
    if (gOriginalFlags >= 0) {
        uGnssPwrSetFlag(gHandles.gnssHandle, (uint32_t) gOriginalFlags);
        uGnssPwrClearFlag(gHandles.gnssHandle, ~(uint32_t) gOriginalFlags);
    }
    if ((gOriginalAcquisitionPeriodSeconds >= 0) ||
        (gOriginalAcquisitionRetryPeriodSeconds >= 0) ||
        (gOriginalOnTimeSeconds >= 0) ||
        (gOriginalMaxAcquisitionTimeSeconds >= 0) ||
        (gOriginalMinAcquisitionTimeSeconds >= 0)) {
        uGnssPwrSetTiming(gHandles.gnssHandle,
                          gOriginalAcquisitionPeriodSeconds,
                          gOriginalAcquisitionRetryPeriodSeconds,
                          gOriginalOnTimeSeconds,
                          gOriginalMaxAcquisitionTimeSeconds,
                          gOriginalMinAcquisitionTimeSeconds);
    }
    if (gOriginalOffsetSeconds >= 0) {
        uGnssPwrSetTimingOffset(gHandles.gnssHandle, gOriginalOffsetSeconds);
    }
    if (gOriginalTimeoutMs >= 0) {
        uGnssPwrSetExtintInactivityTimeout(gHandles.gnssHandle, gOriginalTimeoutMs);
    }
    if (gOriginalRetries >= 0) {
        uGnssSetRetries(gHandles.gnssHandle, gOriginalRetries);
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
        U_TEST_PRINT_LINE("heap had a minimum of %d byte(s) free"
                          " at the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

#endif // #ifdef U_CFG_TEST_GNSS_MODULE_TYPE

// End of file
