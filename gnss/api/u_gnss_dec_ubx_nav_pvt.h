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

#ifndef _U_GNSS_DEC_UBX_NAV_PVT_H_
#define _U_GNSS_DEC_UBX_NAV_PVT_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup _GNSS
 *  @{
 */

/** @file
 * @brief This header file defines the types of a UBX-NAV-PVT
 * message.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The message class of a UBX-NAV-PVT message.
 */
#define U_GNSS_DEC_UBX_NAV_PVT_MESSAGE_CLASS 0x01

/** The message ID of a UBX-NAV-PVT message.
 */
#define U_GNSS_DEC_UBX_NAV_PVT_MESSAGE_ID 0x07

/** The minimum length of the body of a UBX-NAV-PVT message.
 */
#define U_GNSS_DEC_UBX_NAV_PVT_BODY_MIN_LENGTH 36

/** Bit mask for the #U_GNSS_DEC_UBX_NAV_PVT_FLAGS_PSM_STATE field
 * of #uGnssDecUbxNavPvtFlags_t.
 */
#define U_GNSS_DEC_UBX_NAV_PVT_FLAGS_PSM_STATE_MASK (0x07 << U_GNSS_DEC_UBX_NAV_PVT_FLAGS_PSM_STATE)

/** Bit mask for the #U_GNSS_DEC_UBX_NAV_PVT_FLAGS_CARR_SOLN field
 * of #uGnssDecUbxNavPvtFlags_t.
 */
#define U_GNSS_DEC_UBX_NAV_PVT_FLAGS_CARR_SOLN_MASK (0x07 << U_GNSS_DEC_UBX_NAV_PVT_FLAGS_CARR_SOLN)

/** Bit mask for the #U_GNSS_DEC_UBX_NAV_PVT_FLAGS3_LAST_CORRECTION_AGE
 * field of #uGnssDecUbxNavPvtFlags_t.
 */
#define U_GNSS_DEC_UBX_NAV_PVT_FLAGS3_LAST_CORRECTION_AGE_MASK (0x1f << U_GNSS_DEC_UBX_NAV_PVT_FLAGS3_LAST_CORRECTION_AGE)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Bit fields of the "valid" field of #uGnssDecUbxNavPvt_t; use
 * these to mask specific bits, e.g.
 *
 * `if (valid & (1 << U_GNSS_DEC_UBX_NAV_PVT_VALID_MAG)) {`
 *
 * ...would determine if the "magDec" and "magAcc" fields were
 * valid/populated.
 */
typedef enum {
    U_GNSS_DEC_UBX_NAV_PVT_VALID_DATE = 0, /**< UTC date is valid. */
    U_GNSS_DEC_UBX_NAV_PVT_VALID_TIME = 1, /**< UTC time is valid. */
    U_GNSS_DEC_UBX_NAV_PVT_VALID_FULLY_RESOLVED = 2, /**< UTC time has
                                                          no seconds
                                                          uncertainty. */
    U_GNSS_DEC_UBX_NAV_PVT_VALID_MAG = 3 /**< magnetic declination
                                              (see "magDec" and "magAcc"
                                              fields of
                                              #uGnssDecUbxNavPvt_t) are
                                              valid. */
} uGnssDecUbxNavPvtValid_t;

/** Possible values of the "fixType" field of #uGnssDecUbxNavPvt_t.
 */
typedef enum {
    U_GNSS_DEC_UBX_NAV_PVT_FIX_TYPE_NO_FIX = 0,
    U_GNSS_DEC_UBX_NAV_PVT_FIX_TYPE_DEAD_RECKONING_ONLY = 1,
    U_GNSS_DEC_UBX_NAV_PVT_FIX_TYPE_2D = 2,
    U_GNSS_DEC_UBX_NAV_PVT_FIX_TYPE_3D = 3,
    U_GNSS_DEC_UBX_NAV_PVT_FIX_TYPE_GNSS_PLUS_DEAD_RECKONING = 4,
    U_GNSS_DEC_UBX_NAV_PVT_FIX_TYPE_TIME_ONLY = 5
} uGnssDecUbxNavPvtFixType_t;

/** Possible values of the #U_GNSS_DEC_UBX_NAV_PVT_FLAGS_PSM_STATE
 * bit-field of #uGnssDecUbxNavPvtFlags_t.  To obtain the
 * enum, do as follows:
 *
 * `(flags & (U_GNSS_DEC_UBX_NAV_PVT_FLAGS_PSM_STATE_MASK)) >> U_GNSS_DEC_UBX_NAV_PVT_FLAGS_PSM_STATE`
 */
typedef enum {
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS_PSM_STATE_NOT_ACTIVE = 0,
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS_PSM_STATE_ENABLED = 1,
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS_PSM_STATE_ACQUISITION = 2,
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS_PSM_STATE_TRACKING = 3,
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS_PSM_STATE_POWER_OPTIMIZED_TRACKING = 4,
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS_PSM_STATE_INACTIVE = 5
} uGnssDecUbxNavPvtFlagsPsmState_t;

/** Possible values of the #U_GNSS_DEC_UBX_NAV_PVT_FLAGS_CARR_SOLN
 * bit-field of #uGnssDecUbxNavPvtFlags_t.  To obtain the
 * enum, do as follows:
 *
 * `(flags & (U_GNSS_DEC_UBX_NAV_PVT_FLAGS_CARR_SOLN_MASK)) >> U_GNSS_DEC_UBX_NAV_PVT_FLAGS_CARR_SOLN`
 *
 * Carrier phase range solution may also be referred to as
 * RTK, Real Time Kinematics.
 */
typedef enum {
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS_CARR_SOLN_NONE = 0,  /**< no carrier phase
                                                           range solution. */
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS_CARR_SOLN_FLOAT = 1, /**< carrier phase range
                                                           solution with floating
                                                           ambiguities. */
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS_CARR_SOLN_FIXED = 2  /**< carrier phase range
                                                           solution with fixed
                                                           ambiguities. */
} uGnssDecUbxNavPvtFlagsCarrSoln_t;

/** Bit fields of the "flags" field of #uGnssDecUbxNavPvt_t; use
 * these to mask specific bits, e.g.
 *
 * `if (flags & (1 << U_GNSS_DEC_UBX_NAV_PVT_FLAGS_GNSS_FIX_OK)) {`
 *
 * ...would determine if a fix had been achieved.  Note that
 * the fields #U_GNSS_DEC_UBX_NAV_PVT_FLAGS_PSM_STATE and
 * #U_GNSS_DEC_UBX_NAV_PVT_FLAGS_CARR_SOLN are wider than a
 * single bit.
 */
typedef enum {
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS_GNSS_FIX_OK = 0, /**< fix is within DOP
                                                       and accuracy masks. */
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS_DIFF_SOLN = 1, /**< differential corrections
                                                     were applied. */
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS_PSM_STATE = 2, /**< not a single bit,
                                                     the start of a 3-bit
                                                     field, use
                                                     #U_GNSS_DEC_UBX_NAV_PVT_FLAGS_PSM_STATE_MASK
                                                     to mask it, this to shift it
                                                     down and then it will
                                                     map to
                                                     #uGnssDecUbxNavPvtFlagsPsmState_t. */
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS_HEAD_VEH_VALID = 5, /**< headVeh field of
                                                          #uGnssDecUbxNavPvt_t
                                                          is valid, only set if the
                                                          receiver is in sensor
                                                          fusion mode. */
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS_CARR_SOLN = 6  /**< not a single bit,
                                                     the start of a 2-bit
                                                     field, use
                                                     #U_GNSS_DEC_UBX_NAV_PVT_FLAGS_CARR_SOLN_MASK
                                                     to mask it, this to shift it
                                                     down and then it will
                                                     map to
                                                     #uGnssDecUbxNavPvtFlagsCarrSoln_t. */
} uGnssDecUbxNavPvtFlags_t;

/** Bit fields of the "flags2" field of #uGnssDecUbxNavPvt_t; use
 * these to mask specific bits, e.g.
 *
 * `if (flags2 & (1 << U_GNSS_DEC_UBX_NAV_PVT_FLAGS2_CONFIRMED_TIME)) {`
 *
 * ...would determine if UTC time of day validity was confirmed.
 */
typedef enum {
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS2_CONFIRMED_AVAI = 5, /**< information about UTC
                                                           time and date validity
                                                           is available. */
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS2_CONFIRMED_DATE = 6, /**< UTC date validity is
                                                           confirmed. */
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS2_CONFIRMED_TIME = 7  /**< UTC time of day validity
                                                           is confirmed. */
} uGnssDecUbxNavPvtFlags2_t;

/** Possible values of the #U_GNSS_DEC_UBX_NAV_PVT_FLAGS3_LAST_CORRECTION_AGE
 * bit-field of #uGnssDecUbxNavPvtFlags3_t.  To obtain the
 * enum, do as follows:
 *
 * `(flags3 & (U_GNSS_DEC_UBX_NAV_PVT_FLAGS3_LAST_CORRECTION_AGE_MASK)) >> U_GNSS_DEC_UBX_NAV_PVT_FLAGS3_LAST_CORRECTION_AGE`
 *
 * Values above #U_GNSS_DEC_UBX_NAV_PVT_FLAGS_LAST_CORRECTION_AGE_120_SECONDS_OR_MORE
 * should be considered to be
 * #U_GNSS_DEC_UBX_NAV_PVT_FLAGS_LAST_CORRECTION_AGE_120_SECONDS_OR_MORE.
 */
typedef enum {
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS_LAST_CORRECTION_AGE_NOT_AVAILABLE = 0,
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS_LAST_CORRECTION_AGE_BETWEEN_0_AND_1_SECONDS = 1,
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS_LAST_CORRECTION_AGE_BETWEEN_1_AND_2_SECONDS = 2,
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS_LAST_CORRECTION_AGE_BETWEEN_2_AND_5_SECONDS = 3,
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS_LAST_CORRECTION_AGE_BETWEEN_5_AND_10_SECONDS = 4,
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS_LAST_CORRECTION_AGE_BETWEEN_10_AND_15_SECONDS = 5,
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS_LAST_CORRECTION_AGE_BETWEEN_15_AND_20_SECONDS = 6,
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS_LAST_CORRECTION_AGE_BETWEEN_20_AND_30_SECONDS = 7,
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS_LAST_CORRECTION_AGE_BETWEEN_30_AND_45_SECONDS = 8,
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS_LAST_CORRECTION_AGE_BETWEEN_45_AND_60_SECONDS = 9,
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS_LAST_CORRECTION_AGE_BETWEEN_60_AND_90_SECONDS = 10,
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS_LAST_CORRECTION_AGE_BETWEEN_90_AND_120_SECONDS = 11,
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS_LAST_CORRECTION_AGE_120_SECONDS_OR_MORE = 12
} uGnssDecUbxNavPvtFlags3LastCorrectionAge_t;

/** Bit fields of the "flags3" field of #uGnssDecUbxNavPvt_t; use
 * these to mask specific bits, e.g.
 *
 * `if (flags3 & (1 << U_GNSS_DEC_UBX_NAV_PVT_FLAGS3_INVALID_LLH)) {`
 *
 * ...would determine if the lon, lat, height and hMSL fields
 * are invalid, though note that the field
 * #U_GNSS_DEC_UBX_NAV_PVT_FLAGS3_LAST_CORRECTION_AGE is wider than a
 * single bit.
 */
typedef enum {
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS3_INVALID_LLH = 0, /**< the lon, lat, height and
                                                        hMSL fields are invalid. */
    U_GNSS_DEC_UBX_NAV_PVT_FLAGS3_LAST_CORRECTION_AGE = 1  /**< not a single bit,
                                                                the start of a 5-bit
                                                                field, use
                                                                #U_GNSS_DEC_UBX_NAV_PVT_FLAGS3_LAST_CORRECTION_AGE_MASK
                                                                to mask it, this to shift it
                                                                down and then it will map to
                                                                #uGnssDecUbxNavPvtFlags3LastCorrectionAge_t. */
} uGnssDecUbxNavPvtFlags3_t;

/** UBX-NAV-PVT message structure; the naming and type of each
 * element follows that of the interface manual.
 */
typedef struct {
    uint32_t iTOW;                       /**< GPS time of week of the
                                              navigation epoch
                                              in milliseconds. */
    uint16_t year;                       /**< year (UTC); to obtain this and
                                              the other time-related fields
                                              in this structure as a Unix-based
                                              UTC timestamp, see
                                              uGnssDecUbxNavPvtGetTimeUtc(). */
    uint8_t month;                       /**< month, range 1 to 12 (UTC). */
    uint8_t day;                         /**< day of month, range 1 to 31 (UTC). */
    uint8_t hour;                        /**< hour of day, range 0 to 23 (UTC). */
    uint8_t min;                         /**< minute of hour, range 0 to 59 (UTC). */
    uint8_t sec;                         /**< seconds of minute, range 0 to 60 (UTC). */
    uint8_t valid;                       /**< validity flags, see
                                              #uGnssDecUbxNavPvtValid_t. */
    uint32_t tAcc;                       /**< time accuracy estimate in
                                              nanoseconds. */
    int32_t nano;                        /**< fractional seconds part of UTC
                                              time in nanoseconds. */
    uGnssDecUbxNavPvtFixType_t fixType;  /**< the fix type achieved. */
    uint8_t flags;                       /**< see #uGnssDecUbxNavPvtFlags_t. */
    uint8_t flags2;                      /**< see #uGnssDecUbxNavPvtFlags2_t. */
    uint8_t numSV;                       /**< the number of satellites used. */
    int32_t lon;                         /**< longitude in degrees times 1e7. */
    int32_t lat;                         /**< latitude in degrees times 1e7. */
    int32_t height;                      /**< height above ellipsoid in mm. */
    int32_t hMSL;                        /**< height above mean sea level in mm. */
    uint32_t hAcc;                       /**< horizontal accuracy estimate in mm. */
    uint32_t vAcc;                       /**< vertical accuracy estimate in mm. */
    int32_t velN;                        /**< NED north velocity in mm/second. */
    int32_t velE;                        /**< NED east velocity in mm/second. */
    int32_t velD;                        /**< NED down velocity in mm/second. */
    int32_t gSpeed;                      /**< 2D ground speed in mm/second. */
    int32_t headMot;                     /**< 2D heading of motion in degrees times 1e5. */
    uint32_t sAcc;                       /**< speed accuracy estimate in mm/second. */
    uint32_t headAcc;                    /**< heading accuracy estimate (motion and
                                              vehicle) in degrees times 1e5. */
    uint16_t pDOP;                       /**< position DOP times 100. */
    uint16_t flags3;                     /**< see #uGnssDecUbxNavPvtFlags3_t. */
    int32_t headVeh;                     /**< if the #U_GNSS_DEC_UBX_NAV_PVT_FLAGS_HEAD_VEH_VALID
                                              bit of the flags field is set
                                              then this is the 2D vehicle
                                              heading in degrees times 1e5,
                                              else it is set to the same
                                              value as headMot. */
    int16_t magDec;                      /**< if the #U_GNSS_DEC_UBX_NAV_PVT_VALID_MAG
                                              bit of the valid field is set then this
                                              is the magnetic declination in degrees
                                              times 100; only supported on ADR 4.10
                                              and later. */
    uint16_t magAcc;                     /**< if the #U_GNSS_DEC_UBX_NAV_PVT_VALID_MAG
                                              bit of the valid field is set then this
                                              is the accuracy of the magnetic
                                              declination in degrees times 100; only
                                              supported on ADR 4.10 and later. */
} uGnssDecUbxNavPvt_t;

/* ----------------------------------------------------------------
 * FUNCTIONS: HELPERS
 * -------------------------------------------------------------- */

/** Derive Unix time (in nanoseconds) from the components of
 * #uGnssDecUbxNavPvt_t.
 *
 * @param[in] pPvt  a pointer to a #uGnssDecUbxNavPvt_t structure
 *                  returned by pUGnssDecAlloc(); cannot be NULL.
 * @return          on success, the UTC time in nanoseconds using
 *                  the Unix time-base of midnight on 1st Jan 1970,
 *                  else negative error code.
 */
int64_t uGnssDecUbxNavPvtGetTimeUtc(const uGnssDecUbxNavPvt_t *pPvt);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_GNSS_DEC_UBX_NAV_PVT_H_

// End of file
