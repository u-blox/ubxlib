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
 * @brief Tests for the configuration calls of the cellular MQTT
 * API; for testing of the connectivity parts see the tests in
 * common/mqtt_client.  These test should pass on all platforms
 * that have a cellular module connected to them.  They are only
 * compiled if U_CFG_TEST_CELL_MODULE_TYPE is defined.
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */

#ifdef U_CFG_TEST_CELL_MODULE_TYPE

# ifdef U_CFG_OVERRIDE
#  include "u_cfg_override.h" // For a customer's configuration override
# endif

#include "stdlib.h"    // rand()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"   // Required by u_cell_private.h

#include "u_at_client.h"

#include "u_cell_module_type.h"
#include "u_cell.h"
#include "u_cell_net.h"     // Required by u_cell_private.h
#include "u_cell_private.h" // So that we can get at some innards
#include "u_cell_mqtt.h"

#include "u_cell_test_cfg.h"
#include "u_cell_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_CELL_MQTT_TEST_MQTT_SERVER_IP_ADDRESS
/** Server to use for MQTT testing.
 */
# define U_CELL_MQTT_TEST_MQTT_SERVER_IP_ADDRESS  broker.hivemq.com
#endif

#ifndef U_CELL_MQTT_TEST_MQTT_SERVER_IP_ADDRESS_SECURED
/** Server to use for MQTT testing on a secured connection,
 * can'tbe hivemq as that doesn't support security.
 */
# define U_CELL_MQTT_TEST_MQTT_SERVER_IP_ADDRESS_SECURED  test.mosquitto.org:8883
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Used for keepGoingCallback() timeout.
 */
static int64_t gStopTimeMs;

/** Generic handles.
 */
static uCellTestPrivate_t gHandles = U_CELL_TEST_PRIVATE_DEFAULTS;

/** A string of all possible characters, including strings
 * that might appear as terminators in an AT interface, that
 * is less than 128 characters long.
 */
static const char gAllChars[] = "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e"
                                "\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c"
                                "\x1d\x1e!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~\x7f"
                                "\r\nOK\r\n \r\nERROR\r\n \r\nABORTED\r\n";

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Callback function for the cellular connection process
static bool keepGoingCallback()
{
    bool keepGoing = true;

    if (uPortGetTickTimeMs() > gStopTimeMs) {
        keepGoing = false;
    }

    return keepGoing;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** A test of the configuration functions in the cellular MQTT API.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */
U_PORT_TEST_FUNCTION("[cellMqtt]", "cellMqtt")
{
    int32_t cellHandle;
    const uCellPrivateModule_t *pModule;
    int32_t heapUsed;
    char buffer1[32];
    char buffer2[32];
    char *pBuffer;
    int32_t x;
    int32_t y;
    size_t z;
    uCellMqttQos_t qos = U_CELL_MQTT_QOS_MAX_NUM;
    bool retained = false;
    const char *pServerAddress = U_PORT_STRINGIFY_QUOTED(U_CELL_MQTT_TEST_MQTT_SERVER_IP_ADDRESS);

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Only run if MQTT is supported
    if (uCellMqttIsSupported(cellHandle)) {

        // Get the private module data as we need it for testing
        pModule = pUCellPrivateGetModule(cellHandle);
        U_PORT_TEST_ASSERT(pModule != NULL);
        //lint -esym(613, pModule) Suppress possible use of NULL pointer
        // for pModule from now on

        // Make a cellular connection, since we will need to do a
        // DNS look-up on the MQTT broker domain name
        gStopTimeMs = uPortGetTickTimeMs() +
                      (U_CELL_TEST_CFG_CONNECT_TIMEOUT_SECONDS * 1000);
        U_PORT_TEST_ASSERT(uCellNetConnect(cellHandle, NULL,
#ifdef U_CELL_TEST_CFG_APN
                                           U_PORT_STRINGIFY_QUOTED(U_CELL_TEST_CFG_APN),
#else
                                           NULL,
#endif
#ifdef U_CELL_TEST_CFG_USERNAME
                                           U_PORT_STRINGIFY_QUOTED(U_CELL_TEST_CFG_USERNAME),
#else
                                           NULL,
#endif
#ifdef U_CELL_TEST_CFG_PASSWORD
                                           U_PORT_STRINGIFY_QUOTED(U_CELL_TEST_CFG_PASSWORD),
#else
                                           NULL,
#endif
                                           keepGoingCallback) == 0);

        if (U_CELL_PRIVATE_MODULE_IS_SARA_R4(pModule->moduleType)) {
            // If the module does not permit us to switch off TLS security once it
            // has been switched on then we need to use the secured server IP address
            // since we will have tested switching security on by the time we do
            // the connect
            pServerAddress = U_PORT_STRINGIFY_QUOTED(U_CELL_MQTT_TEST_MQTT_SERVER_IP_ADDRESS_SECURED);
        }

        // Initialise the MQTT client.
        U_PORT_TEST_ASSERT(uCellMqttInit(cellHandle,
                                         pServerAddress,
                                         NULL,
#ifdef U_CELL_MQTT_TEST_MQTT_USERNAME
                                         U_PORT_STRINGIFY_QUOTED(U_CELL_MQTT_TEST_MQTT_USERNAME),
#else
                                         NULL,
#endif
#ifdef U_CELL_MQTT_TEST_MQTT_PASSWORD
                                         U_PORT_STRINGIFY_QUOTED(U_CELL_MQTT_TEST_MQTT_PASSWORD),
#else
                                         NULL,
#endif
                                         NULL, false) == 0);

        // Get the client ID
        uPortLog("U_CELL_MQTT_TEST: testing getting client ID...\n");
        memset(buffer1, 0, sizeof(buffer1));
        x = uCellMqttGetClientId(cellHandle, buffer1, sizeof(buffer1));
        U_PORT_TEST_ASSERT(x > 0);
        U_PORT_TEST_ASSERT(x == strlen(buffer1));

        // Set/get the local port number
        uPortLog("U_CELL_MQTT_TEST: testing getting/setting local port...\n");
        if (U_CELL_PRIVATE_HAS(pModule,
                               U_CELL_PRIVATE_FEATURE_MQTT_SET_LOCAL_PORT)) {
            x = uCellMqttGetLocalPort(cellHandle);
            U_PORT_TEST_ASSERT(x >= 0);
            U_PORT_TEST_ASSERT(x != 666);
            U_PORT_TEST_ASSERT(uCellMqttSetLocalPort(cellHandle, 666) == 0);
            x = uCellMqttGetLocalPort(cellHandle);
            U_PORT_TEST_ASSERT(x == 666);
        } else {
            x = uCellMqttGetLocalPort(cellHandle);
            if (uCellMqttIsSecured(cellHandle, NULL)) {
                U_PORT_TEST_ASSERT(x == 8883);
            } else {
                U_PORT_TEST_ASSERT(x == 1883);
            }
        }

        // Set/get the inactivity timeout
        uPortLog("U_CELL_MQTT_TEST: testing getting/setting inactivity timeout...\n");
        x = uCellMqttGetInactivityTimeout(cellHandle);
        U_PORT_TEST_ASSERT(x >= 0);
        U_PORT_TEST_ASSERT(uCellMqttSetInactivityTimeout(cellHandle, x + 60) == 0);
        y = uCellMqttGetInactivityTimeout(cellHandle);
        U_PORT_TEST_ASSERT(y == x + 60);
        // Leave the timeout where it is: reason is that on SARA-R5 if the
        // inactivity timeout is *set* to zero (as opposed to being
        // left at the default of zero) it doesn't mean no timeout,
        // it means a zero timeout and, if you switch on MQTT keep-alive
        // (AKA ping) you get +UUMQTTC: 8,0 poured at you forever

        // Set/get retention
        uPortLog("U_CELL_MQTT_TEST: testing getting/setting session retention...\n");
        if (U_CELL_PRIVATE_HAS(pModule,
                               U_CELL_PRIVATE_FEATURE_MQTT_SESSION_RETAIN)) {
            if (uCellMqttIsRetained(cellHandle)) {
                U_PORT_TEST_ASSERT(uCellMqttSetRetainOn(cellHandle) == 0);
                U_PORT_TEST_ASSERT(!uCellMqttIsRetained(cellHandle));
            } else {
                U_PORT_TEST_ASSERT(uCellMqttSetRetainOff(cellHandle) == 0);
                U_PORT_TEST_ASSERT(uCellMqttIsRetained(cellHandle));
            }
        } else {
            U_PORT_TEST_ASSERT(!uCellMqttIsRetained(cellHandle));
        }

        // Set/get security
        uPortLog("U_CELL_MQTT_TEST: testing getting/setting security...\n");
        if (uCellMqttIsSecured(cellHandle, NULL)) {
            if (!U_CELL_PRIVATE_MODULE_IS_SARA_R4(pModule->moduleType)) {
                // On SARA-R4 modules TLS security cannot be disabled once
                // it is disabled without power-cycling the module.
                U_PORT_TEST_ASSERT(uCellMqttSetSecurityOff(cellHandle) == 0);
                x = -1;
                U_PORT_TEST_ASSERT(!uCellMqttIsSecured(cellHandle, &x));
                U_PORT_TEST_ASSERT(x == -1);
            }
        } else {
            U_PORT_TEST_ASSERT(uCellMqttSetSecurityOn(cellHandle, 0) == 0);
            x = -1;
            U_PORT_TEST_ASSERT(uCellMqttIsSecured(cellHandle, &x));
            U_PORT_TEST_ASSERT(x == 0);
        }

        if (!U_CELL_PRIVATE_MODULE_IS_SARA_R4(pModule->moduleType)) {
            // Switch security off again before we continue
            U_PORT_TEST_ASSERT(uCellMqttSetSecurityOff(cellHandle) == 0);
            U_PORT_TEST_ASSERT(uCellMqttIsSecured(cellHandle, &x) == 0);
        }

        // Set/get a "will" message
        if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_MQTT_WILL)) {
            uPortLog("U_CELL_MQTT_TEST: testing getting/setting \"will\"...\n");
            // Malloc memory to put the will message in
            pBuffer = (char *) malloc(sizeof(gAllChars) + 1);
            U_PORT_TEST_ASSERT(pBuffer != NULL);
            U_PORT_TEST_ASSERT(uCellMqttSetWill(cellHandle,
                                                "In the event of my death",
                                                gAllChars, sizeof(gAllChars),
                                                U_CELL_MQTT_QOS_AT_MOST_ONCE,
                                                true) == 0);
            z = sizeof(gAllChars) + 1;
            memset(buffer2, 0, sizeof(buffer2));
            memset(pBuffer, 0, sizeof(gAllChars) + 1);
            U_PORT_TEST_ASSERT(uCellMqttGetWill(cellHandle,
                                                buffer2, sizeof(buffer2),
                                                pBuffer, &z,
                                                &qos, &retained) == 0);
            U_PORT_TEST_ASSERT(strcmp(buffer2, "In the event of my death") == 0);
            U_PORT_TEST_ASSERT(memcmp(pBuffer, gAllChars, sizeof(gAllChars)) == 0);
            U_PORT_TEST_ASSERT(z == sizeof(gAllChars));
            U_PORT_TEST_ASSERT(qos == U_CELL_MQTT_QOS_AT_MOST_ONCE);
            U_PORT_TEST_ASSERT(retained);
            free(pBuffer);
        }

        // Need to connect before keep-alive can be set
        uPortLog("U_CELL_MQTT_TEST: connecting to broker \"%s\"...\n", pServerAddress);
        U_PORT_TEST_ASSERT(uCellMqttConnect(cellHandle) == 0);

        // Set/get keep-alive
        uPortLog("U_CELL_MQTT_TEST: testing getting/setting keep-alive...\n");
        if (uCellMqttIsKeptAlive(cellHandle)) {
            U_PORT_TEST_ASSERT(uCellMqttSetKeepAliveOff(cellHandle) == 0);
            U_PORT_TEST_ASSERT(!uCellMqttIsKeptAlive(cellHandle));
        } else {
            U_PORT_TEST_ASSERT(uCellMqttSetKeepAliveOn(cellHandle) == 0);
            U_PORT_TEST_ASSERT(uCellMqttIsKeptAlive(cellHandle));
        }

        // Disconnect
        uPortLog("U_CELL_MQTT_TEST: disconnecting from broker...\n");
        U_PORT_TEST_ASSERT(uCellMqttDisconnect(cellHandle) == 0);
        U_PORT_TEST_ASSERT(uCellNetDisconnect(cellHandle, NULL) == 0);

        if (!U_CELL_PRIVATE_MODULE_IS_SARA_R4(pModule->moduleType)) {
            // Initialise the MQTT client again: this should return
            // success but do nothing, so the client ID should be
            // unchanged, even though we have given one.
            U_PORT_TEST_ASSERT(uCellMqttInit(cellHandle, "2.2.2.2",
                                             "flibble", NULL, NULL,
                                             NULL, false) == 0);

            // Get the client ID and check it is the same, unless
            // this is SARA-R4, which doesn't support reading the
            // client ID at this point for reasons I don't
            // understand
            memset(buffer2, 0, sizeof(buffer2));
            x = uCellMqttGetClientId(cellHandle, buffer2, sizeof(buffer2));
            U_PORT_TEST_ASSERT(x > 0);
            U_PORT_TEST_ASSERT(strcmp(buffer2, buffer1) == 0);
        }

        // Finally Deinitialise MQTT
        uCellMqttDeinit(cellHandle);
    } else {
        uPortLog("U_CELL_MQTT_TEST: MQTT not supported, skipping...\n");
    }

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_CELL_MQTT_TEST: we have leaked %d byte(s).\n", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}


/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[cellMqtt]", "cellMqttCleanUp")
{
    int32_t x;

    if (gHandles.cellHandle >= 0) {
        uCellMqttDeinit(gHandles.cellHandle);
    }
    uCellTestPrivateCleanup(&gHandles);

    x = uPortTaskStackMinFree(NULL);
    uPortLog("U_CELL_MQTT_TEST: main task stack had a minimum of %d"
             " byte(s) free at the end of these tests.\n", x);
    U_PORT_TEST_ASSERT(x >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);

    uPortDeinit();

    x = uPortGetHeapMinFree();
    if (x >= 0) {
        uPortLog("U_CELL_MQTT_TEST: heap had a minimum of %d"
                 " byte(s) free at the end of these tests.\n", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

#endif // #ifdef U_CFG_TEST_CELL_MODULE_TYPE

// End of file
