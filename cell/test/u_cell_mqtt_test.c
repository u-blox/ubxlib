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
#include "string.h"    // memset(), strncpy()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_os.h"   // Required by u_cell_private.h

#include "u_at_client.h"

#include "u_cell_module_type.h"
#include "u_cell.h"
#include "u_cell_file.h"
#include "u_cell_net.h"     // Required by u_cell_private.h
#include "u_cell_private.h" // So that we can get at some innards
#include "u_cell_mqtt.h"

#include "u_cell_test_cfg.h"
#include "u_cell_test_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The base string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX_BASE "U_CELL_MQTT"

/** The string to put at the start of all MQTT prints from this test.
 */
#define U_TEST_PREFIX U_TEST_PREFIX_BASE "_TEST: "

/** Print a whole line, with terminator, prefixed for an MQTT test.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

/** The string to put at the start of all MQTT-SN prints from this test.
 */
#define U_TEST_PREFIX_SN U_TEST_PREFIX_BASE "SN_TEST: "

/** Print a whole line, with terminator, prefixed for an MQTT-SN test.
 */
#define U_TEST_PRINT_LINE_SN(format, ...) uPortLog(U_TEST_PREFIX_SN format "\n", ##__VA_ARGS__)

#ifndef U_CELL_MQTT_TEST_MQTT_SERVER_IP_ADDRESS
/** Server to use for MQTT testing.
 */
# define U_CELL_MQTT_TEST_MQTT_SERVER_IP_ADDRESS  ubxlib.redirectme.net
#endif

#ifndef U_CELL_MQTT_TEST_MQTT_SERVER_IP_ADDRESS_SECURED
/** Server to use for MQTT testing on a secured connection,
 * can't be hivemq as that doesn't support security.
 */
# define U_CELL_MQTT_TEST_MQTT_SERVER_IP_ADDRESS_SECURED  ubxlib.redirectme.net:8883
#endif

#ifndef U_CELL_MQTT_TEST_MQTTSN_SERVER_IP_ADDRESS
/** Server to use for MQTT-SN testing.
 */
# define U_CELL_MQTT_TEST_MQTTSN_SERVER_IP_ADDRESS  ubxlib.redirectme.net
#endif

#ifndef U_CELL_MQTT_TEST_MQTTSN_SERVER_IP_ADDRESS_SECURED
/** Server to use for MQTT-SN testing on a secured connection.
 */
# define U_CELL_MQTT_TEST_MQTTSN_SERVER_IP_ADDRESS_SECURED  ubxlib.redirectme.net:8883
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

#ifdef U_CELL_MQTT_TEST_ENABLE_WILL_TEST
/** A string of all possible characters, including strings
 * that might appear as terminators in an AT interface, that
 * is less than 128 characters long.
 */
static const char gAllChars[] = "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e"
                                "\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c"
                                "\x1d\x1e!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~\x7f"
                                "\r\nOK\r\n \r\nERROR\r\n \r\nABORTED\r\n";

/** A string of all printable characters, and not including quotation
 * marks either, that is less than 128 characters long.
 */
static const char gPrintableChars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                      "0123456789!#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
#endif

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Callback function for the cellular connection process.
static bool keepGoingCallback(uDeviceHandle_t unused)
{
    bool keepGoing = true;

    (void) unused;

    if (uPortGetTickTimeMs() > gStopTimeMs) {
        keepGoing = false;
    }

    return keepGoing;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** A test of the configuration functions in the cellular MQTT API
 * when run in MQTT mode.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */
U_PORT_TEST_FUNCTION("[cellMqtt]", "cellMqtt")
{
    uDeviceHandle_t cellHandle;
    const uCellPrivateModule_t *pModule;
    int32_t heapUsed;
    char buffer1[32];
    char buffer2[32];
    int32_t x;
    int32_t y;
    size_t z;
    const char *pServerAddress = U_PORT_STRINGIFY_QUOTED(U_CELL_MQTT_TEST_MQTT_SERVER_IP_ADDRESS);
#ifdef U_CELL_MQTT_TEST_ENABLE_WILL_TEST
    uCellMqttQos_t qos = U_CELL_MQTT_QOS_MAX_NUM;
    bool retained = false;
    char *pBuffer;
#endif

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
        x = uCellNetConnect(cellHandle, NULL,
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
                            keepGoingCallback);
        U_PORT_TEST_ASSERT(x == 0);

        if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_MQTT_SECURITY) &&
            U_CELL_PRIVATE_MODULE_IS_SARA_R4(pModule->moduleType) &&
            (pModule->moduleType != U_CELL_MODULE_TYPE_SARA_R422) &&
            (pModule->moduleType != U_CELL_MODULE_TYPE_SARA_R412M_02B)) {
            // If the module does not permit us to switch off TLS security once it
            // has been switched on (which is the case for SARA-R10M-02B and
            // SARA_R410M-03B but not SARA-R422 or SARA-R5) then we need to use
            // the secured server IP address since we will have tested switching
            // security on by the time we do the connect
            // SARA-R412M will only let security be switched on if all of a root
            // CA, private key and certificate have been defined, hence we don't
            // test that here.
            pServerAddress = U_PORT_STRINGIFY_QUOTED(U_CELL_MQTT_TEST_MQTT_SERVER_IP_ADDRESS_SECURED);
        }

        // Initialise the MQTT client.
        x = uCellMqttInit(cellHandle, pServerAddress, NULL,
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
                          NULL, false);
        U_PORT_TEST_ASSERT(x == 0);

        // Check retry count setting/getting.
        U_PORT_TEST_ASSERT(uCellMqttGetRetries(cellHandle) == U_CELL_MQTT_RETRIES_DEFAULT);
        uCellMqttSetRetries(cellHandle, 0);
        U_PORT_TEST_ASSERT(uCellMqttGetRetries(cellHandle) == 0);
        uCellMqttSetRetries(cellHandle, U_CELL_MQTT_RETRIES_DEFAULT);
        U_PORT_TEST_ASSERT(uCellMqttGetRetries(cellHandle) == U_CELL_MQTT_RETRIES_DEFAULT);

        // Note: deliberately not setting a disconnect callback
        // here; here we test having none, testing with a disconnect
        // callback is done at the MQTT client layer above

        // Get the client ID
        U_TEST_PRINT_LINE("testing getting client ID...");
        memset(buffer1, 0, sizeof(buffer1));
        x = uCellMqttGetClientId(cellHandle, buffer1, sizeof(buffer1));
        U_PORT_TEST_ASSERT(x > 0);
        U_TEST_PRINT_LINE("client ID is \"%.*s\"...", x, buffer1);
        U_PORT_TEST_ASSERT(x == strlen(buffer1));

        // Set/get the local port number
        if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_MQTT_SET_LOCAL_PORT)) {
            U_TEST_PRINT_LINE("testing getting/setting local port...");
            x = uCellMqttGetLocalPort(cellHandle);
            U_PORT_TEST_ASSERT(x >= 0);
            U_PORT_TEST_ASSERT(x != 666);
            U_PORT_TEST_ASSERT(uCellMqttSetLocalPort(cellHandle, 666) == 0);
            x = uCellMqttGetLocalPort(cellHandle);
            U_PORT_TEST_ASSERT(x == 666);
        }

        // Set/get retention
        U_TEST_PRINT_LINE("testing getting/setting retention...");
        if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_MQTT_SESSION_RETAIN)) {
            for (z = 0; z < 2; z++) {
                if (uCellMqttIsRetained(cellHandle)) {
                    U_TEST_PRINT_LINE("retention is on, switching it off...");
                    U_PORT_TEST_ASSERT(uCellMqttSetRetainOff(cellHandle) == 0);
                    x = uCellMqttIsRetained(cellHandle);
                    U_TEST_PRINT_LINE("retention is now %s.", x ? "on" : "off");
                    U_PORT_TEST_ASSERT(!x);
                } else {
                    U_TEST_PRINT_LINE("retention is off, switching it on...");
                    U_PORT_TEST_ASSERT(uCellMqttSetRetainOn(cellHandle) == 0);
                    x = uCellMqttIsRetained(cellHandle);
                    U_TEST_PRINT_LINE("retention is now %s.", x ? "on" : "off");
                    U_PORT_TEST_ASSERT(x);
                }
            }
        } else {
            U_PORT_TEST_ASSERT(!uCellMqttIsRetained(cellHandle));
        }

        // Set/get security
        U_TEST_PRINT_LINE("testing getting/setting security...");
        if (uCellMqttIsSecured(cellHandle, NULL)) {
            if (!U_CELL_PRIVATE_MODULE_IS_SARA_R4(pModule->moduleType) ||
                (pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R422)) {
                // On SARA-R4 modules (excepting SARA-R422) TLS security cannot
                // be disabled once it is enabled without power-cycling the module.
                U_PORT_TEST_ASSERT(uCellMqttSetSecurityOff(cellHandle) == 0);
                x = -1;
                U_PORT_TEST_ASSERT(!uCellMqttIsSecured(cellHandle, &x));
                U_PORT_TEST_ASSERT(x == -1);
            }
        } else {
            // Only switch on security if it is supported and if this is
            // not SARA-R412M, since SARA-R412M will only let security
            // be switched on if all of a root CA, private key and
            // certificate have been defined
            if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_MQTT_SECURITY) &&
                (pModule->moduleType != U_CELL_MODULE_TYPE_SARA_R412M_02B)) {
                x = 0;
                U_TEST_PRINT_LINE("security is off, switching it on with profile %d...", x);
                U_PORT_TEST_ASSERT(uCellMqttSetSecurityOn(cellHandle, x) == 0);
                x = -1;
                y = uCellMqttIsSecured(cellHandle, &x);
                U_TEST_PRINT_LINE("security is now %s, profile is %d.",
                                  y ? "on" : "off", x);
                U_PORT_TEST_ASSERT(y);
                U_PORT_TEST_ASSERT(x == 0);
            }
        }

        if (!U_CELL_PRIVATE_MODULE_IS_SARA_R4(pModule->moduleType) ||
            (pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R422)) {
            // Switch security off again before we continue
            U_PORT_TEST_ASSERT(uCellMqttSetSecurityOff(cellHandle) == 0);
            y = uCellMqttIsSecured(cellHandle, &x);
            U_TEST_PRINT_LINE("security is now %s.", y ? "on" : "off");
            U_PORT_TEST_ASSERT(!y);
        }


        // Can't set/get a "will" message as the test broker we use
        // doesn't connect if you set one
#ifdef U_CELL_MQTT_TEST_ENABLE_WILL_TEST
        // Set/get a "will" message
        if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_MQTT_WILL)) {
            U_TEST_PRINT_LINE("testing getting/setting \"will\"...");
            // Malloc memory to put the will message in
            pBuffer = (char *) pUPortMalloc(sizeof(gAllChars) + 1);
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
                                                pBuffer, &z, &qos,
                                                &retained) == 0);
            U_PORT_TEST_ASSERT(strcmp(buffer2, "In the event of my death") == 0);
            U_PORT_TEST_ASSERT(memcmp(pBuffer, gAllChars, sizeof(gAllChars)) == 0);
            U_PORT_TEST_ASSERT(z == sizeof(gAllChars));
            U_PORT_TEST_ASSERT(qos == U_CELL_MQTT_QOS_AT_MOST_ONCE);
            U_PORT_TEST_ASSERT(retained);
            uPortFree(pBuffer);
        }
#endif

        // Test that we can get and set the inactivity timeout
        z = 60;
        U_TEST_PRINT_LINE("testing getting/setting inactivity timeout"
                          " of %d second(s)...", z);
        U_PORT_TEST_ASSERT(uCellMqttGetInactivityTimeout(cellHandle) >= 0);
        U_PORT_TEST_ASSERT(uCellMqttSetInactivityTimeout(cellHandle, z) == 0);
        U_PORT_TEST_ASSERT(uCellMqttGetInactivityTimeout(cellHandle) == z);

        // Put it back to zero for the first connection to the broker
        U_TEST_PRINT_LINE("testing setting inactivity timeout to 0.");
        U_PORT_TEST_ASSERT(uCellMqttSetInactivityTimeout(cellHandle, 0) == 0);
        U_PORT_TEST_ASSERT(uCellMqttGetInactivityTimeout(cellHandle) == 0);

        // Need to connect before keep-alive can be set
        U_TEST_PRINT_LINE("connecting to broker \"%s\"...", pServerAddress);
        U_PORT_TEST_ASSERT(uCellMqttConnect(cellHandle) == 0);

        if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_MQTT_KEEP_ALIVE)) {
            // Try to set keep-alive on
            U_TEST_PRINT_LINE("trying to set keep-alive on (should fail)...");
            U_PORT_TEST_ASSERT(!uCellMqttIsKeptAlive(cellHandle));
            // Should not be possible when the inactivity timeout is zero
            U_PORT_TEST_ASSERT(uCellMqttSetKeepAliveOn(cellHandle) < 0);
            U_PORT_TEST_ASSERT(!uCellMqttIsKeptAlive(cellHandle));

            if (pModule->moduleType != U_CELL_MODULE_TYPE_SARA_R410M_03B) {
                // For reasons I don't understand, SARA-R410M-03B won't let
                // me set a new timeout value after a connect/disconnect, so
                // there's no point in doing this bit

                // Disconnect from the broker again to test with a non-zero
                // inactivity timeout set
                U_TEST_PRINT_LINE("disconnecting from broker to test with"
                                  " an inactivity timeout...");
                U_PORT_TEST_ASSERT(uCellMqttDisconnect(cellHandle) == 0);

                // Set an inactivity timeout of 60 seconds
                z = 60;
                U_TEST_PRINT_LINE("setting inactivity timeout of %d second(s)...", z);
                U_PORT_TEST_ASSERT(uCellMqttSetInactivityTimeout(cellHandle, z) == 0);
                U_PORT_TEST_ASSERT(uCellMqttGetInactivityTimeout(cellHandle) == z);

                // Connect to the broker again
                U_TEST_PRINT_LINE("connecting to broker \"%s\" again...",
                                  pServerAddress);
                U_PORT_TEST_ASSERT(uCellMqttConnect(cellHandle) == 0);

                U_TEST_PRINT_LINE("setting keep-alive on...");
                U_PORT_TEST_ASSERT(!uCellMqttIsKeptAlive(cellHandle));
                U_PORT_TEST_ASSERT(uCellMqttSetKeepAliveOn(cellHandle) == 0);
                U_PORT_TEST_ASSERT(uCellMqttIsKeptAlive(cellHandle));
            }
        } else {
            U_TEST_PRINT_LINE("keep-alive is not supported.");
            U_PORT_TEST_ASSERT(uCellMqttSetKeepAliveOn(cellHandle) < 0);
            U_PORT_TEST_ASSERT(!uCellMqttIsKeptAlive(cellHandle));
        }

        // Disconnect
        U_TEST_PRINT_LINE("disconnecting from broker...");
        U_PORT_TEST_ASSERT(uCellMqttDisconnect(cellHandle) == 0);
        uPortTaskBlock(U_CFG_OS_YIELD_MS);

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
        U_TEST_PRINT_LINE("MQTT not supported, skipping...");
    }

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
}

/** A test of the configuration functions in the cellular MQTT API
 * when run in MQTT-SN mode.
 */
U_PORT_TEST_FUNCTION("[cellMqtt]", "cellMqttSn")
{
    uDeviceHandle_t cellHandle;
    const uCellPrivateModule_t *pModule;
    int32_t heapUsed;
    char buffer1[32];
    int32_t x;
    int32_t y;
    size_t z;
    const char *pServerAddress = U_PORT_STRINGIFY_QUOTED(U_CELL_MQTT_TEST_MQTTSN_SERVER_IP_ADDRESS);
#ifdef U_CELL_MQTT_TEST_ENABLE_WILL_TEST
    uCellMqttQos_t qos = U_CELL_MQTT_QOS_MAX_NUM;
    bool retained = false;
    char buffer2[32];
    char *pBuffer;
#endif

    // In case a previous test failed
    uCellTestPrivateCleanup(&gHandles);

    // Obtain the initial heap size
    heapUsed = uPortGetHeapFree();

    // Do the standard preamble
    U_PORT_TEST_ASSERT(uCellTestPrivatePreamble(U_CFG_TEST_CELL_MODULE_TYPE,
                                                &gHandles, true) == 0);
    cellHandle = gHandles.cellHandle;

    // Only run if MQTT-SN is supported
    if (uCellMqttSnIsSupported(cellHandle)) {

        // Get the private module data as we need it for testing
        pModule = pUCellPrivateGetModule(cellHandle);
        U_PORT_TEST_ASSERT(pModule != NULL);
        //lint -esym(613, pModule) Suppress possible use of NULL pointer
        // for pModule from now on

        // Make a cellular connection, since we will need to do a
        // DNS look-up on the MQTT-SN broker domain name
        gStopTimeMs = uPortGetTickTimeMs() +
                      (U_CELL_TEST_CFG_CONNECT_TIMEOUT_SECONDS * 1000);
        x = uCellNetConnect(cellHandle, NULL,
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
                            keepGoingCallback);
        U_PORT_TEST_ASSERT(x == 0);

        // Initialise the MQTT client for MQTT-SN
        x = uCellMqttInit(cellHandle, pServerAddress, NULL, NULL, NULL, NULL, true);
        U_PORT_TEST_ASSERT(x == 0);

        // Note: deliberately not setting a disconnect callback
        // here; here we test having none, testing with a disconnect
        // callback is done at the MQTT client layer above

        // Get the client ID
        U_TEST_PRINT_LINE_SN("testing getting client ID...");
        memset(buffer1, 0, sizeof(buffer1));
        x = uCellMqttGetClientId(cellHandle, buffer1, sizeof(buffer1));
        U_PORT_TEST_ASSERT(x > 0);
        U_TEST_PRINT_LINE_SN("client ID is \"%.*s\"...", x, buffer1);
        U_PORT_TEST_ASSERT(x == strlen(buffer1));

        // Set/get retention
        U_TEST_PRINT_LINE_SN("testing getting/setting retention...");
        if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_MQTT_SESSION_RETAIN)) {
            for (z = 0; z < 2; z++) {
                if (uCellMqttIsRetained(cellHandle)) {
                    U_TEST_PRINT_LINE_SN("retention is on, switching it off...");
                    U_PORT_TEST_ASSERT(uCellMqttSetRetainOff(cellHandle) == 0);
                    x = uCellMqttIsRetained(cellHandle);
                    U_TEST_PRINT_LINE_SN("retention is now %s.", x ? "on" : "off");
                    U_PORT_TEST_ASSERT(!x);
                } else {
                    U_TEST_PRINT_LINE_SN("retention is off, switching it on...");
                    U_PORT_TEST_ASSERT(uCellMqttSetRetainOn(cellHandle) == 0);
                    x = uCellMqttIsRetained(cellHandle);
                    U_TEST_PRINT_LINE_SN("retention is now %s.", x ? "on" : "off");
                    U_PORT_TEST_ASSERT(x);
                }
            }
        } else {
            U_PORT_TEST_ASSERT(!uCellMqttIsRetained(cellHandle));
        }

        // Set/get security
        U_TEST_PRINT_LINE_SN("testing getting/setting security...");
        if (uCellMqttIsSecured(cellHandle, NULL)) {
            U_PORT_TEST_ASSERT(uCellMqttSetSecurityOff(cellHandle) == 0);
            x = -1;
            U_PORT_TEST_ASSERT(!uCellMqttIsSecured(cellHandle, &x));
            U_PORT_TEST_ASSERT(x == -1);
        } else {
            x = 0;
            U_TEST_PRINT_LINE_SN("security is off, switching it on"
                                 " with profile %d...", x);
            U_PORT_TEST_ASSERT(uCellMqttSetSecurityOn(cellHandle, x) == 0);
            x = -1;
            y = uCellMqttIsSecured(cellHandle, &x);
            U_TEST_PRINT_LINE_SN("security is now %s, profile is"
                                 " %d.", y ? "on" : "off", x);
            U_PORT_TEST_ASSERT(y);
            U_PORT_TEST_ASSERT(x == 0);
        }

        // Switch security off again before we continue
        U_PORT_TEST_ASSERT(uCellMqttSetSecurityOff(cellHandle) == 0);
        y = uCellMqttIsSecured(cellHandle, &x);
        U_TEST_PRINT_LINE_SN("security is now %s.", y ? "on" : "off");
        U_PORT_TEST_ASSERT(!y);

        // Can't set/get a "will" message as the test broker we use
        // doesn't connect if you set one
#ifdef U_CELL_MQTT_TEST_ENABLE_WILL_TEST
        // Set/get a "will" message
        if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_MQTT_WILL)) {
            U_TEST_PRINT_LINE_SN("testing getting/setting \"will\"...");
            // Malloc memory to put the will message in
            // Note that for MQTT-SN the "will" message has to be a null-terminated
            // string, hence we don't try to include the null on the end of
            // gPrintableChars
            pBuffer = (char *) pUPortMalloc(sizeof(gPrintableChars));
            U_PORT_TEST_ASSERT(pBuffer != NULL);
            U_PORT_TEST_ASSERT(uCellMqttSetWill(cellHandle, "In the event of my SN death",
                                                gPrintableChars, strlen(gPrintableChars),
                                                U_CELL_MQTT_QOS_AT_MOST_ONCE,
                                                true) == 0);
            z = sizeof(gPrintableChars);
            memset(buffer2, 0, sizeof(buffer2));
            memset(pBuffer, 0, sizeof(gPrintableChars));
            U_PORT_TEST_ASSERT(uCellMqttGetWill(cellHandle,
                                                buffer2, sizeof(buffer2),
                                                pBuffer, &z, &qos,
                                                &retained) == 0);
            U_PORT_TEST_ASSERT(strcmp(buffer2, "In the event of my SN death") == 0);
            U_PORT_TEST_ASSERT(z == strlen(gPrintableChars));
            U_PORT_TEST_ASSERT(memcmp(pBuffer, gPrintableChars, z) == 0);
            U_PORT_TEST_ASSERT(qos == U_CELL_MQTT_QOS_AT_MOST_ONCE);
            U_PORT_TEST_ASSERT(retained);
            uPortFree(pBuffer);
        }
#endif

        // Test that we can get and set the inactivity timeout
        z = 60;
        U_TEST_PRINT_LINE_SN("testing getting/setting inactivity timeout"
                             " of %d second(s)...", z);
        U_PORT_TEST_ASSERT(uCellMqttGetInactivityTimeout(cellHandle) >= 0);
        U_PORT_TEST_ASSERT(uCellMqttSetInactivityTimeout(cellHandle, z) == 0);
        U_PORT_TEST_ASSERT(uCellMqttGetInactivityTimeout(cellHandle) == z);

        // Put it back to zero for the first connection to the broker
        U_TEST_PRINT_LINE_SN("testing setting inactivity timeout to 0.");
        U_PORT_TEST_ASSERT(uCellMqttSetInactivityTimeout(cellHandle, 0) == 0);
        U_PORT_TEST_ASSERT(uCellMqttGetInactivityTimeout(cellHandle) == 0);

        // Need to connect before keep-alive can be set and the "will" stuff
        // can be updated
        U_TEST_PRINT_LINE_SN("connecting to MQTT-SN broker \"%s\"...", pServerAddress);
        U_PORT_TEST_ASSERT(uCellMqttConnect(cellHandle) == 0);

        if (U_CELL_PRIVATE_HAS(pModule, U_CELL_PRIVATE_FEATURE_MQTT_KEEP_ALIVE)) {
            // Try to set keep-alive on
            U_TEST_PRINT_LINE_SN("trying to set keep-alive on (should fail)...");
            U_PORT_TEST_ASSERT(!uCellMqttIsKeptAlive(cellHandle));
            // Should not be possible when the inactivity timeout is zero
            U_PORT_TEST_ASSERT(uCellMqttSetKeepAliveOn(cellHandle) < 0);
            U_PORT_TEST_ASSERT(!uCellMqttIsKeptAlive(cellHandle));

            // Would test updating of the "will" message and its parameters
            // here but unfortunately such updates are not supported by either
            // the Paho MQTT-SN Gateway that we use during testing or by Thingstream.

            // Disconnect from the broker again to test with a non-zero
            // inactivity timeout set
            U_TEST_PRINT_LINE_SN("disconnecting from mQTT-SN broker to test with"
                                 " an inactivity timeout...");
            U_PORT_TEST_ASSERT(uCellMqttDisconnect(cellHandle) == 0);

            // Set an inactivity timeout of 60 seconds
            z = 60;
            U_TEST_PRINT_LINE_SN("setting inactivity timeout of %d second(s)...", z);
            U_PORT_TEST_ASSERT(uCellMqttSetInactivityTimeout(cellHandle, z) == 0);
            U_PORT_TEST_ASSERT(uCellMqttGetInactivityTimeout(cellHandle) == z);

            // Connect to the broker again
            U_TEST_PRINT_LINE_SN("connecting to broker \"%s\" again...",
                                 pServerAddress);
            U_PORT_TEST_ASSERT(uCellMqttConnect(cellHandle) == 0);

            U_TEST_PRINT_LINE_SN("setting keep-alive on...");
            U_PORT_TEST_ASSERT(!uCellMqttIsKeptAlive(cellHandle));
            U_PORT_TEST_ASSERT(uCellMqttSetKeepAliveOn(cellHandle) == 0);
            U_PORT_TEST_ASSERT(uCellMqttIsKeptAlive(cellHandle));
        } else {
            U_TEST_PRINT_LINE_SN("keep-alive is not supported.");
            U_PORT_TEST_ASSERT(uCellMqttSetKeepAliveOn(cellHandle) < 0);
            U_PORT_TEST_ASSERT(!uCellMqttIsKeptAlive(cellHandle));
        }

        // Disconnect
        U_TEST_PRINT_LINE_SN("disconnecting from MQTT-SN broker...");
        U_PORT_TEST_ASSERT(uCellMqttDisconnect(cellHandle) == 0);
        uPortTaskBlock(U_CFG_OS_YIELD_MS);

        U_PORT_TEST_ASSERT(uCellNetDisconnect(cellHandle, NULL) == 0);

        // Finally Deinitialise MQTT
        uCellMqttDeinit(cellHandle);
    } else {
        U_TEST_PRINT_LINE_SN("MQTT-SN not supported, skipping...");
    }

    // Do the standard postamble, leaving the module on for the next
    // test to speed things up
    uCellTestPrivatePostamble(&gHandles, false);

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE_SN("we have leaked %d byte(s).", heapUsed);
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

    if (gHandles.cellHandle != NULL) {
        uCellMqttDeinit(gHandles.cellHandle);
    }
    uCellTestPrivateCleanup(&gHandles);

    x = uPortTaskStackMinFree(NULL);
    if (x != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_TEST_PRINT_LINE("main task stack had a minimum of %d"
                          " byte(s) free at the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);
    }

    uPortDeinit();

    x = uPortGetHeapMinFree();
    if (x >= 0) {
        U_TEST_PRINT_LINE("heap had a minimum of %d byte(s) free at"
                          " the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

#endif // #ifdef U_CFG_TEST_CELL_MODULE_TYPE

// End of file
