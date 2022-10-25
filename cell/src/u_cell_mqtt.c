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
 * @brief Implementation of the u-blox MQTT client API for cellular.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // strtol()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "ctype.h"     // isdigit(), isprint()
#include "string.h"    // memset(), strncpy(), strtok_r(), strtol(), strncmp()
#include "stdio.h"     // snprintf()

#include "u_cfg_sw.h"

#include "u_error_common.h"

#include "u_assert.h"

#include "u_port_clib_platform_specific.h" /* strtok_r and integer stdio, must
                                              be included before the other port
                                              files if any print or scan function
                                              is used. */
#include "u_port_heap.h"
#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"

#include "u_hex_bin_convert.h"

#include "u_at_client.h"

#include "u_mqtt_common.h"
#include "u_mqtt_client.h"

#include "u_sock.h"

#include "u_cell_module_type.h"
#include "u_cell_file.h"
#include "u_cell.h"         // Order is
#include "u_cell_net.h"     // important here
#include "u_cell_private.h" // don't change it
#include "u_cell_info.h"    // For U_CELL_INFO_IMEI_SIZE

#include "u_cell_mqtt.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_CELL_MQTT_LOCAL_URC_TIMEOUT_MS
/** The time to wait for a URC with information we need when
 * that information is collected locally, rather than waiting
 * on the MQTT broker.
 */
# define U_CELL_MQTT_LOCAL_URC_TIMEOUT_MS 5000
#endif

#ifndef U_CELL_MQTT_CONNECT_DELAY_MILLISECONDS
/** It can take a little while for the MQTT client inside
 * the module to become aware that a radio connection has been
 * made so we wait at least this long to give it time to realise.
 */
# define U_CELL_MQTT_CONNECT_DELAY_MILLISECONDS 1000
#endif

/** Helper macro to make sure that the entry and exit functions
 * are always called.
 */
#define U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, ppInstance, pErrorCode, mustBeInitialised) \
                                   { entryFunction(cellHandle, \
                                                   ppInstance, \
                                                   pErrorCode, \
                                                   mustBeInitialised)

/** Helper macro to make sure that the entry and exit functions
 * are always called.
 */
#define U_CELL_MQTT_EXIT_FUNCTION() } exitFunction()

/** Flag bits for the flags field in uCellMqttUrcStatus_t.
 */
#define U_CELL_MQTT_URC_FLAG_CONNECT_UPDATED          0
#define U_CELL_MQTT_URC_FLAG_PUBLISH_UPDATED          1
#define U_CELL_MQTT_URC_FLAG_PUBLISH_SUCCESS          2
#define U_CELL_MQTT_URC_FLAG_SUBSCRIBE_UPDATED        3
#define U_CELL_MQTT_URC_FLAG_SUBSCRIBE_SUCCESS        4
#define U_CELL_MQTT_URC_FLAG_UNSUBSCRIBE_UPDATED      5
#define U_CELL_MQTT_URC_FLAG_UNSUBSCRIBE_SUCCESS      6
#define U_CELL_MQTT_URC_FLAG_UNREAD_MESSAGES_UPDATED  7
#define U_CELL_MQTT_URC_FLAG_SECURED                  8  // Only required for SARA-R4
#define U_CELL_MQTT_URC_FLAG_RETAINED                 9  // Only required for SARA-R4
#define U_CELL_MQTT_URC_FLAG_SECURED_FILLED_IN        10 // Only required for SARA-R4
#define U_CELL_MQTT_URC_FLAG_RETAINED_FILLED_IN       11 // Only required for SARA-R4
#define U_CELL_MQTT_URC_FLAG_REGISTER_UPDATED         12 // MQTT-SN only
#define U_CELL_MQTT_URC_FLAG_REGISTER_SUCCESS         13 // MQTT-SN only
#define U_CELL_MQTT_URC_FLAG_WILL_PARAMETERS_UPDATED  14 // MQTT-SN only
#define U_CELL_MQTT_URC_FLAG_WILL_PARAMETERS_SUCCESS  15 // MQTT-SN only
#define U_CELL_MQTT_URC_FLAG_WILL_MESSAGE_UPDATED     16 // MQTT-SN only
#define U_CELL_MQTT_URC_FLAG_WILL_MESSAGE_SUCCESS     17 // MQTT-SN only

/** Macro to get the right AT command string for AT+UMQTTC,
 * AKA the "MQTT command" AT command, in its SN and non-SN version. */
#define MQTT_COMMAND_AT_COMMAND_STRING(mqttSn) (mqttSn ? "AT+UMQTTSNC=" : "AT+UMQTTC=")

/** Macro to get the right AT response string for AT+UMQTTC in
 * its SN and non-SN version. */
#define MQTT_COMMAND_AT_RESPONSE_STRING(mqttSn) (mqttSn ? "+UMQTTSNC:" : "+UMQTTC:")

/** Macro to get the right AT command string for AT+UMQTT,
 * AKA the "MQTT profile" AT command, in its SN and non-SN version. */
#define MQTT_PROFILE_AT_COMMAND_STRING(mqttSn) (mqttSn ? "AT+UMQTTSN=" : "AT+UMQTT=")

/** Macro to get the right AT response string for AT+UMQTT in
 * its SN and non-SN version. */
#define MQTT_PROFILE_AT_RESPONSE_STRING(mqttSn) (mqttSn ? "+UMQTTSN:" : "+UMQTT:")

/** Macro to get the right AT command string for AT+UMQTTER in
 * its SN and non-SN version. */
#define MQTT_ERROR_AT_COMMAND_STRING(mqttSn) (mqttSn ? "AT+UMQTTSNER" : "AT+UMQTTER")

/** Macro to get the right AT response string for AT+UMQTTER in
 * its SN and non-SN version. */
#define MQTT_ERROR_AT_RESPONSE_STRING(mqttSn) (mqttSn ? "+UMQTTSNER:" : "+UMQTTER:")

/** Macro to get the AT+UMQTT/AT+UMQTTSN opcode for "client ID".
 */
#define MQTT_PROFILE_OPCODE_CLIENT_ID(mqttSn) (0)

/** Macro to get the AT+UMQTT/AT+UMQTTSN opcode for "broker name".
 */
#define MQTT_PROFILE_OPCODE_BROKER_URL(mqttSn) (mqttSn ? 1 : 2)

/** Macro to get the AT+UMQTT/AT+UMQTTSN opcode for "broker IP address".
 */
#define MQTT_PROFILE_OPCODE_BROKER_IP_ADDRESS(mqttSn) (mqttSn ? 2 : 3)

/** Macro to get the AT+UMQTT/AT+UMQTTSN opcode for "will QoS".
 */
#define MQTT_PROFILE_OPCODE_WILL_QOS(mqttSn) (mqttSn ? 4 : 6)

/** Macro to get the AT+UMQTT/AT+UMQTTSN opcode for "will retention".
 */
#define MQTT_PROFILE_OPCODE_WILL_RETAIN(mqttSn) (mqttSn ? 5 : 7)

/** Macro to get the AT+UMQTT/AT+UMQTTSN opcode for "will topic".
 */
#define MQTT_PROFILE_OPCODE_WILL_TOPIC(mqttSn) (mqttSn ? 6 : 8)

/** Macro to get the AT+UMQTT/AT+UMQTTSN opcode for "will message".
 */
#define MQTT_PROFILE_OPCODE_WILL_MESSAGE(mqttSn) (mqttSn ? 7 : 9)

/** Macro to get the AT+UMQTT/AT+UMQTTSN opcode for "inactivity timeout".
 */
#define MQTT_PROFILE_OPCODE_INACTIVITY_TIMEOUT(mqttSn) (mqttSn ? 8 : 10)

/** Macro to get the AT+UMQTT/AT+UMQTTSN opcode for "secure".
 */
#define MQTT_PROFILE_OPCODE_SECURE(mqttSn) (mqttSn ? 9 : 11)

/** Macro to get the AT+UMQTT/AT+UMQTTSN opcode for "clean session".
 */
#define MQTT_PROFILE_OPCODE_CLEAN_SESSION(mqttSn) (mqttSn ? 10 : 12)

/** Macro to get the AT+UMQTTC/AT+UMQTTSNC opcode for "publish string".
 */
#define MQTT_COMMAND_OPCODE_PUBLISH_STRING(mqttSn) (mqttSn ? 4 : 2)

/** Macro to get the AT+UMQTTC/AT+UMQTTSNC opcode for "subscribe".
 */
#define MQTT_COMMAND_OPCODE_SUBSCRIBE(mqttSn) (mqttSn ? 5 : 4)

/** Macro to get the AT+UMQTTC/AT+UMQTTSNC opcode for "unsubscribe".
 */
#define MQTT_COMMAND_OPCODE_UNSUBSCRIBE(mqttSn) (mqttSn ? 6 : 5)

/** Macro to get the AT+UMQTTC/AT+UMQTTSNC opcode for "read".
 */
#define MQTT_COMMAND_OPCODE_READ(mqttSn) (mqttSn ? 9 : 6)

/** Macro to get the AT+UMQTTC/AT+UMQTTSNC opcode for "ping".
 */
#define MQTT_COMMAND_OPCODE_PING(mqttSn) (mqttSn ? 10 : 8)

/** The amount of storage required for an MQTT-SN 16-bit topic name;
 * as a string, including a null terminator.
 */
#define U_CELL_MQTT_SN_TOPIC_NAME_MAX_LENGTH_BYTES 6

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Struct defining a buffer with a length, for use in
 * uCellMqttUrcStatus_t.
 */
typedef struct {
    char *pContents;
    size_t sizeBytes;
    bool filledIn;
} uCellMqttBuffer_t;

/** Struct to hold all the things an MQTT URC might tell us.
 */
typedef struct {
    uint32_t flagsBitmap;
    uCellMqttQos_t subscribeQoS;
    int32_t topicId;
    char topicNameShort[U_CELL_MQTT_SN_TOPIC_NAME_MAX_LENGTH_BYTES];
    // The remaining parameters are only
    // required for SARA-R4 which sends
    // the status back in a URC
    uCellMqttBuffer_t clientId;
    int32_t localPortNumber;
    int32_t inactivityTimeoutSeconds;
    int32_t securityProfileId;
} uCellMqttUrcStatus_t;

/** Struct to hold a message that has been read in a callback,
 * required for SARA-R4 only.
 */
typedef struct {
    char *pTopicNameStr;
    int32_t topicNameSizeBytes;
    char *pMessage;
    int32_t messageSizeBytes;
    uCellMqttQos_t qos;
    bool messageRead;
} uCellMqttUrcMessage_t;

/** Struct bringing all of the above together.
 */
typedef struct {
    bool (*pKeepGoingCallback)(void); /**< callback to be called while
                                           in a function which may have
                                           to wait for a broker's response.*/
    void (*pMessageIndicationCallback) (int32_t, void *); /**< callback to
                                                               be called when
                                                               an indication
                                                               of messages
                                                               waiting to be
                                                               read has been
                                                               received. */
    void *pMessageIndicationCallbackParam; /**< user parameter to be
                                                passed to the message
                                                indication callback. */
    void (*pDisconnectCallback) (int32_t, void *); /**< callback to
                                                        be called when
                                                        the connection
                                                        is dropped. */
    void *pDisconnectCallbackParam; /**< user parameter to be
                                         passed to the disconnect
                                         callback. */
    bool keptAlive;  /**< keep track of whether "keep alive" is on or not. */
    bool connected;  /**< keep track of whether we are connected or not. */
    size_t numUnreadMessages; /**< keep track of the number of unread messages. */
    char *pBrokerNameStr; /**< broker name string, required for SARA-R4 only. */
    volatile uCellMqttUrcStatus_t urcStatus; /**< store the status values from a URC. */
    volatile uCellMqttUrcMessage_t *pUrcMessage; /**< storage for an MQTT message
                                                      received in a URC, only
                                                      required for SARA-R4. */
    size_t numTries; /**< The number of tries for a radio-related operation. */
    bool mqttSn; /**< true if this is an MQTT-SN session, else false. */
} uCellMqttContext_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** The values of MQTT error code that mean a retry should be performed.
 */
const int32_t gMqttRetryErrorCode[] = {33 /* Timeout */, 34 /* No radio service */};

/** The values of MQTT-SN error code that mean a retry should be performed.
 */
const int32_t gMqttSnRetryErrorCode[] = {21 /* Timeout */, 22 /* No radio service */};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: URCS AND RELATED FUNCTIONS
 * -------------------------------------------------------------- */

// Get the last MQTT error code.
static int32_t getLastMqttErrorCode(const uCellPrivateInstance_t *pInstance)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_UNKNOWN;
    volatile uCellMqttContext_t *pContext;
    bool mqttSn;
    uAtClientHandle_t atHandle;
    int32_t x;

    if ((pInstance != NULL) && (pInstance->pMqttContext != NULL)) {
        atHandle = pInstance->atHandle;
        pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
        mqttSn = pContext->mqttSn;
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, MQTT_ERROR_AT_COMMAND_STRING(mqttSn));
        uAtClientCommandStop(atHandle);
        uAtClientResponseStart(atHandle, MQTT_ERROR_AT_RESPONSE_STRING(mqttSn));
        // Skip the first error code, which is a generic thing
        uAtClientSkipParameters(atHandle, 1);
        x = uAtClientReadInt(atHandle);
        uAtClientResponseStop(atHandle);
        errorCode = uAtClientUnlock(atHandle);
        if (errorCode == 0) {
            errorCode = x;
        }
    }

    return errorCode;
}

// A local "trampoline" for the message indication callback,
// here so that it can call pMessageIndicationCallback
// in a separate task.
static void messageIndicationCallback(uAtClientHandle_t atHandle,
                                      void *pParam)
{
    //lint -e(507) Suppress size incompatibility due to the compiler
    // we use for Linting being a 64 bit one where the pointer
    // is 64 bit.
    volatile uCellMqttContext_t *pContext = (volatile uCellMqttContext_t *) pParam;

    (void) atHandle;

    // This task can lock the mutex to ensure we are thread-safe
    // for the call below
    U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

    if ((pContext != NULL) && (pContext->pMessageIndicationCallback != NULL)) {
        pContext->pMessageIndicationCallback((int32_t) pContext->numUnreadMessages,
                                             pContext->pMessageIndicationCallbackParam);
    }

    U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
}

// A local "trampoline" for the disconnect callback,
// here so that it can call pDisconnectCallback
// in a separate task.
//lint -esym(818, pParam) Suppress "could be pointing to const",
// gotta follow the function signature
static void disconnectCallback(uAtClientHandle_t atHandle,
                               void *pParam)
{
    //lint -e(507) Suppress size incompatibility due to the compiler
    // we use for Linting being a 64 bit one where the pointer
    // is 64 bit.
    const uCellPrivateInstance_t *pInstance = (const uCellPrivateInstance_t *) pParam;
    volatile uCellMqttContext_t *pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;

    (void) atHandle;

    // This task can lock the mutex to ensure we are thread-safe
    // for the call below
    U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

    if ((pContext != NULL) && (pContext->pDisconnectCallback != NULL)) {
        pContext->pDisconnectCallback(getLastMqttErrorCode(pInstance),
                                      pContext->pDisconnectCallbackParam);
    }

    U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
}

// "+UUMQTTC:"/"+UUMQTTSNC" URC handler, called by the UUMQTT_urc()
// URC handler..
static void UUMQTTC_UUMQTTSNC_urc(uAtClientHandle_t atHandle,
                                  volatile uCellMqttContext_t *pContext,
                                  const uCellPrivateInstance_t *pInstance)
{
    volatile uCellMqttUrcStatus_t *pUrcStatus = &(pContext->urcStatus);
    bool mqttSn = pContext->mqttSn;
    int32_t urcType;
    int32_t urcParam1;
    int32_t urcParam2;

    urcType = uAtClientReadInt(atHandle);
    // All of the MQTTC/MQTTSNC URC types have at least one parameter
    urcParam1 = uAtClientReadInt(atHandle);
    // Can't use a switch() statement here as some of the values we get
    // back are different depending on whether this is UUMQTTC (MQTT)
    // or UUMQTTSNC (MQTT-SN)
    if (urcType == 0) {
        // Logout/disonnect, where 1 means success
        if ((urcParam1 == 1) ||
            (urcParam1 == 100) || // SARA-R5/R422, inactivity
            (urcParam1 == 101) || // SARA-R5/R422, connection lost
            (urcParam1 == 102)) { // SARA-R5/R422, connection lost due to protocol violation
            // Disconnected
            if (pContext->connected &&
                (pContext->pDisconnectCallback != NULL)) {
                // Launch the local callback via the AT
                // parser's callback facility.
                //lint -e(1773) Suppress complaints about
                // passing the pointer as non-volatile
                uAtClientCallback(atHandle, disconnectCallback,
                                  (void *) pInstance);
            }
            pContext->connected = false;
            // Keep alive returns to "off" when the session ends,
            // it must be set afresh each time
            pContext->keptAlive = false;
        }
        pUrcStatus->flagsBitmap |= 1 << U_CELL_MQTT_URC_FLAG_CONNECT_UPDATED;
    } else if (urcType == 1) {
        // Login
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
            // In the old SARA-R4 syntax, 0 means success,
            // non-zero values are errors
            if (urcParam1 == 0) {
                // Connected
                pContext->connected = true;
            }
        } else {
            if (urcParam1 == 1) {
                // Connected
                pContext->connected = true;
            }
        }
        pUrcStatus->flagsBitmap |= 1 << U_CELL_MQTT_URC_FLAG_CONNECT_UPDATED;
    } else if ((urcType == MQTT_COMMAND_OPCODE_PUBLISH_STRING(mqttSn)) ||
               (!mqttSn && (urcType == 9))) {
        // Publish hex or binary, 1 means success
        if (urcParam1 == 1) {
            // Published
            pUrcStatus->flagsBitmap |= 1 << U_CELL_MQTT_URC_FLAG_PUBLISH_SUCCESS;
        }
        pUrcStatus->flagsBitmap |= 1 << U_CELL_MQTT_URC_FLAG_PUBLISH_UPDATED;
    } else if (urcType == MQTT_COMMAND_OPCODE_SUBSCRIBE(mqttSn)) {
        // Subscribe
        // Get the QoS
        urcParam2 = uAtClientReadInt(atHandle);
        if (!mqttSn) {
            // For normal MQTT, skip the topic string
            uAtClientSkipParameters(atHandle, 1);
        } else {
            // For MQTT-SN the topic ID or short topic name to use when
            // publishing to this topic may come next
            //lint -e{1773} Suppress attempt to cast away volatile
            uAtClientReadString(atHandle, (char *) pUrcStatus->topicNameShort,
                                sizeof(pUrcStatus->topicNameShort), false);
        }
        if (U_CELL_PRIVATE_MODULE_IS_SARA_R4(pInstance->pModule->moduleType)) {
            // On SARA-R4, 0 to 2 mean success
            if ((urcParam1 >= 0) && (urcParam1 <= 2) &&
                (urcParam2 >= 0)) {
                // Subscribed
                pUrcStatus->flagsBitmap |= 1 << U_CELL_MQTT_URC_FLAG_SUBSCRIBE_SUCCESS;
                pUrcStatus->subscribeQoS = (uCellMqttQos_t) urcParam2;
            }
        } else {
            // Elsewhere 1 means success
            if ((urcParam1 == 1) && (urcParam2 >= 0)) {
                // Subscribed
                pUrcStatus->flagsBitmap |= 1 << U_CELL_MQTT_URC_FLAG_SUBSCRIBE_SUCCESS;
                pUrcStatus->subscribeQoS = (uCellMqttQos_t) urcParam2;
            }
        }
        pUrcStatus->flagsBitmap |= 1 << U_CELL_MQTT_URC_FLAG_SUBSCRIBE_UPDATED;
    } else if (urcType == MQTT_COMMAND_OPCODE_UNSUBSCRIBE(mqttSn)) {
        // Unsubscribe, 1 means success
        if (urcParam1 == 1) {
            // Unsubscribed
            pUrcStatus->flagsBitmap |= 1 << U_CELL_MQTT_URC_FLAG_UNSUBSCRIBE_SUCCESS;
        }
        pUrcStatus->flagsBitmap |= 1 << U_CELL_MQTT_URC_FLAG_UNSUBSCRIBE_UPDATED;
    } else if (urcType == MQTT_COMMAND_OPCODE_READ(mqttSn)) {
        // Read: urcParam1 contains the number of unread messages
        if (urcParam1 >= 0) {
            pContext->numUnreadMessages = urcParam1;
            if (pContext->pMessageIndicationCallback != NULL) {
                // Launch our local callback via the AT
                // parser's callback facility.
                // GCC can complain here that
                // we're discarding volatile
                // from the pointer: just need to follow
                // the function signature guys...
                //lint -e(1773) Suppress complaints about
                // passing the pointer as non-volatile
                uAtClientCallback(atHandle, messageIndicationCallback,
                                  (void *) pContext);
            }
        }
        pUrcStatus->flagsBitmap |= 1 << U_CELL_MQTT_URC_FLAG_UNREAD_MESSAGES_UPDATED;
    } else {
        if (mqttSn) {
            // For MQTT-SN there are some additional possibilities
            switch (urcType) {
                case 2: // Register, 1 means success
                    // Read the topic ID, which is an integer at this point
                    urcParam2 = uAtClientReadInt(atHandle);
                    if ((urcParam1 == 1) && (urcParam2 >= 0)) {
                        pUrcStatus->topicId = urcParam2;
                        pUrcStatus->flagsBitmap |= 1 << U_CELL_MQTT_URC_FLAG_REGISTER_SUCCESS;
                    }
                    pUrcStatus->flagsBitmap |= 1 << U_CELL_MQTT_URC_FLAG_REGISTER_UPDATED;
                    break;
                case 7: // Will parameters update, 1 means success
                    if (urcParam1 == 1) {
                        pUrcStatus->flagsBitmap |= 1 << U_CELL_MQTT_URC_FLAG_WILL_PARAMETERS_SUCCESS;
                    }
                    pUrcStatus->flagsBitmap |= 1 << U_CELL_MQTT_URC_FLAG_WILL_PARAMETERS_UPDATED;
                    break;
                case 8: // Will message update, 1 means success
                    if (urcParam1 == 1) {
                        pUrcStatus->flagsBitmap |= 1 << U_CELL_MQTT_URC_FLAG_WILL_MESSAGE_SUCCESS;
                    }
                    pUrcStatus->flagsBitmap |= 1 << U_CELL_MQTT_URC_FLAG_WILL_MESSAGE_UPDATED;
                    break;
                default:
                    break;
            }
        }
    }
}

// "+UUMQTTx:" URC handler, for SARA-R4 (old style) only,
// called by the UUMQTT_urc() URC handler.
// The switch statement here needs to match those in
// resetUrcStatusField() and checkUrcStatusField()
static void UUMQTTx_urc(uAtClientHandle_t atHandle,
                        volatile uCellMqttContext_t *pContext,
                        int32_t x)
{
    volatile uCellMqttUrcStatus_t *pUrcStatus = &(pContext->urcStatus);
    char delimiter = uAtClientDelimiterGet(atHandle);
    char buffer[10]; // Enough room for a number as a string
    int32_t y;

    // All these parameters are delimited by
    // a carriage return
    uAtClientDelimiterSet(atHandle, '\r');

    // Note: no need to macroise half the world and use
    // if/else instead of switch() here because the old-style
    // AT command SARA-R4's do not support MQTT-SN
    switch (x) {
        case 0: // Client name
            if (!pUrcStatus->clientId.filledIn) {
                y = uAtClientReadString(atHandle,
                                        pUrcStatus->clientId.pContents,
                                        pUrcStatus->clientId.sizeBytes,
                                        false);
                if (y > 0) {
                    pUrcStatus->clientId.filledIn = true;
                    pUrcStatus->clientId.sizeBytes = (size_t) (unsigned) y;
                }
            }
            break;
        case 1: // Local port number
            // If the local port number has not been set then what we
            // get is an empty string and not an integer at all, so
            // need to read it as a string and convert it
            y = uAtClientReadString(atHandle, buffer, sizeof(buffer), false);
            if (y >= 0) {
                pUrcStatus->localPortNumber = strtol(buffer, NULL, 10);
            }
            break;
        case 2: // Server name
        case 3: // Server IP address
        case 4: // User name and password
            // Nothing to do, we never read these back
            break;
        // There is no number 5
        case 6: // Will QoS value
        case 7: // Will clean value
        case 8: // Will topic value
        case 9: // The will message
            // Not supported in the old SARA-R4 syntax
            break;
        case 10: // Inactivity timeout
            pUrcStatus->inactivityTimeoutSeconds = uAtClientReadInt(atHandle);
            break;
        case 11: // TLS secured
            y = uAtClientReadInt(atHandle);
            if (y >= 0) {
                pUrcStatus->flagsBitmap |= 1 << U_CELL_MQTT_URC_FLAG_SECURED_FILLED_IN;
                if (y == 1) {
                    pUrcStatus->flagsBitmap |= 1 << U_CELL_MQTT_URC_FLAG_SECURED;
                    pUrcStatus->securityProfileId = uAtClientReadInt(atHandle);
                }
            }
            break;
        case 12: // Session retained (actually session cleaned, hence the inversion)
            y = uAtClientReadInt(atHandle);
            if (y >= 0) {
                pUrcStatus->flagsBitmap |= 1 << U_CELL_MQTT_URC_FLAG_RETAINED_FILLED_IN;
                if (y == 0) {
                    pUrcStatus->flagsBitmap |= 1 << U_CELL_MQTT_URC_FLAG_RETAINED;
                }
            }
            break;
        default:
            // Do nothing
            break;
    }
    uAtClientDelimiterSet(atHandle, delimiter);
}

// "+UUMQTTCM:" URC handler, for SARA-R4 only,
// called by the UUMQTT_urc() URC handler.
static void UUMQTTCM_urc(uAtClientHandle_t atHandle,
                         volatile uCellMqttContext_t *pContext)
{
    volatile uCellMqttUrcMessage_t *pUrcMessage = pContext->pUrcMessage;
    int32_t x;
    int32_t topicNameBytesRead = 0;
    int32_t messageBytesAvailable = 0;
    char buffer[20]; // Enough room for "Len:xxxx QoS:y\r\n"
    char *pSaved;
    char *pStr;
    bool gotLengthAndQos = false;
    char delimiter = uAtClientDelimiterGet(atHandle);

    // Skip the op code
    uAtClientSkipParameters(atHandle, 1);
    // Set the delimiter to '\r' so that we stop after
    // reading number of unread messages
    uAtClientDelimiterSet(atHandle, '\r');
    // Switch off the stop tag also; the format here
    // is way too wacky, we just have to knife-and-fork it
    uAtClientIgnoreStopTag(atHandle);
    // Read the new number of unread messages
    x = uAtClientReadInt(atHandle);
    if (x >= 0) {
        pContext->numUnreadMessages = x;
    }
    // If this URC is a result of a message
    // arriving what follows will be
    // \r\n
    // Topic:blah\r\r\n
    // Len:64 QoS:2\r\r\n
    // Msg:blah\r\n
    // ...noting no quotations marks around anything
    // Carry on with a delimiter of '\r' to wend our
    // way through this merry maze.

    // Read the next 8 bytes and to see if they are
    // "\r\nTopic:"
    x = uAtClientReadBytes(atHandle, buffer, 8, true);
    if ((x == 8) &&
        (memcmp(buffer, "\r\nTopic:", 8) == 0)) {
        if (pUrcMessage != NULL) {
            if (pUrcMessage->pTopicNameStr != NULL) {
                // Read the rest of this line, which will be the topic
                // the delimiter will stop us
                topicNameBytesRead = uAtClientReadString(atHandle,
                                                         pUrcMessage->pTopicNameStr,
                                                         pUrcMessage->topicNameSizeBytes,
                                                         false);
            }
            if (topicNameBytesRead >= 0) {
                pUrcMessage->topicNameSizeBytes = topicNameBytesRead;
                // Skip the "\r\n"
                uAtClientSkipBytes(atHandle, 2);
                // Read the next line and find the length of the message
                // and the QoS from it; again the delimiter will stop us
                x = uAtClientReadString(atHandle, buffer, sizeof(buffer) - 1, false);
                if (x >= 0) {
                    buffer[x] = '\0';
                    pStr = strtok_r(buffer, " ", &pSaved);
                    if ((pStr != NULL) && (strncmp(pStr, "Len:", 4) == 0)) {
                        messageBytesAvailable = strtol(pStr + 4, NULL, 10);
                    }
                    pStr = strtok_r(NULL, " ", &pSaved);
                    if ((pStr != NULL) && (strncmp(pStr, "QoS:", 4) == 0)) {
                        pUrcMessage->qos = (uCellMqttQos_t) strtol(pStr + 4, NULL, 10);
                        gotLengthAndQos = true;
                    }
                    if (gotLengthAndQos && (messageBytesAvailable >= 0)) {
                        // Skip the "\r\nMsg:" bit
                        uAtClientSkipBytes(atHandle, 6);
                        // Now read the exact number of message
                        // bytes, ignoring delimiters
                        x = messageBytesAvailable;
                        if (x > pUrcMessage->messageSizeBytes) {
                            x = pUrcMessage->messageSizeBytes;
                        }
                        pUrcMessage->messageSizeBytes = 0;
                        pUrcMessage->messageSizeBytes = uAtClientReadBytes(atHandle,
                                                                           pUrcMessage->pMessage,
                                                                           x, true);
                        if (pUrcMessage->messageSizeBytes == x) {
                            // Done.  Phew.
                            pUrcMessage->messageRead = true;
                            // Throw away any remainder
                            if (messageBytesAvailable > x) {
                                uAtClientReadBytes(atHandle, NULL,
                                                   // Cast in two stages to keep Lint happy
                                                   (size_t) (unsigned) (messageBytesAvailable - x),
                                                   true);
                            }
                        }
                    }
                }
            }
        }
    } else {
        // If there was no topic name this must be just an indication
        // of the number of messages read so call the callback
        if (pContext->pMessageIndicationCallback != NULL) {
            // Launch our local callback via the AT
            // parser's callback facility
            //lint -e(1773) Suppress complaints about
            // passing the pointer as non-volatile
            uAtClientCallback(atHandle, messageIndicationCallback,
                              (void *) pContext);
        }
    }
    uAtClientRestoreStopTag(atHandle);
    uAtClientDelimiterSet(atHandle, delimiter);
}

// MQTT URC handler, which hands
// off to the four MQTT URC types,
// "+UUMQTTx:" (where x can be a two
// digit number), "+UUMQTTC:", "+UUMQTTSNC:"
// and "+UUMQTTCM:".
static void UUMQTT_urc(uAtClientHandle_t atHandle,
                       void *pParameter)
{
    uCellPrivateInstance_t *pInstance = (uCellPrivateInstance_t *) pParameter;
    volatile uCellMqttContext_t *pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
    char bytes[3];

    if (pContext != NULL) {
        // Sort out if this is "+UUMQTTC:"/"+UUMQTTSNC:"
        // or "+UUMQTTx:" or [SARA-R4 only] "+UUMQTTCM:"
        if (uAtClientReadBytes(atHandle, bytes, sizeof(bytes), true) == sizeof(bytes)) {
            if (U_CELL_PRIVATE_MODULE_IS_SARA_R4(pInstance->pModule->moduleType)) {
                if (bytes[0] == 'C') {
                    // Either "+UUMQTTC" or "+UUMQTTCM"
                    if (bytes[1] == 'M') {
                        if (pContext->pUrcMessage != NULL) {
                            UUMQTTCM_urc(atHandle, pContext);
                        }
                    } else {
                        UUMQTTC_UUMQTTSNC_urc(atHandle, pContext, pInstance);
                    }
                } else if ((bytes[0] == 'S') && (bytes[1] == 'N') && (bytes[2] == 'C')) {
                    // "+UUMQTTSNC"
                    // Clear the ": " out and then call the handler
                    uAtClientSkipBytes(atHandle, 2);
                    UUMQTTC_UUMQTTSNC_urc(atHandle, pContext, pInstance);
                } else {
                    // Probably "+UUMQTTx:"
                    // Derive x as a string, noting
                    // that it can be two digits
                    if (isdigit((int32_t) bytes[0])) {
                        if (isdigit((int32_t) bytes[1])) {
                            bytes[2] = 0;
                        } else {
                            bytes[1] = 0;
                        }
                        UUMQTTx_urc(atHandle, pContext,
                                    strtol((char *) bytes, NULL, 10));
                    }
                }
            } else {
                if (bytes[0] == 'C') {
                    // Just call the handler, bytes 1 and 2 will have read-out the ": "
                    UUMQTTC_UUMQTTSNC_urc(atHandle, pContext, pInstance);
                } else if ((bytes[0] == 'S') && (bytes[1] == 'N') && (bytes[2] == 'C')) {
                    // Clear the ": " out and then call the handler
                    uAtClientSkipBytes(atHandle, 2);
                    UUMQTTC_UUMQTTSNC_urc(atHandle, pContext, pInstance);
                }
            }
        }
    }
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: MISC
 * -------------------------------------------------------------- */

// Check all the basics and lock the mutex, MUST be called at the
// start of every API function; use the helper macro
// U_CELL_MQTT_ENTRY_FUNCTION to be sure of this, rather than calling
// this function directly.
// IMPORTANT: if mustBeInitialised is true then the returned value
// in pErrorCode will be zero if there is a valid cellular instance
// with an already initialised MQTT context.  If mustBeInitialised
// is false then the same is true except that there may NOT be an
// already initialised MQTT context, i.e. pInstance->pMqttContext
// may be NULL.  This latter case is only useful when this function
// is called from uCellMqttInit(), normally you want to call this
// function with mustBeInitialised set to true.  In all cases the
// cellular mutex will be locked.
static void entryFunction(uDeviceHandle_t cellHandle,
                          uCellPrivateInstance_t **ppInstance,
                          int32_t *pErrorCode,
                          bool mustBeInitialised)
{
    uCellPrivateInstance_t *pInstance = NULL;
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gUCellPrivateMutex != NULL) {

        uPortMutexLock(gUCellPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUCellPrivateGetInstance(cellHandle);
        if (pInstance != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_MQTT) ||
                U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_MQTTSN)) {
                errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
                if (!mustBeInitialised || (pInstance->pMqttContext != NULL)) {
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                } else {
                    // NULL pInstance in case the caller isn't checkiing
                    // pErrorCode
                    pInstance = NULL;
                }
            } else {
                // NULL pInstance in case the caller isn't checkiing
                // pErrorCode
                pInstance = NULL;
            }
        }
    }

    if (ppInstance != NULL) {
        *ppInstance = pInstance;
    }
    if (pErrorCode != NULL) {
        *pErrorCode = errorCode;
    }
}

// MUST be called at the end of every API function to unlock
// the cellular mutex; use the helper macro
// U_CELL_MQTT_EXIT_FUNCTION to be sure of this, rather than calling
// this function directly.
static void exitFunction()
{
    if (gUCellPrivateMutex != NULL) {
        uPortMutexUnlock(gUCellPrivateMutex);
    }
}

// Print the error state of MQTT.
//lint -esym(522, printErrorCodes) Suppress "lacks side effects"
// when compiled out.
static void printErrorCodes(const uCellPrivateInstance_t *pInstance)
{
#if U_CFG_ENABLE_LOGGING
    uAtClientHandle_t atHandle = pInstance->atHandle;
    volatile uCellMqttContext_t *pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
    bool mqttSn = pContext->mqttSn;
    int32_t err1;
    int32_t err2;

    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, MQTT_ERROR_AT_COMMAND_STRING(mqttSn));
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, MQTT_ERROR_AT_RESPONSE_STRING(mqttSn));
    err1 = uAtClientReadInt(atHandle);
    err2 = uAtClientReadInt(atHandle);
    uAtClientResponseStop(atHandle);
    uAtClientUnlock(atHandle);
    uPortLog("U_CELL_MQTT: error codes %d, %d.\n", err1, err2);
#else
    (void) pInstance;
#endif
}

// Process the response to an AT+UMQTT command.
static int32_t atMqttStopCmdGetRespAndUnlock(const uCellPrivateInstance_t *pInstance)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    int32_t status = 1;

    if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                           U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
        uAtClientCommandStop(atHandle);
        // Don't need to worry about the MQTT-SN form of the AT
        // command here since the old syntax SARA-R4's do not
        // support MQTT-SN
        uAtClientResponseStart(atHandle, "+UMQTT:");
        // Skip the first parameter, which is just
        // our UMQTT command number again
        uAtClientSkipParameters(atHandle, 1);
        status = uAtClientReadInt(atHandle);
        uAtClientResponseStop(atHandle);
    } else {
        uAtClientCommandStopReadResponse(atHandle);
    }
    if ((uAtClientUnlock(atHandle) == 0) && (status == 1)) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
    } else {
        printErrorCodes(pInstance);
    }

    return errorCode;
}

// Set the given pInstance->pMqttContext->urcStatus item to "not filled in".
// The switch statement here should match that in UUMQTTx_urc().
// Used by old SARA-R4-style.only.
static void resetUrcStatusField(volatile uCellMqttUrcStatus_t *pUrcStatus,
                                int32_t number)
{
    // Note: no need to macroise half the world and use
    // if/else instead of switch() here because the old-style
    // AT command SARA-R4's do not support MQTT-SN
    switch (number) {
        case 0: // Client name
            pUrcStatus->clientId.filledIn = false;
            break;
        case 1: // Local port number
            pUrcStatus->localPortNumber = -1;
            break;
        case 2: // Server name
        case 3: // Server IP address
        case 4: // User name and password
            // Nothing to do, we never read these back
            break;
        // There is no number 5
        case 6: // Will QoS value
        case 7: // Will clean value
        case 8: // Will topic value
        case 9: // The will message
            // Not currently read back via URCs
            // as MQTT "will"s are not supported
            // on SARA-R4
            break;
        case 10: // Inactivity timeout
            pUrcStatus->inactivityTimeoutSeconds = -1;
            break;
        case 11: // TLS secured
            pUrcStatus->flagsBitmap &= ~(1 << U_CELL_MQTT_URC_FLAG_SECURED);
            pUrcStatus->flagsBitmap &= ~(1 << U_CELL_MQTT_URC_FLAG_SECURED_FILLED_IN);
            pUrcStatus->securityProfileId = -1;
            break;
        case 12: // Session retained
            pUrcStatus->flagsBitmap &= ~(1 << U_CELL_MQTT_URC_FLAG_RETAINED);
            pUrcStatus->flagsBitmap &= ~(1 << U_CELL_MQTT_URC_FLAG_RETAINED_FILLED_IN);
            break;
        default:
            // Do nothing
            break;
    }
}

// Check if the given pUrcStatus item has been filled in.
// The switch statement here should match that in UUMQTTx_urc()
// Used by old SARA-R4-style.only.
//lint -esym(818, pUrcStatus) Suppress could be declared as const
static bool checkUrcStatusField(volatile uCellMqttUrcStatus_t *pUrcStatus,
                                int32_t number)
{
    volatile bool filledIn = false;

    // Note: no need to macroise half the world and use
    // if/else instead of switch() here because the old-style
    // AT command SARA-R4's do not support MQTT-SN
    switch (number) {
        case 0: // Client name
            filledIn = pUrcStatus->clientId.filledIn;
            break;
        case 1: // Local port number
            filledIn = (pUrcStatus->localPortNumber >= 0);
            break;
        case 2: // Server name
        case 3: // Server IP address
        case 4: // User name and password
            // Nothing to do, we never read these back
            break;
        // There is no number 5
        case 6: // Will QoS value
        case 7: // Will clean value
        case 8: // Will topic value
        case 9: // The will message
            // Not currently read back via URCs
            // as MQTT "will"s are not supported
            // on SARA-R4
            break;
        case 10: // Inactivity timeout
            filledIn = (pUrcStatus->inactivityTimeoutSeconds >= 0);
            break;
        case 11: // TLS secured
            filledIn = (pUrcStatus->flagsBitmap & (1 << U_CELL_MQTT_URC_FLAG_SECURED_FILLED_IN)) != 0;
            break;
        case 12: // Session retained
            filledIn = (pUrcStatus->flagsBitmap & (1 << U_CELL_MQTT_URC_FLAG_RETAINED_FILLED_IN)) != 0;
            break;
        default:
            // Do nothing
            break;
    }

    return filledIn;
}

// Make AT+UMQTT=x? read happen, old SARA-R4-style.
// Note: caller MUST lock the mutex before calling this
// function and unlock it afterwards.
static int32_t doSaraR4OldSyntaxUmqttQuery(const uCellPrivateInstance_t *pInstance,
                                           int32_t number)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
    volatile uCellMqttUrcStatus_t *pUrcStatus = &(((volatile uCellMqttContext_t *)
                                                   pInstance->pMqttContext)->urcStatus);
    uAtClientHandle_t atHandle = pInstance->atHandle;
    char buffer[13];  // Enough room for "AT+UMQTT=x?"
    int32_t status;
    int32_t startTimeMs;

    // The old SARA-R4 MQTT AT interface syntax gets very
    // peculiar here.
    // Have to send in AT+UMQTT=x? and then wait for a URC

    // Set the relevant urcStatus item to "not filled in"
    resetUrcStatusField(pUrcStatus, number);

    // Now send the AT command
    // Don't need to worry about the MQTT-SN form of the AT
    // command here since the old syntax SARA-R4's do not
    // support MQTT-SN
    snprintf(buffer, sizeof(buffer), "AT+UMQTT=%d?", (int) number);
    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, buffer);
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+UMQTT:");
    // Skip the first parameter, which is just
    // our UMQTT command number again
    uAtClientSkipParameters(atHandle, 1);
    status = uAtClientReadInt(atHandle);
    uAtClientResponseStop(atHandle);
    if ((uAtClientUnlock(atHandle) == 0) && (status == 1)) {
        // Wait for the URC to capture the answer
        // This is just a local thing so set a short timeout
        // and don't bother with keepGoingCallback
        startTimeMs = uPortGetTickTimeMs();
        while ((!checkUrcStatusField(pUrcStatus, number)) &&
               (uPortGetTickTimeMs() - startTimeMs < U_CELL_MQTT_LOCAL_URC_TIMEOUT_MS)) {
            uPortTaskBlock(250);
        }
        if (checkUrcStatusField(pUrcStatus, number)) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
    }

    return errorCode;
}

// Determine whether an MQTT operation should be retried.
static bool mqttRetry(const uCellPrivateInstance_t *pInstance, bool mqttSn)
{
    bool retry = false;
    int32_t errorCode;

    errorCode = getLastMqttErrorCode(pInstance);
    if (errorCode >= 0) {
        if (mqttSn) {
            for (size_t x = 0; (x < sizeof(gMqttSnRetryErrorCode) / sizeof(gMqttSnRetryErrorCode[0])) &&
                 !retry; x++) {
                retry = (errorCode == gMqttSnRetryErrorCode[x]);
            }
        } else {
            for (size_t x = 0; (x < sizeof(gMqttRetryErrorCode) / sizeof(gMqttRetryErrorCode[0])) &&
                 !retry; x++) {
                retry = (errorCode == gMqttRetryErrorCode[x]);
            }
        }
    }

    return retry;
}


// Determine whether MQTT TLS security is on or off.
static bool isSecured(const uCellPrivateInstance_t *pInstance,
                      int32_t *pSecurityProfileId)
{
    bool secured = false;
    volatile uCellMqttContext_t *pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
    bool mqttSn = pContext->mqttSn;
    volatile uCellMqttUrcStatus_t *pUrcStatus = &(pContext->urcStatus);
    uAtClientHandle_t atHandle;

    if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                           U_CELL_PRIVATE_FEATURE_MQTT_SECURITY)) {
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
            // Run the query, answers come back in pUrcStatus
            if (doSaraR4OldSyntaxUmqttQuery(pInstance, MQTT_PROFILE_OPCODE_SECURE(mqttSn)) == 0) {
                // SARA-R4 doesn't report the security status
                // if it is the default of unsecured,
                // so if we got nothing back we are unsecured.
                if ((pUrcStatus->flagsBitmap & (1 << U_CELL_MQTT_URC_FLAG_SECURED)) != 0) {
                    secured = true;
                    if (pSecurityProfileId != NULL) {
                        *pSecurityProfileId = pUrcStatus->securityProfileId;
                    }
                }
            }
        } else {
            atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, MQTT_PROFILE_AT_COMMAND_STRING(mqttSn));
            uAtClientWriteInt(atHandle, MQTT_PROFILE_OPCODE_SECURE(mqttSn));
            uAtClientCommandStop(atHandle);
            uAtClientResponseStart(atHandle, MQTT_PROFILE_AT_RESPONSE_STRING(mqttSn));
            // Skip the first parameter, which is just
            // our UMQTT command number again
            uAtClientSkipParameters(atHandle, 1);
            secured = uAtClientReadInt(atHandle) == 1;
            if (secured && (pSecurityProfileId != NULL)) {
                *pSecurityProfileId = uAtClientReadInt(atHandle);
            }
            uAtClientResponseStop(atHandle);
            uAtClientUnlock(atHandle);
        }
    }

    return secured;
}

// Set MQTT ping or "keep alive" on or off.
static int32_t setKeepAlive(uDeviceHandle_t cellHandle, bool onNotOff)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;
    bool mqttSn;
    uAtClientHandle_t atHandle;
    int32_t status = 1;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTT_KEEP_ALIVE)) {
            pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
            mqttSn = pContext->mqttSn;
            errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
            atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, MQTT_COMMAND_AT_COMMAND_STRING(mqttSn));
            uAtClientWriteInt(atHandle, MQTT_COMMAND_OPCODE_PING(mqttSn));
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
                // Somewhat bizzarely, for the SARA-R4 old
                // syntax, the server name has to be included
                // here (maybe it is going to ping an arbitrary
                // server?)
                uAtClientWriteString(atHandle, pContext->pBrokerNameStr, true);
                uAtClientCommandStop(atHandle);
                // Don't need to worry about the MQTT-SN form of the AT
                // command here since the old syntax SARA-R4's do not
                // support MQTT-SN
                uAtClientResponseStart(atHandle, "+UMQTTC:");
                // Skip the first parameter, which is just
                // our UMQTT command number again
                uAtClientSkipParameters(atHandle, 1);
                status = uAtClientReadInt(atHandle);
                uAtClientResponseStop(atHandle);
            } else {
                // Just set ping on or off
                uAtClientWriteInt(atHandle, (int32_t) onNotOff);
                uAtClientCommandStopReadResponse(atHandle);
            }
            if ((uAtClientUnlock(atHandle) == 0) &&
                (status == 1)) {
                // This has no URCness to it, that's it
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                pContext->keptAlive = onNotOff;
            } else {
                printErrorCodes(pInstance);
            }
        } else {
            if (!onNotOff) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            }
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

// Set MQTT session retention on or off.
static int32_t setSessionRetain(uDeviceHandle_t cellHandle, bool onNotOff)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;
    bool mqttSn;
    uAtClientHandle_t atHandle;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTT_SESSION_RETAIN)) {
            pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
            mqttSn = pContext->mqttSn;
            atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle,  MQTT_PROFILE_AT_COMMAND_STRING(mqttSn));
            // Set retention (actually it is "session cleaned",
            // hence the inversion)
            uAtClientWriteInt(atHandle, MQTT_PROFILE_OPCODE_CLEAN_SESSION(mqttSn));
            uAtClientWriteInt(atHandle, (int32_t) !onNotOff);
            errorCode = atMqttStopCmdGetRespAndUnlock(pInstance);
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

// Set security on or off.
static int32_t setSecurity(uDeviceHandle_t cellHandle, bool onNotOff,
                           int32_t securityProfileId)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;
    bool mqttSn;
    uAtClientHandle_t atHandle;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTT_SECURITY)) {
            pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
            mqttSn = pContext->mqttSn;
            atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, MQTT_PROFILE_AT_COMMAND_STRING(mqttSn));
            // Set security
            uAtClientWriteInt(atHandle, MQTT_PROFILE_OPCODE_SECURE(mqttSn));
            uAtClientWriteInt(atHandle, (int32_t) onNotOff);
            if (onNotOff && (securityProfileId >= 0)) {
                uAtClientWriteInt(atHandle, securityProfileId);
            }
            errorCode = atMqttStopCmdGetRespAndUnlock(pInstance);
        } else {
            if (!onNotOff) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            }
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

// Connect or disconnect.
static int32_t connect(const uCellPrivateInstance_t *pInstance,
                       bool onNotOff)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
    volatile uCellMqttContext_t *pContext;
    bool mqttSn;
    volatile uCellMqttUrcStatus_t *pUrcStatus;
    uAtClientHandle_t atHandle;
    int32_t startTimeMs;
    int32_t status = 1;
    size_t tryCount = 0;

    pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
    mqttSn = pContext->mqttSn;
    pUrcStatus = &(pContext->urcStatus);
    atHandle = pInstance->atHandle;
    uPortLog("U_CELL_MQTT: trying to %s...\n", onNotOff ? "connect" : "disconnect");
    if (onNotOff) {
        // The internal MQTT client in a cellular module can
        // take a little while to find out that the connection
        // has actually been made and hence we wait here for
        // it to be ready to connect
        while (uPortGetTickTimeMs() - pInstance->connectedAtMs <
               U_CELL_MQTT_CONNECT_DELAY_MILLISECONDS) {
            uPortTaskBlock(100);
        }
    }

    // Note that we retry this if the failure was due to radio conditions
    do {
        uAtClientLock(atHandle);
        pUrcStatus->flagsBitmap = 0;
        // Have seen this take a little while to respond
        uAtClientTimeoutSet(atHandle, 15000);
        uAtClientCommandStart(atHandle, MQTT_COMMAND_AT_COMMAND_STRING(mqttSn));
        // Conveniently log-in/connect is always command 0 and
        // log out/disconnect is always command 1
        uAtClientWriteInt(atHandle, (int32_t) onNotOff);
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
            uAtClientCommandStop(atHandle);
            // Don't need to worry about the MQTT-SN form of the AT
            // command here since the old syntax SARA-R4's do not
            // support MQTT-SN
            uAtClientResponseStart(atHandle, "+UMQTTC:");
            // Skip the first parameter, which is just
            // our UMQTTC command number again
            uAtClientSkipParameters(atHandle, 1);
            status = uAtClientReadInt(atHandle);
            uAtClientResponseStop(atHandle);
        } else {
            uAtClientCommandStopReadResponse(atHandle);
            // Catch errors such as +CME ERROR: operation not allowed,
            // which is issued if this command is sent before a
            // previous MQTT command was finished
            uAtClientDeviceError_t deviceError;
            uAtClientDeviceErrorGet(atHandle, &deviceError);
            status = (deviceError.type == U_AT_CLIENT_DEVICE_ERROR_TYPE_NO_ERROR);
        }

        if ((uAtClientUnlock(atHandle) == 0) && (status == 1)) {
            if (!onNotOff &&
                U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
                // For disconnections on SARA-R4 old syntax that's it
                pContext->connected = false;
                pContext->keptAlive = false;
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            } else {
                // Otherwise wait for the URC for success
                uPortLog("U_CELL_MQTT: waiting for response for up to %d"
                         " second(s)...\n",
                         U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS);
                errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
                startTimeMs = uPortGetTickTimeMs();
                while (((pUrcStatus->flagsBitmap & (1 << U_CELL_MQTT_URC_FLAG_CONNECT_UPDATED)) == 0) &&
                       (uPortGetTickTimeMs() - startTimeMs < (U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS * 1000) ) &&
                       ((pContext->pKeepGoingCallback == NULL) ||
                        pContext->pKeepGoingCallback())) {
                    uPortTaskBlock(1000);
                }
                if ((int32_t) onNotOff == pContext->connected) {
                    uPortLog("U_CELL_MQTT: %s after %d second(s).\n",
                             onNotOff ? "connected" : "disconnected",
                             (uPortGetTickTimeMs() - startTimeMs) / 1000);
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                } else {
                    printErrorCodes(pInstance);
                }
            }
        }
        tryCount++;
    } while ((errorCode != (int32_t) U_ERROR_COMMON_SUCCESS) &&
             (tryCount < pContext->numTries) && mqttRetry(pInstance, mqttSn));

    if (errorCode != (int32_t) U_ERROR_COMMON_SUCCESS) {
        printErrorCodes(pInstance);
    }

    return errorCode;
}

// Return true if the given string is allowed
// in a message for mqttSn.
static bool isAllowedMqttSn(const char *pBuffer, size_t bufferLength)
{
    bool isAllowed = false;

    if (pBuffer != NULL) {
        isAllowed = true;
        // Must be printable and not contain a quotation mark
        for (size_t x = 0; (x < bufferLength) && isAllowed; x++) {
            if (!isprint((int32_t) *pBuffer) || (*pBuffer == '\"')) {
                isAllowed = false;
            }
            pBuffer++;
        }
    }

    return isAllowed;
}

// Return true if the given string is allowed for
// SARA-R41x modules
static bool isAllowedMqttSaraR41x(const char *pBuffer, size_t bufferLength)
{
    bool isAllowed = false;
    bool inQuotes = false;

    if (pBuffer != NULL) {
        isAllowed = true;
        // Must be printable and not include a "," or a ";"
        // character within a pair of quotation marks
        // (outside quotation marks is fine)
        for (size_t x = 0; (x < bufferLength) && isAllowed; x++) {
            if (!isprint((int32_t) *pBuffer)) {
                isAllowed = false;
            } else {
                if (*pBuffer == '\"') {
                    inQuotes = !inQuotes;
                }
                if (inQuotes &&
                    ((*pBuffer == ',') || (*pBuffer == ';'))) {
                    isAllowed = false;
                }
            }
            pBuffer++;
        }
    }

    return isAllowed;
}

// For the given MQTT-SN topic name, fill in the right
// format of string for the AT interface into pTopicNameStr and
// return the correct integer to pass to the AT interface to
// specify its type.  pTopicNameStr must point to a buffer of
// length at least U_CELL_MQTT_SN_TOPIC_NAME_MAX_LENGTH_BYTES.
static int32_t mqttSnTopicNameToStr(const uCellMqttSnTopicName_t *pTopicName,
                                    char *pTopicNameStr)
{
    int32_t topicNameType = -1;

    switch (pTopicName->type) {
        case U_CELL_MQTT_SN_TOPIC_NAME_TYPE_ID_NORMAL:
        case U_CELL_MQTT_SN_TOPIC_NAME_TYPE_ID_PREDEFINED:
            snprintf(pTopicNameStr,
                     U_CELL_MQTT_SN_TOPIC_NAME_MAX_LENGTH_BYTES,
                     "%d", pTopicName->name.id);
            topicNameType = (int32_t) pTopicName->type;
            break;
        case U_CELL_MQTT_SN_TOPIC_NAME_TYPE_NAME_SHORT:
            // Must use memcpy() as nameShort does not include a terminator
            memcpy(pTopicNameStr, pTopicName->name.nameShort,
                   sizeof(pTopicName->name.nameShort));
            // Ensure a terminator
            *(pTopicNameStr + sizeof(pTopicName->name.nameShort)) = 0;
            topicNameType = (int32_t) pTopicName->type;
            break;
        default:
            break;
    }

    return topicNameType;
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: PUBLISH/SUBSCRIBE/UNSUBSCRIBE/READ
 * -------------------------------------------------------------- */

// Publish a message, MQTT or MQTT-SN style.
static int32_t publish(const uCellPrivateInstance_t *pInstance,
                       const char *pTopicNameStr,
                       int32_t topicNameType,
                       const char *pMessage,
                       size_t messageSizeBytes,
                       uCellMqttQos_t qos, bool retain)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    volatile uCellMqttContext_t *pContext;
    bool mqttSn;
    volatile uCellMqttUrcStatus_t *pUrcStatus;
    uAtClientHandle_t atHandle;
    char *pTextMessage = NULL;
    int32_t status = 1;
    bool isAscii;
    bool messageWritten = false;
    int32_t startTimeMs;
    size_t tryCount = 0;

    pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
    mqttSn = pContext->mqttSn;
    pUrcStatus = &(pContext->urcStatus);
    if (mqttSn) {
        isAscii = isAllowedMqttSn(pMessage, messageSizeBytes);
    } else {
        // This will be ignored for module types that support binary
        // publish, which eveything except SARA-R41x does
        isAscii = isAllowedMqttSaraR41x(pMessage, messageSizeBytes);
    }
    //lint -e(568) Suppress value never being negative, who knows
    // what warnings levels a customer might compile with
    if (((int32_t) qos >= 0) &&
        ((mqttSn && (qos < U_CELL_MQTT_QOS_SN_PUBLISH_MAX_NUM)) || (qos <  U_CELL_MQTT_QOS_MAX_NUM)) &&
        (pTopicNameStr != NULL) &&
        (strlen(pTopicNameStr) <= U_CELL_MQTT_WRITE_TOPIC_MAX_LENGTH_BYTES) &&
        (pMessage != NULL) &&
        ((U_CELL_PRIVATE_HAS(pInstance->pModule,
                             U_CELL_PRIVATE_FEATURE_MQTT_BINARY_PUBLISH) &&
          (messageSizeBytes <= U_CELL_MQTT_PUBLISH_BIN_MAX_LENGTH_BYTES)) ||
         (!U_CELL_PRIVATE_HAS(pInstance->pModule,
                              U_CELL_PRIVATE_FEATURE_MQTT_BINARY_PUBLISH) &&
          ((isAscii && (messageSizeBytes <= U_CELL_MQTT_PUBLISH_HEX_MAX_LENGTH_BYTES * 2)) ||
           (messageSizeBytes <= U_CELL_MQTT_PUBLISH_HEX_MAX_LENGTH_BYTES))))) {
        errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
        if (!U_CELL_PRIVATE_HAS(pInstance->pModule,
                                U_CELL_PRIVATE_FEATURE_MQTT_BINARY_PUBLISH) ||
            mqttSn) {
            // Note: the MQTT-SN AT interface never supports binary
            // publishing (even where the MQTT one does)
            // If we aren't able to publish a message as a binary
            // blob then allocate space to publish it as a string,
            // either as hex or as ASCII with a terminator added
            if (isAscii) {
                pTextMessage = (char *) pUPortMalloc(messageSizeBytes + 1);
                if (pTextMessage != NULL) {
                    // Just copy in the text and add a terminator
                    memcpy(pTextMessage, pMessage, messageSizeBytes);
                    *(pTextMessage + messageSizeBytes) = '\0';
                }
            } else {
                pTextMessage = (char *) pUPortMalloc((messageSizeBytes * 2) + 1);
                if (pTextMessage != NULL) {
                    // Convert to hex
                    uBinToHex(pMessage, messageSizeBytes, pTextMessage);
                    // Add a terminator to make it a string
                    *(pTextMessage + (messageSizeBytes * 2)) = '\0';
                }
            }
        }

        if ((pTextMessage != NULL) ||
            U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTT_BINARY_PUBLISH)) {
            errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
            atHandle = pInstance->atHandle;
            // We retry this if the failure was due to radio conditions
            do {
                uAtClientLock(atHandle);
                pUrcStatus->flagsBitmap = 0;
                if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                       U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
                    // In the old SARA-R4 syntax there's no URC
                    // for a publish, so the timeout is that
                    // of the AT command
                    uAtClientTimeoutSet(atHandle,
                                        U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS * 1000);
                }
                uAtClientCommandStart(atHandle, MQTT_COMMAND_AT_COMMAND_STRING(mqttSn));
                // Publish the message
                if (pTextMessage != NULL) {
                    // ASCII or hex mode
                    uAtClientWriteInt(atHandle, MQTT_COMMAND_OPCODE_PUBLISH_STRING(mqttSn));
                } else {
                    // Binary mode (not supported by MQTT-SN, hence we don't need a macro)
                    uAtClientWriteInt(atHandle, 9);
                }
                // QoS
                uAtClientWriteInt(atHandle, (int32_t) qos);
                // Retention
                uAtClientWriteInt(atHandle, (int32_t) retain);
                if (pTextMessage != NULL) {
                    // If we aren't doing binary mode...
                    if (isAscii) {
                        // ASCII mode
                        uAtClientWriteInt(atHandle, 0);
                    } else {
                        // Hex mode
                        uAtClientWriteInt(atHandle, 1);
                    }
                }
                if (mqttSn) {
                    // Specify the topic type for MQTT-SN
                    uAtClientWriteInt(atHandle, topicNameType);
                }
                // Topic
                uAtClientWriteString(atHandle, pTopicNameStr, true);
                if (pTextMessage == NULL) {
                    // The length of the binary message
                    uAtClientWriteInt(atHandle, (int32_t) messageSizeBytes);
                    uAtClientCommandStop(atHandle);
                    // Wait for the prompt
                    if (uAtClientWaitCharacter(atHandle, '>') == 0) {
                        // Allow plenty of time for this to complete
                        uAtClientTimeoutSet(atHandle, 10000);
                        // Wait for it...
                        uPortTaskBlock(50);
                        // Write the binary message
                        messageWritten = (uAtClientWriteBytes(atHandle,
                                                              pMessage,
                                                              messageSizeBytes,
                                                              true) == messageSizeBytes);
                    }
                } else {
                    // ASCII or hex message
                    uAtClientWriteString(atHandle, pTextMessage, true);
                    messageWritten = true;
                    uAtClientCommandStop(atHandle);
                }

                if (messageWritten) {
                    if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                           U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
                        uAtClientResponseStart(atHandle, MQTT_COMMAND_AT_RESPONSE_STRING(mqttSn));
                        // Skip the first parameter, which is just
                        // our UMQTTC command number again
                        uAtClientSkipParameters(atHandle, 1);
                        status = uAtClientReadInt(atHandle);
                    } else {
                        uAtClientResponseStart(atHandle, NULL);
                    }
                }
                // If the message wasn't written this will tidy
                // up any rubbish lying around in the AT buffer
                uAtClientResponseStop(atHandle);

                if ((uAtClientUnlock(atHandle) == 0) && (status == 1)) {
                    if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                           U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
                        // For the old SARA-R4 syntax, that's it
                        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    } else {
                        // Wait for a URC to say that the publish
                        // has succeeded
                        errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
                        startTimeMs = uPortGetTickTimeMs();
                        while (((pUrcStatus->flagsBitmap & (1 << U_CELL_MQTT_URC_FLAG_PUBLISH_UPDATED)) == 0) &&
                               (uPortGetTickTimeMs() - startTimeMs < (U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS * 1000)) &&
                               ((pContext->pKeepGoingCallback == NULL) ||
                                pContext->pKeepGoingCallback())) {
                            uPortTaskBlock(1000);
                            // When UART power saving is switched on some
                            // modules (e.g. SARA-R422) can somteimes
                            // withhold URCs so poke the module here to be
                            // sure that it has not gone to sleep on us
                            uAtClientLock(atHandle);
                            uAtClientCommandStart(atHandle, "AT");
                            uAtClientCommandStopReadResponse(atHandle);
                            uAtClientUnlock(atHandle);
                        }
                        if ((pUrcStatus->flagsBitmap & (1 << U_CELL_MQTT_URC_FLAG_PUBLISH_SUCCESS)) != 0) {
                            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                        }
                    }
                }
                tryCount++;
            } while ((errorCode != (int32_t) U_ERROR_COMMON_SUCCESS) &&
                     (tryCount < pContext->numTries) && mqttRetry(pInstance, mqttSn));

            uPortFree(pTextMessage);

            if (errorCode != (int32_t) U_ERROR_COMMON_SUCCESS) {
                printErrorCodes(pInstance);
            }
        }
    }

    return errorCode;
}

// Subscribe to an MQTT topic, MQTT or MQTT-SN style.
static int32_t subscribe(const uCellPrivateInstance_t *pInstance,
                         const char *pTopicFilterStr,
                         int32_t topicNameType,
                         uCellMqttQos_t maxQos,
                         uint16_t *pTopicId)
{
    int32_t errorCodeOrQos = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    volatile uCellMqttContext_t *pContext;
    bool mqttSn;
    volatile uCellMqttUrcStatus_t *pUrcStatus;
    uAtClientHandle_t atHandle;
    int32_t status = 1;
    int32_t startTimeMs;
    size_t tryCount = 0;

    pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
    mqttSn = pContext->mqttSn;
    pUrcStatus = &(pContext->urcStatus);
    //lint -e(568) Suppress value never being negative, who knows
    // what warnings levels a customer might compile with
    if (((int32_t) maxQos >= 0) && (maxQos < U_CELL_MQTT_QOS_MAX_NUM) &&
        (pTopicFilterStr != NULL) &&
        (strlen(pTopicFilterStr) <= U_CELL_MQTT_WRITE_TOPIC_MAX_LENGTH_BYTES)) {
        errorCodeOrQos = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
        atHandle = pInstance->atHandle;
        // We retry this if the failure was due to radio conditions
        do {
            uAtClientLock(atHandle);
            pUrcStatus->flagsBitmap = 0;
            pUrcStatus->topicNameShort[0] = 0;
            uAtClientCommandStart(atHandle, MQTT_COMMAND_AT_COMMAND_STRING(mqttSn));
            uAtClientWriteInt(atHandle, MQTT_COMMAND_OPCODE_SUBSCRIBE(mqttSn));
            // Max QoS
            uAtClientWriteInt(atHandle, (int32_t) maxQos);
            if (mqttSn) {
                if (pTopicId != NULL) {
                    // If we're retrieving a topic ID then this must be a normal
                    // MQTT topic
                    uAtClientWriteInt(atHandle, 0);
                } else {
                    // Specify the topic type given to us
                    uAtClientWriteInt(atHandle, topicNameType);
                }
            }
            // Topic
            uAtClientWriteString(atHandle, pTopicFilterStr, true);
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
                uAtClientCommandStop(atHandle);
                // Don't need to worry about the MQTT-SN form of the AT
                // command here since the old syntax SARA-R4's do not
                // support MQTT-SN
                uAtClientResponseStart(atHandle, "+UMQTTC:");
                // Skip the first parameter, which is just
                // our UMQTTC command number again
                uAtClientSkipParameters(atHandle, 1);
                status = uAtClientReadInt(atHandle);
                uAtClientResponseStop(atHandle);
            } else {
                uAtClientCommandStopReadResponse(atHandle);
                // Catch +CME ERROR etc.
                uAtClientDeviceError_t deviceError;
                uAtClientDeviceErrorGet(atHandle, &deviceError);
                status = (deviceError.type == U_AT_CLIENT_DEVICE_ERROR_TYPE_NO_ERROR);
            }

            if ((uAtClientUnlock(atHandle) == 0) && (status == 1)) {
                // On all platforms need to wait for a URC to
                // say that the subscribe has succeeded
                errorCodeOrQos = (int32_t) U_ERROR_COMMON_TIMEOUT;
                startTimeMs = uPortGetTickTimeMs();
                while (((pUrcStatus->flagsBitmap & (1 << U_CELL_MQTT_URC_FLAG_SUBSCRIBE_UPDATED)) == 0) &&
                       (uPortGetTickTimeMs() - startTimeMs < (U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS * 1000)) &&
                       ((pContext->pKeepGoingCallback == NULL) ||
                        pContext->pKeepGoingCallback())) {
                    uPortTaskBlock(1000);
                }
                if ((pUrcStatus->flagsBitmap & (1 << U_CELL_MQTT_URC_FLAG_SUBSCRIBE_SUCCESS)) != 0) {
                    errorCodeOrQos = (int32_t) pUrcStatus->subscribeQoS;
                    if (pTopicId != NULL) {
                        //lint -e{1773} Suppress attempt to cast away volatile
                        *pTopicId = (uint16_t) strtol((char *) pUrcStatus->topicNameShort, NULL, 10);
                    }
                }
            }
            tryCount++;
        } while ((errorCodeOrQos < 0) && (tryCount < pContext->numTries) &&
                 mqttRetry(pInstance, mqttSn));

        if (errorCodeOrQos < 0) {
            printErrorCodes(pInstance);
        }
    }

    return errorCodeOrQos;
}

// Unsubscribe from an MQTT topic, MQTT or MQTT-SN style.
static int32_t unsubscribe(const uCellPrivateInstance_t *pInstance,
                           const char *pTopicFilterStr,
                           int32_t topicNameType)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    volatile uCellMqttContext_t *pContext;
    bool mqttSn;
    volatile uCellMqttUrcStatus_t *pUrcStatus;
    uAtClientHandle_t atHandle;
    int32_t status = 1;
    int32_t startTimeMs;
    size_t tryCount = 0;

    pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
    mqttSn = pContext->mqttSn;
    pUrcStatus = &(pContext->urcStatus);
    if ((pTopicFilterStr != NULL) &&
        (strlen(pTopicFilterStr) <= U_CELL_MQTT_WRITE_TOPIC_MAX_LENGTH_BYTES)) {
        errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
        atHandle = pInstance->atHandle;
        // We retry this if the failure was due to radio conditions
        do {
            uAtClientLock(atHandle);
            pUrcStatus->flagsBitmap = 0;
            uAtClientCommandStart(atHandle, MQTT_COMMAND_AT_COMMAND_STRING(mqttSn));
            uAtClientWriteInt(atHandle, MQTT_COMMAND_OPCODE_UNSUBSCRIBE(mqttSn));
            if (mqttSn) {
                // Specify the topic type for MQTT-SN
                uAtClientWriteInt(atHandle, topicNameType);
            }
            // Topic
            uAtClientWriteString(atHandle, pTopicFilterStr, true);
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
                uAtClientCommandStop(atHandle);
                // Don't need to worry about the MQTT-SN form of the AT
                // command here since the old syntax SARA-R4's do not
                // support MQTT-SN
                uAtClientResponseStart(atHandle, "+UMQTTC:");
                // Skip the first parameter, which is just
                // our UMQTTC command number again
                uAtClientSkipParameters(atHandle, 1);
                status = uAtClientReadInt(atHandle);
                uAtClientResponseStop(atHandle);
            } else {
                uAtClientCommandStopReadResponse(atHandle);
                // Catch +CME ERROR etc.
                uAtClientDeviceError_t deviceError;
                uAtClientDeviceErrorGet(atHandle, &deviceError);
                status = (deviceError.type == U_AT_CLIENT_DEVICE_ERROR_TYPE_NO_ERROR);
            }

            if ((uAtClientUnlock(atHandle) == 0) && (status == 1)) {
                if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                       U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                } else {
                    // If this is the new syntax we need to wait
                    // for a URC to say that the unsubscribe has succeeded
                    errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
                    startTimeMs = uPortGetTickTimeMs();
                    while (((pUrcStatus->flagsBitmap & (1 << U_CELL_MQTT_URC_FLAG_UNSUBSCRIBE_UPDATED)) == 0) &&
                           (uPortGetTickTimeMs() - startTimeMs < (U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS * 1000)) &&
                           ((pContext->pKeepGoingCallback == NULL) ||
                            pContext->pKeepGoingCallback())) {
                        uPortTaskBlock(1000);
                    }
                    if ((pUrcStatus->flagsBitmap & (1 << U_CELL_MQTT_URC_FLAG_UNSUBSCRIBE_SUCCESS)) != 0) {
                        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    }
                }
            }
            tryCount++;
        } while ((errorCode != (int32_t) U_ERROR_COMMON_SUCCESS) &&
                 (tryCount < pContext->numTries) && mqttRetry(pInstance, mqttSn));

        if (errorCode != (int32_t) U_ERROR_COMMON_SUCCESS) {
            printErrorCodes(pInstance);
        }
    }

    return errorCode;
}

// Read a message, MQTT or MQTT-SN style.
static int32_t readMessage(const uCellPrivateInstance_t *pInstance,
                           char *pTopicNameStr,
                           size_t topicNameSizeBytes,
                           int32_t *pTopicNameType,
                           char *pMessage, size_t *pMessageSizeBytes,
                           uCellMqttQos_t *pQos)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    volatile uCellMqttContext_t *pContext;
    bool mqttSn;
    volatile uCellMqttUrcMessage_t *pUrcMessage = NULL;
    uAtClientHandle_t atHandle;
    size_t messageSizeBytes = 0;
    int32_t status;
    int32_t startTimeMs;
    uCellMqttQos_t qos;
    int32_t topicNameType = -1;
    int32_t topicNameBytesRead;
    int32_t messageBytesAvailable;
    int32_t messageBytesRead = 0;
    int32_t topicBytesAvailable;

    pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
    mqttSn = pContext->mqttSn;
    if ((pTopicNameStr != NULL) && (!mqttSn || (pTopicNameType != NULL)) &&
        ((pMessageSizeBytes != NULL) || (pMessage == NULL))) {
        pUrcMessage = pContext->pUrcMessage;
        if (pMessageSizeBytes != NULL) {
            messageSizeBytes = *pMessageSizeBytes;
        }
        errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
            U_ASSERT(pUrcMessage != NULL);
            // For the old-style SARA-R4 interface we need a URC capture
            U_ASSERT(U_CELL_PRIVATE_MODULE_IS_SARA_R4(pInstance->pModule->moduleType));
            pUrcMessage->messageRead = false;
            pUrcMessage->pTopicNameStr = pTopicNameStr;
            pUrcMessage->topicNameSizeBytes = (int32_t) topicNameSizeBytes;
            pUrcMessage->pMessage = pMessage;
            pUrcMessage->messageSizeBytes = (int32_t) messageSizeBytes;
        }
        atHandle = pInstance->atHandle;
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, MQTT_COMMAND_AT_COMMAND_STRING(mqttSn));
        uAtClientWriteInt(atHandle, MQTT_COMMAND_OPCODE_READ(mqttSn));
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
            // We get a standard indication of success here then we need
            // to wait for a URC to get the message
            uAtClientCommandStop(atHandle);
            // Don't need to worry about the MQTT-SN form of the AT
            // command here since the old syntax SARA-R4's do not
            // support MQTT-SN
            uAtClientResponseStart(atHandle, "+UMQTTC:");
            // Skip the first parameter, which is just
            // our UMQTTC command number again
            uAtClientSkipParameters(atHandle, 1);
            status = uAtClientReadInt(atHandle);
            uAtClientResponseStop(atHandle);
            if ((uAtClientUnlock(atHandle) == 0) && (status == 1)) {
                // Wait for a URC containing the message
                errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
                startTimeMs = uPortGetTickTimeMs();
                while (!pUrcMessage->messageRead &&
                       (uPortGetTickTimeMs() - startTimeMs < (U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS * 1000)) &&
                       ((pContext->pKeepGoingCallback == NULL) ||
                        pContext->pKeepGoingCallback())) {
                    uPortTaskBlock(1000);
                }
                if (pUrcMessage->messageRead) {
                    if (pContext->numUnreadMessages > 0) {
                        pContext->numUnreadMessages--;
                    }
                    if (pMessageSizeBytes != NULL) {
                        *pMessageSizeBytes = pUrcMessage->messageSizeBytes;
                    }
                    if (pQos != NULL) {
                        *pQos = pUrcMessage->qos;
                    }
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                } else {
                    printErrorCodes(pInstance);
                }
            }
        } else {
            // We want just the one message
            uAtClientWriteInt(atHandle, 1);
            uAtClientCommandStop(atHandle);
            uAtClientResponseStart(atHandle, MQTT_COMMAND_AT_RESPONSE_STRING(mqttSn));
            // The message now arrives directly
            // Skip the first parameter, which is just
            // our UMQTTC command number again
            uAtClientSkipParameters(atHandle, 1);
            // Next comes the QoS
            qos = (uCellMqttQos_t) uAtClientReadInt(atHandle);
            if (mqttSn) {
                // For MQTT-SN retrieve the topic name type
                topicNameType = uAtClientReadInt(atHandle);
            }
            // Then we can skip the length of
            // the topic and message added together
            uAtClientSkipParameters(atHandle, 1);
            // Read the topic name length
            topicBytesAvailable = uAtClientReadInt(atHandle);
            // Now read the part of the topic name string
            // we can absorb
            if ((int32_t) topicNameSizeBytes > topicBytesAvailable) {
                topicNameSizeBytes = topicBytesAvailable;
            }
            topicNameBytesRead = uAtClientReadString(atHandle,
                                                     pTopicNameStr,
                                                     topicNameSizeBytes + 1, // +1 for terminator
                                                     false);
            // Read the number of message bytes to follow
            messageBytesAvailable = uAtClientReadInt(atHandle);
            if (messageBytesAvailable > 0) {
                if ((int32_t) messageSizeBytes > messageBytesAvailable) {
                    messageSizeBytes = messageBytesAvailable;
                }
                // Now read the message bytes, being careful
                // to not look for stop tags as this can be
                // a binary message
                uAtClientIgnoreStopTag(atHandle);
                // Get the leading quote mark out of the way
                uAtClientReadBytes(atHandle, NULL, 1, true);
                // Now read out all the actual data,
                // first the bit we want
                messageBytesRead = uAtClientReadBytes(atHandle, pMessage,
                                                      messageSizeBytes, true);
                if (messageBytesAvailable > messageBytesRead) {
                    //...and then the rest poured away to NULL
                    uAtClientReadBytes(atHandle, NULL,
                                       // Cast in two stages to keep Lint happy
                                       (size_t) (unsigned) (messageBytesAvailable -
                                                            messageBytesRead), false);
                }
            }
            // Make sure to wait for the stop tag before
            // we finish
            uAtClientRestoreStopTag(atHandle);
            uAtClientResponseStop(atHandle);
            if (uAtClientUnlock(atHandle) == 0) {
                // Now have all the bits, check them
                if ((topicNameBytesRead >= 0) &&
                    //lint -e(568) Suppress value never being negative
                    ((int32_t) qos >= 0) &&
                    (qos < U_CELL_MQTT_QOS_MAX_NUM) &&
                    //lint -e(568) Suppress value never being negative
                    (!mqttSn || ((topicNameType >= 0) &&
                                 (topicNameType < (int32_t) U_CELL_MQTT_SN_TOPIC_NAME_TYPE_MAX_NUM)))) {
                    // Good.  Topic and message have
                    // already been done above,
                    // now fill in the other bits
                    if (pMessageSizeBytes != NULL) {
                        *pMessageSizeBytes = messageBytesRead;
                    }
                    if (pQos != NULL) {
                        *pQos = qos;
                    }
                    if (pTopicNameType != NULL) {
                        *pTopicNameType = topicNameType;
                    }
                    if (pContext->numUnreadMessages > 0) {
                        pContext->numUnreadMessages--;
                    }
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
            } else {
                printErrorCodes(pInstance);
            }
        }
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: MQTT AND MQTT-SN
 * -------------------------------------------------------------- */

// Initialise the cellular MQTT client.
int32_t uCellMqttInit(uDeviceHandle_t cellHandle, const char *pBrokerNameStr,
                      const char *pClientIdStr, const char *pUserNameStr,
                      const char *pPasswordStr,
                      bool (*pKeepGoingCallback)(void),
                      bool mqttSn)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;
    uAtClientHandle_t atHandle;
    uSockAddress_t address;
    char *pTmp;
    int32_t port;
    int32_t status = 1;
    bool keepGoing = true;
    char imei[U_CELL_INFO_IMEI_SIZE + 1];

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, false);

    if ((errorCode == 0) && (pInstance != NULL)) {
        // Only continue if MQTT is not already initialised for this handler
        if (pInstance->pMqttContext == NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
            // Check parameters, only pBrokerNameStr has to be present
            if (((!mqttSn && U_CELL_PRIVATE_HAS(pInstance->pModule,
                                                U_CELL_PRIVATE_FEATURE_MQTT)) ||
                 (mqttSn && U_CELL_PRIVATE_HAS(pInstance->pModule,
                                               U_CELL_PRIVATE_FEATURE_MQTTSN))) &&
                (pBrokerNameStr != NULL) &&
                (strlen(pBrokerNameStr) <=
                 U_CELL_MQTT_BROKER_ADDRESS_STRING_MAX_LENGTH_BYTES)) {
                errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                // Allocate memory for the MQTT context
                pContext = (volatile uCellMqttContext_t *) pUPortMalloc(sizeof(*pContext));
                if (pContext != NULL) {
                    pContext->pKeepGoingCallback = pKeepGoingCallback;
                    pContext->pMessageIndicationCallback = NULL;
                    pContext->pMessageIndicationCallbackParam = NULL;
                    pContext->pDisconnectCallback = NULL;
                    pContext->pDisconnectCallbackParam = NULL;
                    pContext->keptAlive = false;
                    pContext->connected = false;
                    pContext->numUnreadMessages = 0;
                    pContext->pBrokerNameStr = NULL;
                    pContext->pUrcMessage = NULL;
                    pContext->numTries = U_CELL_MQTT_RETRIES_DEFAULT + 1;
                    pContext->mqttSn = mqttSn;
                    pInstance->pMqttContext = pContext;
                    if (U_CELL_PRIVATE_MODULE_IS_SARA_R4(pInstance->pModule->moduleType)) {
                        // SARA-R4 requires a pUrcMessage as well
                        pContext->pUrcMessage = (uCellMqttUrcMessage_t *) pUPortMalloc(sizeof(*(pContext->pUrcMessage)));
                    }
                    if ((pContext->pUrcMessage != NULL) ||
                        !U_CELL_PRIVATE_MODULE_IS_SARA_R4(pInstance->pModule->moduleType)) {
                        atHandle = pInstance->atHandle;
                        // Deal with the broker name string
                        // Allocate space to fiddle with the
                        // server address, +1 for terminator
                        pContext->pBrokerNameStr = (char *) pUPortMalloc(U_CELL_MQTT_BROKER_ADDRESS_STRING_MAX_LENGTH_BYTES
                                                                         + 1);
                        if (pContext->pBrokerNameStr != NULL) {
                            errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
                            // Determine if the server name given
                            // is an IP address or a domain name
                            // by processing it as an IP address
                            memset(&address, 0, sizeof(address));
                            if (uSockStringToAddress(pBrokerNameStr,
                                                     &address) == 0) {
                                // We have an IP address
                                // Convert the bit that isn't a port
                                // number back into a string
                                if (uSockIpAddressToString(&(address.ipAddress),
                                                           pContext->pBrokerNameStr,
                                                           U_CELL_MQTT_BROKER_ADDRESS_STRING_MAX_LENGTH_BYTES) > 0) {
                                    uAtClientLock(atHandle);
                                    uAtClientCommandStart(atHandle, MQTT_PROFILE_AT_COMMAND_STRING(mqttSn));
                                    // Set the broker IP address
                                    uAtClientWriteInt(atHandle, MQTT_PROFILE_OPCODE_BROKER_IP_ADDRESS(mqttSn));
                                    uAtClientWriteString(atHandle, pContext->pBrokerNameStr, true);
                                    // If there was a port number, write
                                    // that also
                                    if (address.port > 0) {
                                        uAtClientWriteInt(atHandle, address.port);
                                    }
                                    keepGoing = (atMqttStopCmdGetRespAndUnlock(pInstance) == 0);
                                }
                            } else {
                                // We must have a domain name,
                                // make a copy of it as we need to
                                // manipulate it
                                strncpy(pContext->pBrokerNameStr, pBrokerNameStr,
                                        U_CELL_MQTT_BROKER_ADDRESS_STRING_MAX_LENGTH_BYTES + 1);
                                // Grab any port number off the end
                                // and then remove it from the string
                                port = uSockDomainGetPort(pContext->pBrokerNameStr);
                                pTmp = pUSockDomainRemovePort(pContext->pBrokerNameStr);
                                uAtClientLock(atHandle);
                                uAtClientCommandStart(atHandle, MQTT_PROFILE_AT_COMMAND_STRING(mqttSn));
                                // Set the broker URL
                                uAtClientWriteInt(atHandle, MQTT_PROFILE_OPCODE_BROKER_URL(mqttSn));
                                uAtClientWriteString(atHandle, pTmp, true);
                                // If there was a port number, write that also
                                if (port > 0) {
                                    uAtClientWriteInt(atHandle, port);
                                }
                                keepGoing = (atMqttStopCmdGetRespAndUnlock(pInstance) == 0);
                            }

                            if (!U_CELL_PRIVATE_HAS(pInstance->pModule,
                                                    U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
                                // We only need to  keep hold of the broker string
                                // if we're using the old SARA-R4 syntax (since
                                // the keep alive AT command needs it)
                                uPortFree(pContext->pBrokerNameStr);
                                pContext->pBrokerNameStr = NULL;
                            }

                            // Now deal with the credentials
                            if (!mqttSn && keepGoing && (pUserNameStr != NULL)) {
                                uAtClientLock(atHandle);
                                uAtClientCommandStart(atHandle, "AT+UMQTT=");
                                // Set credentials; not supported by MQTT-SN, hence no need for a macro
                                uAtClientWriteInt(atHandle, 4);
                                // The user name
                                uAtClientWriteString(atHandle, pUserNameStr, true);
                                // If there was a password, write that also
                                if (pPasswordStr != NULL) {
                                    uAtClientWriteString(atHandle, pPasswordStr, true);
                                }
                                keepGoing = (atMqttStopCmdGetRespAndUnlock(pInstance) == 0);
                            }

                            // Finally deal with the client ID
                            if (keepGoing) {
                                if ((pClientIdStr == NULL) &&
                                    U_CELL_PRIVATE_MODULE_IS_SARA_R4(pInstance->pModule->moduleType)) {
                                    // If there is no client ID, SARA-R4 doesn't generate
                                    // one automagically, so use the IMEI
                                    if (uCellPrivateGetImei(pInstance, imei) == 0) {
                                        // Add a null terminator to make it a string
                                        imei[sizeof(imei) - 1] = 0;
                                        pClientIdStr = imei;
                                    }
                                }
                                if (pClientIdStr != NULL) {
                                    uAtClientLock(atHandle);
                                    uAtClientCommandStart(atHandle, MQTT_PROFILE_AT_COMMAND_STRING(mqttSn));
                                    // Set client ID
                                    uAtClientWriteInt(atHandle, MQTT_PROFILE_OPCODE_CLIENT_ID(mqttSn));
                                    // The ID
                                    uAtClientWriteString(atHandle, pClientIdStr, true);
                                    keepGoing = (atMqttStopCmdGetRespAndUnlock(pInstance) == 0);
                                }
                            }

                            if (keepGoing &&
                                U_CELL_PRIVATE_HAS(pInstance->pModule,
                                                   U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
                                // If we're dealing with old SARA-R4 syntax,
                                // select verbose message reads
                                uAtClientLock(atHandle);
                                // Don't need to worry about the MQTT-SN form of the AT
                                // command here since the old syntax SARA-R4's do not
                                // support MQTT-SN
                                uAtClientCommandStart(atHandle, "AT+UMQTTC=");
                                // Message read format
                                uAtClientWriteInt(atHandle, 7);
                                // Format: verbose
                                uAtClientWriteInt(atHandle, 2);
                                uAtClientCommandStop(atHandle);
                                uAtClientResponseStart(atHandle, "+UMQTTC:");
                                // Skip the first parameter, which is just
                                // our UMQTTC command number again
                                uAtClientSkipParameters(atHandle, 1);
                                status = uAtClientReadInt(atHandle);
                                uAtClientResponseStop(atHandle);
                                keepGoing = (uAtClientUnlock(atHandle) == 0 &&
                                             (status == 1));
                            }

                            // Almost done
                            if (keepGoing) {
                                // Set up the URC
                                errorCode = uAtClientSetUrcHandler(atHandle,
                                                                   "+UUMQTT",
                                                                   UUMQTT_urc,
                                                                   pInstance);
                            } else {
                                printErrorCodes(pInstance);
                            }
                        }
                    }

                    // And we're done
                    if (errorCode != 0) {
                        // Free memory again if we failed somewhere
                        if (pInstance->pMqttContext != NULL) {
                            //lint -e(605) Suppress complaints about
                            // freeing a volatile pointer as well
                            volatile uCellMqttContext_t *pCtx = (volatile uCellMqttContext_t *)pInstance->pMqttContext;
                            uPortFree((void *)pCtx->pUrcMessage);
                        }
                        //lint -e(605) Suppress complaints about
                        // freeing this volatile pointer as well
                        uPortFree((void *)pInstance->pMqttContext);
                        pInstance->pMqttContext = NULL;
                    }
                }
            }
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

// Shut-down the cellular MQTT client.
void uCellMqttDeinit(uDeviceHandle_t cellHandle)
{
    uCellPrivateInstance_t *pInstance;
    volatile uCellMqttContext_t *pContext;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, NULL, true);

    if (pInstance != NULL) {
        pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
        if (pContext->connected) {
            (void)connect(pInstance, false);
        }

        uAtClientRemoveUrcHandler(pInstance->atHandle, "+UUMQTT");
        uPortFree(pContext->pBrokerNameStr);
        //lint -e(605) Suppress complaints about
        // freeing a volatile pointer as well
        uPortFree((void *)pContext->pUrcMessage);
        //lint -e(605) Suppress complaints about
        // freeing this volatile pointer as well
        uPortFree((void *)pContext);
        pInstance->pMqttContext = NULL;
    }

    U_CELL_MQTT_EXIT_FUNCTION();
}

// Get the current cellular MQTT client ID.
int32_t uCellMqttGetClientId(uDeviceHandle_t cellHandle, char *pClientIdStr,
                             size_t sizeBytes)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;
    bool mqttSn;
    volatile uCellMqttUrcStatus_t *pUrcStatus;
    uAtClientHandle_t atHandle;
    int32_t bytesRead;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
        mqttSn = pContext->mqttSn;
        pUrcStatus = &(pContext->urcStatus);
        if (pClientIdStr != NULL) {
            atHandle = pInstance->atHandle;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
                pUrcStatus->clientId.pContents = pClientIdStr;
                pUrcStatus->clientId.sizeBytes = sizeBytes;
                // This will fill in the string and populate
                // clientId.sizeBytes with the number of bytes read
                errorCode = doSaraR4OldSyntaxUmqttQuery(pInstance, MQTT_PROFILE_OPCODE_CLIENT_ID(mqttSn));
                if (errorCode == 0) {
                    errorCode = (int32_t) pUrcStatus->clientId.sizeBytes;
                }
            } else {
                errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, MQTT_PROFILE_AT_COMMAND_STRING(mqttSn));
                uAtClientWriteInt(atHandle, MQTT_PROFILE_OPCODE_CLIENT_ID(mqttSn));
                uAtClientCommandStop(atHandle);
                uAtClientResponseStart(atHandle,  MQTT_PROFILE_AT_RESPONSE_STRING(mqttSn));
                // Skip the first parameter, which is just
                // our UMQTT command number again
                uAtClientSkipParameters(atHandle, 1);
                bytesRead = uAtClientReadString(atHandle,
                                                pClientIdStr,
                                                sizeBytes,
                                                false);
                uAtClientResponseStop(atHandle);
                if ((uAtClientUnlock(atHandle) == 0) &&
                    (bytesRead >= 0)) {
                    errorCode = bytesRead;
                }
            }
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

// Get the local port used by the MQTT client.
int32_t uCellMqttGetLocalPort(uDeviceHandle_t cellHandle)
{
    int32_t errorCodeOrPort = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;
    volatile uCellMqttUrcStatus_t *pUrcStatus;
    uAtClientHandle_t atHandle;
    int32_t x;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCodeOrPort, true);

    if ((errorCodeOrPort == 0) && (pInstance != NULL)) {
        errorCodeOrPort = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
        pUrcStatus = &(pContext->urcStatus);
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTT_SET_LOCAL_PORT) &&
            !pContext->mqttSn) {
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
                errorCodeOrPort = doSaraR4OldSyntaxUmqttQuery(pInstance, 1);
                if ((errorCodeOrPort == 0) &&
                    (pUrcStatus->localPortNumber >= 0)) {
                    errorCodeOrPort = pUrcStatus->localPortNumber;
                }
            } else {
                errorCodeOrPort = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
                atHandle = pInstance->atHandle;
                uAtClientLock(atHandle);
                // Don't need to worry about the MQTT-SN form of the AT
                // command here since setting the local port is not
                // supported for MQTT-SN
                uAtClientCommandStart(atHandle, "AT+UMQTT=");
                // Get the local port
                uAtClientWriteInt(atHandle, 1);
                uAtClientCommandStop(atHandle);
                uAtClientResponseStart(atHandle, "+UMQTT:");
                // Skip the first parameter, which is just
                // our UMQTT command number again
                uAtClientSkipParameters(atHandle, 1);
                x = uAtClientReadInt(atHandle);
                uAtClientResponseStop(atHandle);
                if ((uAtClientUnlock(atHandle) == 0) &&
                    (x >= 0)) {
                    errorCodeOrPort = x;
                }
            }
            if ((errorCodeOrPort < 0) &&
                U_CELL_PRIVATE_MODULE_IS_SARA_R4(pInstance->pModule->moduleType)) {
                // SARA-R4 doesn't respond with a port number if the
                // port number is just the default one.
                errorCodeOrPort = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            }
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCodeOrPort;
}

// Set the inactivity timeout used by the MQTT client.
int32_t uCellMqttSetInactivityTimeout(uDeviceHandle_t cellHandle,
                                      size_t seconds)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;
    bool mqttSn;
    uAtClientHandle_t atHandle;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
        mqttSn = pContext->mqttSn;
        atHandle = pInstance->atHandle;
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, MQTT_PROFILE_AT_COMMAND_STRING(mqttSn));
        // Set the inactivity timeout
        uAtClientWriteInt(atHandle, MQTT_PROFILE_OPCODE_INACTIVITY_TIMEOUT(mqttSn));
        uAtClientWriteInt(atHandle, (int32_t) seconds);
        errorCode = atMqttStopCmdGetRespAndUnlock(pInstance);
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

// Get the inactivity timeout used by the MQTT client.
int32_t uCellMqttGetInactivityTimeout(uDeviceHandle_t cellHandle)
{
    int32_t errorCodeOrTimeout = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;
    bool mqttSn;
    volatile uCellMqttUrcStatus_t *pUrcStatus;
    uAtClientHandle_t atHandle;
    int32_t x;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCodeOrTimeout, true);

    if ((errorCodeOrTimeout == 0) && (pInstance != NULL)) {
        pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
        mqttSn = pContext->mqttSn;
        pUrcStatus = &(pContext->urcStatus);
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
            errorCodeOrTimeout = doSaraR4OldSyntaxUmqttQuery(pInstance,
                                                             MQTT_PROFILE_OPCODE_INACTIVITY_TIMEOUT(mqttSn));
            if ((errorCodeOrTimeout == 0) &&
                (pUrcStatus->inactivityTimeoutSeconds >= 0)) {
                errorCodeOrTimeout = pUrcStatus->inactivityTimeoutSeconds;
            }
        } else {
            errorCodeOrTimeout = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
            atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, MQTT_PROFILE_AT_COMMAND_STRING(mqttSn));
            // Get the inactivity timeout
            uAtClientWriteInt(atHandle, MQTT_PROFILE_OPCODE_INACTIVITY_TIMEOUT(mqttSn));
            uAtClientCommandStop(atHandle);
            uAtClientResponseStart(atHandle, MQTT_PROFILE_AT_RESPONSE_STRING(mqttSn));
            // Skip the first parameter, which is just
            // our UMQTT command number again
            uAtClientSkipParameters(atHandle, 1);
            x = uAtClientReadInt(atHandle);
            uAtClientResponseStop(atHandle);
            if ((uAtClientUnlock(atHandle) == 0) &&
                (x >= 0)) {
                errorCodeOrTimeout = x;
            }
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCodeOrTimeout;
}

// Switch MQTT ping or "keep alive" on.
int32_t uCellMqttSetKeepAliveOn(uDeviceHandle_t cellHandle)
{
    int32_t errorCode;

    // First get the inactivity timeout
    errorCode = uCellMqttGetInactivityTimeout(cellHandle);
    if (errorCode > 0) {
        // If the inactivity timeout function does not
        // return an error and does not return a timeout
        // value of zero then we can switch keep alive on
        errorCode = setKeepAlive(cellHandle, true);
    } else {
        if (errorCode == 0) {
            errorCode = (int32_t) U_CELL_ERROR_NOT_ALLOWED;
        }
    }

    return errorCode;
}

// Switch MQTT ping or "keep alive" off.
int32_t uCellMqttSetKeepAliveOff(uDeviceHandle_t cellHandle)
{
    return setKeepAlive(cellHandle, false);
}

// Determine whether MQTT ping or "keep alive" is on or off.
bool uCellMqttIsKeptAlive(uDeviceHandle_t cellHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    bool keptAlive = false;
    uCellPrivateInstance_t *pInstance = NULL;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        // There is no way to ask the module this,
        // just return what we set
        keptAlive = ((volatile uCellMqttContext_t *) pInstance->pMqttContext)->keptAlive;
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return keptAlive;
}

// Set session retention on.
int32_t uCellMqttSetRetainOn(uDeviceHandle_t cellHandle)
{
    return setSessionRetain(cellHandle, true);
}

// Switch MQTT session retention off.
int32_t uCellMqttSetRetainOff(uDeviceHandle_t cellHandle)
{
    return setSessionRetain(cellHandle, false);
}

// Determine whether MQTT session retention is on or off.
bool uCellMqttIsRetained(uDeviceHandle_t cellHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    bool isRetained = false;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;
    bool mqttSn;
    volatile uCellMqttUrcStatus_t *pUrcStatus;
    uAtClientHandle_t atHandle;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTT_SESSION_RETAIN)) {
            pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
            mqttSn = pContext->mqttSn;
            pUrcStatus = &(pContext->urcStatus);
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
                // Run the query, answers come back in pUrcStatus
                if ((doSaraR4OldSyntaxUmqttQuery(pInstance,
                                                 MQTT_PROFILE_OPCODE_CLEAN_SESSION(mqttSn)) == 0) &&
                    ((pUrcStatus->flagsBitmap & (1 << U_CELL_MQTT_URC_FLAG_RETAINED)) != 0)) {
                    isRetained = true;
                }
            } else {
                atHandle = pInstance->atHandle;
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, MQTT_PROFILE_AT_COMMAND_STRING(mqttSn));
                // Get the session retention status
                uAtClientWriteInt(atHandle, MQTT_PROFILE_OPCODE_CLEAN_SESSION(mqttSn));
                uAtClientCommandStop(atHandle);
                uAtClientResponseStart(atHandle, MQTT_PROFILE_AT_RESPONSE_STRING(mqttSn));
                // Skip the first parameter, which is just
                // our UMQTT command number again
                uAtClientSkipParameters(atHandle, 1);
                isRetained = uAtClientReadInt(atHandle) == 0;
                uAtClientResponseStop(atHandle);
                uAtClientUnlock(atHandle);
            }
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return isRetained;
}

// Switch MQTT TLS security on.
int32_t uCellMqttSetSecurityOn(uDeviceHandle_t cellHandle,
                               int32_t securityProfileId)
{
    return setSecurity(cellHandle, true, securityProfileId);
}

// Switch MQTT TLS security off.
int32_t uCellMqttSetSecurityOff(uDeviceHandle_t cellHandle)
{
    return setSecurity(cellHandle, false, 0);
}

// Determine whether MQTT TLS security is on or off.
bool uCellMqttIsSecured(uDeviceHandle_t cellHandle,
                        int32_t *pSecurityProfileId)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    bool secured = false;
    uCellPrivateInstance_t *pInstance = NULL;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        secured = isSecured(pInstance, pSecurityProfileId);
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return secured;
}

// Set the MQTT "will" message.
int32_t uCellMqttSetWill(uDeviceHandle_t cellHandle,
                         const char *pTopicNameStr,
                         const char *pMessage,
                         size_t messageSizeBytes,
                         uCellMqttQos_t qos, bool retain)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;
    bool mqttSn;
    uAtClientHandle_t atHandle;
    char *pHexMessage = NULL;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTT_WILL)) {
            errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
            pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
            mqttSn = pContext->mqttSn;
            //lint -e(568) Suppress value never being negative, who knows
            // what warnings levels a customer might compile with
            if (((int32_t) qos >= 0) &&
                (qos < U_CELL_MQTT_QOS_MAX_NUM) &&
                ((pTopicNameStr == NULL) ||
                 (strlen(pTopicNameStr) <= U_CELL_MQTT_WRITE_TOPIC_MAX_LENGTH_BYTES)) &&
                ((pMessage == NULL) ||
                 ((mqttSn && (strlen(pMessage) == messageSizeBytes) &&
                   isAllowedMqttSn(pMessage, messageSizeBytes)) ||
                  (messageSizeBytes <= U_CELL_MQTT_WILL_MESSAGE_MAX_LENGTH_BYTES)))) {
                atHandle = pInstance->atHandle;
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                if ((pMessage != NULL) && !mqttSn) {
                    // For MQTT we can do it in hex, so allocate space
                    // to encode the hex version of the message
                    errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                    pHexMessage = (char *) pUPortMalloc((messageSizeBytes * 2) + 1);
                    if (pHexMessage != NULL) {
                        // Convert to hex
                        uBinToHex(pMessage, messageSizeBytes, pHexMessage);
                        // Add a terminator to make it a string
                        *(pHexMessage + (messageSizeBytes * 2)) = '\0';
                        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    }
                }

                // The following operations must be done in
                // this order if they are to work
                if (errorCode == 0) {
                    // Write the "will" QOS
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, MQTT_PROFILE_AT_COMMAND_STRING(mqttSn));
                    // Set "will" QOS
                    uAtClientWriteInt(atHandle, MQTT_PROFILE_OPCODE_WILL_QOS(mqttSn));
                    // The "will" QOS
                    uAtClientWriteInt(atHandle, (int32_t) qos);
                    errorCode = atMqttStopCmdGetRespAndUnlock(pInstance);
                }
                if (errorCode == 0) {
                    // Write the "will" retention flag
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, MQTT_PROFILE_AT_COMMAND_STRING(mqttSn));
                    // Set "will" retention
                    uAtClientWriteInt(atHandle, MQTT_PROFILE_OPCODE_WILL_RETAIN(mqttSn));
                    // The "will" retention flag
                    uAtClientWriteInt(atHandle, (int32_t) retain);
                    errorCode = atMqttStopCmdGetRespAndUnlock(pInstance);
                }
                if ((errorCode == 0) && (pTopicNameStr != NULL)) {
                    // Write the "will" topic name string
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, MQTT_PROFILE_AT_COMMAND_STRING(mqttSn));
                    // Set "will" topic name
                    uAtClientWriteInt(atHandle, MQTT_PROFILE_OPCODE_WILL_TOPIC(mqttSn));
                    // The "will" topic name
                    uAtClientWriteString(atHandle, pTopicNameStr, true);
                    errorCode = atMqttStopCmdGetRespAndUnlock(pInstance);
                }
                if ((errorCode == 0) && (pMessage != NULL)) {
                    // Finally, and it must be finally,
                    // write the "will" message
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, MQTT_PROFILE_AT_COMMAND_STRING(mqttSn));
                    // Set "will" message
                    uAtClientWriteInt(atHandle, MQTT_PROFILE_OPCODE_WILL_MESSAGE(mqttSn));
                    // Write the "will" message
                    if (pHexMessage != NULL) {
                        uAtClientWriteString(atHandle, pHexMessage, true);
                        // Hex mode
                        uAtClientWriteInt(atHandle, 1);
                    } else {
                        uAtClientWriteString(atHandle, pMessage, true);
                    }
                    errorCode = atMqttStopCmdGetRespAndUnlock(pInstance);
                }
                // Free memory
                uPortFree(pHexMessage);
            }
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

// Get the MQTT "will" message.
// Note: if SARA-R4 ever supports this the SARA-R4
// return-things-via-URC pattern will probably
// need to be added here.
int32_t uCellMqttGetWill(uDeviceHandle_t cellHandle, char *pTopicNameStr,
                         size_t topicNameSizeBytes,
                         char *pMessage,
                         size_t *pMessageSizeBytes,
                         uCellMqttQos_t *pQos, bool *pRetain)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;
    bool mqttSn;
    uAtClientHandle_t atHandle;
    char *pBuffer;
    int32_t bytesRead = 0;
    int32_t messageBytesAvailable;
    int32_t x;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTT_WILL)) {
            errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
            pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
            mqttSn = pContext->mqttSn;
            if ((pMessage == NULL) || (pMessageSizeBytes != NULL)) {
                atHandle = pInstance->atHandle;
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                if (pTopicNameStr != NULL) {
                    // Create a buffer to store the "will" topic name
                    // in, since it may be larger than the user has
                    // asked for and we have to read in the lot
                    errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                    pBuffer = (char *) pUPortMalloc(U_CELL_MQTT_READ_TOPIC_MAX_LENGTH_BYTES + 1);
                    if (pBuffer != NULL) {
                        // Get the "will" topic name string
                        uAtClientLock(atHandle);
                        uAtClientCommandStart(atHandle, MQTT_PROFILE_AT_COMMAND_STRING(mqttSn));
                        // "will" topic name
                        uAtClientWriteInt(atHandle, MQTT_PROFILE_OPCODE_WILL_TOPIC(mqttSn));
                        uAtClientCommandStop(atHandle);
                        uAtClientResponseStart(atHandle, MQTT_PROFILE_AT_RESPONSE_STRING(mqttSn));
                        // Skip the first parameter, which is just
                        // our UMQTT command number again
                        uAtClientSkipParameters(atHandle, 1);
                        // Read the "will" topic name, which is good-ole ASCII
                        bytesRead = uAtClientReadString(atHandle, pBuffer,
                                                        U_CELL_MQTT_READ_TOPIC_MAX_LENGTH_BYTES + 1,
                                                        false);
                        uAtClientResponseStop(atHandle);
                        errorCode = uAtClientUnlock(atHandle);
                        if ((errorCode == 0) && (bytesRead >= 0)) {
                            if ((topicNameSizeBytes > 0) && (bytesRead > (int32_t) topicNameSizeBytes - 1)) {
                                bytesRead = (int32_t) topicNameSizeBytes - 1;
                            }
                            if (topicNameSizeBytes > 0) {
                                // Copy the answer out
                                strncpy(pTopicNameStr, pBuffer, topicNameSizeBytes);
                            }
                        }
                        // Free memory.
                        uPortFree(pBuffer);
                    }
                }
                if ((errorCode == 0) && (pMessage != NULL)) {
                    errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
                    // Get the "will" message string
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, MQTT_PROFILE_AT_COMMAND_STRING(mqttSn));
                    // "will" message
                    uAtClientWriteInt(atHandle, MQTT_PROFILE_OPCODE_WILL_MESSAGE(mqttSn));
                    uAtClientCommandStop(atHandle);
                    uAtClientResponseStart(atHandle, MQTT_PROFILE_AT_RESPONSE_STRING(mqttSn));
                    // Skip the first parameter, which is just
                    // our UMQTT command number again
                    uAtClientSkipParameters(atHandle, 1);
                    // Read the number of message bytes to follow
                    messageBytesAvailable = uAtClientReadInt(atHandle);
                    if (messageBytesAvailable > U_CELL_MQTT_WILL_MESSAGE_MAX_LENGTH_BYTES) {
                        messageBytesAvailable = U_CELL_MQTT_WILL_MESSAGE_MAX_LENGTH_BYTES;
                    }
                    if (messageBytesAvailable > 0) {
                        // Now read the message bytes, being careful
                        // to not look for stop tags as this can be
                        // a binary message
                        uAtClientIgnoreStopTag(atHandle);
                        // Get the leading quote mark out of the way
                        uAtClientReadBytes(atHandle, NULL, 1, true);
                        // Now read out all the actual data,
                        // first the bit we want
                        bytesRead = uAtClientReadBytes(atHandle, pMessage,
                                                       *pMessageSizeBytes, true);
                        if (messageBytesAvailable > (int32_t) *pMessageSizeBytes) {
                            //...and then the rest poured away to NULL
                            uAtClientReadBytes(atHandle, NULL,
                                               messageBytesAvailable -
                                               *pMessageSizeBytes, true);
                        }
                    }
                    // Make sure to wait for the top tag before
                    // we finish
                    uAtClientRestoreStopTag(atHandle);
                    uAtClientResponseStop(atHandle);
                    if ((uAtClientUnlock(atHandle) == 0) && (bytesRead > 0)) {
                        // -1 to remove the length of the closing quote mark
                        *pMessageSizeBytes = bytesRead - 1;
                        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    }
                }
                if ((errorCode == 0) && (pQos != NULL)) {
                    errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
                    // Get the "will" QoS
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, MQTT_PROFILE_AT_COMMAND_STRING(mqttSn));
                    // "will" QoS
                    uAtClientWriteInt(atHandle, MQTT_PROFILE_OPCODE_WILL_QOS(mqttSn));
                    uAtClientCommandStop(atHandle);
                    uAtClientResponseStart(atHandle, MQTT_PROFILE_AT_RESPONSE_STRING(mqttSn));
                    // Skip the first parameter, which is just
                    // our UMQTT command number again
                    uAtClientSkipParameters(atHandle, 1);
                    // Read the "will" QoS
                    x = uAtClientReadInt(atHandle);
                    uAtClientResponseStop(atHandle);
                    if ((uAtClientUnlock(atHandle) == 0) && (x >= 0)) {
                        *pQos = (uCellMqttQos_t) x;
                        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    }
                }
                if ((errorCode == 0) && (pRetain != NULL)) {
                    errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
                    // Get the "will" retention flag
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, MQTT_PROFILE_AT_COMMAND_STRING(mqttSn));
                    // "will" retention
                    uAtClientWriteInt(atHandle, MQTT_PROFILE_OPCODE_WILL_RETAIN(mqttSn));
                    uAtClientCommandStop(atHandle);
                    uAtClientResponseStart(atHandle, MQTT_PROFILE_AT_RESPONSE_STRING(mqttSn));
                    // Skip the first parameter, which is just
                    // our UMQTT command number again
                    uAtClientSkipParameters(atHandle, 1);
                    // Read the "will" retention flag
                    x = uAtClientReadInt(atHandle);
                    uAtClientResponseStop(atHandle);
                    if ((uAtClientUnlock(atHandle) == 0) && (x >= 0)) {
                        *pRetain = (bool) x;
                        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    }
                }
            }
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

// Start an MQTT session.
int32_t uCellMqttConnect(uDeviceHandle_t cellHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    // Deliberately don't check if we're connected
    // already: want to tickle it, have an effect,
    // just in case we're locally out of sync
    // with the MQTT stack in the module.

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        errorCode = connect(pInstance, true);
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

// Stop an MQTT session.
int32_t uCellMqttDisconnect(uDeviceHandle_t cellHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, NULL, true);

    if (pInstance != NULL) {
        errorCode = connect(pInstance, false);
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

// Determine whether an MQTT session is active or not.
bool uCellMqttIsConnected(uDeviceHandle_t cellHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    bool connected = false;
    uCellPrivateInstance_t *pInstance = NULL;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        // There is no way to ask the module this,
        // just return our last status
        connected = ((volatile uCellMqttContext_t *) pInstance->pMqttContext)->connected;
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return connected;
}

// Set a callback to be called when new messages arrive.
int32_t uCellMqttSetMessageCallback(uDeviceHandle_t cellHandle,
                                    void (*pCallback) (int32_t, void *),
                                    void *pCallbackParam)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        ((volatile uCellMqttContext_t *) pInstance->pMqttContext)->pMessageIndicationCallback = pCallback;
        ((volatile uCellMqttContext_t *) pInstance->pMqttContext)->pMessageIndicationCallbackParam =
            pCallbackParam;
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

// Get the number of unread messages.
int32_t uCellMqttGetUnread(uDeviceHandle_t cellHandle)
{
    int32_t errorCodeOrUnread = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCodeOrUnread, true);

    if ((errorCodeOrUnread == 0) && (pInstance != NULL)) {
        errorCodeOrUnread = (int32_t) ((volatile uCellMqttContext_t *)
                                       pInstance->pMqttContext)->numUnreadMessages;
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCodeOrUnread;
}

// Get the last MQTT error code.
int32_t uCellMqttGetLastErrorCode(uDeviceHandle_t cellHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        errorCode = getLastMqttErrorCode(pInstance);
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

// Set a callback for when the MQTT connection is dropped.
int32_t uCellMqttSetDisconnectCallback(uDeviceHandle_t cellHandle,
                                       void (*pCallback) (int32_t, void *),
                                       void *pCallbackParam)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        ((volatile uCellMqttContext_t *) pInstance->pMqttContext)->pDisconnectCallback = pCallback;
        ((volatile uCellMqttContext_t *) pInstance->pMqttContext)->pDisconnectCallbackParam =
            pCallbackParam;
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

// Set the number of retries on radio-related failure.
void uCellMqttSetRetries(uDeviceHandle_t cellHandle, size_t numRetries)
{
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, NULL, true);

    if (pInstance != NULL) {
        pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
        pContext->numTries = numRetries + 1;
    }

    U_CELL_MQTT_EXIT_FUNCTION();
}

// Get the number of retries on radio-related failure.
int32_t uCellMqttGetRetries(uDeviceHandle_t cellHandle)
{
    int32_t errorCodeOrRetries = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCodeOrRetries, true);

    if ((errorCodeOrRetries == 0) && (pInstance != NULL)) {
        pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
        errorCodeOrRetries = ((int32_t) pContext->numTries) - 1;
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCodeOrRetries;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: MQTT ONLY
 * -------------------------------------------------------------- */

// Determine if MQTT is supported by the given cellHandle.
bool uCellMqttIsSupported(uDeviceHandle_t cellHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, false);
    U_CELL_MQTT_EXIT_FUNCTION();

    return (pInstance != NULL ? U_CELL_PRIVATE_HAS(pInstance->pModule,
                                                   U_CELL_PRIVATE_FEATURE_MQTT) : false);
}

// Set the local port to use for the MQTT client.
int32_t uCellMqttSetLocalPort(uDeviceHandle_t cellHandle, uint16_t port)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;
    uAtClientHandle_t atHandle;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
        if (!pContext->mqttSn &&
            U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTT_SET_LOCAL_PORT)) {
            atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
            // Don't need to worry about the MQTT-SN form of the AT
            // command here since the setting the local port is not
            // supported for MQTT-SN
            uAtClientCommandStart(atHandle, "AT+UMQTT=");
            // Set the local port
            uAtClientWriteInt(atHandle, 1);
            uAtClientWriteInt(atHandle, port);
            errorCode = atMqttStopCmdGetRespAndUnlock(pInstance);
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

// Publish an MQTT message.
int32_t uCellMqttPublish(uDeviceHandle_t cellHandle,
                         const char *pTopicNameStr,
                         const char *pMessage,
                         size_t messageSizeBytes,
                         uCellMqttQos_t qos, bool retain)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTT) &&
            !pContext->mqttSn) {
            errorCode = publish(pInstance, pTopicNameStr, -1,
                                pMessage, messageSizeBytes, qos, retain);
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

// Subscribe to an MQTT topic.
int32_t uCellMqttSubscribe(uDeviceHandle_t cellHandle,
                           const char *pTopicFilterStr,
                           uCellMqttQos_t maxQos)
{
    int32_t errorCodeOrQos = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCodeOrQos, true);

    if ((errorCodeOrQos == 0) && (pInstance != NULL)) {
        errorCodeOrQos = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTT) &&
            !pContext->mqttSn) {
            errorCodeOrQos = subscribe(pInstance, pTopicFilterStr, -1,
                                       maxQos, NULL);
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCodeOrQos;
}

// Unsubscribe from an MQTT topic.
int32_t uCellMqttUnsubscribe(uDeviceHandle_t cellHandle,
                             const char *pTopicFilterStr)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTT) &&
            !pContext->mqttSn) {
            errorCode = unsubscribe(pInstance, pTopicFilterStr, -1);
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

// Read an MQTT message.
int32_t uCellMqttMessageRead(uDeviceHandle_t cellHandle,
                             char *pTopicNameStr,
                             size_t topicNameSizeBytes,
                             char *pMessage, size_t *pMessageSizeBytes,
                             uCellMqttQos_t *pQos)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTT) &&
            !pContext->mqttSn) {
            errorCode = readMessage(pInstance, pTopicNameStr,
                                    topicNameSizeBytes, NULL,
                                    pMessage, pMessageSizeBytes, pQos);
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: MQTT-SN ONLY
 * -------------------------------------------------------------- */

// Determine if MQTT-SN is supported by the given cellHandle.
bool uCellMqttSnIsSupported(uDeviceHandle_t cellHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, false);
    U_CELL_MQTT_EXIT_FUNCTION();

    return (pInstance != NULL ? U_CELL_PRIVATE_HAS(pInstance->pModule,
                                                   U_CELL_PRIVATE_FEATURE_MQTTSN) : false);
}

// Ask the MQTT-SN broker for a topic ID for a normal MQTT topic.
int32_t uCellMqttSnRegisterNormalTopic(uDeviceHandle_t cellHandle,
                                       const char *pTopicNameStr,
                                       uCellMqttSnTopicName_t *pTopicName)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;
    volatile uCellMqttUrcStatus_t *pUrcStatus;
    uAtClientHandle_t atHandle;
    int32_t startTimeMs;
    size_t tryCount = 0;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTTSN) &&
            pContext->mqttSn) {
            errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
            pUrcStatus = &(pContext->urcStatus);
            if ((pTopicNameStr != NULL) && (pTopicName != NULL)) {
                errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
                atHandle = pInstance->atHandle;
                // We retry this if the failure was due to radio conditions
                do {
                    uAtClientLock(atHandle);
                    pUrcStatus->flagsBitmap = 0;
                    // Don't need to worry about the MQTT form of the AT
                    // command here since this is MQTT-SN only
                    uAtClientCommandStart(atHandle, "AT+UMQTTSNC=");
                    // Register a topic
                    uAtClientWriteInt(atHandle, 2);
                    // The topic
                    uAtClientWriteString(atHandle, pTopicNameStr, true);
                    uAtClientCommandStopReadResponse(atHandle);
                    if (uAtClientUnlock(atHandle) == 0) {
                        // Wait for a URC to get the ID
                        errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
                        startTimeMs = uPortGetTickTimeMs();
                        while (((pUrcStatus->flagsBitmap & (1 << U_CELL_MQTT_URC_FLAG_REGISTER_UPDATED)) == 0) &&
                               (uPortGetTickTimeMs() - startTimeMs < (U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS * 1000)) &&
                               ((pContext->pKeepGoingCallback == NULL) ||
                                pContext->pKeepGoingCallback())) {
                            uPortTaskBlock(1000);
                        }
                        if ((pUrcStatus->flagsBitmap & (1 << U_CELL_MQTT_URC_FLAG_REGISTER_SUCCESS)) != 0) {
                            pTopicName->name.id = (uint16_t) pUrcStatus->topicId;
                            pTopicName->type = U_CELL_MQTT_SN_TOPIC_NAME_TYPE_ID_NORMAL;
                            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                        }
                    }
                    tryCount++;
                } while ((errorCode != (int32_t) U_ERROR_COMMON_SUCCESS) &&
                         (tryCount < pContext->numTries) && mqttRetry(pInstance, true));

                if (errorCode != (int32_t) U_ERROR_COMMON_SUCCESS) {
                    printErrorCodes(pInstance);
                }
            }
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

// Publish a message.
int32_t uCellMqttSnPublish(uDeviceHandle_t cellHandle,
                           const uCellMqttSnTopicName_t *pTopicName,
                           const char *pMessage,
                           size_t messageSizeBytes,
                           uCellMqttQos_t qos, bool retain)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;
    char topicNameStr[U_CELL_MQTT_SN_TOPIC_NAME_MAX_LENGTH_BYTES];
    int32_t topicNameType;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTTSN) &&
            pContext->mqttSn) {
            errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
            topicNameType = mqttSnTopicNameToStr(pTopicName, topicNameStr);
            if (topicNameType >= 0) {
                errorCode = publish(pInstance, topicNameStr,
                                    topicNameType, pMessage,
                                    messageSizeBytes, qos, retain);
            }
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

// Subscribe to an MQTT-SN topic.
int32_t uCellMqttSnSubscribe(uDeviceHandle_t cellHandle,
                             const uCellMqttSnTopicName_t *pTopicName,
                             uCellMqttQos_t maxQos)
{
    int32_t errorCodeOrQos = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;
    char topicNameStr[U_CELL_MQTT_SN_TOPIC_NAME_MAX_LENGTH_BYTES];
    int32_t topicNameType;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCodeOrQos, true);

    if ((errorCodeOrQos == 0) && (pInstance != NULL)) {
        errorCodeOrQos = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTTSN) &&
            pContext->mqttSn) {
            errorCodeOrQos = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
            topicNameType = mqttSnTopicNameToStr(pTopicName, topicNameStr);
            if (topicNameType >= 0) {
                errorCodeOrQos = subscribe(pInstance, topicNameStr,
                                           topicNameType, maxQos, NULL);
            }
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCodeOrQos;
}

// Subscribe to a normal MQTT topic.
int32_t uCellMqttSnSubscribeNormalTopic(uDeviceHandle_t cellHandle,
                                        const char *pTopicFilterStr,
                                        uCellMqttQos_t maxQos,
                                        uCellMqttSnTopicName_t *pTopicName)
{
    int32_t errorCodeOrQos = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCodeOrQos, true);

    if ((errorCodeOrQos == 0) && (pInstance != NULL)) {
        errorCodeOrQos = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTTSN) &&
            pContext->mqttSn) {
            errorCodeOrQos = subscribe(pInstance, pTopicFilterStr, -1,
                                       maxQos, &(pTopicName->name.id));
            if (errorCodeOrQos >= 0) {
                pTopicName->type = U_CELL_MQTT_SN_TOPIC_NAME_TYPE_ID_NORMAL;
            }
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCodeOrQos;
}

// Unsubscribe from an MQTT-SN topic.
int32_t uCellMqttSnUnsubscribe(uDeviceHandle_t cellHandle,
                               const uCellMqttSnTopicName_t *pTopicName)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;
    char topicNameStr[U_CELL_MQTT_SN_TOPIC_NAME_MAX_LENGTH_BYTES];
    int32_t topicNameType;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTTSN) &&
            pContext->mqttSn) {
            errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
            topicNameType = mqttSnTopicNameToStr(pTopicName, topicNameStr);
            if (topicNameType >= 0) {
                errorCode = unsubscribe(pInstance, topicNameStr, topicNameType);
            }
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

// Unsubscribe from a normal MQTT topic.
int32_t uCellMqttSnUnsubscribeNormalTopic(uDeviceHandle_t cellHandle,
                                          const char *pTopicFilterStr)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTTSN) &&
            pContext->mqttSn) {
            // Note: even though it's not an ID, the MQTT SN topic type
            // is still "normal" for this case
            errorCode = unsubscribe(pInstance, pTopicFilterStr,
                                    (int32_t) U_CELL_MQTT_SN_TOPIC_NAME_TYPE_ID_NORMAL);
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

// Read a message.
int32_t uCellMqttSnMessageRead(uDeviceHandle_t cellHandle,
                               uCellMqttSnTopicName_t *pTopicName,
                               char *pMessage, size_t *pMessageSizeBytes,
                               uCellMqttQos_t *pQos)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;
    char topicNameStr[U_CELL_MQTT_SN_TOPIC_NAME_MAX_LENGTH_BYTES];
    int32_t topicNameType = U_CELL_MQTT_SN_TOPIC_NAME_TYPE_ID_NORMAL;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTTSN) &&
            pContext->mqttSn) {
            errorCode = readMessage(pInstance, topicNameStr, sizeof(topicNameStr),
                                    &topicNameType, pMessage, pMessageSizeBytes,
                                    pQos);
            if (errorCode == 0) {
                pTopicName->name.id = (uint16_t) strtol(topicNameStr, NULL, 10);
                pTopicName->type = (uCellMqttSnTopicNameType_t) topicNameType;
            }
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

// Update an existing MQTT "will" message.
int32_t uCellMqttSnSetWillMessaage(uDeviceHandle_t cellHandle,
                                   const char *pMessage,
                                   size_t messageSizeBytes)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;
    volatile uCellMqttUrcStatus_t *pUrcStatus;
    uAtClientHandle_t atHandle;
    int32_t startTimeMs;
    size_t tryCount = 0;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTTSN) &&
            pContext->mqttSn) {
            errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
            if (messageSizeBytes == strlen(pMessage) &&
                isAllowedMqttSn(pMessage, messageSizeBytes)) {
                errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
                pUrcStatus = &(pContext->urcStatus);
                atHandle = pInstance->atHandle;
                // We retry this if the failure was due to radio conditions
                do {
                    uAtClientLock(atHandle);
                    pUrcStatus->flagsBitmap = 0;
                    // Don't need to worry about the MQTT form of the AT
                    // command here since this is MQTT-SN only
                    uAtClientCommandStart(atHandle, "AT+UMQTTSNC=");
                    // "will" message update
                    uAtClientWriteInt(atHandle, 8);
                    // The new "will" message
                    uAtClientWriteString(atHandle, pMessage, true);
                    uAtClientCommandStopReadResponse(atHandle);
                    if (uAtClientUnlock(atHandle) == 0) {
                        // Wait for a URC to indicate success
                        errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
                        startTimeMs = uPortGetTickTimeMs();
                        while (((pUrcStatus->flagsBitmap & (1 << U_CELL_MQTT_URC_FLAG_WILL_MESSAGE_UPDATED)) == 0) &&
                               (uPortGetTickTimeMs() - startTimeMs < (U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS * 1000)) &&
                               ((pContext->pKeepGoingCallback == NULL) ||
                                pContext->pKeepGoingCallback())) {
                            uPortTaskBlock(1000);
                        }
                        if ((pUrcStatus->flagsBitmap & (1 << U_CELL_MQTT_URC_FLAG_WILL_MESSAGE_SUCCESS)) != 0) {
                            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                        }
                    }
                } while ((errorCode != (int32_t) U_ERROR_COMMON_SUCCESS) &&
                         (tryCount < pContext->numTries) && mqttRetry(pInstance, true));

                if (errorCode != (int32_t) U_ERROR_COMMON_SUCCESS) {
                    printErrorCodes(pInstance);
                }
            }
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

// Update the parameters for an existing MQTT "will".
int32_t uCellMqttSnSetWillParameters(uDeviceHandle_t cellHandle,
                                     const char *pTopicNameStr,
                                     uCellMqttQos_t qos, bool retain)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;
    volatile uCellMqttUrcStatus_t *pUrcStatus;
    uAtClientHandle_t atHandle;
    int32_t startTimeMs;
    size_t tryCount = 0;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTTSN) &&
            pContext->mqttSn) {
            errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
            //lint -e(568) Suppress value never being negative, who knows
            // what warnings levels a customer might compile with
            if (((int32_t) qos >= 0) && (qos < U_CELL_MQTT_QOS_MAX_NUM) &&
                (pTopicNameStr != NULL) &&
                (strlen(pTopicNameStr) <= U_CELL_MQTT_WRITE_TOPIC_MAX_LENGTH_BYTES)) {
                errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
                pUrcStatus = &(pContext->urcStatus);
                atHandle = pInstance->atHandle;
                // We retry this if the failure was due to radio conditions
                do {
                    uAtClientLock(atHandle);
                    pUrcStatus->flagsBitmap = 0;
                    // Don't need to worry about the MQTT form of the AT
                    // command here since this is MQTT-SN only
                    uAtClientCommandStart(atHandle, "AT+UMQTTSNC=");
                    // "will" parameters update
                    uAtClientWriteInt(atHandle, 7);
                    // The QoS
                    uAtClientWriteInt(atHandle, (int32_t) qos);
                    // Retention
                    uAtClientWriteInt(atHandle, (int32_t) retain);
                    // The topic string
                    uAtClientWriteString(atHandle, pTopicNameStr, true);
                    uAtClientCommandStopReadResponse(atHandle);
                    if (uAtClientUnlock(atHandle) == 0) {
                        // Wait for a URC to indicate success
                        errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
                        startTimeMs = uPortGetTickTimeMs();
                        while (((pUrcStatus->flagsBitmap & (1 << U_CELL_MQTT_URC_FLAG_WILL_PARAMETERS_UPDATED)) == 0) &&
                               (uPortGetTickTimeMs() - startTimeMs < (U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS * 1000)) &&
                               ((pContext->pKeepGoingCallback == NULL) ||
                                pContext->pKeepGoingCallback())) {
                            uPortTaskBlock(1000);
                        }
                        if ((pUrcStatus->flagsBitmap & (1 << U_CELL_MQTT_URC_FLAG_WILL_PARAMETERS_SUCCESS)) != 0) {
                            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                        }
                    }
                } while ((errorCode != (int32_t) U_ERROR_COMMON_SUCCESS) &&
                         (tryCount < pContext->numTries) && mqttRetry(pInstance, true));

                if (errorCode != (int32_t) U_ERROR_COMMON_SUCCESS) {
                    printErrorCodes(pInstance);
                }
            }
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

// End of file
