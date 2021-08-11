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
 * @brief Implementation of the u-blox MQTT client API for cellular.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // malloc(), free()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "ctype.h"     // isdigit(), isprint()
#include "string.h"    // memset(), strcpy(), strtok_r(), strtol()
#include "stdio.h"     // snprintf()
#include "assert.h"

#include "u_cfg_sw.h"

#include "u_error_common.h"

#include "u_port_clib_platform_specific.h" /* strtok_r and integer stdio, must
                                              be included before the other port
                                              files if any print or scan function
                                              is used. */
#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"

#include "u_hex_bin_convert.h"

#include "u_at_client.h"

#include "u_mqtt_common.h"
#include "u_mqtt_client.h"

#include "u_sock.h"

#include "u_cell_module_type.h"
#include "u_cell_net.h"     // Order is important
#include "u_cell_private.h" // here don't change it
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
#define U_CELL_MQTT_URC_FLAG_CONNECT_UPDATED         0
#define U_CELL_MQTT_URC_FLAG_PUBLISH_UPDATED         1
#define U_CELL_MQTT_URC_FLAG_PUBLISH_SUCCESS         2
#define U_CELL_MQTT_URC_FLAG_SUBSCRIBE_UPDATED       3
#define U_CELL_MQTT_URC_FLAG_SUBSCRIBE_SUCCESS       4
#define U_CELL_MQTT_URC_FLAG_UNSUBSCRIBE_UPDATED     5
#define U_CELL_MQTT_URC_FLAG_UNSUBSCRIBE_SUCCESS     6
#define U_CELL_MQTT_URC_FLAG_UNREAD_MESSAGES_UPDATED 7
#define U_CELL_MQTT_URC_FLAG_SECURED                 8  // Only required for SARA-R4
#define U_CELL_MQTT_URC_FLAG_RETAINED                9  // Only required for SARA-R4

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
    int32_t flagsBitmap;
    uCellMqttQos_t subscribeQoS;
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
    bool (*pKeepGoingCallback)(void); /**< Callback to be called while
                                           in a function which may have
                                           to wait for a broker's response.*/
    void (*pMessageIndicationCallback) (int32_t, void *); /**< Callback to
                                                               be called when
                                                               an indication
                                                               of messages
                                                               waiting to be
                                                               read has been
                                                               received. */
    void *pMessageIndicationCallbackParam; /**< User parameter to be
                                                passed to the message
                                                indication callback. */
    bool keptAlive;  /**< Keep track of whether "keep alive" is on or not. */
    bool connected;  /**< Keep track of whether we are connected or not. */
    size_t numUnreadMessages; /**< Keep track of the number of unread messages. */
    volatile uCellMqttUrcStatus_t urcStatus; /**< Store the status values from a URC. */
    volatile uCellMqttUrcMessage_t *pUrcMessage; /**< Storage for an MQTT message
                                                      received in a URC, only
                                                      required for SARA-R4. */
} uCellMqttContext_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: URCS AND RELATED FUNCTIONS
 * -------------------------------------------------------------- */

// A local "trampoline" for the message indication callback,
// here so so that it can call pMessageIndicationCallback
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

// "+UUMQTTC:" URC handler, called by the UUMQTT_urc()
// URC handler..
static void UUMQTTC_urc(uAtClientHandle_t atHandle,
                        volatile uCellMqttContext_t *pContext,
                        const uCellPrivateInstance_t *pInstance)
{
    volatile uCellMqttUrcStatus_t *pUrcStatus = &(pContext->urcStatus);
    int32_t urcType;
    int32_t urcParam1;
    int32_t urcParam2;

    urcType = uAtClientReadInt(atHandle);
    // All of the MQTTC URC types have at least one parameter
    urcParam1 = uAtClientReadInt(atHandle);
    switch (urcType) {
        case 0: // Logout, 1 means success
            if ((urcParam1 == 1) ||
                (urcParam1 == 100) || // SARA-R5, inactivity
                (urcParam1 == 101)) { // SARA-R5, connection lost
                // Disconnected
                pContext->connected = false;
            }
            pUrcStatus->flagsBitmap |= 1 << U_CELL_MQTT_URC_FLAG_CONNECT_UPDATED;
            break;
        case 1: // Login
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
            break;
        case 2: // Publish hex, 1 means success
        case 9: // Publish binary, 1 means success
            if (urcParam1 == 1) {
                // Published
                pUrcStatus->flagsBitmap |= 1 << U_CELL_MQTT_URC_FLAG_PUBLISH_SUCCESS;
            }
            pUrcStatus->flagsBitmap |= 1 << U_CELL_MQTT_URC_FLAG_PUBLISH_UPDATED;
            break;
        // 3 (publish file) is not used by this driver
        case 4: // Subscribe
            // Get the QoS
            urcParam2 = uAtClientReadInt(atHandle);
            // Skip the topic string
            uAtClientSkipParameters(atHandle, 1);
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
            break;
        case 5: // Unsubscribe, 1 means success
            if (urcParam1 == 1) {
                // Unsubscribed
                pUrcStatus->flagsBitmap |= 1 << U_CELL_MQTT_URC_FLAG_UNSUBSCRIBE_SUCCESS;
            }
            pUrcStatus->flagsBitmap |= 1 << U_CELL_MQTT_URC_FLAG_UNSUBSCRIBE_UPDATED;
            break;
        case 6: // Num unread messages
            if (urcParam1 >= 0) {
                pContext->numUnreadMessages = urcParam1;
                if (pContext->pMessageIndicationCallback != NULL) {
                    // Launch our local callback via the AT
                    // parser's callback facility
                    // GCC can complain here that
                    // we're discarding volatile
                    // from the pointer: just need to follow
                    // the function signature guys...
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
                    //lint -e(1773) Suppress complaints about
                    // passing the pointer as non-volatile
                    uAtClientCallback(atHandle, messageIndicationCallback,
                                      (void *) pContext);
#pragma GCC diagnostic pop
                }
            }
            pUrcStatus->flagsBitmap |= 1 << U_CELL_MQTT_URC_FLAG_UNREAD_MESSAGES_UPDATED;
            break;
        default:
            // Do nothing
            break;
    }
}

// "+UUMQTTx:" URC handler, for SARA-R4 only,
// called by the UUMQTT_urc() URC handler.
// The switch statement here needs to match those in
// resetUrcStatusField() and checkUrcStatusField()
static void UUMQTTx_urc(uAtClientHandle_t atHandle,
                        volatile uCellMqttContext_t *pContext,
                        int32_t x)
{
    volatile uCellMqttUrcStatus_t *pUrcStatus = &(pContext->urcStatus);
    char delimiter = uAtClientDelimiterGet(atHandle);

    // All these parameters are delimited by
    // a carriage return
    uAtClientDelimiterSet(atHandle, '\r');
    switch (x) {
        case 0: // Client name
            if (!pUrcStatus->clientId.filledIn &&
                (uAtClientReadString(atHandle,
                                     pUrcStatus->clientId.pContents,
                                     pUrcStatus->clientId.sizeBytes,
                                     false) > 0)) {
                pUrcStatus->clientId.filledIn = true;
            }
            break;
        case 1: // Local port number
            pUrcStatus->localPortNumber = uAtClientReadInt(atHandle);
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
            // TODO
            break;
        case 10: // Inactivity timeout
            pUrcStatus->inactivityTimeoutSeconds = uAtClientReadInt(atHandle);
            break;
        case 11: // TLS secured
            if (uAtClientReadInt(atHandle) == 1) {
                pUrcStatus->flagsBitmap |= 1 << U_CELL_MQTT_URC_FLAG_SECURED;
                pUrcStatus->securityProfileId = uAtClientReadInt(atHandle);
            }
            break;
        case 12: // Session retained
            if (uAtClientReadInt(atHandle) == 0) {
                pUrcStatus->flagsBitmap |= 1 << U_CELL_MQTT_URC_FLAG_RETAINED;
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
    // Set the delimiter to '\'r to make this stop after the integer
    uAtClientDelimiterSet(atHandle, '\r');
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
    // way through this maze.

    // Switch off the stop tag and read
    // in the next 8 bytes and to see if they are "\r\nTopic:"
    uAtClientIgnoreStopTag(atHandle);
    x = uAtClientReadBytes(atHandle, buffer, 8, false);
    if ((x == 8) &&
        (memcmp(buffer, "\r\nTopic:", 8) == 0)) {
        if (pUrcMessage != NULL) {
            if (pUrcMessage->pTopicNameStr != NULL) {
                // Read the rest of this line, which will be the topic
                topicNameBytesRead = uAtClientReadString(atHandle,
                                                         pUrcMessage->pTopicNameStr,
                                                         pUrcMessage->topicNameSizeBytes,
                                                         false);
            }
            if (topicNameBytesRead >= 0) {
                pUrcMessage->topicNameSizeBytes = topicNameBytesRead;
                // Skip the additional '\r\n'
                uAtClientSkipBytes(atHandle, 2);
                // Read the next line and find the length of the message
                // and the QoS from it
                x = uAtClientReadString(atHandle, buffer, sizeof(buffer) - 1, false);
                if (x >= 0) {
                    buffer[x] = '\0';
                    pStr = strtok_r(buffer, " ", &pSaved);
                    if ((pStr != NULL) && (strcmp(pStr, "Len:") == 0)) {
                        messageBytesAvailable = strtol(pStr + 4, NULL, 10);
                    }
                    pStr = strtok_r(NULL, " ", &pSaved);
                    if ((pStr != NULL) && (strcmp(pStr, "QoS:") == 0)) {
                        pUrcMessage->qos = (uCellMqttQos_t) strtol(pStr + 4, NULL, 10);
                        gotLengthAndQos = true;
                    }
                    if (gotLengthAndQos && (messageBytesAvailable >= 0)) {
                        // Finally, read the next messageBytesAvailable bytes
                        // Skip the additional '\r\n'
                        uAtClientSkipBytes(atHandle, 2);
                        // Throw away the "Msg:" bit
                        uAtClientReadBytes(atHandle, NULL, 4, false);
                        // Now read the exact length of message
                        // bytes, being careful to not look for
                        // delimiters or the like as this can be
                        // a binary message
                        x = messageBytesAvailable;
                        if (x > pUrcMessage->messageSizeBytes) {
                            x = pUrcMessage->messageSizeBytes;
                        }
                        uAtClientIgnoreStopTag(atHandle);
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
                        uAtClientRestoreStopTag(atHandle);
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
            // Launch our local callback via the AT
            // parser's callback facility
            // GCC can complain here that
            // we're discarding volatile
            // from the pointer: just need to follow
            // the function signature guys...
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
            //lint -e(1773) Suppress complaints about
            // passing the pointer as non-volatile
            uAtClientCallback(atHandle, messageIndicationCallback,
                              (void *) pContext);
#pragma GCC diagnostic pop
        }
    }
    uAtClientDelimiterSet(atHandle, delimiter);
}

// MQTT URC handler, which hands
// off to the three MQTT URC types,
// "+UUMQTTx:" (where x can be a two
// digit number), "+UUMQTTC:" and
// "+UUMQTTCM:".
static void UUMQTT_urc(uAtClientHandle_t atHandle,
                       void *pParameter)
{
    uCellPrivateInstance_t *pInstance = (uCellPrivateInstance_t *) pParameter;
    volatile uCellMqttContext_t *pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
    char bytes[3];

    if (pContext != NULL) {
        // Sort out if this is "+UUMQTTC:"
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
                        UUMQTTC_urc(atHandle, pContext, pInstance);
                    }
                } else {
                    // Probably "+UUMQTTx:"
                    // Derive x as an integer, noting
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
                    UUMQTTC_urc(atHandle, pContext, pInstance);
                }
            }
        }
    }
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: EVERYTHING ELSE
 * -------------------------------------------------------------- */

// Check all the basics and lock the mutex, MUST be called
// at the start of every API function; use the helper macro
// U_CELL_MQTT_ENTRY_FUNCTION to be sure of this, rather than
// calling this function directly.
// IMPORTANT: if mustBeInitialised is true then the returned value
// in pErrorCode will be zero if there is a valid cellular instance
// with an already initialised MQTT context.  If mustBeInitialised
// is false then the same is true except that there may NOT be an
// already initialised MQTT context, i.e. pInstance->pMqttContext
// may be NULL.  This latter case is only useful when this function
// is called from uCellMqttInit(), normally you want to call this
// function with mustBeInitialised set to true.  In all cases the
// cellular mutex will be locked.
static void entryFunction(int32_t cellHandle,
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
                                   U_CELL_PRIVATE_FEATURE_MQTT)) {
                errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
                if (!mustBeInitialised || (pInstance->pMqttContext != NULL)) {
                    *ppInstance = pInstance;
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
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
// U_CELL_MQTT_EXIT_FUNCTION to be sure of this, rather than
// calling this function directly.
static void exitFunction()
{
    if (gUCellPrivateMutex != NULL) {
        uPortMutexUnlock(gUCellPrivateMutex);
    }
}

// Print the error state of MQTT.
//lint -esym(522, printErrorCodes) Suppress "lacks side effects"
// when compiled out.
static void printErrorCodes(uAtClientHandle_t atHandle)
{
#if U_CFG_ENABLE_LOGGING
    int32_t err1;
    int32_t err2;

    uAtClientLock(atHandle);
    uAtClientCommandStart(atHandle, "AT+UMQTTER");
    uAtClientCommandStop(atHandle);
    uAtClientResponseStart(atHandle, "+UMQTTER:");
    err1 = uAtClientReadInt(atHandle);
    err2 = uAtClientReadInt(atHandle);
    uAtClientResponseStop(atHandle);
    uAtClientUnlock(atHandle);
    uPortLog("U_CELL_MQTT: error codes %d, %d.\n", err1, err2);
#else
    (void) atHandle;
#endif
}

// Process the response to an AT+MQTT command.
static int32_t atMqttStopCmdGetRespAndUnlock(const uCellPrivateInstance_t *pInstance)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
    uAtClientHandle_t atHandle = pInstance->atHandle;
    int32_t status = 1;

    if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                           U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
        uAtClientCommandStop(atHandle);
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
        printErrorCodes(atHandle);
    }

    return errorCode;
}

// Set the given pInstance->pMqttContext->urcStatus item to "not filled in".
// The switch statement here should match that in UUMQTTx_urc()
static void resetUrcStatusField(volatile uCellMqttUrcStatus_t *pUrcStatus,
                                int32_t number)
{
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
            pUrcStatus->securityProfileId = -1;
            break;
        case 12: // Session retained
            pUrcStatus->flagsBitmap &= ~(1 << U_CELL_MQTT_URC_FLAG_RETAINED);
            break;
        default:
            // Do nothing
            break;
    }
}

// Check if the given pUrcStatus item has been filled in.
// The switch statement here should match that in UUMQTTx_urc()
static bool checkUrcStatusField(volatile uCellMqttUrcStatus_t *pUrcStatus,
                                int32_t number)
{
    bool filledIn = false;

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
            filledIn = (pUrcStatus->flagsBitmap &= (1 << U_CELL_MQTT_URC_FLAG_SECURED)) != 0;
            break;
        case 12: // Session retained
            filledIn = (pUrcStatus->flagsBitmap &= (1 << U_CELL_MQTT_URC_FLAG_RETAINED)) != 0;
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
    int64_t stopTimeMs;

    // The old SARA-R4 MQTT AT interface syntax gets very
    // peculiar here.
    // Have to send in AT+UMQTT=x? and then wait for a URC

    // Set the relevant urcStatus item to "not filled in"
    resetUrcStatusField(pUrcStatus, number);

    // Now send the AT command
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
        stopTimeMs = uPortGetTickTimeMs() + U_CELL_MQTT_LOCAL_URC_TIMEOUT_MS;
        while ((!checkUrcStatusField(pUrcStatus, number)) &&
               (uPortGetTickTimeMs() < stopTimeMs)) {
            uPortTaskBlock(250);
        }
        if (checkUrcStatusField(pUrcStatus, number)) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
    }

    return errorCode;
}

// Determine whether MQTT TLS security is on or off.
static bool isSecured(const uCellPrivateInstance_t *pInstance,
                      int32_t *pSecurityProfileId)
{
    bool secured = false;
    volatile uCellMqttContext_t *pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
    volatile uCellMqttUrcStatus_t *pUrcStatus = &(pContext->urcStatus);
    uAtClientHandle_t atHandle;

    if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                           U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
        // Run the query, answers come back in pUrcStatus
        if (doSaraR4OldSyntaxUmqttQuery(pInstance, 11) == 0) {
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
        uAtClientCommandStart(atHandle, "AT+UMQTT=");
        uAtClientWriteInt(atHandle, 11);
        uAtClientCommandStop(atHandle);
        uAtClientResponseStart(atHandle, "+UMQTT:");
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

    return secured;
}

// Set MQTT ping or "keep alive" on or off.
static int32_t setKeepAlive(int32_t cellHandle, bool onNotOff)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;
    uAtClientHandle_t atHandle;
    int32_t status = 1;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
        errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
        atHandle = pInstance->atHandle;
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UMQTTC=");
        // Set ping
        uAtClientWriteInt(atHandle, 8);
        uAtClientWriteInt(atHandle, (int32_t) onNotOff);
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
            uAtClientCommandStop(atHandle);
            uAtClientResponseStart(atHandle, "+UMQTTC:");
            // Skip the first parameter, which is just
            // our UMQTT command number again
            uAtClientSkipParameters(atHandle, 1);
            status = uAtClientReadInt(atHandle);
            uAtClientResponseStop(atHandle);
        } else {
            uAtClientCommandStopReadResponse(atHandle);
        }
        if ((uAtClientUnlock(atHandle) == 0) &&
            (status == 1)) {
            // This has no URCness to it, that's it
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            pContext->keptAlive = onNotOff;
        } else {
            printErrorCodes(atHandle);
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

// Set MQTT session retention on or off.
static int32_t setSessionRetain(int32_t cellHandle, bool onNotOff)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    uAtClientHandle_t atHandle;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTT_SESSION_RETAIN)) {
            atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UMQTT=");
            // Set retention
            uAtClientWriteInt(atHandle, 12);
            uAtClientWriteInt(atHandle, (int32_t) onNotOff);
            errorCode = atMqttStopCmdGetRespAndUnlock(pInstance);
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

// Set security on or off.
static int32_t setSecurity(int32_t cellHandle, bool onNotOff,
                           int32_t securityProfileId)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    uAtClientHandle_t atHandle;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        atHandle = pInstance->atHandle;
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UMQTT=");
        // Set security
        uAtClientWriteInt(atHandle, 11);
        uAtClientWriteInt(atHandle, (int32_t) onNotOff);
        if (onNotOff && (securityProfileId >= 0)) {
            uAtClientWriteInt(atHandle, securityProfileId);
        }
        errorCode = atMqttStopCmdGetRespAndUnlock(pInstance);
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
    volatile uCellMqttUrcStatus_t *pUrcStatus;
    uAtClientHandle_t atHandle;
    int64_t startTimeMs;
    int64_t stopTimeMs;
    int32_t status = 1;

    pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
    pUrcStatus = &(pContext->urcStatus);
    atHandle = pInstance->atHandle;
    uPortLog("U_CELL_MQTT: trying to %s...\n", onNotOff ? "connect" : "disconnect");
    uAtClientLock(atHandle);
    pUrcStatus->flagsBitmap = 0;
    // Have seen this take a little while to respond
    uAtClientTimeoutSet(atHandle, 15000);
    uAtClientCommandStart(atHandle, "AT+UMQTTC=");
    // Conveniently log-in is command 0 and
    // log out is command 1
    uAtClientWriteInt(atHandle, (int32_t) onNotOff);
    if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                           U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
        uAtClientCommandStop(atHandle);
        uAtClientResponseStart(atHandle, "+UMQTTC:");
        // Skip the first parameter, which is just
        // our UMQTTC command number again
        uAtClientSkipParameters(atHandle, 1);
        status = uAtClientReadInt(atHandle);
        uAtClientResponseStop(atHandle);
    } else {
        uAtClientCommandStopReadResponse(atHandle);
    }

    if ((uAtClientUnlock(atHandle) == 0) && (status == 1)) {
        if (!onNotOff &&
            U_CELL_PRIVATE_MODULE_IS_SARA_R4(pInstance->pModule->moduleType)) {
            // For disconnections on SARA-R4 no need to wait,
            // that's it
            pContext->connected = false;
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        } else {
            // Otherwise wait for the URC for success
            uPortLog("U_CELL_MQTT: waiting for response for up to %d"
                     " second(s)...\n",
                     U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS);
            errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
            startTimeMs = uPortGetTickTimeMs();
            stopTimeMs = startTimeMs + (U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS * 1000);
            while (((pUrcStatus->flagsBitmap & (1 << U_CELL_MQTT_URC_FLAG_CONNECT_UPDATED)) == 0) &&
                   (uPortGetTickTimeMs() < stopTimeMs) &&
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
                printErrorCodes(atHandle);
            }
        }
    } else {
        printErrorCodes(atHandle);
    }

    return errorCode;
}

// Return true if all of pBuffer is printable.
static bool isPrint(const char *pBuffer, size_t bufferLength)
{
    bool printable = true;

    for (size_t x = 0; (x < bufferLength) && printable; x++) {
        if (!isprint((int32_t) *pBuffer)) {
            printable = false;
        }
        pBuffer++;
    }

    return printable;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise the cellular MQTT client.
int32_t uCellMqttInit(int32_t cellHandle, const char *pBrokerNameStr,
                      const char *pClientIdStr, const char *pUserNameStr,
                      const char *pPasswordStr,
                      bool (*pKeepGoingCallback)(void),
                      bool futureExpansion)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;
    uAtClientHandle_t atHandle;
    uSockAddress_t address;
    char *pAddress;
    char *pTmp;
    int32_t port;
    bool keepGoing = true;
    char imei[U_CELL_INFO_IMEI_SIZE + 1];

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, false);

    if ((errorCode == 0) && (pInstance != NULL)) {
        // Only continue if MQTT is not already initialised for this handler
        if (pInstance->pMqttContext == NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
            // Check parameters, only pBrokerNameStr has to be present
            if (!futureExpansion && (pBrokerNameStr != NULL) &&
                (strlen(pBrokerNameStr) <=
                 U_CELL_MQTT_BROKER_ADDRESS_STRING_MAX_LENGTH_BYTES)) {
                errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                // Allocate memory for the MQTT context
                pContext = (volatile uCellMqttContext_t *) malloc(sizeof(*pContext));
                if (pContext != NULL) {
                    pContext->pKeepGoingCallback = pKeepGoingCallback;
                    pContext->pMessageIndicationCallback = NULL;
                    pContext->pMessageIndicationCallbackParam = NULL;
                    pContext->keptAlive = false;
                    pContext->connected = false;
                    pContext->numUnreadMessages = 0;
                    pContext->pUrcMessage = NULL;
                    pInstance->pMqttContext = pContext;
                    if (U_CELL_PRIVATE_MODULE_IS_SARA_R4(pInstance->pModule->moduleType)) {
                        // SARA-R4 requires a pUrcMessage as well
                        pContext->pUrcMessage = (uCellMqttUrcMessage_t *) malloc(sizeof(*(pContext->pUrcMessage)));
                    }
                    if ((pContext->pUrcMessage != NULL) ||
                        !U_CELL_PRIVATE_MODULE_IS_SARA_R4(pInstance->pModule->moduleType)) {
                        atHandle = pInstance->atHandle;
                        // Deal with the broker name string
                        // Allocate space to fiddle with the
                        // server address, +1 for terminator
                        pAddress = (char *) malloc(U_CELL_MQTT_BROKER_ADDRESS_STRING_MAX_LENGTH_BYTES + 1);
                        if (pAddress != NULL) {
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
                                                           pAddress,
                                                           U_CELL_MQTT_BROKER_ADDRESS_STRING_MAX_LENGTH_BYTES) > 0) {
                                    uAtClientLock(atHandle);
                                    uAtClientCommandStart(atHandle, "AT+UMQTT=");
                                    // Set the broker IP address
                                    uAtClientWriteInt(atHandle, 3);
                                    uAtClientWriteString(atHandle, pAddress, true);
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
                                strcpy(pAddress, pBrokerNameStr);
                                // Grab any port number off the end
                                // and then remove it from the string
                                port = uSockDomainGetPort(pAddress);
                                pTmp = pUSockDomainRemovePort(pAddress);
                                uAtClientLock(atHandle);
                                uAtClientCommandStart(atHandle, "AT+UMQTT=");
                                // Set the broker URL
                                uAtClientWriteInt(atHandle, 2);
                                uAtClientWriteString(atHandle, pTmp, true);
                                // If there was a port number, write
                                // that also
                                if (port > 0) {
                                    uAtClientWriteInt(atHandle, port);
                                }
                                keepGoing = (atMqttStopCmdGetRespAndUnlock(pInstance) == 0);
                            }

                            // Free memory
                            free(pAddress);

                            // Now deal with the credentials
                            if (keepGoing && (pUserNameStr != NULL)) {
                                uAtClientLock(atHandle);
                                uAtClientCommandStart(atHandle, "AT+UMQTT=");
                                // Set credentials
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
                                    uAtClientCommandStart(atHandle, "AT+UMQTT=");
                                    // Set client ID
                                    uAtClientWriteInt(atHandle, 0);
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
                                uAtClientCommandStart(atHandle, "AT+UMQTTC=");
                                // Message read format
                                uAtClientWriteInt(atHandle, 7);
                                // Format: verbose
                                uAtClientWriteInt(atHandle, 2);
                                keepGoing = (atMqttStopCmdGetRespAndUnlock(pInstance) == 0);
                            }

                            // Almost done
                            if (keepGoing) {
                                // Set up the URC
                                errorCode = uAtClientSetUrcHandler(atHandle,
                                                                   "+UUMQTT",
                                                                   UUMQTT_urc,
                                                                   pInstance);
                            } else {
                                printErrorCodes(atHandle);
                            }
                        }
                    }

                    // And we're done
                    if (errorCode != 0) {
                        // Free memory again if we failed somewhere
                        if (pInstance->pMqttContext != NULL) {
                            // GCC can complain here that
                            // we're discarding volatile
                            // from the pointers when freeing
                            // them
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
                            //lint -e(605) Suppress complaints about
                            // freeing a volatile pointer as well
                            free(((volatile uCellMqttContext_t *) pInstance->pMqttContext)->pUrcMessage);
                        }
                        //lint -e(605) Suppress complaints about
                        // freeing this volatile pointer as well
                        free(pInstance->pMqttContext);
#pragma GCC diagnostic pop
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
void uCellMqttDeinit(int32_t cellHandle)
{
    uCellPrivateInstance_t *pInstance;
    volatile uCellMqttContext_t *pContext;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, NULL, true);

    if (pInstance != NULL) {
        pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
        if (pContext->connected) {
            connect(pInstance, false);
        }

        uAtClientRemoveUrcHandler(pInstance->atHandle,
                                  "+UUMQTT");
        // GCC can complain here that
        // we're discarding volatile
        // from the pointers when freeing
        // them
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
        //lint -e(605) Suppress complaints about
        // freeing a volatile pointer as well
        free(pContext->pUrcMessage);
        //lint -e(605) Suppress complaints about
        // freeing this volatile pointer as well
        free(pContext);
#pragma GCC diagnostic pop
        pInstance->pMqttContext = NULL;
    }

    U_CELL_MQTT_EXIT_FUNCTION();
}

// Get the current cellular MQTT client ID.
int32_t uCellMqttGetClientId(int32_t cellHandle, char *pClientIdStr,
                             size_t sizeBytes)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;
    volatile uCellMqttUrcStatus_t *pUrcStatus;
    uAtClientHandle_t atHandle;
    int32_t bytesRead;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
        pUrcStatus = &(pContext->urcStatus);
        if (pClientIdStr != NULL) {
            atHandle = pInstance->atHandle;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
                pUrcStatus->clientId.pContents = pClientIdStr;
                pUrcStatus->clientId.sizeBytes = sizeBytes;
                // This will fill in the string
                errorCode = doSaraR4OldSyntaxUmqttQuery(pInstance, 0);
            } else {
                errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+UMQTT=");
                uAtClientWriteInt(atHandle, 0);
                uAtClientCommandStop(atHandle);
                uAtClientResponseStart(atHandle, "+UMQTT:");
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

// Set the local port to use for the MQTT client.
int32_t uCellMqttSetLocalPort(int32_t cellHandle, uint16_t port)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    uAtClientHandle_t atHandle;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTT_SET_LOCAL_PORT)) {
            atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
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

// Get the local port used by the MQTT client.
int32_t uCellMqttGetLocalPort(int32_t cellHandle)
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
                               U_CELL_PRIVATE_FEATURE_MQTT_SET_LOCAL_PORT)) {
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
                // SARA=R4 doesn't respond with a port number if the
                // port number is just the default one.  Determine if
                // we are secured so that we can send back the correct
                // default port number
                errorCodeOrPort = U_MQTT_BROKER_PORT_UNSECURE;
                if (isSecured(pInstance, NULL)) {
                    errorCodeOrPort = U_MQTT_BROKER_PORT_SECURE;
                }
            }
        } else {
            // The port number will be based upon whether
            // security is enabled or not
            errorCodeOrPort = U_MQTT_BROKER_PORT_UNSECURE;
            if (isSecured(pInstance, NULL)) {
                errorCodeOrPort = U_MQTT_BROKER_PORT_SECURE;
            }
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCodeOrPort;
}

// Set the inactivity timeout used by the MQTT client.
int32_t uCellMqttSetInactivityTimeout(int32_t cellHandle,
                                      size_t seconds)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    uAtClientHandle_t atHandle;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        if ((seconds == 0) &&
            (pInstance->pModule->moduleType == U_CELL_MODULE_TYPE_SARA_R5)) {
            // Setting an inactivity timeout of zero for SARA-R5
            // is not supported
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        } else {
            atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UMQTT=");
            // Set the inactivity timeout
            uAtClientWriteInt(atHandle, 10);
            uAtClientWriteInt(atHandle, (int32_t) seconds);
            errorCode = atMqttStopCmdGetRespAndUnlock(pInstance);
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

// Get the inactivity timeout used by the MQTT client.
int32_t uCellMqttGetInactivityTimeout(int32_t cellHandle)
{
    int32_t errorCodeOrTimeout = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;
    volatile uCellMqttUrcStatus_t *pUrcStatus;
    uAtClientHandle_t atHandle;
    int32_t x;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCodeOrTimeout, true);

    if ((errorCodeOrTimeout == 0) && (pInstance != NULL)) {
        pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
        pUrcStatus = &(pContext->urcStatus);
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
            errorCodeOrTimeout = doSaraR4OldSyntaxUmqttQuery(pInstance, 10);
            if ((errorCodeOrTimeout == 0) &&
                (pUrcStatus->inactivityTimeoutSeconds >= 0)) {
                errorCodeOrTimeout = pUrcStatus->inactivityTimeoutSeconds;
            }
        } else {
            errorCodeOrTimeout = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
            atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UMQTT=");
            // Get the inactivity timeout
            uAtClientWriteInt(atHandle, 10);
            uAtClientCommandStop(atHandle);
            uAtClientResponseStart(atHandle, "+UMQTT:");
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
int32_t uCellMqttSetKeepAliveOn(int32_t cellHandle)
{
    return setKeepAlive(cellHandle, true);
}

// Switch MQTT ping or "keep alive" off.
int32_t uCellMqttSetKeepAliveOff(int32_t cellHandle)
{
    return setKeepAlive(cellHandle, false);
}

// Determine whether MQTT ping or "keep alive" is on or off.
bool uCellMqttIsKeptAlive(int32_t cellHandle)
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
int32_t uCellMqttSetRetainOn(int32_t cellHandle)
{
    return setSessionRetain(cellHandle, true);
}

// Switch MQTT session retention off.
int32_t uCellMqttSetRetainOff(int32_t cellHandle)
{
    return setSessionRetain(cellHandle, false);
}

// Determine whether MQTT session retention is on or off.
bool uCellMqttIsRetained(int32_t cellHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    bool isRetained = false;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;
    volatile uCellMqttUrcStatus_t *pUrcStatus;
    uAtClientHandle_t atHandle;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTT_SESSION_RETAIN)) {
            pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
            pUrcStatus = &(pContext->urcStatus);
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
                // Run the query, answers come back in pUrcStatus
                if ((doSaraR4OldSyntaxUmqttQuery(pInstance, 12) == 0) &&
                    ((pUrcStatus->flagsBitmap & (1 << U_CELL_MQTT_URC_FLAG_PUBLISH_SUCCESS)) != 0)) {
                    isRetained = true;
                }
            } else {
                atHandle = pInstance->atHandle;
                uAtClientLock(atHandle);
                uAtClientCommandStart(atHandle, "AT+UMQTT=");
                // Get the session retention status
                uAtClientWriteInt(atHandle, 12);
                uAtClientCommandStop(atHandle);
                uAtClientResponseStart(atHandle, "+UMQTT:");
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
int32_t uCellMqttSetSecurityOn(int32_t cellHandle,
                               int32_t securityProfileId)
{
    return setSecurity(cellHandle, true, securityProfileId);
}

// Switch MQTT TLS security off.
int32_t uCellMqttSetSecurityOff(int32_t cellHandle)
{
    return setSecurity(cellHandle, false, 0);
}

// Determine whether MQTT TLS security is on or off.
bool uCellMqttIsSecured(int32_t cellHandle,
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
int32_t uCellMqttSetWill(int32_t cellHandle,
                         const char *pTopicNameStr,
                         const char *pMessage,
                         size_t messageSizeBytes,
                         uCellMqttQos_t qos, bool retain)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    uAtClientHandle_t atHandle;
    char *pHexMessage = NULL;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                               U_CELL_PRIVATE_FEATURE_MQTT_WILL)) {
            errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
            //lint -e(568) Suppress value never being negative, who knows
            // what warnings levels a customer might compile with
            if (((int32_t) qos >= 0) &&
                (qos < U_CELL_MQTT_QOS_MAX_NUM) &&
                ((pTopicNameStr == NULL) ||
                 (strlen(pTopicNameStr) <= U_CELL_MQTT_WRITE_TOPIC_MAX_LENGTH_BYTES)) &&
                ((pMessage == NULL) ||
                 (messageSizeBytes <= U_CELL_MQTT_WILL_MESSAGE_MAX_LENGTH_BYTES))) {
                atHandle = pInstance->atHandle;
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                if (pMessage != NULL) {
                    // Allocate space to encode the hex version of the message
                    errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                    pHexMessage = (char *) malloc((messageSizeBytes * 2) + 1);
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
                    uAtClientCommandStart(atHandle, "AT+UMQTT=");
                    // Set "will" QOS
                    uAtClientWriteInt(atHandle, 6);
                    // The "will" QOS
                    uAtClientWriteInt(atHandle, (int32_t) qos);
                    errorCode = atMqttStopCmdGetRespAndUnlock(pInstance);
                }
                if (errorCode == 0) {
                    // Finally, write the "will" retention flag
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, "AT+UMQTT=");
                    // Set "will" retention
                    uAtClientWriteInt(atHandle, 7);
                    // The "will" retention flag
                    uAtClientWriteInt(atHandle, (int32_t) retain);
                    errorCode = atMqttStopCmdGetRespAndUnlock(pInstance);
                }
                if ((errorCode == 0) && (pTopicNameStr != NULL)) {
                    // Write the "will" topic name string
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, "AT+UMQTT=");
                    // Set "will" topic name
                    uAtClientWriteInt(atHandle, 8);
                    // The "will" topic name
                    uAtClientWriteString(atHandle, pTopicNameStr, true);
                    errorCode = atMqttStopCmdGetRespAndUnlock(pInstance);
                }
                if ((errorCode == 0) && (pHexMessage != NULL)) {
                    // Finally, and it must be finally,
                    // write the "will" message
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, "AT+UMQTT=");
                    // Set "will" message
                    uAtClientWriteInt(atHandle, 9);
                    // Write the "will" message
                    uAtClientWriteString(atHandle, pHexMessage, true);
                    // Hex mode
                    uAtClientWriteInt(atHandle, 1);
                    errorCode = atMqttStopCmdGetRespAndUnlock(pInstance);
                }
                // Free memory
                free(pHexMessage);
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
int32_t uCellMqttGetWill(int32_t cellHandle, char *pTopicNameStr,
                         size_t topicNameSizeBytes,
                         char *pMessage,
                         size_t *pMessageSizeBytes,
                         uCellMqttQos_t *pQos, bool *pRetain)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
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
            if ((pMessage == NULL) || (pMessageSizeBytes != NULL)) {
                atHandle = pInstance->atHandle;
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                if (pTopicNameStr != NULL) {
                    // Create a buffer to store the "will" topic name
                    // in, since it may be larger than the user has
                    // asked for and we have to read in the lot
                    errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                    pBuffer = (char *) malloc(U_CELL_MQTT_READ_TOPIC_MAX_LENGTH_BYTES + 1);
                    if (pBuffer != NULL) {
                        // Get the "will" topic name string
                        uAtClientLock(atHandle);
                        uAtClientCommandStart(atHandle, "AT+UMQTT=");
                        // "will" topic name
                        uAtClientWriteInt(atHandle, 8);
                        uAtClientCommandStop(atHandle);
                        uAtClientResponseStart(atHandle, "+UMQTT:");
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
                        free(pBuffer);
                    }
                }
                if ((errorCode == 0) && (pMessage != NULL)) {
                    errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
                    // Get the "will" message string
                    uAtClientLock(atHandle);
                    uAtClientCommandStart(atHandle, "AT+UMQTT=");
                    // "will" message
                    uAtClientWriteInt(atHandle, 9);
                    uAtClientCommandStop(atHandle);
                    uAtClientResponseStart(atHandle, "+UMQTT:");
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
                    uAtClientCommandStart(atHandle, "AT+UMQTT=");
                    // "will" QoS
                    uAtClientWriteInt(atHandle, 6);
                    uAtClientCommandStop(atHandle);
                    uAtClientResponseStart(atHandle, "+UMQTT:");
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
                    uAtClientCommandStart(atHandle, "AT+UMQTT=");
                    // "will" retention
                    uAtClientWriteInt(atHandle, 7);
                    uAtClientCommandStop(atHandle);
                    uAtClientResponseStart(atHandle, "+UMQTT:");
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
int32_t uCellMqttConnect(int32_t cellHandle)
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
int32_t uCellMqttDisconnect(int32_t cellHandle)
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
bool uCellMqttIsConnected(int32_t cellHandle)
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

// Publish an MQTT message.
int32_t uCellMqttPublish(int32_t cellHandle,
                         const char *pTopicNameStr,
                         const char *pMessage,
                         size_t messageSizeBytes,
                         uCellMqttQos_t qos, bool retain)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;
    volatile uCellMqttUrcStatus_t *pUrcStatus;
    uAtClientHandle_t atHandle;
    char *pTextMessage = NULL;
    int32_t status = 1;
    bool isAscii = false;
    bool messageWritten = false;
    int64_t stopTimeMs;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
        pUrcStatus = &(pContext->urcStatus);
        isAscii = isPrint(pMessage, messageSizeBytes);
        //lint -e(568) Suppress value never being negative, who knows
        // what warnings levels a customer might compile with
        if (((int32_t) qos >= 0) && (qos < U_CELL_MQTT_QOS_MAX_NUM) &&
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
                                    U_CELL_PRIVATE_FEATURE_MQTT_BINARY_PUBLISH)) {
                // If we aren't able to publish a message as a binary
                // blob then allocate space to publish it as a string,
                // either as hex or as ASCII with a terminator added
                if (isAscii) {
                    pTextMessage = (char *) malloc(messageSizeBytes + 1);
                    if (pTextMessage != NULL) {
                        // Just copy in the text and add a terminator
                        memcpy(pTextMessage, pMessage, messageSizeBytes);
                        *(pTextMessage + messageSizeBytes) = '\0';
                    }
                } else {
                    pTextMessage = (char *) malloc((messageSizeBytes * 2) + 1);
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
                uAtClientCommandStart(atHandle, "AT+UMQTTC=");
                // Publish the message
                if (pTextMessage != NULL) {
                    // ASCII or hex mode
                    uAtClientWriteInt(atHandle, 2);
                } else {
                    // Binary mode
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

                // Free memory (it is legal C to free a NULL pointer)
                free(pTextMessage);

                if (messageWritten) {
                    if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                           U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
                        uAtClientResponseStart(atHandle, "+UMQTTC:");
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
                        stopTimeMs = uPortGetTickTimeMs() +
                                     (U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS * 1000);
                        while (((pUrcStatus->flagsBitmap & (1 << U_CELL_MQTT_URC_FLAG_PUBLISH_UPDATED)) == 0) &&
                               (uPortGetTickTimeMs() < stopTimeMs) &&
                               ((pContext->pKeepGoingCallback == NULL) ||
                                pContext->pKeepGoingCallback())) {
                            uPortTaskBlock(1000);
                        }
                        if ((pUrcStatus->flagsBitmap & (1 << U_CELL_MQTT_URC_FLAG_PUBLISH_SUCCESS)) != 0) {
                            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                        } else {
                            printErrorCodes(atHandle);
                        }
                    }
                } else {
                    printErrorCodes(atHandle);
                }
            }
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

// Subscribe to an MQTT topic.
int32_t uCellMqttSubscribe(int32_t cellHandle,
                           const char *pTopicFilterStr,
                           uCellMqttQos_t maxQos)
{
    int32_t errorCodeOrQos = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;
    volatile uCellMqttUrcStatus_t *pUrcStatus;
    uAtClientHandle_t atHandle;
    int32_t status = 1;
    int64_t stopTimeMs;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCodeOrQos, true);

    if ((errorCodeOrQos == 0) && (pInstance != NULL)) {
        errorCodeOrQos = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
        pUrcStatus = &(pContext->urcStatus);
        //lint -e(568) Suppress value never being negative, who knows
        // what warnings levels a customer might compile with
        if (((int32_t) maxQos >= 0) &&
            (maxQos < U_CELL_MQTT_QOS_MAX_NUM) &&
            (pTopicFilterStr != NULL) &&
            (strlen(pTopicFilterStr) <= U_CELL_MQTT_WRITE_TOPIC_MAX_LENGTH_BYTES)) {
            errorCodeOrQos = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
            atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
            pUrcStatus->flagsBitmap = 0;
            uAtClientCommandStart(atHandle, "AT+UMQTTC=");
            // Subscribe to a topic
            uAtClientWriteInt(atHandle, 4);
            // Max QoS
            uAtClientWriteInt(atHandle, (int32_t) maxQos);
            // Topic
            uAtClientWriteString(atHandle, pTopicFilterStr, true);
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
                uAtClientCommandStop(atHandle);
                uAtClientResponseStart(atHandle, "+UMQTTC:");
                // Skip the first parameter, which is just
                // our UMQTTC command number again
                uAtClientSkipParameters(atHandle, 1);
                status = uAtClientReadInt(atHandle);
                uAtClientResponseStop(atHandle);
            } else {
                uAtClientCommandStopReadResponse(atHandle);
            }

            if ((uAtClientUnlock(atHandle) == 0) && (status == 1)) {
                // On all platforms need to wait for a URC to
                // say that the subscribe has succeeded
                errorCodeOrQos = (int32_t) U_ERROR_COMMON_TIMEOUT;
                stopTimeMs = uPortGetTickTimeMs() +
                             (U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS * 1000);
                while (((pUrcStatus->flagsBitmap & (1 << U_CELL_MQTT_URC_FLAG_SUBSCRIBE_UPDATED)) == 0) &&
                       (uPortGetTickTimeMs() < stopTimeMs) &&
                       ((pContext->pKeepGoingCallback == NULL) ||
                        pContext->pKeepGoingCallback())) {
                    uPortTaskBlock(1000);
                }
                if ((pUrcStatus->flagsBitmap & (1 << U_CELL_MQTT_URC_FLAG_SUBSCRIBE_SUCCESS)) != 0) {
                    errorCodeOrQos = (int32_t) pUrcStatus->subscribeQoS;
                } else {
                    printErrorCodes(atHandle);
                }
            } else {
                printErrorCodes(atHandle);
            }
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCodeOrQos;
}

// Unsubscribe from an MQTT topic.
int32_t uCellMqttUnsubscribe(int32_t cellHandle,
                             const char *pTopicFilterStr)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;
    volatile uCellMqttUrcStatus_t *pUrcStatus;
    uAtClientHandle_t atHandle;
    int32_t status = 1;
    int64_t stopTimeMs;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
        pUrcStatus = &(pContext->urcStatus);
        if ((pTopicFilterStr != NULL) &&
            (strlen(pTopicFilterStr) <= U_CELL_MQTT_WRITE_TOPIC_MAX_LENGTH_BYTES)) {
            errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
            atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
            pUrcStatus->flagsBitmap = 0;
            uAtClientCommandStart(atHandle, "AT+UMQTTC=");
            // Unsubscribe from a topic
            uAtClientWriteInt(atHandle, 5);
            // Topic
            uAtClientWriteString(atHandle, pTopicFilterStr, true);
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
                uAtClientCommandStop(atHandle);
                uAtClientResponseStart(atHandle, "+UMQTTC:");
                // Skip the first parameter, which is just
                // our UMQTTC command number again
                uAtClientSkipParameters(atHandle, 1);
                status = uAtClientReadInt(atHandle);
                uAtClientResponseStop(atHandle);
            } else {
                uAtClientCommandStopReadResponse(atHandle);
            }

            if ((uAtClientUnlock(atHandle) == 0) && (status == 1)) {
                // On all platforms need to wait for a URC to
                // say that the subscribe has succeeded
                errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
                stopTimeMs = uPortGetTickTimeMs() +
                             (U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS * 1000);
                while (((pUrcStatus->flagsBitmap & (1 << U_CELL_MQTT_URC_FLAG_UNSUBSCRIBE_UPDATED)) == 0) &&
                       (uPortGetTickTimeMs() < stopTimeMs) &&
                       ((pContext->pKeepGoingCallback == NULL) ||
                        pContext->pKeepGoingCallback())) {
                    uPortTaskBlock(1000);
                }
                if ((pUrcStatus->flagsBitmap & (1 << U_CELL_MQTT_URC_FLAG_UNSUBSCRIBE_SUCCESS)) != 0) {
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                } else {
                    printErrorCodes(atHandle);
                }
            } else {
                printErrorCodes(atHandle);
            }
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

// Set a callback to be called when new messages arrive.
int32_t uCellMqttSetMessageCallback(int32_t cellHandle,
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
int32_t uCellMqttGetUnread(int32_t cellHandle)
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

// Read an MQTT message.
int32_t uCellMqttMessageRead(int32_t cellHandle, char *pTopicNameStr,
                             size_t topicNameSizeBytes,
                             char *pMessage, size_t *pMessageSizeBytes,
                             uCellMqttQos_t *pQos)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    volatile uCellMqttContext_t *pContext;
    volatile uCellMqttUrcMessage_t *pUrcMessage = NULL;
    uAtClientHandle_t atHandle;
    size_t messageSizeBytes = 0;
    int32_t status;
    int64_t stopTimeMs;
    uCellMqttQos_t qos;
    int32_t topicNameBytesRead;
    int32_t messageBytesAvailable;
    int32_t messageBytesRead = 0;
    int32_t topicBytesAvailable;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pTopicNameStr != NULL) &&
            ((pMessageSizeBytes != NULL) || (pMessage == NULL))) {
            pContext = (volatile uCellMqttContext_t *) pInstance->pMqttContext;
            pUrcMessage = pContext->pUrcMessage;
            if (pMessageSizeBytes != NULL) {
                messageSizeBytes = *pMessageSizeBytes;
            }
            errorCode = (int32_t) U_ERROR_COMMON_DEVICE_ERROR;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
                assert (pUrcMessage != NULL);
                // For the old-style SARA-R4 interface we need a URC capture
                assert(U_CELL_PRIVATE_MODULE_IS_SARA_R4(pInstance->pModule->moduleType));
                pUrcMessage->messageRead = false;
                pUrcMessage->pTopicNameStr = pTopicNameStr;
                pUrcMessage->topicNameSizeBytes = (int32_t) topicNameSizeBytes;
                pUrcMessage->pMessage = pMessage;
                pUrcMessage->messageSizeBytes = (int32_t) messageSizeBytes;
            }
            atHandle = pInstance->atHandle;
            uAtClientLock(atHandle);
            uAtClientCommandStart(atHandle, "AT+UMQTTC=");
            // Read a message
            uAtClientWriteInt(atHandle, 6);
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_MQTT_SARA_R4_OLD_SYNTAX)) {
                // We get a standard indication
                // of success here then we need
                // to wait for a URC to get the
                // message
                uAtClientCommandStop(atHandle);
                uAtClientResponseStart(atHandle, "+UMQTTC:");
                // Skip the first parameter, which is just
                // our UMQTTC command number again
                uAtClientSkipParameters(atHandle, 1);
                status = uAtClientReadInt(atHandle);
                uAtClientResponseStop(atHandle);
                if ((uAtClientUnlock(atHandle) == 0) && (status == 1)) {
                    // Wait for a URC containing the message
                    errorCode = (int32_t) U_ERROR_COMMON_TIMEOUT;
                    stopTimeMs = uPortGetTickTimeMs() +
                                 (U_MQTT_CLIENT_RESPONSE_WAIT_SECONDS * 1000);
                    while (!pUrcMessage->messageRead &&
                           (uPortGetTickTimeMs() < stopTimeMs) &&
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
                        printErrorCodes(atHandle);
                    }
                }
            } else {
                // We want just the one message
                uAtClientWriteInt(atHandle, 1);
                uAtClientCommandStop(atHandle);
                uAtClientResponseStart(atHandle, "+UMQTTC:");
                // The message now arrives directly
                // Skip the first parameter, which is just
                // our UMQTTC command number again
                uAtClientSkipParameters(atHandle, 1);
                // Next comes the QoS
                qos = (uCellMqttQos_t) uAtClientReadInt(atHandle);
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
                        //lint -e(568) Suppress value never being negative,
                        // I feel safer checking
                        ((int32_t) qos >= 0) &&
                        (qos < U_CELL_MQTT_QOS_MAX_NUM)) {
                        // Good.  Topic and message have
                        // already been done above,
                        // now fill in the other bits
                        if (pMessageSizeBytes != NULL) {
                            *pMessageSizeBytes = messageBytesRead;
                        }
                        if (pQos != NULL) {
                            *pQos = qos;
                        }
                        if (pContext->numUnreadMessages > 0) {
                            pContext->numUnreadMessages--;
                        }
                        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    }
                } else {
                    printErrorCodes(atHandle);
                }
            }
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

// Get the last MQTT error code.
int32_t uCellMqttGetLastErrorCode(int32_t cellHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;
    uAtClientHandle_t atHandle;
    int32_t x;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, true);

    if ((errorCode == 0) && (pInstance != NULL)) {
        atHandle = pInstance->atHandle;
        uAtClientLock(atHandle);
        uAtClientCommandStart(atHandle, "AT+UMQTTER");
        uAtClientCommandStop(atHandle);
        uAtClientResponseStart(atHandle, "+UMQTTER:");
        // Skip the first error code, which is a generic thing
        uAtClientSkipParameters(atHandle, 1);
        x = uAtClientReadInt(atHandle);
        uAtClientResponseStop(atHandle);
        errorCode = uAtClientUnlock(atHandle);
        if (errorCode == 0) {
            errorCode = x;
        }
    }

    U_CELL_MQTT_EXIT_FUNCTION();

    return errorCode;
}

// Determine if MQTT is supported by the given cellHandle.
bool uCellMqttIsSupported(int32_t cellHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance = NULL;

    U_CELL_MQTT_ENTRY_FUNCTION(cellHandle, &pInstance, &errorCode, false);
    U_CELL_MQTT_EXIT_FUNCTION();

    return (errorCode == 0);
}

// End of file
