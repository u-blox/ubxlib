/*
 * Copyright (c) 2017, Arm Limited and affiliates.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  * http://www.apache.org/licenses/LICENSE-2.0
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
 * @brief Implementation of the Cloud Locate part of the common
 * location API.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "limits.h"    // INT_MAX
#include "stdlib.h"    // strtol()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memcpy(), strncmp(), strncpy() and strncat()
#include "ctype.h"     // isdigit()

#include "u_cfg_sw.h"
#include "u_error_common.h"

#include "u_port.h"
#include "u_port_clib_platform_specific.h" // strtok_r()
#include "u_port_heap.h"
#include "u_port_os.h"
#include "u_port_debug.h"

#include "u_time.h"

#include "u_gnss_pos.h"

#include "u_mqtt_common.h"
#include "u_mqtt_client.h"

#include "u_location.h"
#include "u_location_private_cloud_locate.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_LOCATION_PRIVATE_CLOUD_LOCATE_BUFFER_LENGTH_BYTES
/** The size of buffer for the RRLP data used by Cloud Locate,
 * should not be more than 1024 bytes which is the maximum
 * MQTT message length supported by the u-blox cellular modules.
 */
# define U_LOCATION_PRIVATE_CLOUD_LOCATE_BUFFER_LENGTH_BYTES 1024
#endif

#ifndef U_LOCATION_PRIVATE_CLOUD_LOCATE_MQTT_PUBLISH_TOPIC
/** The name of the MQTT topic to which RRLP data is
 * published for Cloud Locate.
 */
# define U_LOCATION_PRIVATE_CLOUD_LOCATE_MQTT_PUBLISH_TOPIC "CloudLocate/GNSS/request"
#endif

#ifndef U_LOCATION_PRIVATE_CLOUD_LOCATE_MQTT_SUBSCRIBE_TOPIC_PREFIX
/** The start of the name of the MQTT topic to subscribe to for
 * a location established through the Cloud Locate service;
 * the ID of the device goes after this.
 */
# define U_LOCATION_PRIVATE_CLOUD_LOCATE_MQTT_SUBSCRIBE_TOPIC_PREFIX "CloudLocate/"
#endif

#ifndef U_LOCATION_PRIVATE_CLOUD_LOCATE_MQTT_SUBSCRIBE_TOPIC_POSTFIX
/** The end of the name of the MQTT topic to subscribe to for
 * a location established through the Cloud Locate service; the
 * ID of the device goes before this.
 */
# define U_LOCATION_PRIVATE_CLOUD_LOCATE_MQTT_SUBSCRIBE_TOPIC_POSTFIX "/GNSS/response"
#endif

#ifndef U_LOCATION_PRIVATE_CLOUD_LOCATE_SUBSCRIBE_TOPIC_LENGTH_BYTES
/** The size of buffer to use for the subscribe topic.  Must be larger
 * enough for  U_LOCATION_PRIVATE_CLOUD_LOCATE_MQTT_SUBSCRIBE_TOPIC_PREFIX
 * plus U_LOCATION_PRIVATE_CLOUD_LOCATE_MQTT_SUBSCRIBE_TOPIC_POSTFIX plus
 * the longest pClientIdStr plus 1 for the terminator.  Note that this is
 * stored on the stack so it also shouldn't be too big.
 */
# define U_LOCATION_PRIVATE_CLOUD_LOCATE_SUBSCRIBE_TOPIC_LENGTH_BYTES 128
#endif

#ifndef U_LOCATION_PRIVATE_CLOUD_LOCATE_READ_MESSAGE_LENGTH_BYTES
/** The size of buffer to use for the MQTT message containing the
 * location read back from the MQTT server.
 */
# define U_LOCATION_PRIVATE_CLOUD_LOCATE_READ_MESSAGE_LENGTH_BYTES 512
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Convert a string containing a [fractional] decimal number,
// something like "-758.7387289" (should be null-terminated) into
// an int32_t with the given power of ten multiplier.
// Any leading crap is ignored and conversion stops when a
// non-numeric character is reached after the number has begun.
static int32_t stringToInt32(char *pStr, int32_t *pNumber,
                             int32_t powerOfTenWanted,
                             int32_t maxFractionalDigits,
                             char **ppEnd)
{
    int32_t errorCode = -1;
    int64_t int64 = 0;
    bool negate = false;
    bool fraction = false;
    int32_t powerOfTen = 0;
    int32_t y = 0;

    *pNumber = 0;

    // Find the start of a decimal number
    while (!isdigit((int32_t) *pStr) &&
           (*pStr != '+') && (*pStr != '-') && (*pStr != '.') && (*pStr != 0)) {
        pStr++;
    }

    if (*pStr != 0) {
        errorCode = 0;
        if (*pStr == '-') {
            negate = true;
        }
        if (!isdigit((int32_t) *pStr)) {
            pStr++;
        }
        // Now we've dealt with the sign, we should have a number
        while ((errorCode == 0) && (*pStr != 0) &&
               (powerOfTen < maxFractionalDigits) &&
               (y == 0)) {
            if (isdigit((int32_t) *pStr)) {
                // Add the digit to the result
                int64 *= 10;
                int64 += *pStr - '0';
                if (int64 > INT_MAX) {
                    errorCode = -1;
                } else {
                    if (fraction) {
                        powerOfTen++;
                    }
                }
            } else if (!fraction && (*pStr == '.')) {
                fraction = true;
            } else {
                y = -1;
            }
            pStr++;
        }
    }

    if (errorCode == 0) {
        // Having obtained the number, adjust it to be the
        // wanted power of 10. For instance, if we had 356.21,
        // which would mean int64 = 35621 and powerOfTen = 2,
        // then if powerOfTenWanted was 3 the result should be
        // 356210
        y = powerOfTenWanted - powerOfTen;
        if (y > 0) {
            for (int32_t x = 0; x < y; x++) {
                int64 *= 10;
            }
        } else {
            for (int32_t x = y; x < 0; x++) {
                int64 /= 10;
            }
        }
        if (negate) {
            int64 = -int64;
        }
        *pNumber = (int32_t) int64;
    }

    if (ppEnd != NULL) {
        *ppEnd = pStr;
    }

    return errorCode;
}

// Find the start of the value of a single item, e.g.
// for \"Lat\":52.018749899999996 pKey would
// be "\"Lat\"", and a pointer to the character 5 would
// be returned
static char *pFindItem(char *pStr, const char *pKey)
{
    if (pStr != NULL) {
        pStr = strstr(pStr, pKey);
        if (pStr != NULL) {
            // Move on to the ':'
            pStr = strstr(pStr, ":");
            if (pStr != NULL) {
                // Move beyond the ":"
                pStr++;
            }
        }
    }

    return pStr;
}

// Parse location out of a message of the form:
//
// "{"Lat":52.018749899999996,"Lon":0.2471071,"Alt":120.21600000000001,"Acc":29.877,"MeasTime":"2021-11-09T18:24:11","Epochs":1}"
//
// Note that pStr is MODIFIED in this process.
static int32_t parseLocation(char *pStr, uLocation_t *pLocation)
{
    char *pSaved;
    int32_t x;
    int32_t year;
    int32_t months;

    pStr = pFindItem(pStr, "\"Lat\"");
    if (pStr != NULL) {
        // pStr should now point at 52.018749899999996
        if (stringToInt32(pStr, &x, 7, 7, &pStr) == 0) {
            pLocation->latitudeX1e7 = x;
        } else {
            pStr = NULL;
        }
    }
    pStr = pFindItem(pStr, "\"Lon\"");
    if (pStr != NULL) {
        // pStr should now point at 0.2471071
        if (stringToInt32(pStr, &x, 7, 7, &pStr) == 0) {
            pLocation->longitudeX1e7 = x;
        } else {
            pStr = NULL;
        }
    }
    pStr = pFindItem(pStr, "\"Alt\"");
    if (pStr != NULL) {
        // pStr should now point at 120.21600000000001
        if (stringToInt32(pStr, &x, 3, 3, &pStr) == 0) {
            pLocation->altitudeMillimetres = x;
        } else {
            pStr = NULL;
        }
    }
    pStr = pFindItem(pStr, "\"Acc\"");
    if (pStr != NULL) {
        // pStr should now point at 29.877
        if (stringToInt32(pStr, &x, 3, 3, &pStr) == 0) {
            pLocation->radiusMillimetres = x;
        } else {
            pStr = NULL;
        }
    }
    pStr = pFindItem(pStr, "\"MeasTime\"");
    if (pStr != NULL) {
        // pStr should now point at \"2021-11-09T18:24:11\",
        // i.e. including the quotes
        // Tokenise on "-"
        pStr = strtok_r(pStr, "-", &pSaved);
        // Skip the opening quote
        pStr++;
        // Four digit year converted to years since 1970
        year = strtol(pStr, NULL, 10) - 1970;
        if (year >= 2021 - 1970) {
            // Move pStr to the start of the month
            pStr = strtok_r(NULL, "-", &pSaved);
            if (pStr != NULL) {
                // Month (1 to 12), so take away 1 to make it zero-based
                months = strtol(pStr, NULL, 10) - 1;
                months += year * 12;
                // Work out the number of seconds due to the year/month count
                pLocation->timeUtc = uTimeMonthsToSecondsUtc(months);
            }
            // Move pStr to the start of the day and tokenize-out the "T"
            pStr = strtok_r(NULL, "T", &pSaved);
            if (pStr != NULL) {
                // Day (1 to 31)
                pLocation->timeUtc += (int64_t) (strtol(pStr, NULL, 10) - 1) * 3600 * 24;
            }
            // Move pStr to the start of the hours and tokenize-out the ":"
            pStr = strtok_r(NULL, ":", &pSaved);
            if (pStr != NULL) {
                // Hours since midnight
                pLocation->timeUtc  += (int64_t) strtol(pStr, NULL, 10) * 3600;
            }
            // Move pStr to Minutes after the hour
            pStr = strtok_r(NULL, ":", &pSaved);
            if (pStr != NULL) {
                pLocation->timeUtc  += (int64_t) strtol(pStr, NULL, 10) * 60;
            }
            // Move pStr to Seconds after the hour, which ends at the final quotation mark
            pStr = strtok_r(NULL, ":", &pSaved);
            if (pStr != NULL) {
                pLocation->timeUtc += (int64_t) strtol(pStr, NULL, 10);
            }
        } else {
            pStr = NULL;
        }
    }

    return pStr != NULL ? (int32_t) U_ERROR_COMMON_SUCCESS : (int32_t) U_ERROR_COMMON_UNKNOWN;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Run Cloud Locate.
int32_t uLocationPrivateCloudLocate(uDeviceHandle_t devHandle,
                                    uDeviceHandle_t gnssDevHandle,
                                    uMqttClientContext_t *pMqttClientContext,
                                    int32_t svsThreshold,
                                    int32_t cNoThreshold,
                                    int32_t multipathIndexLimit,
                                    int32_t pseudorangeRmsErrorIndexLimit,
                                    const char *pClientIdStr,
                                    uLocation_t *pLocation,
                                    bool (*pKeepGoingCallback) (uDeviceHandle_t))
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    char *pBuffer;
    char topicBuffer[U_LOCATION_PRIVATE_CLOUD_LOCATE_SUBSCRIBE_TOPIC_LENGTH_BYTES];
    char *pTopicBufferRead;
    char *pMessageRead;
    int32_t startTimeMs = uPortGetTickTimeMs();
    bool subscribed = false;
    size_t z;

    if ((gnssDevHandle != NULL) && (pMqttClientContext != NULL) &&
        ((pLocation == NULL) || (pClientIdStr != NULL))) {
        errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
        // Allocate memory to store the RRLP information
        pBuffer = (char *) pUPortMalloc(U_LOCATION_PRIVATE_CLOUD_LOCATE_BUFFER_LENGTH_BYTES);
        if (pBuffer != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            if ((pClientIdStr != NULL) && (pLocation != NULL)) {
                // If the device also wanted the location, assemble the name
                // of the subscribe topic and subscribe to it
                strncpy(topicBuffer, U_LOCATION_PRIVATE_CLOUD_LOCATE_MQTT_SUBSCRIBE_TOPIC_PREFIX,
                        sizeof(topicBuffer));
                strncat(topicBuffer, pClientIdStr,
                        sizeof(topicBuffer) - strlen(topicBuffer) - 1); // -1 to allow room for terminator
                strncat(topicBuffer, U_LOCATION_PRIVATE_CLOUD_LOCATE_MQTT_SUBSCRIBE_TOPIC_POSTFIX,
                        sizeof(topicBuffer) - strlen(topicBuffer) - 1); // -1 to allow room for terminator
                errorCode = uMqttClientSubscribe(pMqttClientContext, topicBuffer, U_MQTT_QOS_EXACTLY_ONCE);
                subscribed = (errorCode >= 0);
            }

            if (errorCode >= 0) { // >= 0 since uMqttClientSubscribe() returns QoS
                // Get the RRLP data from the GNSS chip
                errorCode = uGnssPosGetRrlp(gnssDevHandle, pBuffer,
                                            U_LOCATION_PRIVATE_CLOUD_LOCATE_BUFFER_LENGTH_BYTES,
                                            svsThreshold, cNoThreshold, multipathIndexLimit,
                                            pseudorangeRmsErrorIndexLimit,
                                            pKeepGoingCallback);
                if (errorCode >= 0) {
                    // Send the RRLP data to the Cloud Locate service using MQTT
                    errorCode = uMqttClientPublish(pMqttClientContext,
                                                   U_LOCATION_PRIVATE_CLOUD_LOCATE_MQTT_PUBLISH_TOPIC,
                                                   pBuffer, errorCode,
                                                   U_MQTT_QOS_EXACTLY_ONCE, false);
                }
            }

            // Free memory
            uPortFree(pBuffer);

            if ((errorCode == 0) && (pClientIdStr != NULL) && (pLocation != NULL)) {
                // If all of that was successful, and after we've
                // freed memory, if the user wanted the location
                // wait for it to turn up
                pLocation->latitudeX1e7 = 0;
                pLocation->longitudeX1e7 = 0;
                pLocation->altitudeMillimetres = INT_MIN;
                pLocation->radiusMillimetres = -1;
                pLocation->speedMillimetresPerSecond = INT_MIN;
                pLocation->svs = -1;
                pLocation->timeUtc = -1;
                errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                pTopicBufferRead = (char *) pUPortMalloc(
                                       U_LOCATION_PRIVATE_CLOUD_LOCATE_SUBSCRIBE_TOPIC_LENGTH_BYTES);
                if (pTopicBufferRead != NULL) {
                    // +1 to allow us to insert a terminator
                    pMessageRead = (char *) pUPortMalloc(U_LOCATION_PRIVATE_CLOUD_LOCATE_READ_MESSAGE_LENGTH_BYTES + 1);
                    if (pMessageRead != NULL) {
                        errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
                        uPortLog("U_LOCATION_PRIVATE_CLOUD_LOCATE: RRLP sent, waiting for"
                                 " location from server...\n");
                        while ((errorCode == (int32_t) U_ERROR_COMMON_TIMEOUT) &&
                               (((pKeepGoingCallback == NULL) &&
                                 (uPortGetTickTimeMs() - startTimeMs) / 1000 < U_LOCATION_TIMEOUT_SECONDS) ||
                                ((pKeepGoingCallback != NULL) && pKeepGoingCallback(devHandle)))) {
                            if (uMqttClientGetUnread(pMqttClientContext) > 0) {
                                z = U_LOCATION_PRIVATE_CLOUD_LOCATE_READ_MESSAGE_LENGTH_BYTES;
                                errorCode = uMqttClientMessageRead(pMqttClientContext,
                                                                   pTopicBufferRead,
                                                                   U_LOCATION_PRIVATE_CLOUD_LOCATE_SUBSCRIBE_TOPIC_LENGTH_BYTES,
                                                                   pMessageRead,
                                                                   &z, NULL);
                                if (errorCode == 0) {
                                    // Add a terminator to make the message a string so that
                                    // parseLocation() can parse it later
                                    *(pMessageRead + z) = 0;
                                    //lint -esym(645, topicBuffer) Suppress warning about
                                    // topicBuffer not being initialised: it is but it is
                                    // too difficult for Lint to track
                                    if (strncmp(pTopicBufferRead, topicBuffer, sizeof(topicBuffer)) != 0) {
                                        // Not our topic, keep the timeout
                                        errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
                                    }
                                }
                            }
                            if (errorCode < 0) {
                                uPortTaskBlock(1000);
                            }
                        }

                        if (errorCode == 0) {
                            // Parse the location out of the MQTT message
                            errorCode = parseLocation(pMessageRead, pLocation);
                        }

                        // Free message memory
                        uPortFree(pMessageRead);
                    }

                    // Free topic memory
                    uPortFree(pTopicBufferRead);
                }
            }

            if (subscribed) {
                // Unsubscribe from the topic, for neatness
                uMqttClientUnsubscribe(pMqttClientContext, topicBuffer);
            }
        }
    }

    return errorCode;
}

// End of file
