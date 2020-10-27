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

#ifndef _U_SOCK_TEST_CFG_H_
#define _U_SOCK_TEST_CFG_H_

/* No #includes allowed here */

/** @file
 * @brief This header file defines macros used in sockets API,
 * network API testing, and testing of the underlying sockets APIs.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_SOCK_TEST_ECHO_UDP_SERVER_DOMAIN_NAME
/** Echo server to use for UDP sockets testing as a domain name.
 */
# define U_SOCK_TEST_ECHO_UDP_SERVER_DOMAIN_NAME  "echo.u-blox.com"
#endif

#ifndef U_SOCK_TEST_ECHO_UDP_SERVER_IP_ADDRESS
/** Echo server to use for UDP sockets testing as an IP address.
 */
# define U_SOCK_TEST_ECHO_UDP_SERVER_IP_ADDRESS  "195.34.89.241"
#endif

#ifndef U_SOCK_TEST_ECHO_UDP_SERVER_PORT
/** Port number on the echo server to use for UDP testing.
 */
# define U_SOCK_TEST_ECHO_UDP_SERVER_PORT  7
#endif

#ifndef U_SOCK_TEST_ECHO_TCP_SERVER_DOMAIN_NAME
/** Echo server to use for TCP sockets testing as a domain name.
 * (note: the u-blox one adds a prefix to the echoed TCP packets
 * which is undesirable; mbed don't seem to mind us using theirs).
 */
# define U_SOCK_TEST_ECHO_TCP_SERVER_DOMAIN_NAME  "echo.mbedcloudtesting.com"
#endif

#ifndef U_SOCK_TEST_ECHO_TCP_SERVER_IP_ADDRESS
/** Echo server to use for TCP sockets testing as an IP address.
 * (note: the u-blox one adds a prefix to the echoed TCP packets
 * which is undesirable; mbed don't seem to mind us using theirs).
 */
# define U_SOCK_TEST_ECHO_TCP_SERVER_IP_ADDRESS  "52.215.34.155"
#endif

#ifndef U_SOCK_TEST_ECHO_TCP_SERVER_PORT
/** Port number on the echo server to use for TCP testing.
 */
# define U_SOCK_TEST_ECHO_TCP_SERVER_PORT  7
#endif

#ifndef U_SOCK_TEST_LOCAL_PORT
/** Local port number, used when testing binding.
 */
# define U_SOCK_TEST_LOCAL_PORT 65543
#endif

#ifndef U_SOCK_TEST_UDP_RETRIES
/** The number of retries to allow when sending
 * data over UDP.
 */
# define U_SOCK_TEST_UDP_RETRIES 10
#endif

#ifndef U_SOCK_TEST_TCP_CLOSE_SECONDS
/** Time to wait for a TCP socket to close, necessary
 * in the case where the underlying implementation
 * follows the TCP socket closure rules very
 * strictly (e.g. the SARA-R4 cellular modules to).
 */
# define U_SOCK_TEST_TCP_CLOSE_SECONDS 60
#endif

#endif // _U_SOCK_TEST_CFG_H_

// End of file
