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

#ifndef _U_GNSS_DEC_UBX_NAV_HPPOSLLH_H_
#define _U_GNSS_DEC_UBX_NAV_HPPOSLLH_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup _GNSS
 *  @{
 */

/** @file
 * @brief This header file defines the types of a UBX-NAV-HPPOSLLH
 * message.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The message class of a UBX-NAV-HPPOSLLH message.
 */
#define U_GNSS_DEC_UBX_NAV_HPPOSLLH_MESSAGE_CLASS 0x01

/** The message ID of a UBX-NAV-HPPOSLLH message.
 */
#define U_GNSS_DEC_UBX_NAV_HPPOSLLH_MESSAGE_ID 0x14

/** The minimum length of the body of a UBX-NAV-HPPOSLLH message.
 */
#define U_GNSS_DEC_UBX_NAV_HPPOSLLH_BODY_MIN_LENGTH 36

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Bit fields of the "flags" field of #uGnssDecUbxNavHpposllh_t; use
 * these to mask specific bits, e.g.
 *
 * `if (flags & (1 << U_GNSS_DEC_UBX_NAV_HPPOSLLH_FLAGS_INVALID_LLH)) {`
 *
 * ...would determine if the lon, lat, height, hMSL, lonHp, latHp,
 * heightHp and hMSLHp fields are invalid.
 */
typedef enum {
    U_GNSS_DEC_UBX_NAV_HPPOSLLH_FLAGS_INVALID_LLH = 0 /**< the lon, lat,
                                                           height, hMSL,
                                                           lonHp, latHp,
                                                           heightHp and
                                                           hMSLHp fields
                                                           are invalid. */
} uGnssDecUbxNavHpposllhFlags_t;

/** UBX-NAV-HPPOSLLH message structure; the naming and type of each
 * element follows that of the interface manual.
 */
typedef struct {
    uint8_t version; /**< message version. */
    uint8_t flags;   /**< see #uGnssDecUbxNavHpposllhFlags_t. */
    uint32_t iTOW;   /**< GPS time of week of the navigation epoch
                          in milliseconds. */
    int32_t lon;     /**< longitude in degrees times 1e7; to get
                          high precision position from this
                          structure as a whole, see
                          uGnssDecUbxNavHpposllhGetPos(). */
    int32_t lat;     /**< latitude in degrees times 1e7; to get
                          high precision position from this
                          structure as a whole, see
                          uGnssDecUbxNavHpposllhGetPos(). */
    int32_t height;  /**< height above ellipsoid in mm; to get
                          high precision position from this
                          structure as a whole, see
                          uGnssDecUbxNavHpposllhGetPos(). */
    int32_t hMSL;    /**< height above mean sea level in mm; to get
                          high precision position from this
                          structure as a whole, see
                          uGnssDecUbxNavHpposllhGetPos(). */
    int8_t lonHp;    /**< high precision component of longitude;
                          add this to lon to get longitude in
                          degrees times 1e9, or alternatively
                          call uGnssDecUbxNavHpposllhGetPos() on
                          this structure to do it for you. */
    int8_t latHp;    /**< high precision component of latitude; add
                          this to lat to get latitude in degrees
                          times 1e9, or alternatively call
                          uGnssDecUbxNavHpposllhGetPos() on
                          this structure to do it for you. */
    int8_t heightHp; /**< high precision component of height above
                          ellipsoid; add this to height to get
                          height in tenths of a mm, or alternatively
                          call uGnssDecUbxNavHpposllhGetPos() on
                          this structure to do it for you. */
    int8_t hMSLHp;   /**< high precision component of height above
                          mean sea level; add this to hMSL to get
                          hMSL in tenths of a mm, or alternatively
                          call uGnssDecUbxNavHpposllhGetPos() on
                          this structure to do it for you. */
    uint32_t hAcc;   /**< horizontal accuracy estimate in mm. */
    uint32_t vAcc;   /**< vertical accuracy estimate in mm. */
} uGnssDecUbxNavHpposllh_t;

/** High precision position; may be populated by calling
 * uGnssDecUbxNavHpposllhGetPos() on #uGnssDecUbxNavHpposllh_t.
 */
typedef struct {
    int64_t longitudeX1e9;               /**< longitude in degrees times 1e9. */
    int64_t latitudeX1e9;                /**< latitude in degrees times 1e9. */
    int64_t heightMillimetresX1e1;       /**< height above ellipsoid in 10ths
                                              of a millimetre. */
    int64_t heightMeanSeaLevelMillimetresX1e1; /**< height above mean sea level
                                                    in 10ths of a millimetre. */
} uGnssDecUbxNavHpposllhPos_t;

/* ----------------------------------------------------------------
 * FUNCTIONS: HELPERS
 * -------------------------------------------------------------- */

/** Derive a high precision position structure from the components
 * of #uGnssDecUbxNavHpposllh_t.
 *
 * @param[in] pHpposllh  a pointer to a #uGnssDecUbxNavHpposllh_t
 *                       structure returned by pUGnssDecAlloc();
 *                       cannot be NULL.
 * @param[out] pPos      a pointer to a place to put the high
 *                       precision position; cannot be NULL.
 */
void uGnssDecUbxNavHpposllhGetPos(const uGnssDecUbxNavHpposllh_t *pHpposllh,
                                  uGnssDecUbxNavHpposllhPos_t *pPos);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_GNSS_DEC_UBX_NAV_HPPOSLLH_H_

// End of file
