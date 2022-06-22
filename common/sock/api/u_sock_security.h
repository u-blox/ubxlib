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

#ifndef _U_SOCK_SECURITY_H_
#define _U_SOCK_SECURITY_H_

/* This file breaks the usual inclusion rules: u_security_tls.h
 * is an internal API that this API hides, hence it is allowed to
 * be included here.
 */

#include "u_security_tls.h"

/** \addtogroup sock
 *  @{
 */

/** @file
 * @brief This header file defines the security portion of the sockets API.
 * This is placed in a separate header to "u_sock.h" for
 * backwards-compatibility reasons; non-secure applications
 * which include "u_sock.h" would otherwise have to include
 * "u_security_tls.h" for no reason.  This function is thread-safe.
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

/** Add security to the given socket.  Should be called before
 * uSockConnect() or uSockSendTo().
 *
 * @param descriptor  the descriptor of the socket.
 * @param pSettings   a pointer to the security settings to apply.
 * @return            zero on success else negative error code
 *                    (and errno will also be set to a value
 *                    from u_sock_errno.h).
 */
int32_t uSockSecurity(uSockDescriptor_t descriptor,
                      const uSecurityTlsSettings_t *pSettings);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_SOCK_SECURITY_H_

// End of file
