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
 * @brief This source file contains the public message handling functions
 * of the GNSS API.
 *
 * Architectural note: the way message flow works with a streamed connection
 * to a GNSS chip as follows:
 *
 *           reader    <--|
 *           reader    <--|-- ring-buffer <-- source (e.g. UART/I2C)
 *           reader    <--|
 *
 * There is a single ring-buffer for any GNSS device which is populated by
 * ubxlib from the streaming transport (e.g. UART or I2C).  There can be multiple
 * readers of that ring-buffer, currently three: the blocking and non-blocking
 * message readers here and one for messages that ubxlib itself is interested in.
 *
 * When a reader is actively doing something (i.e. reading or parsing a
 * message), it locks its read pointer in the ring-buffer; this means that
 * data can still be brought in from the streaming source, if there's room,
 * but only if there's room while *respecting* such locked read pointers.
 * Each read pointer is independent, so the different readers can absorb
 * data at different rates, and discard things they aren't interested, without
 * affecting the others.
 *
 * When a reader is not actively interested in reading stuff from the
 * ring-buffer, its read pointer is left unlocked, which means that it can
 * be pushed by the pressure of data read from the source, effectively
 * losing data to that reader; this is fine, the reader said it wasn't
 * interested.
 *
 * Using a ring-buffer in this way also means that a reader is able to pull as
 * much data as it wishes from the GNSS chip without being limited by the size
 * of the ring-buffer chosen at compile-time for ubxlib, provided of course
 * another reader hasn't left its read pointer locked.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()

#include "u_cfg_os_platform_specific.h" // U_CFG_OS_YIELD_MS
#include "u_cfg_sw.h"
#include "u_error_common.h"

#include "u_port.h"
#include "u_port_heap.h"
#include "u_port_os.h"  // Required by u_gnss_private.h
#include "u_port_debug.h"
#include "u_port_uart.h"
#include "u_port_i2c.h"

#include "u_at_client.h"

#include "u_ubx_protocol.h"

#include "u_hex_bin_convert.h"

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss.h"
#include "u_gnss_private.h"
#include "u_gnss_msg.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_GNSS_MSG_READ_TIMEOUT_MS
/** Timeout when reading a message from the GNSS chip from a streamed
 * source (e.g. I2C or UART) in milliseconds.
 */
# define U_GNSS_MSG_READ_TIMEOUT_MS 2000
#endif

#ifndef U_GNSS_MSG_RECEIVE_TASK_PRIORITY
/** The priority that the GNSS asynchronous message receive task runs at;
 * intended to be the same as URC/callbackey stuff over in the cellular/
 * short-range world.
 */
# define U_GNSS_MSG_RECEIVE_TASK_PRIORITY (U_CFG_OS_PRIORITY_MAX - 5)
#endif

#ifndef U_GNSS_MSG_TASK_STACK_YIELD_TIME_MS
/** How long the asynchronous message receive task guarantees to give
 * to the rest of the system; if this is made larger the asynchronous
 * receive task won't be able to service the input stream so often
 * and hence the UART/I2C transport may overflow.
 */
# define U_GNSS_MSG_TASK_STACK_YIELD_TIME_MS 50
#endif

#if U_GNSS_MSG_TASK_STACK_YIELD_TIME_MS < U_CFG_OS_YIELD_MS
/* U_GNSS_MSG_TASK_STACK_YIELD_TIME_MS must be at least as big as U_CFG_OS_YIELD_MS
 * or the asynchronous message receive task will be all-consuming.
 */
# error U_GNSS_MSG_TASK_STACK_YIELD_TIME_MS must be at least as big as U_CFG_OS_YIELD_MS
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Task that runs the non-blocking message receive.
static void msgReceiveTask(void *pParam)
{
    uGnssPrivateInstance_t *pInstance = (uGnssPrivateInstance_t *) pParam;
    char queueItem[U_GNSS_MSG_RECEIVE_TASK_QUEUE_ITEM_SIZE_BYTES];
    uGnssPrivateMsgReceive_t *pMsgReceive = pInstance->pMsgReceive;
    uGnssPrivateMsgReader_t *pReader;
    int32_t errorCodeOrLength = (int32_t) U_ERROR_COMMON_UNKNOWN;
    int32_t receiveSize;
    int32_t yieldTimeMs;
    size_t discardSize = 0;
    uGnssMessageId_t messageId;
    uGnssPrivateMessageId_t privateMessageId;
    char nmeaId[U_GNSS_NMEA_MESSAGE_MATCH_LENGTH_CHARACTERS + 1];

    U_PORT_MUTEX_LOCK(pMsgReceive->taskRunningMutexHandle);

    // Lock our ring buffer read handle; now we just have to keep up...
    uRingBufferLockReadHandle(&(pInstance->ringBuffer),
                              pMsgReceive->ringBufferReadHandle);

    // Continue until we receive something on the queue, which
    // will cause us to exit
    while (uPortQueueTryReceive(pMsgReceive->taskExitQueueHandle, 0, queueItem) < 0) {

        // Note that this does NOT lock gUGnssPrivateMutex: it doesn't need to,
        // provided this task is brought up and torn down in an organised way

        // Pull stuff into the ring buffer
        receiveSize = uGnssPrivateStreamFillRingBuffer(pInstance, 0, 0);
        // Deal with any discard from a previous run around this loop
        discardSize -= uRingBufferReadHandle(&(pInstance->ringBuffer),
                                             pMsgReceive->ringBufferReadHandle,
                                             NULL, discardSize);
        errorCodeOrLength = 0;
        if (discardSize == 0) {
            errorCodeOrLength = uRingBufferDataSizeHandle(&(pInstance->ringBuffer),
                                                          pMsgReceive->ringBufferReadHandle);
            // Run around a loop processing the data from the ring buffer
            // for as long as we're still finding messages in it
            while (errorCodeOrLength > 0) {
                privateMessageId.type = U_GNSS_PROTOCOL_ALL;
                // Attempt to decode a message of any type from the ring buffer
                errorCodeOrLength = uGnssPrivateStreamDecodeRingBuffer(&(pInstance->ringBuffer),
                                                                       pMsgReceive->ringBufferReadHandle,
                                                                       &privateMessageId);
                if ((errorCodeOrLength > 0) || (errorCodeOrLength == (int32_t) U_GNSS_ERROR_NACK)) {
                    // Remember how long the message is
                    pMsgReceive->msgBytesLeftToRead = 0;
                    if (errorCodeOrLength > 0) {
                        pMsgReceive->msgBytesLeftToRead = errorCodeOrLength;
                    }

                    if (uGnssPrivateMessageIdToPublic(&privateMessageId, &messageId, nmeaId) == 0) {
                        // Got something, with a message ID now in public form;
                        // go through the list of readers looking for those interested

                        U_PORT_MUTEX_LOCK(pMsgReceive->readerMutexHandle);

                        pReader = pMsgReceive->pReaderList;
                        while (pReader != NULL) {
                            if (uGnssPrivateMessageIdIsWanted(&privateMessageId,
                                                              &(pReader->privateMessageId))) {
                                // This reader is interested, call the callback
                                ((uGnssMsgReceiveCallback_t) pReader->pCallback)(pInstance->gnssHandle,
                                                                                 &messageId,
                                                                                 errorCodeOrLength,
                                                                                 pReader->pCallbackParam);
                            }
                            // Next!
                            pReader = pReader->pNext;
                        }

                        U_PORT_MUTEX_UNLOCK(pMsgReceive->readerMutexHandle);
                    }

                    // Clear out any remaining data
                    uRingBufferReadHandle(&(pInstance->ringBuffer),
                                          pMsgReceive->ringBufferReadHandle, NULL,
                                          pMsgReceive->msgBytesLeftToRead);
                }
            }
        }

        // Relax to let others in; relax for twice as long if we last
        // received nothing and aren't desperately seeking more data,
        // in order to allow some data to build up
        yieldTimeMs = U_GNSS_MSG_TASK_STACK_YIELD_TIME_MS;
        if ((receiveSize == 0) && (errorCodeOrLength != (int32_t) U_ERROR_COMMON_TIMEOUT))  {
            yieldTimeMs *= 2;
        }
        uPortTaskBlock(yieldTimeMs);
    }

    // Now we can unlock our ring buffer read handle.  Phew.
    uRingBufferUnlockReadHandle(&(pInstance->ringBuffer),
                                pMsgReceive->ringBufferReadHandle);

    U_PORT_MUTEX_UNLOCK(pMsgReceive->taskRunningMutexHandle);

    // Delete ourself
    uPortTaskDelete(NULL);
}

// Read a message from the ring buffer into a user's buffer.
int32_t msgReceiveCallbackRead(uDeviceHandle_t gnssHandle,
                               char *pBuffer, size_t size,
                               bool andRemove)
{
    int32_t errorCodeOrLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uGnssPrivateMsgReceive_t *pMsgReceive;
    uGnssPrivateInstance_t *pInstance;

    pInstance = pUGnssPrivateGetInstance(gnssHandle);
    if ((pInstance != NULL) && (pBuffer != NULL)) {
        errorCodeOrLength = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
        pMsgReceive = pInstance->pMsgReceive;
        if ((pMsgReceive != NULL) &&
            uPortTaskIsThis(pMsgReceive->taskHandle)) {
            if (size > pMsgReceive->msgBytesLeftToRead) {
                size = pMsgReceive->msgBytesLeftToRead;
            }
            if (andRemove) {
                errorCodeOrLength = uGnssPrivateStreamReadRingBuffer(pInstance,
                                                                     pMsgReceive->ringBufferReadHandle,
                                                                     pBuffer, size,
                                                                     U_GNSS_MSG_READ_TIMEOUT_MS);
                if (errorCodeOrLength > 0) {
                    pMsgReceive->msgBytesLeftToRead -= errorCodeOrLength;
                }
            } else {
                errorCodeOrLength = uGnssPrivateStreamPeekRingBuffer(pInstance,
                                                                     pMsgReceive->ringBufferReadHandle,
                                                                     pBuffer, size, 0,
                                                                     U_GNSS_MSG_READ_TIMEOUT_MS);
            }
        }
    }

    return errorCodeOrLength;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: MISC
 * -------------------------------------------------------------- */

// Return true if the given message ID is wanted.
bool uGnssMsgIdIsWanted(uGnssMessageId_t *pMessageId,
                        uGnssMessageId_t *pMessageIdWanted)
{
    bool isWanted = false;

    if (pMessageIdWanted->type == U_GNSS_PROTOCOL_ANY) {
        isWanted = true;
    } else if ((pMessageIdWanted->type == U_GNSS_PROTOCOL_ALL) &&
               (pMessageId->type != U_GNSS_PROTOCOL_UNKNOWN)) {
        isWanted = true;
    } else if ((pMessageIdWanted->type == U_GNSS_PROTOCOL_UNKNOWN) &&
               (pMessageId->type == U_GNSS_PROTOCOL_UNKNOWN)) {
        isWanted = true;
    } else if ((pMessageIdWanted->type == U_GNSS_PROTOCOL_RTCM) &&
               (pMessageId->type == U_GNSS_PROTOCOL_RTCM)) {
        isWanted = pMessageIdWanted->id.rtcm == pMessageId->id.rtcm;
    } else if ((pMessageIdWanted->type == U_GNSS_PROTOCOL_NMEA) &&
               (pMessageId->type == U_GNSS_PROTOCOL_NMEA)) {
        isWanted = ((pMessageIdWanted->id.pNmea == NULL) ||   // == any
                    (strstr(pMessageId->id.pNmea,
                            pMessageIdWanted->id.pNmea) == pMessageId->id.pNmea));
    } else if ((pMessageIdWanted->type == U_GNSS_PROTOCOL_UBX) &&
               (pMessageId->type == U_GNSS_PROTOCOL_UBX)) {
        isWanted = ((pMessageIdWanted->id.ubx == ((U_GNSS_UBX_MESSAGE_CLASS_ALL << 8) |
                                                  U_GNSS_UBX_MESSAGE_ID_ALL)) ||
                    (pMessageIdWanted->id.ubx == pMessageId->id.ubx));
    }

    return isWanted;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: SEND/RECEIVE
 * -------------------------------------------------------------- */

// Flush the receive buffer.
void uGnssMsgReceiveFlush(uDeviceHandle_t gnssHandle, bool asyncAlso)
{
    uGnssPrivateInstance_t *pInstance;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            // Bring any existing new data into the ring buffer first
            uGnssPrivateStreamFillRingBuffer(pInstance,
                                             U_GNSS_RING_BUFFER_MIN_FILL_TIME_MS,
                                             U_GNSS_RING_BUFFER_MAX_FILL_TIME_MS);
            // And flush...
            uRingBufferFlushHandle(&(pInstance->ringBuffer),
                                   pInstance->ringBufferReadHandleMsgReceive);
            if (asyncAlso && (pInstance->pMsgReceive != NULL)) {
                uRingBufferFlushHandle(&(pInstance->ringBuffer),
                                       pInstance->pMsgReceive->ringBufferReadHandle);
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }
}

// Send a message to the GNSS chip transparently.
int32_t uGnssMsgSend(uDeviceHandle_t gnssHandle, const char *pBuffer, size_t size)
{
    int32_t errorCodeOrLength = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    int32_t streamType;
    int32_t streamHandle = -1;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCodeOrLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if ((pInstance != NULL) && (pBuffer != NULL)) {

            errorCodeOrLength = (int32_t) U_GNSS_ERROR_TRANSPORT;
            streamType = uGnssPrivateGetStreamType(pInstance->transportType);

            U_PORT_MUTEX_LOCK(pInstance->transportMutex);

            switch (streamType) {
                case U_GNSS_PRIVATE_STREAM_TYPE_UART:
                    streamHandle = pInstance->transportHandle.uart;
                    break;
                case U_GNSS_PRIVATE_STREAM_TYPE_I2C:
                    streamHandle = pInstance->transportHandle.i2c;
                    break;
                default:
                    break;
            }

            if (streamHandle >= 0) {
                // Streaming transport
                switch (streamType) {
                    case U_GNSS_PRIVATE_STREAM_TYPE_UART:
                        errorCodeOrLength = uPortUartWrite(streamHandle,
                                                           pBuffer, size);
                        break;
                    case U_GNSS_PRIVATE_STREAM_TYPE_I2C:
                        errorCodeOrLength = uPortI2cControllerSend(streamHandle,
                                                                   pInstance->i2cAddress,
                                                                   pBuffer, size, false);
                        if (errorCodeOrLength == 0) {
                            errorCodeOrLength = (int32_t) size;
                        }
                        break;
                    default:
                        break;
                }
                if (errorCodeOrLength == size) {
                    if (pInstance->printUbxMessages) {
                        uPortLog("U_GNSS: sent message");
                        uGnssPrivatePrintBuffer(pBuffer, size);
                        uPortLog(".\n");
                    }
                }
            }

            U_PORT_MUTEX_UNLOCK(pInstance->transportMutex);
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCodeOrLength;
}

// Monitor the output of the GNSS chip for a message, blocking version.
int32_t uGnssMsgReceive(uDeviceHandle_t gnssHandle,
                        const uGnssMessageId_t *pMessageId,
                        char **ppBuffer, size_t size, int32_t timeoutMs,
                        bool (*pKeepGoingCallback)(uDeviceHandle_t gnssHandle))
{
    int32_t errorCodeOrLength = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    uGnssPrivateMessageId_t privateMessageId;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCodeOrLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if ((pInstance != NULL) && (pMessageId != NULL) &&
            (uGnssPrivateMessageIdToPrivate(pMessageId, &privateMessageId) == 0)) {
            errorCodeOrLength = uGnssPrivateReceiveStreamMessage(pInstance,
                                                                 &privateMessageId,
                                                                 pInstance->ringBufferReadHandleMsgReceive,
                                                                 ppBuffer, size,
                                                                 timeoutMs,
                                                                 pKeepGoingCallback);
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCodeOrLength;
}

// Monitor the output of the GNSS chip for a message, async version.
int32_t uGnssMsgReceiveStart(uDeviceHandle_t gnssHandle,
                             const uGnssMessageId_t *pMessageId,
                             uGnssMsgReceiveCallback_t pCallback,
                             void *pCallbackParam)
{
    int32_t errorCodeOrHandle = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    uGnssPrivateMsgReceive_t *pMsgReceive;
    uGnssPrivateMsgReader_t *pReader;
    const char *pTaskName = "gnssMsgRx";

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCodeOrHandle = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if ((pInstance != NULL) && (pMessageId != NULL) && (pCallback != NULL)) {
            errorCodeOrHandle = (int32_t) U_ERROR_COMMON_NO_MEMORY;
            pReader = (uGnssPrivateMsgReader_t *) pUPortMalloc(sizeof(uGnssPrivateMsgReader_t));
            if (pReader != NULL) {
                memset(pReader, 0, sizeof(*pReader));
                // If the message receive task is not running
                // at the moment, start it
                if (pInstance->pMsgReceive == NULL) {
                    pInstance->pMsgReceive = (uGnssPrivateMsgReceive_t *) pUPortMalloc(sizeof(
                                                                                           uGnssPrivateMsgReceive_t));
                    if (pInstance->pMsgReceive != NULL) {
                        pMsgReceive = pInstance->pMsgReceive;
                        memset(pMsgReceive, 0, sizeof(*pMsgReceive));
                        // Take a "master" read handle
                        pMsgReceive->ringBufferReadHandle = uRingBufferTakeReadHandle(&(pInstance->ringBuffer));
                        if (pMsgReceive->ringBufferReadHandle >= 0) {
                            // Allocate a temporary buffer that we can use to pull data
                            // from the streaming source into the ring-buffer from our
                            // asynchronous task
                            pMsgReceive->pTemporaryBuffer = (char *) pUPortMalloc(U_GNSS_MSG_TEMPORARY_BUFFER_LENGTH_BYTES);
                            if (pMsgReceive->pTemporaryBuffer != NULL) {
                                // Create the mutex that controls access to the linked-list of readers
                                errorCodeOrHandle = uPortMutexCreate(&(pMsgReceive->readerMutexHandle));
                                if (errorCodeOrHandle == 0) {
                                    // Create the queue that allows us to get the task to exit
                                    errorCodeOrHandle = uPortQueueCreate(U_GNSS_MSG_RECEIVE_TASK_QUEUE_LENGTH,
                                                                         U_GNSS_MSG_RECEIVE_TASK_QUEUE_ITEM_SIZE_BYTES,
                                                                         &(pMsgReceive->taskExitQueueHandle));
                                    if (errorCodeOrHandle == 0) {
                                        // Create the mutex for task running status
                                        errorCodeOrHandle = uPortMutexCreate(&(pMsgReceive->taskRunningMutexHandle));
                                        if (errorCodeOrHandle == 0) {
                                            //... and then the task
                                            errorCodeOrHandle = uPortTaskCreate(msgReceiveTask,
                                                                                pTaskName,
                                                                                U_GNSS_MSG_RECEIVE_TASK_STACK_SIZE_BYTES,
                                                                                pInstance, U_GNSS_MSG_RECEIVE_TASK_PRIORITY,
                                                                                &(pMsgReceive->taskHandle));
                                            if (errorCodeOrHandle == 0) {
                                                // Wait for the task to lock the mutex,
                                                // which shows it is running
                                                while (uPortMutexTryLock(pMsgReceive->taskRunningMutexHandle, 0) == 0) {
                                                    uPortMutexUnlock(pMsgReceive->taskRunningMutexHandle);
                                                    uPortTaskBlock(U_CFG_OS_YIELD_MS);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            if (errorCodeOrHandle != 0) {
                                // Tidy up if we couldn't get OS resources
                                if (pMsgReceive->taskHandle != NULL) {
                                    uPortTaskDelete(msgReceiveTask);
                                }
                                if (pMsgReceive->taskRunningMutexHandle != NULL) {
                                    uPortMutexDelete(pMsgReceive->taskRunningMutexHandle);
                                }
                                if (pMsgReceive->taskExitQueueHandle != NULL) {
                                    uPortQueueDelete(pMsgReceive->taskExitQueueHandle);
                                }
                                if (pMsgReceive->readerMutexHandle != NULL) {
                                    uPortMutexDelete(pMsgReceive->readerMutexHandle);
                                }
                                uPortFree(pInstance->pTemporaryBuffer);
                                uRingBufferGiveReadHandle(&(pInstance->ringBuffer),
                                                          pMsgReceive->ringBufferReadHandle);
                                uPortFree(pInstance->pMsgReceive);
                                pInstance->pMsgReceive = NULL;
                            }
                        } else {
                            // Out of handles already
                            uPortFree(pInstance->pMsgReceive);
                            pInstance->pMsgReceive = NULL;
                        }
                    }
                }
                if (pInstance->pMsgReceive == NULL) {
                    // Clean up on error
                    uPortFree(pReader);
                    pReader = NULL;
                }
            }
            if (pReader != NULL) {
                // The task etc. must be running, we have a read handle,
                // now populate the rest of the reader structure
                // and add it to the front of the list
                pReader->handle = pInstance->pMsgReceive->nextHandle;
                pInstance->pMsgReceive->nextHandle++;
                uGnssPrivateMessageIdToPrivate(pMessageId, &(pReader->privateMessageId));
                pReader->pCallback = (void *) pCallback;
                pReader->pCallbackParam = pCallbackParam;
                pReader->pNext = pInstance->pMsgReceive->pReaderList;

                U_PORT_MUTEX_LOCK(pInstance->pMsgReceive->readerMutexHandle);

                pInstance->pMsgReceive->pReaderList = pReader;

                U_PORT_MUTEX_UNLOCK(pInstance->pMsgReceive->readerMutexHandle);

                // Return the handle
                errorCodeOrHandle = pReader->handle;
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCodeOrHandle;
}

// Read a message from the ring buffer into a user's buffer.
// This function does NOT lock gUGnssPrivateMutex in order
// that it can be called from pCallback; this is fine since
// the asynchronous receive task is brought up and torn down
// in an organised way.
int32_t uGnssMsgReceiveCallbackRead(uDeviceHandle_t gnssHandle,
                                    char *pBuffer, size_t size)
{
    return msgReceiveCallbackRead(gnssHandle, pBuffer, size, false);
}

// Extract a message from the ring buffer into a user's buffer.
// This function does NOT lock gUGnssPrivateMutex in order
// that it can be called from pCallback; this is fine since
// the asynchronous receive task is brought up and torn down
// in an organised way.
int32_t uGnssMsgReceiveCallbackExtract(uDeviceHandle_t gnssHandle,
                                       char *pBuffer, size_t size)
{
    return msgReceiveCallbackRead(gnssHandle, pBuffer, size, true);
}

// Stop monitoring the output of the GNSS chip for a message.
int32_t uGnssMsgReceiveStop(uDeviceHandle_t gnssHandle, int32_t asyncHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    uGnssPrivateMsgReceive_t *pMsgReceive;
    uGnssPrivateMsgReader_t *pCurrent;
    uGnssPrivateMsgReader_t *pPrev = NULL;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (pInstance->pMsgReceive != NULL) {
                pMsgReceive = pInstance->pMsgReceive;

                U_PORT_MUTEX_LOCK(pMsgReceive->readerMutexHandle);

                // Remove the entry from the list
                errorCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
                pCurrent = pMsgReceive->pReaderList;
                while (pCurrent != NULL) {
                    if (pCurrent->handle == asyncHandle) {
                        if (pPrev != NULL) {
                            pPrev->pNext = pCurrent->pNext;
                        } else {
                            pMsgReceive->pReaderList = pCurrent->pNext;
                        }
                        uPortFree(pCurrent);
                        pCurrent = NULL;
                        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    } else {
                        pPrev = pCurrent;
                        pCurrent = pPrev->pNext;
                    }
                }

                U_PORT_MUTEX_UNLOCK(pMsgReceive->readerMutexHandle);

                if (pMsgReceive->pReaderList == NULL) {
                    // All gone, shut the task etc. down also
                    uGnssPrivateStopMsgReceive(pInstance);
                }

            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// Stop monitoring the output of the GNSS chip for all messages.
int32_t uGnssMsgReceiveStopAll(uDeviceHandle_t gnssHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            // We can just call the shut down function to lose the lot
            uGnssPrivateStopMsgReceive(pInstance);
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// Get the minimum number of bytes of stack free in the message receive task.
int32_t uGnssMsgReceiveStackMinFree(uDeviceHandle_t gnssHandle)
{
    int32_t errorCodeOrStackMinFree = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        errorCodeOrStackMinFree = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if ((pInstance != NULL) && (pInstance->pMsgReceive != NULL)) {
            errorCodeOrStackMinFree = uPortTaskStackMinFree(pInstance->pMsgReceive->taskHandle);
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCodeOrStackMinFree;
}

// Count of bytes lost for the non-blocking message receive handler.
size_t uGnssMsgReceiveStatReadLoss(uDeviceHandle_t gnssHandle)
{
    int32_t bytesLost = 0;
    uGnssPrivateInstance_t *pInstance;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if ((pInstance != NULL) && (pInstance->pMsgReceive != NULL)) {
            bytesLost = uRingBufferStatReadLossHandle(&(pInstance->ringBuffer),
                                                      pInstance->pMsgReceive->ringBufferReadHandle);
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return bytesLost;
}

// Count of bytes lost at the input of the ring buffer.
size_t uGnssMsgReceiveStatStreamLoss(uDeviceHandle_t gnssHandle)
{
    int32_t bytesLost = 0;
    uGnssPrivateInstance_t *pInstance;

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            bytesLost = uRingBufferStatAddLoss(&(pInstance->ringBuffer));
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return bytesLost;
}

// End of file
