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

#ifndef _U_MQTT_COMMON_H_
#define _U_MQTT_COMMON_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h"

/** \addtogroup MQTT-Client
 *  @{
 */

/** @file
 * @brief This header file contains definitions common across
 * the MQTT and MQTT-SN protocols.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The default MQTT broker port for unsecured operation.
 */
#define U_MQTT_BROKER_PORT_UNSECURE 1883

/** The default MQTT broker port for TLS secured operation.
 */
#define U_MQTT_BROKER_PORT_SECURE 8883

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** MQTT QoS.  The values here should match those in #uCellMqttQos_t.
 */
typedef enum {
    U_MQTT_QOS_AT_MOST_ONCE = 0,
    U_MQTT_QOS_AT_LEAST_ONCE = 1,
    U_MQTT_QOS_EXACTLY_ONCE = 2,
    U_MQTT_QOS_MAX_NUM,
    U_MQTT_QOS_SEND_AND_FORGET = 3, /**< valid for MQTT-SN publish messages only. */
    U_MQTT_QOS_SN_PUBLISH_MAX_NUM
} uMqttQos_t;

/** The type of MQTT-SN topic name.  The values here
 * should match those in #uCellMqttSnTopicNameType_t.
 */
typedef enum {
    U_MQTT_SN_TOPIC_NAME_TYPE_ID_NORMAL = 0, /**< a two-byte ID, e.g. 0x0001, referring to a normal MQTT topic, e.g. "thing/this". */
    U_MQTT_SN_TOPIC_NAME_TYPE_ID_PREDEFINED = 1, /**< a pre-agreed two byte ID, e.g. 0x0100. */
    U_MQTT_SN_TOPIC_NAME_TYPE_NAME_SHORT = 2,  /**< two-characters, e.g. "ab". */
    U_MQTT_SN_TOPIC_NAME_TYPE_MAX_NUM
} uMqttSnTopicNameType_t;

/** This type holds the two sorts of MQTT-SN topic name; a uint16_t
 * ID (0 to 65535) or a two-character name (for instance "ab"). The
 * structure here MUST match #uCellMqttSnTopicName_t.
 */
typedef struct {
// *INDENT-OFF* (otherwise AStyle makes a mess of this)
    union {
        uint16_t id; /**< populate this for the types #U_MQTT_SN_TOPIC_NAME_TYPE_ID_NORMAL
                          or #U_MQTT_SN_TOPIC_NAME_TYPE_ID_PREDEFINED. */
        // nameShort MUST be of length 2, as defined by the MQTT-SN specifications; the
        // code is written such that no terminating 0 is required in the storage here.
        char nameShort[2]; /**< populate this for #U_MQTT_SN_TOPIC_NAME_TYPE_NAME_SHORT;
                                nameShort must contain two ASCII characters, no
                                terminator is required. */
    } name;
    uMqttSnTopicNameType_t type; /**< if the id field is populated and was obtained
                                      through uMqttClientSnRegisterNormalTopic()
                                      or uMqttClientSnSubscribeNormalTopic() then set this to
                                      #U_MQTT_SN_TOPIC_NAME_TYPE_ID_NORMAL.  If the id field
                                      is populated and is a predefined topic ID then set
                                      this to #U_MQTT_SN_TOPIC_NAME_TYPE_ID_PREDEFINED.  If the
                                      nameShort field is populated, set this to
                                      #U_MQTT_SN_TOPIC_NAME_TYPE_NAME_SHORT. */
// *INDENT-ON*
} uMqttSnTopicName_t;

/** Definition of an MQTT "will" message that the broker can be
 * asked to send on an uncommanded disconnect of the MQTT client.
 */
typedef struct {
    const char *pTopicNameStr; /**< the null-terminated topic string
                                    for the "will" message; may be NULL. */
    const char *pMessage;      /**< a pointer to the "will" message;
                                    for MQTT the "will" message is not
                                    restricted to ASCII values, however,
                                    for MQTT-SN the underlying AT interface
                                    ONLY works if pMessge is a null-terminated
                                    ASCII string containing only printable
                                    characters (isprint() returns true)
                                    and no double quotation marks (").
                                    Cannot be NULL. */
    size_t messageSizeBytes;   /**< since pMessage may include binary
                                    content, including nulls, this
                                    parameter specifies the length of
                                    pMessage. If pMessage is an ASCII
                                    string this parameter should be set
                                    to strlen(pMessage). */
    uMqttQos_t qos;            /**< the MQTT QoS to use for the "will"
                                    message. */
    bool retain;               /**< if true the "will" message will
                                    be kept by the broker across
                                    MQTT disconnects/connects, else
                                    it will be cleared. */
} uMqttWill_t;

/** @}*/

#endif // _U_MQTT_COMMON_H_

// End of file
