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

#ifndef _U_SOCK_TEST_CFG_H_
#define _U_SOCK_TEST_CFG_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

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
# define U_SOCK_TEST_ECHO_UDP_SERVER_DOMAIN_NAME  "ubxlib.com"
#endif

#ifndef U_SOCK_TEST_ECHO_UDP_SERVER_IP_ADDRESS
/** Echo server to use for UDP sockets testing as an IP address.
 */
# define U_SOCK_TEST_ECHO_UDP_SERVER_IP_ADDRESS  "18.133.144.142"
#endif

#ifndef U_SOCK_TEST_ECHO_UDP_SERVER_PORT
/** Port number on the echo server to use for UDP testing.
 */
# define U_SOCK_TEST_ECHO_UDP_SERVER_PORT  5050
#endif

#ifndef U_SOCK_TEST_ECHO_SECURE_UDP_SERVER_DOMAIN_NAME
/** Echo server to use for secure (DTLS) UDP sockets testing as a domain name.
 */
# define U_SOCK_TEST_ECHO_SECURE_UDP_SERVER_DOMAIN_NAME  "ubxlib.com"
#endif

#ifndef U_SOCK_TEST_ECHO_SECURE_UDP_SERVER_IP_ADDRESS
/** Echo server to use for secure (DTLS) UDP sockets testing as an IP address.
 */
# define U_SOCK_TEST_ECHO_SECURE_UDP_SERVER_IP_ADDRESS  "18.133.144.142"
#endif

#ifndef U_SOCK_TEST_ECHO_SECURE_UDP_SERVER_PORT
/** Port number on the echo server to use for secure (DTLS) UDP testing.
 */
# define U_SOCK_TEST_ECHO_SECURE_UDP_SERVER_PORT  5070
#endif

#ifndef U_SOCK_TEST_ECHO_TCP_SERVER_DOMAIN_NAME
/** Echo server to use for TCP sockets testing as a domain name.
 */
# define U_SOCK_TEST_ECHO_TCP_SERVER_DOMAIN_NAME  "ubxlib.com"
#endif

#ifndef U_SOCK_TEST_ECHO_TCP_SERVER_IP_ADDRESS
/** Echo server to use for TCP sockets testing as an IP address.
 */
# define U_SOCK_TEST_ECHO_TCP_SERVER_IP_ADDRESS  "18.133.144.142"
#endif

#ifndef U_SOCK_TEST_ECHO_TCP_SERVER_PORT
/** Port number on the echo server to use for TCP testing.
 */
# define U_SOCK_TEST_ECHO_TCP_SERVER_PORT  5055
#endif

#ifndef U_SOCK_TEST_ECHO_SECURE_TCP_SERVER_DOMAIN_NAME
/** Echo server to use for secure (TLS) TCP sockets testing as a domain name.
 */
# define U_SOCK_TEST_ECHO_SECURE_TCP_SERVER_DOMAIN_NAME  "ubxlib.com"
#endif

#ifndef U_SOCK_TEST_ECHO_SECURE_TCP_SERVER_IP_ADDRESS
/** Echo server to use for secure (TLS) TCP sockets testing as an IP address.
 */
# define U_SOCK_TEST_ECHO_SECURE_TCP_SERVER_IP_ADDRESS  "18.133.144.142"
#endif

#ifndef U_SOCK_TEST_ECHO_SECURE_TCP_SERVER_PORT
/** Port number on the echo server to use for secure (TLS) TCP testing.
 * Note: we used to use port 5060 here but that is commonly used for
 * non-secure SIP and hence can be blocked by firewalls which want to
 * exclude SIP, so 5065 is now used instead.
 */
# define U_SOCK_TEST_ECHO_SECURE_TCP_SERVER_PORT  5065
#endif

#ifndef U_SOCK_TEST_LOCAL_PORT
/** Local port number used when testing.
 */
# define U_SOCK_TEST_LOCAL_PORT 5000
#endif

#ifndef U_SOCK_TEST_UDP_RETRIES
/** The number of retries to allow when sending
 * data over UDP.
 */
# define U_SOCK_TEST_UDP_RETRIES 10
#endif

#ifndef U_SOCK_TEST_MAX_TCP_READ_WRITE_SIZE
/** The maximum TCP read/write size to use during testing.
 */
# define U_SOCK_TEST_MAX_TCP_READ_WRITE_SIZE 1024
#endif

#ifndef U_SOCK_TEST_MIN_TCP_READ_WRITE_SIZE
/** Sending just one byte doesn't always cause all
 * modules to actually send the data in a reasonable
 * time so set a sensible minimum here for testing.
 */
# define U_SOCK_TEST_MIN_TCP_READ_WRITE_SIZE 128
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
