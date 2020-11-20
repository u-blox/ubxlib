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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Test for the network API: these should pass on all platforms
 * that include the appropriate communications hardware, i.e. at least
 * one of cellular or short range.  These tests use the sockets API to
 * prove that communication is possible over the network that has
 * been brought into existence.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset(), strcmp()

#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"

#ifdef U_CFG_TEST_CELL_MODULE_TYPE
#include "u_cell_module_type.h"
#include "u_cell_test_cfg.h" // For the cellular test macros
#endif

#include "u_network.h"
#include "u_network_test_shared_cfg.h"

#include "u_sock.h"           // In order to prove that we can do
#include "u_sock_test_shared_cfg.h"  // something with an "up" network

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** An echo test string.
 */
static const char gTestString[] = "Hello from u-blox.";

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TESTS
 * -------------------------------------------------------------- */

/** Test everything; there isn't much.
 */
U_PORT_TEST_FUNCTION("[network]", "networkTest")
{
    uNetworkTestCfg_t *pNetworkCfg = NULL;
    int32_t networkHandle;
    uSockDescriptor_t descriptor;
    uSockAddress_t address;
    char buffer[32];
    int32_t y;
    int32_t heapUsed;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    U_PORT_TEST_ASSERT(uPortInit() == 0);
    U_PORT_TEST_ASSERT(uNetworkInit() == 0);

    // Add each network type
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        gUNetworkTestCfg[x].handle = -1;
        if (*((const uNetworkType_t *) (gUNetworkTestCfg[x].pConfiguration)) != U_NETWORK_TYPE_NONE) {
            uPortLog("U_NETWORK_TEST: adding %s network...\n",
                     gpUNetworkTestTypeName[gUNetworkTestCfg[x].type]);
            gUNetworkTestCfg[x].handle = uNetworkAdd(gUNetworkTestCfg[x].type,
                                                     gUNetworkTestCfg[x].pConfiguration);
            U_PORT_TEST_ASSERT(gUNetworkTestCfg[x].handle >= 0);
        }
    }

    // Bring up each network type
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {

        if (gUNetworkTestCfg[x].handle >= 0) {
            pNetworkCfg = &(gUNetworkTestCfg[x]);
            networkHandle = pNetworkCfg->handle;

            uPortLog("U_NETWORK_TEST: bringing up %s...\n",
                     gpUNetworkTestTypeName[pNetworkCfg->type]);
            U_PORT_TEST_ASSERT(uNetworkUp(networkHandle) == 0);

            uPortLog("U_NETWORK_TEST: looking up echo server \"%s\"...\n",
                     U_SOCK_TEST_ECHO_UDP_SERVER_DOMAIN_NAME);
            // Look up the address of the server we use for UDP echo
            U_PORT_TEST_ASSERT(uSockGetHostByName(networkHandle,
                                                  U_SOCK_TEST_ECHO_UDP_SERVER_DOMAIN_NAME,
                                                  &(address.ipAddress)) == 0);
            // Add the port number we will use
            address.port = U_SOCK_TEST_ECHO_UDP_SERVER_PORT;

            // Create a UDP socket
            descriptor = uSockCreate(networkHandle, U_SOCK_TYPE_DGRAM,
                                     U_SOCK_PROTOCOL_UDP);

            // Send and wait for the UDP echo data, trying a few
            // times to reduce the chance of internet loss getting
            // in the way
            uPortLog("U_NETWORK_TEST: sending %d byte(s) to %s:%d over"
                     " %s...\n", sizeof(gTestString) - 1,
                     U_SOCK_TEST_ECHO_UDP_SERVER_DOMAIN_NAME,
                     U_SOCK_TEST_ECHO_UDP_SERVER_PORT,
                     gpUNetworkTestTypeName[pNetworkCfg->type]);
            y = 0;
            memset(buffer, 0, sizeof(buffer));
            for (size_t z = 0; (z < U_SOCK_TEST_UDP_RETRIES) &&
                 (y != sizeof(gTestString) - 1); z++) {
                y = uSockSendTo(descriptor, &address, gTestString,
                                sizeof(gTestString) - 1);
                if (y == sizeof(gTestString) - 1) {
                    // Wait for the answer
                    y = 0;
                    for (size_t w = 10; (w > 0) &&
                         (y != sizeof(gTestString) - 1); w--) {
                        y = uSockReceiveFrom(descriptor, NULL,
                                             buffer, sizeof(buffer));
                        if (y <= 0) {
                            uPortTaskBlock(1000);
                        }
                    }
                    if (y != sizeof(gTestString) - 1) {
                        uPortLog("U_NETWORK_TEST: failed to receive UDP echo"
                                 " on try %d.\n", z + 1);
                    }
                } else {
                    uPortLog("U_NETWORK_TEST: failed to send UDP data on"
                             " try %d.\n", z + 1);
                }
            }
            uPortLog("U_NETWORK_TEST: %d byte(s) echoed over UDP on %s.\n",
                     y, gpUNetworkTestTypeName[pNetworkCfg->type]);
            U_PORT_TEST_ASSERT(y == sizeof(gTestString) - 1);
            U_PORT_TEST_ASSERT(strcmp(buffer, gTestString) == 0);

            // Close the socket
            U_PORT_TEST_ASSERT(uSockClose(descriptor) == 0);
        }
    }


    // Remove each network type
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        if (gUNetworkTestCfg[x].handle >= 0) {
            uPortLog("U_NETWORK_TEST: taking down %s...\n",
                     gpUNetworkTestTypeName[gUNetworkTestCfg[x].type]);
            U_PORT_TEST_ASSERT(uNetworkDown(gUNetworkTestCfg[x].handle) == 0);
        }
    }

    uNetworkDeinit();
    uPortDeinit();

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_NETWORK_TEST: 0 byte(s) of heap were lost to"
             " the C library during this test and we have"
             " leaked %d byte(s).\n", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[network]", "networkCleanUp")
{
    int32_t y;

    // The network test configuration is shared
    // between the network and sockets tests so
    // must reset the handles here in case the
    // sockets API tests are coming next.
    for (size_t x = 0; x < gUNetworkTestCfgSize; x++) {
        gUNetworkTestCfg[x].handle = -1;
    }
    uNetworkDeinit();

    y = uPortTaskStackMinFree(NULL);
    uPortLog("U_NETWORK_TEST: main task stack had a minimum of %d"
             " byte(s) free at the end of these tests.\n", y);
    U_PORT_TEST_ASSERT(y >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);

    uPortDeinit();

    y = uPortGetHeapMinFree();
    if (y >= 0) {
        uPortLog("U_NETWORK_TEST: heap had a minimum of %d"
                 " byte(s) free at the end of these tests.\n", y);
        U_PORT_TEST_ASSERT(y >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

// End of file
