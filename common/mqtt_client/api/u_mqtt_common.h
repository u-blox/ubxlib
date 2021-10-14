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

#ifndef _U_MQTT_COMMON_H_
#define _U_MQTT_COMMON_H_

/* No #includes allowed here */

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

/** MQTT QoS.  The values here should match those in uCellMqttQos_t.
 */
typedef enum {
    U_MQTT_QOS_AT_MOST_ONCE = 0,
    U_MQTT_QOS_AT_LEAST_ONCE = 1,
    U_MQTT_QOS_EXACTLY_ONCE = 2,
    U_MQTT_QOS_MAX_NUM
} uMqttQos_t;

/** Definition of an MQTT "will" message that the broker can be
 * asked to send on an uncommanded disconnect of the MQTT client.
 */
typedef struct {
    const char *pTopicNameStr; /**< the null-terminated topic string
                                    for the "will" message; may be NULL. */
    const char *pMessage;      /**< a pointer to the "will" message;
                                    the "will" message is not restricted
                                    to ASCII values.  Cannot be NULL. */
    size_t messageSizeBytes;    /**< since pMessage may include binary
                                    content, including nulls, this
                                    parameter specifies the length of
                                    pMessage. If pMessage happens to
                                    be an ASCII string this parameter
                                    should be set to strlen(pMessage). */
    uMqttQos_t qos;            /**< the MQTT QoS to use for the "will"
                                    message. */
    bool retain;               /**< if true the "will" message will
                                    be kept by the broker across
                                    MQTT disconnects/connects, else
                                    it will be cleared. */
} uMqttWill_t;

#endif // _U_MQTT_COMMON_H_

// End of file
