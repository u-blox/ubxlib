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

#ifndef _U_GNSS_CFG_H_
#define _U_GNSS_CFG_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup _GNSS
 *  @{
 */

/** @file
 * @brief This header file defines the GNSS APIs to configure a GNSS chip.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Get the dynamic platform model from the GNSS chip.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @return            the number of the dynamic platform model or
 *                    negative error code.
 */
int32_t uGnssCfgGetDynamic(uDeviceHandle_t gnssHandle);

/** Set the dynamic platform model of the GNSS chip.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @param dynamic     the number of the dynamic platform model; the
 *                    value is deliberately not range-checked to allow
 *                    future dynamic platform models to be passed
 *                    in without the requirement to modify this code.
 * @return            zero on succes or negative error code.
 */
int32_t uGnssCfgSetDynamic(uDeviceHandle_t gnssHandle, uGnssDynamic_t dynamic);

/** Get the fix mode from the GNSS chip.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @return            the fix mode or negative error code.
 */
int32_t uGnssCfgGetFixMode(uDeviceHandle_t gnssHandle);

/** Set the fix mode of the GNSS chip.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @param fixMode     the fix mode; the value is deliberately not
 *                    range-checked to allow future fix modes to be
 *                    passed in without the requirement to modify
 *                    this code.
 * @return            zero on succes or negative error code.
 */
int32_t uGnssCfgSetFixMode(uDeviceHandle_t gnssHandle, uGnssFixMode_t fixMode);

/** Get the UTC standard from the GNSS chip.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @return            the UTC standard or negative error code.
 */
int32_t uGnssCfgGetUtcStandard(uDeviceHandle_t gnssHandle);

/** Set the UTC standard of the GNSS chip.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @param utcStandard the UTC standard; the value is deliberately not
 *                    range-checked to allow future UTC standards to be
 *                    passed in without the requirement to modify
 *                    this code.  Use #U_GNSS_UTC_STANDARD_AUTOMATIC
 *                    it you don't really care, you'd just like UTC
 *                    time please (which is the default).
 * @return            zero on succes or negative error code.
 */
int32_t uGnssCfgSetUtcStandard(uDeviceHandle_t gnssHandle,
                               uGnssUtcStandard_t utcStandard);

/** Get the protocol types output by the GNSS chip; not relevant
 * where an AT transport is in use since only the ubx protocol is
 * currently supported through that transport.
 *
 * @param gnssHandle the handle of the GNSS instance.
 * @return           a bit-map of the protocol types that are
 *                   being output else negative error code.
 */
int32_t uGnssCfgGetProtocolOut(uDeviceHandle_t gnssHandle);

/** Set the protocol type output by the GNSS chip; not relevant
 * where an AT transport is in use since only the ubx protocol is
 * currently supported through that transport.
 *
 * @param gnssHandle the handle of the GNSS instance.
 * @param protocol   the protocol type; #U_GNSS_PROTOCOL_ALL may
 *                   be used to enable all of the output protocols
 *                   supported by the GNSS chip (though using this
 *                   with onNotOff set to false will return an error).
 *                   ubx protocol output cannot be switched off
 *                   since it is used by this code. The range of
 *                   the parameter is NOT checked, hence you may set
 *                   a value which is known to the GNSS chip but not
 *                   to this code.
 * @param onNotOff   whether the given protocol should be on or off.
 * @return           zero on succes or negative error code.
 */
int32_t uGnssCfgSetProtocolOut(uDeviceHandle_t gnssHandle,
                               uGnssProtocol_t protocol,
                               bool onNotOff);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_GNSS_CFG_H_

// End of file
