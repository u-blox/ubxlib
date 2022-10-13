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

#ifndef _U_HTTP_CLIENT_TEST_SHARED_CFG_H_
#define _U_HTTP_CLIENT_TEST_SHARED_CFG_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file defines macros used in HTTP Client API,
 * testing and the testing of the underlying HTTP technology-specific
 * APIs.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_HTTP_CLIENT_TEST_SERVER_DOMAIN_NAME
/** The domain name of the HTTP test server.
 */
# define U_HTTP_CLIENT_TEST_SERVER_DOMAIN_NAME  "ubxlib.it-sgn.u-blox.com"
#endif

#ifndef U_HTTP_CLIENT_TEST_SERVER_IP_ADDRESS
/** The HTTP test server as an IP address.
 */
# define U_HTTP_CLIENT_TEST_SERVER_IP_ADDRESS  "185.215.195.136"
#endif

#ifndef U_HTTP_CLIENT_TEST_SERVER_PORT
/** Port number for HTTP (i.e. non-secure) connections to the HTTP
 * test server.
 */
# define U_HTTP_CLIENT_TEST_SERVER_PORT  8080
#endif

#ifndef U_HTTP_CLIENT_TEST_SERVER_SECURE_PORT
/** Port number for HTTPS (i.e. secure) connections to the HTTP
 * test server.
 */
# define U_HTTP_CLIENT_TEST_SERVER_SECURE_PORT  8081
#endif

#endif // _U_HTTP_CLIENT_TEST_SHARED_CFG_H_

// End of file
