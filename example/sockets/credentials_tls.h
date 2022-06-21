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

#ifndef _CREDENTIALS_TLS_H_
#define _CREDENTIALS_TLS_H_

/** @file
 * @brief Credentials to use with the main_tls.c example.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

// The credentials that may be required to talk to the uxblib echo server.
extern const char *gpUEchoServerClientCertPem;
extern const char gUEchoServerClientCertHash[];
extern const char *gpUEchoServerClientKeyPem;
extern const char gUEchoServerClientKeyHash[];
extern const char *gpUEchoServerServerCertPem;
extern const char gUEchoServerServerCertHash[];

#ifdef __cplusplus
}
#endif

#endif // _CREDENTIALS_TLS_H_

// End of file
