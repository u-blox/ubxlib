/*
 * Copyright 2020 u-blox
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

#ifndef _U_LOCATION_TEST_SHARED_CFG_H_
#define _U_LOCATION_TEST_SHARED_CFG_H_

/* No #includes allowed here */

/** @file
 * @brief Types and location test configuration information shared
 * between testing of the location and network APIs.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_LOCATION_TEST_CFG_TIMEOUT_SECONDS
/** The location establishment timeout to use during testing, in
 * seconds.
 */
# define U_LOCATION_TEST_CFG_TIMEOUT_SECONDS 240
#endif

#ifndef U_LOCATION_TEST_MIN_UTC_TIME
/** A minimum value for UTC time to test against (21 July 2021 13:40:36).
 */
# define U_LOCATION_TEST_MIN_UTC_TIME 1626874836
#endif

#ifndef U_LOCATION_TEST_MAX_RADIUS_MILLIMETRES
/** The maximum radius we consider valid.
 */
# define U_LOCATION_TEST_MAX_RADIUS_MILLIMETRES (10000 * 1000)
#endif

#ifndef U_LOCATION_TEST_CLOUD_LOCATE_SVS_THRESHOLD
/** The number of satellites to request as being visible for RRLP
 * information to be valid when testing Cloud Locate.
 */
# define U_LOCATION_TEST_CLOUD_LOCATE_SVS_THRESHOLD 6
#endif

#ifndef U_LOCATION_TEST_MQTT_INACTIVITY_TIMEOUT_SECONDS
/** A bit of a balancing act this.  The MQTT server will not allow
 * a device to connect if it is already connected (e.g. it may have
 * failed a test and so not disconnected and now it's trying again).
 * The inactivity timeout is intended to guard against this, but of
 * course if it is too short we'll end up being disconnected before
 * location establishment has succeeded.
 */
# define U_LOCATION_TEST_MQTT_INACTIVITY_TIMEOUT_SECONDS (U_LOCATION_TEST_CFG_TIMEOUT_SECONDS +     \
                                                          (U_LOCATION_TEST_CFG_TIMEOUT_SECONDS / 2))
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Type to hold one of the location configurations for a given
 * network.
 */
typedef struct {
    uLocationType_t locationType;
    uLocationAssist_t *pLocationAssist;
    const char *pAuthenticationTokenStr;
    const char *pServerUrlStr;
    const char *pUserNameStr;
    const char *pPasswordStr;
} uLocationTestCfg_t;

/** Type to hold the list of location configuration data supported by
 * a given network.
 */
typedef struct {
    size_t numEntries;
    const uLocationTestCfg_t *pCfgData;
} uLocationTestCfgList_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Location configurations for each network type.
 * ORDER IS IMPORTANT: follows the order of uNetworkType_t.
 */
extern const uLocationTestCfgList_t *const gpULocationTestCfg[];

/** Number of items in the gpULocationTestCfg array.
 */
extern const size_t gpULocationTestCfgSize;

/** The name of each location type, an array that has
 * U_LOCATION_TYPE_MAX_NUM entries.
 * ORDER IS IMPORTANT: follows the order of uLocationType_t.
 */
extern const char *const gpULocationTestTypeStr[];

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Reset a location structure to default values; use this before
 * a test.  All values as set to INT_MIN except timeUtc which is
 * set to LONG_MIN.
 *
 * @param pLocation  a pointer to the location structure to reset.
 */
void uLocationTestResetLocation(uLocation_t *pLocation);

/** Print a location structure for debug purposes.
 *
 * @param pLocation  a pointer to the location structure to print.
 */
void uLocationTestPrintLocation(const uLocation_t *pLocation);

/** Create a deep copy of a uLocationTestCfg_t.
 * IMPORTANT: make sure that you call uLocationTestCfgDeepCopyFree()
 * to free the memory allocated to the copy afterwards.
 *
 * @param pCfg  a pointer to the location test configuration to copy.
 * @return      a pointer to the malloc()ated copy of the location
 *              test configuration or NULL on failure.
 */
uLocationTestCfg_t *pULocationTestCfgDeepCopyMalloc(const uLocationTestCfg_t *pCfg);

/** Free a deep copy of a uLocationTestCfg_t.
 *
 * @param pCfg  the location test configuration to free.
 */
void uLocationTestCfgDeepCopyFree(uLocationTestCfg_t *pCfg);

/** Log into an MQTT broker with the given client ID.
 *
 * @param networkHandle    the network handle to use for
 *                         the MQTT transport.
 * @param pBrokerNameStr   the URL of the MQTT broker.
 * @param pUserNameStr     the username to log in with.
 * @param pPasswordStr     the password to log in with.
 * @param pClientIdStr     the MQTT client ID to use.
 * @return                 a pointer to the MQTT context,
 *                         or NULL on failure.
 */
void *pULocationTestMqttLogin(int32_t networkHandle,
                              const char *pBrokerNameStr,
                              const char *pUserNameStr,
                              const char *pPasswordStr,
                              const char *pClientIdStr);

/** Log out of an MQTT broker.
 *
 * @param pContext  a pointer to the MQTT context.
 */
void uLocationTestMqttLogout(void *pContext);

#ifdef __cplusplus
}
#endif

#endif // _U_LOCATION_TEST_SHARED_CFG_H_

// End of file
