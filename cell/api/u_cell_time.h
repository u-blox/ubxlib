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

#ifndef _U_CELL_TIME_H_
#define _U_CELL_TIME_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h"

/** \addtogroup _cell
 *  @{
 */

/** @file
 * @brief THIS API IS NOT THE WAY TO GET/SET THE CLOCK/CALENDER TIME!
 * For that, see uCellInfoGetTimeUtc(), uCellInfoGetTimeUtcStr()
 * and uCellInfoGetTime() in u_cell_info.h or uCellCfgSetTime()
 * in u_cell_cfg.h.  But, since you found this file, aliases
 * for those functions are also provided here.
 *
 * This header file defines the CellTime APIs that can be used
 * to employ the highly accurate timing of the cellular network
 * to toggle a GPIO on the cellular module with high accuracy or,
 * conversely, to measure the time that a GPIO was toggled with
 * high accuracy.  In other words, the functions are about
 * timING, using an arbitrary time-base, and NOT about absolute
 * clock/calender time.  This API is only currently supported by
 * SARA-R5 modules.
 *
 * These functions are thread-safe with the proviso that a cellular
 * instance should not be accessed before it has been added or after
 * it has been removed.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_CELL_TIME_PULSE_PERIOD_SECONDS
/** The period of a time pulse from the cellular module, range 0
 * (which equates to 0.5 seconds) to 4 seconds, only used
 * for #U_CELL_TIME_MODE_PULSE.
 */
# define U_CELL_TIME_PULSE_PERIOD_SECONDS 1
#endif

#ifndef U_CELL_TIME_PULSE_WIDTH_MILLISECONDS
/** The width of a time pulse from the cellular module, range 0 to
 * 490 milliseconds if #U_CELL_TIME_PULSE_PERIOD_SECONDS is 0, else
 * range 0 to 990 milliseconds; only used for #U_CELL_TIME_MODE_PULSE.
 */
# define U_CELL_TIME_PULSE_WIDTH_MILLISECONDS 100
#endif

/** The number of seconds between 1st Jan 1970 and 1st Jan 2018;
 * add this to the timeNanoseconds of #uCellTime_t when cellTime
 * is true to convert the arbitrary CellTime time-base to Unix time.
 */
#define U_CELL_TIME_CONVERT_TO_UNIX_SECONDS 1514764800ULL

#ifndef U_CELL_TIME_SYNC_MODE
/** The sync mode used by uCellTimeSyncCellEnable(): 1 includes sending
 * a RACH, 2 does not.
 */
# define U_CELL_TIME_SYNC_MODE 1
#endif

#ifndef U_CELL_TIME_SYNC_TIME_SECONDS
/** A guard time-out value for uCellTimeSyncCellEnable().
 */
# define U_CELL_TIME_SYNC_TIME_SECONDS 30
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The possible modes that CellTime can operate in.
 */
typedef enum {
    U_CELL_TIME_MODE_OFF = 0,               /**< this mode cannot be set, it is
                                                 a _result_ of calling
                                                 uCellTimeDisable(). */
    U_CELL_TIME_MODE_PULSE = 1,             /**< time pulses will be emitted
                                                 on a pin of the cellular module. */
    U_CELL_TIME_MODE_ONE_SHOT = 2,          /**< time synchronisation is a one-shot
                                                 pulse on a pin of the cellular
                                                 module, plus a timestamp URC
                                                 will also be emitted. */
    U_CELL_TIME_MODE_EXT_INT_TIMESTAMP = 3, /**< a timestamp URC will be emitted
                                                 when the EXT_INT pin of the cellular
                                                 module is asserted (see
                                                 uCellTimeSetCallback()). */
    U_CELL_TIME_MODE_BEST_EFFORT = 4        /**< best effort GNSS/RTC/cellular time;
                                                 this mode cannot be set, it is a
                                                 _result_ of calling uCellTimeEnable()
                                                 with cellTimeOnly set to false. */
} uCellTimeMode_t;

/** A structure to contain the time as returned by the URC +UUTIME,
 * used by the callback of uCellTimeSetCallback().
 */
typedef struct {
    bool cellTime;                 /**< true if timeNanoseconds is a high
                                        accuracy relative time, derived from
                                        the timing of the cellular network,
                                        else timeNanoseconds is derived
                                        from GNSS/RTC (and can be treated as UTC). */
    int64_t timeNanoseconds;       /**< the time in nanoseconds.  If cellTime
                                        is true the value is relative, including
                                        any offset passed to uCellTimeEnable(),
                                        else it is UTC Unix time i.e. since
                                        midnight on 1st Jan 1970.  If you wish
                                        to convert the relative CellTime to the
                                        Unix time-base, you can do so by adding to it
                                        #U_CELL_TIME_CONVERT_TO_UNIX_SECONDS * 1000000000;
                                        of course this does NOT make it UTC, just
                                        a Unix time near the start of 2018. */
    int64_t accuracyNanoseconds;   /**< the accuracy of timeNanoseconds in
                                        nanoseconds. */
} uCellTime_t;

/** The possible sources of time synchronisation for CellTime.
 */
typedef enum {
    U_CELL_TIME_SOURCE_INIT = 0, /**< just starting up, no source yet. */
    U_CELL_TIME_SOURCE_GNSS = 1, /**< synchronisation achieved using GNSS,
                                      time will be UTC. */
    U_CELL_TIME_SOURCE_CELL = 2, /**< synchronisation achieved using the
                                      cellular network, time will be
                                      much more accurate but will be of
                                      an arbitrary base, not UTC. */
    U_CELL_TIME_SOURCE_RTC = 3   /**< synchronisation achieved using the
                                      RTC, time will be UTC. */
} uCellTimeSource_t;

/** The possible results of a CellTime operation.
 */
typedef enum {
    U_CELL_TIME_RESULT_SUCCESS = 0,         /**< all done, no error. */
    U_CELL_TIME_RESULT_UTC_ALIGNMENT = 1,   /**< UTC alignment has
                                                 been achieved, the
                                                 offsetNanoseconds element
                                                 of #uCellTimeEvent_t will
                                                 contain the timing
                                                 discontinuity that
                                                 resulted. */
    U_CELL_TIME_RESULT_OFFSET_DETECTED = 2, /**< an offset has been
                                                 detected in cellular
                                                 timing, the
                                                 offsetNanoseconds
                                                 element of
                                                 #uCellTimeEvent_t will
                                                 contain the offset. */
    U_CELL_TIME_RESULT_TIMEOUT = 3,
    U_CELL_TIME_RESULT_GPIO_ERROR = 4,
    U_CELL_TIME_RESULT_SYNC_LOST = 5        /** synchronisation with
                                                the cellular network
                                                has been lost, time
                                                is no longer valid. */
} uCellTimeResult_t;

/** A structure to contain a CellTime event, mostly the contents of the
 * URC +UUTIMEIND, used by the callback of uCellTimeEnable().
 */
typedef struct {
    bool synchronised;        /**< true if synchronisation has been
                                   achieved. */
    uCellTimeResult_t result; /**< the, possibly intermediate, result of
                                   a CellTime operation. */
    uCellTimeMode_t mode;     /**< the mode that CellTime is currently
                                   operating in. */
    uCellTimeSource_t source; /**< the source currently used for timing. */
    int32_t cellIdPhysical;   /**< the physical cell ID of the serving
                                   cell; only populated if source is
                                   #U_CELL_TIME_SOURCE_CELL, -1 otherwise. */
    bool cellTime;            /**< true if high-accuracy timing, derived
                                   from that of the cellular network,
                                   has been achieved, else the timing is
                                   best-effort, derived from GNSS/RTC. */
    int64_t offsetNanoseconds;/**< may be populated when the result field
                                   indicates that a discontinuity in
                                   cellular timing has been detected
                                   (#U_CELL_TIME_RESULT_UTC_ALIGNMENT or
                                   #U_CELL_TIME_RESULT_OFFSET_DETECTED); a
                                   value of LLONG_MIN is used to indicate
                                   "not present". */
} uCellTimeEvent_t;

/* ----------------------------------------------------------------
 * FUNCTIONS: CELLTIME
 * -------------------------------------------------------------- */

/** Enable CellTime, only supported on SARA-R5.  CellTime is about
 * using the highly accurate cellular network for timing of hardware,
 * using an arbitrary time-base, it is NOT about absolute clock/calender
 * time (UTC or local).
 *
 * If this function returns success it doesn't necessarily mean that
 * the requested CellTime operation has succeeded, since the operation
 * may take a while to complete: please monitor pCallback for progress
 * and for the final outcome; look for the synchronised field of
 * #uCellTimeEvent_t being set to true to indicate "done".
 *
 * You may call this function repeatedly provided the mode doesn't
 * change; if you want to change mode please call uCellTimeDisable()
 * first.
 *
 * Any time pulse will appear on pin number/GPIO ID 19, pin name
 * "GPIO6" of the cellular module.  If the mode is
 * #U_CELL_TIME_MODE_PULSE then for SARA-R5xx-00B the pulse width
 * is fixed at 3 ms and the period 1 second while for SARA-R5xx-01B and
 * later the pulse will be of duration #U_CELL_TIME_PULSE_WIDTH_MILLISECONDS
 * and period #U_CELL_TIME_PULSE_PERIOD_SECONDS.  If the mode is
 * #U_CELL_TIME_MODE_EXT_INT_TIMESTAMP then the input pin is
 * the "EXT_INT" pin of the cellular module, pin number/GPIO ID 33.
 *
 * If the GNSS device is external to the cellular module, two additional
 * pins must be connected: pin number/GPIO ID 46, pin name "SDIO_CMD",
 * must be connected to the GNSS device TIMEPULSE output and pin
 * number/GPIO ID 25, pin name "GPIO4", must be conncted to the
 * GNSS device EXTINT output.
 *
 * If uCellGpioConfig() had previously been called to use the pins
 * in question as user-controllable pins, this will override that
 * setting.
 *
 * If the GNSS device available to the cellular module is already in
 * use for something else (e.g. used by the GNSS API or by Cell Locate)
 * then it cannot also be used for CellTime and hence an error may be
 * returned if cellTimeOnly is set to false.
 *
 * If you have compiled with U_CFG_PPP_ENABLE then calling this function,
 * which necessarily disconnects from the network, will disconnect PPP
 * and it will not be re-enabled until a new network connection is made,
 * e.g. by calling uCellNetConnect().
 *
 * @param cellHandle                 the handle of the cellular instance.
 * @param mode                       the mode that CellTime should operate in,
 *                                   must be one of #U_CELL_TIME_MODE_PULSE,
 *                                   #U_CELL_TIME_MODE_ONE_SHOT or
 *                                   #U_CELL_TIME_MODE_EXT_INT_TIMESTAMP.
 * @param cellTimeOnly               if true then only the cellular network is
 *                                   used to provide highly accurate CellTime,
 *                                   else GNSS/RTC may also be used to provide
 *                                   a best-effort result.
 * @param offsetNanoseconds          an offset to be added to the timing of
 *                                   the time pulse in nanoseconds when it is
 *                                   a high accuracy, but relative, CellTime,
 *                                   derived from the cellular network; not
 *                                   used when cellTime is reported as false.
 *                                   To align CellTime with Unix time, set this to
 *                                   #U_CELL_TIME_CONVERT_TO_UNIX_SECONDS * 1000000000.
 * @param[in] pCallback              pointer to the function to monitor the
 *                                   outcome of the CellTime operation, where
 *                                   the first parameter is the handle of the
 *                                   cellular device, the second parameter
 *                                   is a pointer to the latest result (the
 *                                   contents of which MUST be copied by the
 *                                   callback as it will no longer be valid
 *                                   once the callback has returned) and the
 *                                   third parameter is the value of
 *                                   pCallbackParameter; may be NULL.  NOTE
 *                                   that this callback may be called both
 *                                   BEFORE and AFTER uCellTimeEnable() has
 *                                   returned and hence both pCallback and
 *                                   pCallbackParameter must remain valid at
 *                                   least until uCellTimeDisable() is called.
 * @param[in,out] pCallbackParameter a pointer to be passed to pCallback
 *                                   as its third parameter; may be NULL,
 *                                   must remain valid at least until
 *                                   uCellTimeDisable() is called.
 * @return                           zero on success else negative error code.
 */
int32_t uCellTimeEnable(uDeviceHandle_t cellHandle,
                        uCellTimeMode_t mode, bool cellTimeOnly,
                        int64_t offsetNanoseconds,
                        void (*pCallback) (uDeviceHandle_t,
                                           uCellTimeEvent_t *,
                                           void *),
                        void *pCallbackParameter);

/** Disable CellTime.  After this function has returned, the callbacks
 * passed to uCellTimeEnable() and uCellTimeSetCallback() will no longer
 * be called, i.e. you must call uCellTimeSetCallback() again if you
 * decide to re-enable CellTime.
 *
 * Note that this does not change the pin configurations that
 * may have been set by uCellTimeEnable(); should you wish to use
 * any of them as user-controllable pins once more, you must call
 * uCellGpioConfig() to do so.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            zero on success else negative error code.
 */
int32_t uCellTimeDisable(uDeviceHandle_t cellHandle);

/** Set a callback which will be called when time has been received
 * in a +UUTIME URC.  Only relevant when operating in modes
 * #U_CELL_TIME_MODE_ONE_SHOT or #U_CELL_TIME_MODE_EXT_INT_TIMESTAMP.
 *
 * @param cellHandle                 the handle of the cellular instance.
 * @param[in] pCallback              pointer to the function to handle any
 *                                   time-keeping status changes, where the
 *                                   first parameter is the handle of the
 *                                   cellular device, the second parameter
 *                                   is a pointer to the new time information
 *                                   (the contents of which MUST be copied by
 *                                   the callback as it will no longer be valid
 *                                   once the callback has returned) and the
 *                                   third parameter is the value of
 *                                   pCallbackParameter.  Use NULL to cancel
 *                                   an existing callback.
 * @param[in,out] pCallbackParameter a pointer to be passed to pCallback
 *                                   as its third parameter; may be NULL.
 * @return                           zero on success else negative error code.
 */
int32_t uCellTimeSetCallback(uDeviceHandle_t cellHandle,
                             void (*pCallback) (uDeviceHandle_t,
                                                uCellTime_t *,
                                                void *),
                             void *pCallbackParameter);

/** Force the cellular module to synchronize to a specific cell
 * of a specific MNO for CellTime purposes, for example using one
 * of the cells returned by uCellNetDeepScan(); supported on
 * SARA-R5xx-01B and later.  When this has returned successfully
 * uCellTimeEnable() may be called to perform HW timing based on
 * that cell.
 *
 * By default a RACH is sent as part of the synchronisation; see
 * #U_CELL_TIME_SYNC_MODE if you wish to change this.  A guard
 * timer of #U_CELL_TIME_SYNC_TIME_SECONDS is applied.
 *
 * No SIM is required/used, since there is no network connection,
 * and hence there is no limitation as to network operator choice.
 *
 * Note that calling this function will de-register/disconnect the
 * cellular module from the network WHATEVER the result, e.g. even
 * if synchronisation fails; it is up to you to re-connect to
 * the network by calling uCellNetConnect() afterwards.
 *
 * @param cellHandle             the handle of the cellular instance.
 * @param[in] pCell              a pointer to the cell to synchronize
 *                               with, cannot be NULL.
 * @param[in,out] pTimingAdvance on entry this should point to the
 *                               timing advance to use with the cell,
 *                               -1 or a NULL pointer if the timing
 *                               advance is not known.  On return, if
 *                               this pointer it not NULL it will be
 *                               set to the timing advance that was
 *                               employed with the cell, or -1 if the
 *                               timing advance cannot be established,
 *                               e.g. due to RACH failure.
 * @return                       zero on success else negative error code.
 */
int32_t uCellTimeSyncCellEnable(uDeviceHandle_t cellHandle,
                                uCellNetCellInfo_t *pCell,
                                int32_t *pTimingAdvance);

/** Disable synchronisation to a specific cell.  This does NOT
 * restore the module to a connected/registered state: please call
 * uCellNetConnect() to do that.
 *
 * @param cellHandle the handle of the cellular instance.
 * @return           zero on success else negative error code.
 */
int32_t uCellTimeSyncCellDisable(uDeviceHandle_t cellHandle);

/* ----------------------------------------------------------------
 * FUNCTIONS: ALIASES OF THE TIME-RELATED FUNCTIONS OF CFG AND INFO
 * -------------------------------------------------------------- */

/** An alias of uCellInfoGetTimeUtc(); get the clock/calender UTC time.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            on success the Unix UTC time, else negative
 *                    error code.
 */
int64_t uCellTimeGetUtc(uDeviceHandle_t cellHandle);

/** An alias of uCellInfoGetTimeUtcStr(); get the clock/calender
 * UTC time as a string.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param[out] pStr   a pointer to size bytes of storage into which
 *                    the UTC time string will be copied.
 *                    Room should be allowed for a null terminator,
 *                    which will be added to terminate the string.
 *                    This pointer cannot be NULL.
 * @param size        the number of bytes available at pStr, including
 *                    room for a null terminator. Must be greater or equal
 *                    to 32 bytes.
 * @return            on success, the number of characters copied into
 *                    pStr NOT include the terminator (as strlen() would
 *                    return), on failure negative error code.
 */
int32_t uCellTimeGetUtcStr(uDeviceHandle_t cellHandle,
                           char *pStr, size_t size);

/** An alias of uCellInfoGetTime(); get the clock/calender local time.
 *
 * @param cellHandle             the handle of the cellular
 *                               instance.
 * @param[in] pTimeZoneSeconds   a place to put the time-zone
 *                               offset in seconds; may be NULL.
 * @return                       on success the local time in
 *                               seconds since midnight on 1st
 *                               Jan 1970 (Unix time but local
 *                               instead of UTC) else negative
 *                               error code.
 */
int64_t uCellTimeGet(uDeviceHandle_t cellHandle, int32_t *pTimeZoneSeconds);

/** An alias of uCellCfgSetTime(); set the clock/calender local time.
 *
 * @param cellHandle          the handle of the cellular instance.
 * @param timeLocal           the local time in seconds since midnight on
 *                            1st Jan 1970, (Unix time, but local rather
 *                            than UTC).
 * @param timeZoneSeconds     the time-zone offset of timeLocal in seconds; for
 *                            example, if you are one hour ahead of UTC
 *                            timeZoneSeconds would be 3600.
 * @return                    zero on success or negative error code on failure.
 */
int64_t uCellTimeSet(uDeviceHandle_t cellHandle, int64_t timeLocal,
                     int32_t timeZoneSeconds);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_CELL_TIME_H_

// End of file
