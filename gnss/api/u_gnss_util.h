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

#ifndef _U_GNSS_UTIL_H_
#define _U_GNSS_UTIL_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_ringbuffer.h"

/** \addtogroup _GNSS
 *  @{
 */

/** @file
 * @brief This header file defines the utility functions of the GNSS API.
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

/** \deprecated Send a command of your choosing to the GNSS chip and,
 * optionally, wait for the response.  This function is deprecated,
 * using it could cause loss of data required by other parts of this
 * code: please instead use the uGnssMsg API, where no such danger
 * exists.
 *
 * @param gnssHandle             the handle of the GNSS instance.
 * @param[in] pCommand           the command to send; may be NULL.
 * @param commandLengthBytes     the amount of data at pCommand; must
 *                               be non-zero if pCommand is non-NULL.
 * @param[out] pResponse         a pointer to somewhere to store the
 *                               response, if one is expected; may
 *                               be NULL.
 * @param maxResponseLengthBytes the amount of storage at pResponse;
 *                               must be non-zero if pResponse is non-NULL.
 * @return                       on success the number of bytes copied
 *                               into pResponse (zero if pResponse is
 *                               NULL), else negative error code.
 */
int32_t uGnssUtilUbxTransparentSendReceive(uDeviceHandle_t gnssHandle,
                                           const char *pCommand,
                                           size_t commandLengthBytes,
                                           char *pResponse,
                                           size_t maxResponseLengthBytes);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_GNSS_UTIL_H_

// End of file
