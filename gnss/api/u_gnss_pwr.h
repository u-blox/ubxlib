/*
 * Copyright 2020 u-blox Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _U_GNSS_PWR_H_
#define _U_GNSS_PWR_H_

/* No #includes allowed here */

/** @file
 * @brief This header file defines the GNSS APIs to control the power
 * state of a GNSS chip.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_GNSS_POWER_UP_TIME_MILLISECONDS
/** How long to wait for a GNSS chip to be available after it is
 * powered up.
 */
# define U_GNSS_POWER_UP_TIME_MILLISECONDS 2000
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Power a GNSS chip on.  If the transport type for the given GNSS
 * instance is U_GNSS_TRANSPORT_UBX_AT then you must have powered
 * the associated cellular module up (e.g. with a call to uNetworkAdd()
 * or uCellPwrOn()) before calling this function.
 *
 * @param gnssHandle  the handle of the GNSS instance to power on.
 */
int32_t uGnssPwrOn(int32_t gnssHandle);

/** Check that a GNSS chip is responsive.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 */
bool uGnssPwrIsAlive(int32_t gnssHandle);

/** Power a GNSS chip off
 *
 * @param gnssHandle  the handle of the GNSS instance to power off.
 */
int32_t uGnssPwrOff(int32_t gnssHandle);

#ifdef __cplusplus
}
#endif

#endif // _U_GNSS_PWR_H_

// End of file
