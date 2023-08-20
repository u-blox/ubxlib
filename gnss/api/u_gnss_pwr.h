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

#ifndef _U_GNSS_PWR_H_
#define _U_GNSS_PWR_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup _GNSS
 *  @{
 */

/** @file
 * @brief This header file defines the GNSS APIs to control the power
 * state of a GNSS device.
 *
 * The power-saving behaviours of the GNSS device are COMPLICATED!
 * See the Power Management section of the interface manual for your
 * GNSS device for an overview of the terms used and a description
 * of the states involved; the state machine diagram is particularly
 * useful.  Note that power-saving is not supported on all GNSS devices
 * (e.g. ADR, FTS and HPG devices do not).
 *
 * To define a few terms:
 *
 * Acquisition mode/state: in acquisition mode the GNSS device searches
 * for new satellites, either to establish initial position or because
 * an acquisition retry timer has expired.
 *
 * Tracking mode/state: in tracking mode the GNSS device maintains
 * position by tracking the satellites it found in acquisition mode;
 * it does NOT acquire any new satellites and saves power as a
 * result.  Tracking mode/state may be split into a user-definable
 * "on" state, where less power can be saved, and "power optimized
 * tracking" state, which is entered once the "on" state timer has
 * expired.
 *
 * Inactive mode/state: the GNSS device enters inactive state either
 * because it has established position and there is nothing more to
 * do or because it has failed to establish position and is awaiting
 * the expiry of a retry timer; power consumption will be at a minimum
 * and communications with the GNSS device may fail until the device is
 * woken up again, either through timers expiring or by the attempt to
 * contact it (though note that the I2C communications lines are not
 * in the "wake-up" set; UART RXD and SPI CS are, as is EXTINT 0 and 1
 * of course).
 *
 * Note: the M8-and-earlier UBX-CFG-PMS message is not currently exposed
 * by this API; UBX-CFG-PM2 and the configuration items of M9-and-later
 * provide the same functionality at a more detailed level.  Should you
 * require UBX-CFG-PMS please let us know and we will add it.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_GNSS_POWER_UP_TIME_SECONDS
/** How long to wait for a GNSS chip to be available after it is
 * powered up.  If you change this and you use the cell locate
 * API then you might want to change the value of
 * #U_CELL_LOC_GNSS_POWER_UP_TIME_SECONDS also.
 */
# define U_GNSS_POWER_UP_TIME_SECONDS 2
#endif

#ifndef U_GNSS_RESET_TIME_SECONDS
/** How long to wait for a GNSS chip to be available after it has
 * been asked to reset.
 */
# define U_GNSS_RESET_TIME_SECONDS 5
#endif

#ifndef U_GNSS_AT_POWER_UP_TIME_SECONDS
/** How long to wait for the response to AT+UGPS=1.  If you
 * change this and you use the cell locate API then you
 * might want to change the value of
 * #U_CELL_LOC_GNSS_POWER_UP_TIME_SECONDS also.
 */
# define U_GNSS_AT_POWER_UP_TIME_SECONDS 30
#endif

#ifndef U_GNSS_AT_POWER_DOWN_TIME_SECONDS
/** How long to wait for the response to AT+UGPS=0.  If you
 * change this and you use the cell locate API then you
 * might want to change the value of
 * #U_CELL_LOC_GNSS_POWER_DOWN_TIME_SECONDS also.
 */
# define U_GNSS_AT_POWER_DOWN_TIME_SECONDS 30
#endif

#ifndef U_GNSS_AT_POWER_CHANGE_WAIT_MILLISECONDS
/** Some intermediate modules (for example SARA-R4) can be touchy
 * about a power-up or power-down request occurring close
 * on the heels of a previous GNSS-related command  If you
 * change this and you use the cell locate API then you
 * might want to change the value of
 * #U_CELL_LOC_GNSS_POWER_CHANGE_WAIT_MILLISECONDS also.
 */
# define U_GNSS_AT_POWER_CHANGE_WAIT_MILLISECONDS 500
#endif

#ifndef U_GNSS_AT_POWER_ON_RETRIES
/** When GNSS is connected via an intermediate module that
 * intermediate module can sometimes already be talking to
 * the GNSS module when we ask it to power the GNSS module
 * on, resulting in the error response "+CME ERROR: Invalid
 * operation with LOC running / GPS Busy".  In order to
 * avoid that we retry a few times in case of error.
 */
# define U_GNSS_AT_POWER_ON_RETRIES 2
#endif

#ifndef U_GNSS_AT_POWER_ON_RETRY_INTERVAL_SECONDS
/** How long to wait between power-on retries; only
 * relevant if #U_GNSS_AT_POWER_ON_RETRIES is greater than
 * zero.
 */
# define U_GNSS_AT_POWER_ON_RETRY_INTERVAL_SECONDS 10
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The power-saving mode, see uGnssPwrSetMode(); for M9 devices and
 * later this enum matches #uGnssCfgValKeyItemValuePmOperatemode_t.
 */
typedef enum {
    U_GNSS_PWR_SAVING_MODE_NONE = 0, /**< the GNSS chip never attempts to save power, performance
                                          is maximised: for M8 devices there is no such setting,
                                          either #U_GNSS_PWR_SAVING_MODE_ON_OFF or
                                          #U_GNSS_PWR_SAVING_MODE_CYCLIC_TRACKING applies. */
    U_GNSS_PWR_SAVING_MODE_ON_OFF = 1, /**< the receiver switches entirely off when it can: use this to save
                                            power when position updates are required relatively infrequently
                                            (for example less than every 10 seconds); note that this mode
                                            is not supported in protocol versions 23 to 23.01. */
    U_GNSS_PWR_SAVING_MODE_CYCLIC_TRACKING = 2, /**< the receiver enters a low-power state when it can during
                                                     tracking; use this when position updates are required
                                                     frequently (for example at least every 10 seconds), and hence
                                                     the GNSS device will spend most of its time in tracking
                                                     state, but you still want to save power.  For M8 and M9
                                                     devices see also
                                                     #U_GNSS_PWR_FLAG_CYCLIC_TRACKING_OPTIMISE_FOR_POWER_ENABLE. */
    U_GNSS_PWR_SAVING_MODE_MAX_NUM
} uGnssPwrSavingMode_t;

/** The power-saving flags: use these as bit positions in the bit-map
 * parameter passed to uGnssPwrSetFlag() and uGnssPwrClearFlag().  Not
 * all flags are supported by all GNSS devices.  Note that the "mode"
 * bits are not included here; they are set through uGnssPwrSetMode() /
 * uGnssPwrGetMode().
 */
typedef enum {
    U_GNSS_PWR_FLAG_CYCLIC_TRACKING_OPTIMISE_FOR_POWER_ENABLE = 1, /**< when cyclic tracking is in use, optimise
                                                                        for maximum power-saving rather than
                                                                        maximum performance; not supported
                                                                        by all GNSS devices, check the interface
                                                                        manual for your device (UBX-CFG-PM2) for
                                                                        for details. */
    U_GNSS_PWR_FLAG_EXTINT_PIN_1_NOT_0 = 4, /**< set the EXTINT pin used by #U_GNSS_PWR_FLAG_EXTINT_WAKE_ENABLE
                                                 and #U_GNSS_PWR_FLAG_EXTINT_BACKUP_ENABLE to be pin 1
                                                 instead of pin 0; for M9 devices and later this is
                                                 equivalent to setting the key ID
                                                 #U_GNSS_CFG_VAL_KEY_ID_PM_EXTINTSEL_E1. */
    U_GNSS_PWR_FLAG_EXTINT_WAKE_ENABLE = 5, /**< keep the receiver on as long as the EXTINT pin is high;
                                                 use this if you wish to control the power-saving behaviour
                                                 directly, through external hardware, rather than letting
                                                 the GNSS device do so itself. For M9 devices and later this is
                                                 equivalent to setting the key ID
                                                 #U_GNSS_CFG_VAL_KEY_ID_PM_EXTINTWAKE_L.*/
    U_GNSS_PWR_FLAG_EXTINT_BACKUP_ENABLE = 6, /**< keep the receiver in low-power back-up mode as long as the
                                                   EXTINT pin is low; use this if you wish to control the
                                                   power-saving behaviour directly, through external hardware,
                                                   rather than letting the GNSS device do so itself. For M9
                                                   devices and later this is equivalent to settings the key ID
                                                   #U_GNSS_CFG_VAL_KEY_ID_PM_EXTINTBACKUP_L. */
    U_GNSS_PWR_FLAG_EXTINT_INACTIVITY_ENABLE = 7, /**< enter back-up mode if the EXTINT pin is inactive for
                                                       longer than a given number of milliseconds, see
                                                       uGnssPwrSetExtintInactivityTimeout(); use this if you wish
                                                       to control the power-saving behaviour directly, through
                                                       external hardware, rather than letting the GNSS device do
                                                       so itself.  Not supported by all GNSS devices: refer to the
                                                       interface manual for your device (UBX-CFG-PM2 for M8/M9
                                                       devices or CFG-PM-OPERATEMODE for M9 and later devices)
                                                       for details; this is equivalent to setting the key ID
                                                       #U_GNSS_CFG_VAL_KEY_ID_PM_EXTINTINACTIVE_L. */
    U_GNSS_PWR_FLAG_LIMIT_PEAK_CURRENT_ENABLE = 8, /**< limit the peak current; if this flag is set, the
                                                        start-up time of the receiver will be increased.  For M9
                                                        devices and later this is equivalent to setting the key ID
                                                        #U_GNSS_CFG_VAL_KEY_ID_PM_LIMITPEAKCURR_L. */
    U_GNSS_PWR_FLAG_WAIT_FOR_TIME_FIX_ENABLE = 10, /**< wait for an exact time fix, instead of just a position fix,
                                                        before entering the tracking state; only use this if you
                                                        rely on the GNSS chip for exact timing as it will prolong
                                                        exit from the relatively high power acquisition state by
                                                        about two seconds.  For M9 devices and later this is
                                                        equivalent to setting the key ID
                                                        #U_GNSS_CFG_VAL_KEY_ID_PM_WAITTIMEFIX_L. */
    U_GNSS_PWR_FLAG_RTC_WAKE_ENABLE = 11, /**< perform extra wake-ups, as necessary, to update the RTC; if this
                                               flag is not set, the start-up time of the receiver may be increased,
                                               not supported by all modules, refer to the interface manual for your
                                               device (UBX-CFG-PM2) for details. */
    U_GNSS_PWR_FLAG_EPHEMERIS_WAKE_ENABLE = 12, /**< perform extra wake-ups, as necessary, to update ephemeris data;
                                                     if this flag is not set, the start-up time of the receiver may
                                                     be increased.  For M9 devices and later this is equivalent to
                                                     setting the key ID #U_GNSS_CFG_VAL_KEY_ID_PM_UPDATEEPH_L. */
    U_GNSS_PWR_FLAG_ACQUISITION_RETRY_IMMEDIATELY_ENABLE = 16 /**< do not enter off state after an acquisition failure,
                                                                   keep trying; obviously there may be little power-saving
                                                                   if this flag is set, the acquisitionPeriodMs and
                                                                   minAcquisitionTimeSeconds parameters to
                                                                   uGnssPwrSetTiming() will be ignored.  For M9 devices
                                                                   and later this is equivalent to setting the key ID
                                                                   #U_GNSS_CFG_VAL_KEY_ID_PM_DONOTENTEROFF_L. */
} uGnssPwrFlag_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Power a GNSS chip on.  If the transport type for the given GNSS
 * instance is #U_GNSS_TRANSPORT_AT or #U_GNSS_TRANSPORT_VIRTUAL_SERIAL
 * then you must have powered any associated [cellular] module up
 * (e.g. with a call to uDeviceOpen() or uCellPwrOn()) before calling
 * this function; for the #U_GNSS_TRANSPORT_VIRTUAL_SERIAL case you
 * should likely call uGnssSetIntermediate() before calling this function.
 * Also, powering up a GNSS module which is attached via a cellular
 * module will "claim" the GNSS module for this GNSS interface and so,
 * if you use the cellLoc API at the same time you MUST either call
 * uGnssPwrOff() first or you must disable GNSS for Cell Locate (either
 * by setting disableGnss to true in the pLocationAssist structure when
 * calling the location API or by calling uCellLocSetGnssEnable() with
 * false), otherwise cellLoc location establishment will fail.
 *
 * @param gnssHandle  the handle of the GNSS instance to power on.
 * @return            zero on success else negative error code.
 */
int32_t uGnssPwrOn(uDeviceHandle_t gnssHandle);

/** Check that a GNSS chip is responsive.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 */
bool uGnssPwrIsAlive(uDeviceHandle_t gnssHandle);

/** Power a GNSS chip off.
 *
 * @param gnssHandle  the handle of the GNSS instance to power off.
 * @return            zero on success else negative error code.
 */
int32_t uGnssPwrOff(uDeviceHandle_t gnssHandle);

/** Power a GNSS chip off and put it into back-up mode.  All of the
 * possible HW wake-up lines (UART RXD, SPI CS, EXTINT 0 and 1) will
 * wake the module up from this state but note that none of these
 * lines are I2C and hence, if you call this function when talking to
 * a GNSS module via I2C, the ONLY WAY back again is to have wired
 * the GNSS module's RESET_N line to this MCU and to toggle it low and
 * high again to wake the GNSS module up again. Or you can power-cycle
 * the GNSS chip of course.
 *
 * IMPORTANT: this function will return an error if the GNSS chip
 * is connected via an intermediate [e.g. cellular] module; this is
 * because the module will be communicating with the GNSS chip over I2C.
 *
 * @param gnssHandle  the handle of the GNSS instance to power on.
 * @return            zero on success else negative error code.
 */
int32_t uGnssPwrOffBackup(uDeviceHandle_t gnssHandle);

/** Set the power-saving mode.
 *
 * If you select #U_GNSS_PWR_SAVING_MODE_CYCLIC_TRACKING then you
 * may need to set the value of acquisitionPeriodMs using
 * uGnssPwrSetTiming() (default 1 second) to your desired period but
 * you probably don't have to set any of the other values of
 * uGnssPwrSetTiming() as the GNSS device will in any case be waking
 * up relatively frequently (e.g. faster than once every 10 seconds).
 *
 * If you select #U_GNSS_PWR_SAVING_MODE_ON_OFF then you will likely
 * need to put more thought into the settings of all of the parameters
 * of uGnssPwrSetTiming() to gain the power savings you need.
 *
 * Note: when either #U_GNSS_PWR_SAVING_MODE_ON_OFF or
 * #U_GNSS_PWR_SAVING_MODE_CYCLIC_TRACKING has been selected the GNSS
 * device may enter a state where it is asleep to save power and
 * hence will not necessarily be responsive to communications from
 * this MCU; under these circumstances you may wish to call
 * uGnssSetRetries() to set a non-zero number of retries when no
 * response is received, though note that the I2C communications lines
 * are not in the "wake-up" set; UART RXD and SPI CS are, as is EXTINT
 * 0 and 1 of course.
 *
 * For M9 devices and later this is equivalent to setting key ID
 * #U_GNSS_CFG_VAL_KEY_ID_PM_OPERATEMODE_E1.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @param mode        the power-saving mode to adopt.
 * @return            zero on success else negative error code.
 */
int32_t uGnssPwrSetMode(uDeviceHandle_t gnssHandle, uGnssPwrSavingMode_t mode);

/** Get the power-saving mode.  Note that power-saving is not
 * supported on all GNSS devices (e.g. ADR, FTS and HPG devices do not).
 *
 * For M9 devices and later this is equivalent to getting key ID
 * #U_GNSS_CFG_VAL_KEY_ID_PM_OPERATEMODE_E1.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @return            the power-saving mode else negative error code.
 */
int32_t uGnssPwrGetMode(uDeviceHandle_t gnssHandle);

/** Set one or more power-saving flags, chosen from #uGnssPwrFlag_t.
 * For instance, to set the EXTINT pin to 1 and enable wake-up on that
 * pin, setBitMap would be
 * `((1UL << U_GNSS_PWR_FLAG_EXTINT_PIN_1_NOT_0) | (1UL << U_GNSS_PWR_FLAG_EXTINT_WAKE_ENABLE))`.
 *
 * Not all flags are supported by all GNSS devices: check the return
 * value of the function to determine the outcome.
 *
 * It is advisable to read back the flags that have been set using
 * uGnssPwrGetFlag() after you have performed all of your configuration
 * to ensure that everything was received and actioned by the GNSS device.
 *
 * See uGnssPwrClearFlag() if you want to clear a power-saving flag.
 *
 * Note that the power-saving mode is set with a call to uGnssPwrSetMode()
 * rather than via this API.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @param setBitMap   the bits to set, chosen from the bit-positions
 *                    of #uGnssPwrFlag_t.
 * @return            on success the bit-positions of #uGnssPwrFlag_t
 *                    that are now set, else negative error code.
 */
int32_t uGnssPwrSetFlag(uDeviceHandle_t gnssHandle, uint32_t setBitMap);

/** Clear one or more power-saving flags, chosen from #uGnssPwrFlag_t.
 * For instance, to not allow extra wake-ups for RTC and ephemeris data,
 * clearBitMap would be
 * `((1UL << U_GNSS_PWR_FLAG_RTC_WAKE_ENABLE) | (1UL << U_GNSS_PWR_FLAG_EPHEMERIS_WAKE_ENABLE))`.
 *
 * Not all flags are supported by all GNSS devices: check the return
 * value of the function to determine the outcome.
 *
 * It is advisable to read back the flags that have been set using
 * uGnssPwrGetFlag() after you have performed all of your configuration
 * to ensure that everything was received and actioned by the GNSS device.
 *
 * See uGnssPwrSetFlag() if you want to set a power-saving flag.
 *
 * Note that the power-saving mode is set with a call to uGnssPwrSetMode()
 * rather than via this API.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @param clearBitMap the bits to clear, chosen from the bit-positions
 *                    of #uGnssPwrFlag_t.
 * @return            on success the bit-positions of #uGnssPwrFlag_t that
 *                    are now set, else negative error code.
 */
int32_t uGnssPwrClearFlag(uDeviceHandle_t gnssHandle, uint32_t clearBitMap);

/** Get the current values of all of the flags of #uGnssPwrFlag_t as a bit-map.
 *
 * For instance, to determine if
 * #U_GNSS_PWR_FLAG_CYCLIC_TRACKING_OPTIMISE_FOR_POWER_ENABLE is set, you
 * might do the following:
 *
 * ```
 * int32_t x = uGnssPwrGetFlag(gnssHandle);
 * if ((x >= 0) &&
 *     (((uint32_t) x) & (1UL << U_GNSS_PWR_FLAG_CYCLIC_TRACKING_OPTIMISE_FOR_POWER_ENABLE))) {
 *     // do something because U_GNSS_PWR_FLAG_CYCLIC_TRACKING_OPTIMISE_FOR_POWER_ENABLE is set
 * }
 * ```
 *
 * Note that the power-saving mode is read with a call to uGnssPwrGetMode()
 * rather than via this API.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @return            on success the bit-positions of #uGnssPwrFlag_t that are
 *                    set, else negative error code.
 */
int32_t uGnssPwrGetFlag(uDeviceHandle_t gnssHandle);

/** Set the various timings for the GNSS device.
 *
 * @param gnssHandle                    the handle of the GNSS instance.
 * @param acquisitionPeriodSeconds      the period at which the GNSS device re-enters
 *                                      acquisition mode in seconds; the timing is
 *                                      relative to UTC start of week (plus
 *                                      uGnssPwrSetTimingOffset()), not relative to the
 *                                      time of last acquisition.  Use zero to indicate
 *                                      that acquisition mode should not be re-entered
 *                                      periodically, -1 to leave this setting unchanged.
 *                                      For instance, use 3600 * 24 to acquire position
 *                                      once a day at midnight.  For M9 devices and later
 *                                      this is equivalent to setting the key ID
 *                                      #U_GNSS_CFG_VAL_KEY_ID_PM_POSUPDATEPERIOD_U4.
 * @param acquisitionRetryPeriodSeconds the period after which the GNSS device will re-enter
 *                                      acquisition mode after a failure to (re)establish
 *                                      position, in seconds; the timing is relative
 *                                      to UTC start of week (plus uGnssPwrSetTimingOffset()),
 *                                      not relative to the time of acquisition failure.
 *                                      Use zero to indicate no retry, -1 to leave this
 *                                      setting unchanged.  For instance, use 3600 * 2 to
 *                                      retry every 2 hours from midnight if acquisition
 *                                      failed. For M9 devices and later this is equivalent
 *                                      to setting the key ID
 *                                      #U_GNSS_CFG_VAL_KEY_ID_PM_ACQPERIOD_U4.
 * @param onTimeSeconds                 the time that the GNSS device will stay on,
 *                                      continuously tracking satellites, after having
 *                                      established position, in seconds; after this time
 *                                      the behaviour of the GNSS device will depend on
 *                                      #uGnssPwrSavingMode_t (the receiver may be
 *                                      left on or switched off or cyclic tracking may be
 *                                      employed).  Range 0 to 65535, use -1 to leave this
 *                                      setting unchanged.  For M9 devices and later this is
 *                                      equivalent to setting the key ID
 *                                      #U_GNSS_CFG_VAL_KEY_ID_PM_ONTIME_U2.
 * @param maxAcquisitionTimeSeconds     the maximum time to stay in acquisition mode,
 *                                      trying to (re)establish position, in seconds;
 *                                      use zero to indicate no maximum, -1 to leave
 *                                      this setting unchanged, range 0 to 255.  Values
 *                                      lower than about 45 seconds will degrade a
 *                                      receiver's ability to collect new ephemeris data at
 *                                      low signal levels.  For M9 devices and later
 *                                      this is equivalent to setting the key ID
 *                                      #U_GNSS_CFG_VAL_KEY_ID_PM_MAXACQTIME_U1.
 * @param minAcquisitionTimeSeconds     the minimum time to stay in acquisition mode,
 *                                      trying to (re)establish position, in seconds;
 *                                      use zero to indicate no minimum (let the GNSS
 *                                      device decide), -1 to leave this setting
 *                                      unchanged, range 0 to 65535 for M8 and M9 devices,
 *                                      0 to 255 for M10 devices and later.  When the GNSS
 *                                      device has left acquisition mode and is only tracking
 *                                      satellites then any that are lost will not be regained
 *                                      and so, eventually, postion may be lost; setting
 *                                      minAcquisitionTimeSeconds allows the GNSS device
 *                                      to acquire more than the minimum required
 *                                      number of satellites and hence it may be able
 *                                      to spend longer in tracking mode.  For M9 devices
 *                                      and later this is equivalent to setting the key ID
 *                                      #U_GNSS_CFG_VAL_KEY_ID_PM_MINACQTIME_U1.
 * @return                              zero on success else negative error code.
 */
int32_t uGnssPwrSetTiming(uDeviceHandle_t gnssHandle,
                          int32_t acquisitionPeriodSeconds,
                          int32_t acquisitionRetryPeriodSeconds,
                          int32_t onTimeSeconds,
                          int32_t maxAcquisitionTimeSeconds,
                          int32_t minAcquisitionTimeSeconds);

/** Get the various timings for the GNSS device.
 *
 * @param gnssHandle                          the handle of the GNSS instance.
 * @param[out] pAcquisitionPeriodSeconds      a pointer to a place to store the period at
 *                                            which the GNSS device re-enters acquisition state
 *                                            in seconds relative to UTC start of week
 *                                            (plus uGnssPwrSetTimingOffset()); may be NULL.
 *                                            For M9 devices and later this is equivalent to
 *                                            getting the key ID
 *                                            #U_GNSS_CFG_VAL_KEY_ID_PM_POSUPDATEPERIOD_U4.
 * @param[out] pAcquisitionRetryPeriodSeconds a pointer to a place to store the period after
 *                                            which the GNSS device will re-enter acquisition
 *                                            state following a failure to (re)establish position
 *                                            in seconds relative to UTC start of week
 *                                            (plus uGnssPwrSetTimingOffset()); may be NULL.
 *                                            For M9 devices and later this is equivalent
 *                                            to getting the key ID
 *                                            #U_GNSS_CFG_VAL_KEY_ID_PM_ACQPERIOD_U4.
 * @param[out] pOnTimeSeconds                 a pointer to a place to store the time that the
 *                                            GNSS device will stay on, continuously tracking
 *                                            satellites, after having established position,
 *                                            in seconds; may be NULL.  For M9 devices and later
 *                                            this is equivalent to getting the key ID
 *                                            #U_GNSS_CFG_VAL_KEY_ID_PM_ONTIME_U2.
 * @param[out] pMaxAcquisitionTimeSeconds     a pointer to a place to store the maximum
 *                                            time the GNSS device will stay in acquisition
 *                                            mode, when trying to (re)establish position,
 *                                            in seconds; may be NULL.  For M9 devices and later
 *                                            this is equivalent to getting the key ID
 *                                            #U_GNSS_CFG_VAL_KEY_ID_PM_MAXACQTIME_U1.
 * @param[out] pMinAcquisitionTimeSeconds     a pointer to a place to store the minimum time to
 *                                            stay in acquisition mode trying to (re)establish
 *                                            position, in seconds; may be NULL.  For M9 devices
 *                                            and later this is equivalent to getting the key ID
 *                                            #U_GNSS_CFG_VAL_KEY_ID_PM_MINACQTIME_U1.
 * @return                                    zero on success else negative error code.
 */
int32_t uGnssPwrGetTiming(uDeviceHandle_t gnssHandle,
                          int32_t *pAcquisitionPeriodSeconds,
                          int32_t *pAcquisitionRetryPeriodSeconds,
                          int32_t *pOnTimeSeconds,
                          int32_t *pMaxAcquisitionTimeSeconds,
                          int32_t *pMinAcquisitionTimeSeconds);

/** Set the offset of the acquisition and acquisition retry periods relative
 * to UTC start of week.  For instance, use 3600 * 12 to shift the
 * acquisition and acquisition retry periods from midnight to midday.  For
 * M9 devices and later this is equivalent to setting the key ID
 * #U_GNSS_CFG_VAL_KEY_ID_PM_GRIDOFFSET_U4.
 *
 * @param gnssHandle    the handle of the GNSS instance.
 * @param offsetSeconds the offset in seconds.
 * @return              zero on success else negative error code.
 */
int32_t uGnssPwrSetTimingOffset(uDeviceHandle_t gnssHandle, int32_t offsetSeconds);

/** Get the offset of the acquisition and acquisition retry periods relative
 * to UTC start of week in seconds; not supported by all module types,
 * see.  For M9 devices and later this is equivalent to getting the key ID
 * #U_GNSS_CFG_VAL_KEY_ID_PM_GRIDOFFSET_U4.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @return            the offset in seconds, else negative error code.
 */
int32_t uGnssPwrGetTimingOffset(uDeviceHandle_t gnssHandle);

/** Set the inactivity timeout of the EXTINT pin; only relevant if
 * the flag #U_GNSS_PWR_FLAG_EXTINT_INACTIVITY_ENABLE is supported and set.
 * For M9 devices and later this is equivalent to setting the key ID
 * #U_GNSS_CFG_VAL_KEY_ID_PM_EXTINTINACTIVITY_U4.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @param timeoutMs   the inactivity timeout in milliseconds.
 * @return            zero on success else negative error code.
 */
int32_t uGnssPwrSetExtintInactivityTimeout(uDeviceHandle_t gnssHandle,
                                           int32_t timeoutMs);

/** Get the inactivity timeout used with the EXTINT pin when
 * #U_GNSS_PWR_FLAG_EXTINT_INACTIVITY_ENABLE is set, in milliseconds.
 * For M9 devices and later this is equivalent to getting the key ID
 * #U_GNSS_CFG_VAL_KEY_ID_PM_EXTINTINACTIVITY_U4.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @return            the inactivity timeout in milliseconds, else
 *                    negative error code.
 */
int32_t uGnssPwrGetExtintInactivityTimeout(uDeviceHandle_t gnssHandle);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_GNSS_PWR_H_

// End of file
