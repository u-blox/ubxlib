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

#ifndef _U_GNSS_MSG_H_
#define _U_GNSS_MSG_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_ringbuffer.h"

/** \addtogroup _GNSS
 *  @{
 */

/** @file
 * @brief This header file defines the generic message handle
 * functions of the GNSS API.
 *
 * IMPORTANT: the functions in this API currently only work for GNSS
 * chips directly connected to this MCU (e.g. via I2C or UART), they
 * do NOT work for GNSS chips connected via an intermediate
 * [e.g. cellular] module.  To send and receive messages to a GNSS
 * module connected via an intermediate module please use
 * uGnssUtilUbxTransparentSendReceive().
 *
 * It is planned, in future, to make transport via an intermediate
 * cellular module work in the same way as the UART and I2C streaming
 * interfaces (by implementing support for 3GPP TS 27.010 +CMUX in this
 * code), at which point this function will be deprecated.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_GNSS_MSG_RING_BUFFER_LENGTH_BYTES
/** The size of the ring buffer that is used to hold messages
 * streamed (e.g. over I2C or UART) from the GNSS chip.  Should
 * be big enough to hold a few long messages from the device
 * while these are read asynchronously in task-space by the
 * application.
 */
# define U_GNSS_MSG_RING_BUFFER_LENGTH_BYTES 2048
#endif

#ifndef U_GNSS_MSG_TEMPORARY_BUFFER_LENGTH_BYTES
/** A temporary buffer, used as a staging post to
 * get stuff from a streaming source (e.g. I2C or UART)
 * into the ring buffer; must be less than
 * U_GNSS_MSG_RING_BUFFER_LENGTH_BYTES - 1 but, since this
 * is just a "chunking" temporary buffer, a rather smaller
 * value is usually a good idea anyway.
 */
# define U_GNSS_MSG_TEMPORARY_BUFFER_LENGTH_BYTES (U_GNSS_MSG_RING_BUFFER_LENGTH_BYTES / 8)
#endif

#ifndef U_GNSS_MSG_RECEIVER_MAX_NUM
/** The maximum number of receivers that can be listening to the
 * message stream from the GNSS chip at any one time.
 */
# define U_GNSS_MSG_RECEIVER_MAX_NUM 10
#endif

#ifndef U_GNSS_MSG_RECEIVE_TASK_STACK_SIZE_BYTES
/** The number of bytes of stack to allocate to the task started
 * by uGnssMsgReceiveStart(), the context in which the
 * callback is running.  This should really be smaller, less
 * than 2048 bytes, however the Zephyr platform on NRF52/53
 * occasionally spits out error messages when I2C errors occur,
 * which take up large amounts of stack, potentially crashing the
 * callback task, hence it is made larger for Zephyr.
 */
# define U_GNSS_MSG_RECEIVE_TASK_STACK_SIZE_BYTES (1024 * 3)
#endif

#ifndef U_GNSS_MSG_RECEIVE_TASK_QUEUE_LENGTH
/** The length of the queue controlling the message receive
 * task: just need the one.
 */
# define U_GNSS_MSG_RECEIVE_TASK_QUEUE_LENGTH 1
#endif

#ifndef U_GNSS_MSG_RECEIVE_TASK_QUEUE_ITEM_SIZE_BYTES
/** The size of each item in the queue controlling the message
 * receive task: just need the one to make it exit.
 */
# define U_GNSS_MSG_RECEIVE_TASK_QUEUE_ITEM_SIZE_BYTES 1
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** A callback which will be called by uGnssMsgReceiveStart()
 * when a matching message has been received from the GNSS chip.
 * This callback should be executed as quickly as possible to
 * avoid data loss.  The ONLY GNSS API calls that pCallback may make
 * are uGnssMsgReceiveCallbackRead() / uGnssMsgReceiveCallbackExtract(),
 * no others or you risk getting* mutex-locked.
 * If you are checking for a specific UBX-format message (i.e. no
 * wild-cards) and a NACK is received for that message then
 * errorCodeOrLength will be set to #U_GNSS_ERROR_NACK and there
 * will be no message to read, otherwise errorCodeOrLength will
 * indicate the length of the message.
 * A simple construction might be to have set the pCallbackParam
 * when you called uGnssMsgReceiveStart() to the address of your
 * buffer and then the callback might be:
 *
 * ```
 * void myCallback(uDeviceHandle_t gnssHandle,
 *                 const uGnssMessageId_t *pMessageId,
 *                 int32_t errorCodeOrLength,
 *                 void *pCallbackParam)
 * {
 *     (void) pMessageId;
 *     if (errorCodeOrLength > 0) {
 *         if (errorCodeOrLength > MY_MESSAGE_BUFFER_SIZE) {
 *             errorCodeOrLength = MY_MESSAGE_BUFFER_SIZE;
 *         }
 *         uGnssMsgReceiveCallbackRead(gnssHandle,
 *                                     (char *) pCallbackParam,
 *                                     errorCodeOrLength);
 *     }
 * }
 * ```
 *
 * @param gnssHandle             the handle of the GNSS instance.
 * @param[out] pMessageId        a pointer to the message ID that was
 *                               detected.
 * @param errorCodeOrLength      the size of the message or, if
 *                               pMessageId specifies a particular
 *                               UBX-format message (i.e. no wild-cards)
 *                               and a NACK was received for that
 *                               message, then #U_GNSS_ERROR_NACK
 *                               will be returned (and there will
 *                               be no message to read).
 * @param[in,out] pCallbackParam the callback parameter that was originally
 *                               given to uGnssMsgReceiveStart().
 */
typedef void (*uGnssMsgReceiveCallback_t)(uDeviceHandle_t gnssHandle,
                                          const uGnssMessageId_t *pMessageId,
                                          int32_t errorCodeOrLength,
                                          void *pCallbackParam);

/* ----------------------------------------------------------------
 * FUNCTIONS: MISC
 * -------------------------------------------------------------- */

/** Determine if a message ID is a wanted one.  For instance if
 * pMessageIdWanted is NULL or has the protocol type #U_GNSS_PROTOCOL_ALL
 * then true will always be returned, pMessageIdWanted has the
 * protocol type #U_GNSS_PROTOCOL_NMEA and an empty pNmea then
 * all NMEA message IDs will match, if pNmea contains "G" then
 * "GX" and "GAZZN" would match, if pMessageIdWanted has the
 * protocol type #U_GNSS_PROTOCOL_UBX and the message class
 * (upper byte) set to #U_GNSS_UBX_MESSAGE_CLASS_ALL then all UBX
 * format messages of that class will match, etc.
 *
 * @param[in] pMessageId       the message ID to check.
 * @param[in] pMessageIdWanted the wanted message ID.
 *                             pMessageIdWanted, else false.
 * @return                     true if pMessageId is inside
 */
bool uGnssMsgIdIsWanted(uGnssMessageId_t *pMessageId,
                        uGnssMessageId_t *pMessageIdWanted);

/* ----------------------------------------------------------------
 * FUNCTIONS: SEND/RECEIVE
 * -------------------------------------------------------------- */

/** Flush the receive buffer used by uGnssMsgReceive(); call this before
 * calling uGnssMsgSend() followed by uGnssMsgReceive() if you want to
 * be sure that the message you receive is a consequence of what you
 * sent, rather than a message that was already in the buffer.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @param asyncAlso   if this is set to true then the buffers used
 *                    for the handles returned by
 *                    uGnssMsgReceiveStart() are also flushed.
 */
void uGnssMsgReceiveFlush(uDeviceHandle_t gnssHandle, bool asyncAlso);

/** Send a message of your choosing to the GNSS chip. You must
 * encode the message correctly (e.g. using the encode/decode
 * functions of the UBX protocol API if you are using UBX format).
 * If you expect a response you may follow this function with
 * a call to uGnssMsgReceive() containing the ID of the message that
 * you expect back.  If you use a wildcard in that message ID, or that
 * message ID is a very commonly seen one, you may wish to call
 * uGnssMsgReceiveFlush(), BEFORE calling uGnssMsgSend(), to be sure
 * that the message that you receive is a response to what you have sent,
 * rather than an as-yet-unprocessed message already in the buffer.
 * In other words, your code may look something like this:
 *
 * ```
 * char messageSend[MY_MESSAGE_BUFFER_SIZE];
 * size_t messageSendSize;
 * uGnssMessageId_t responseMessageId;
 * char *pMsgReceive = NULL;
 *
 * // Populate messageSend and set messageSendSize here
 * // Populate responseMessageId with the expected response here
 * // gnssHandle is assumed to have already been set up
 *
 * uGnssMsgReceiveFlush(gnssHandle, false);
 * if (uGnssMsgSend(gnssHandle, &messageSend,
 *                  messageSendSize) == messageSendSize) {
 *     int32_t messageReceiveSize = uGnssMsgReceive(gnssHandle,
 *                                                  &responseMessageId,
 *                                                  &pMsgReceive, 0,
 *                                                  1000, NULL);
 *     if (messageReceiveSize >= 0) {
 *
 *         // Process the received message here
 *
 *         uPortFree(pMsgReceive);
 *    }
 * }
 * ```
 *
 * IMPORTANT: this currently only works for GNSS chips directly
 * connected to this MCU, it does NOT work for GNSS chips connected
 * via an intermediate [e.g. cellular] module.  To send
 * messages to a GNSS module connected via an intermediate
 * module please use uGnssUtilUbxTransparentSendReceive().
 *
 * @param gnssHandle      the handle of the GNSS instance.
 * @param[in] pBuffer     the message to send; cannot be NULL.
 * @param size            the amount of data at pBuffer.
 * @return                on success the number of bytes sent, else
 *                        negative error code.
 */
int32_t uGnssMsgSend(uDeviceHandle_t gnssHandle,
                     const char *pBuffer, size_t size);

/** Monitor the output of the GNSS chip for a given message, blocking
 * (see uGnssMsgReceiveStart() for a non-blocking version).
 *
 * Note that for UBX-format messages, which may be quite long, the
 * checksum on the message is NOT checked: if you are interested
 * in the message you may confirm that it is coherent by calling
 * uGnssMsgIsGood().
 *
 * Note: if the message ID is set to a particular UBX-format message (i.e.
 * no wild-cards) and a NACK is received for that message then the
 * error code #U_GNSS_ERROR_NACK will be returned.
 *
 * This function does not pass back the message ID it has decoded;
 * if you used a wildcard in pMessageId and you don't want to decode
 * the message ID from the message yourself (e.g. in the case of a
 * UBX protocol message by using uUbxProtocolDecode()), then you
 * could instead use uGnssMsgReceiveStart(), which does pass back
 * the decoded message ID to the pCallback.
 *
 * IMPORTANT: this currently only works for GNSS chips directly
 * connected to this MCU, it does NOT work for GNSS chips connected
 * via an intermediate [e.g. cellular] module.
 *
 * @param gnssHandle             the handle of the GNSS instance.
 * @param[in] pMessageId         the message ID to capture.  If
 *                               the message ID is a wildcard then
 *                               this function will return on the
 *                               first matching message ID; if
 *                               you want to wait for multiple
 *                               messages use the asynchronous
 *                               uGnssMsgReceiveStart()
 *                               mechanism instead.  Cannot be NULL.
 * @param[in,out] ppBuffer       a pointer to a pointer to a buffer
 *                               in which the message will be placed,
 *                               cannot be NULL.  If ppBuffer points
 *                               to NULL (i.e *ppBuffer is NULL) then
 *                               this function will allocate a buffer
 *                               of the correct size and populate
 *                               *ppBuffer with the allocated buffer
 *                               pointer; in this case IT IS UP TO
 *                               THE CALLER TO uPortFree(*ppBuffer) WHEN
 *                               DONE.  The entire message, with
 *                               any header, $, CRC, etc. included,
 *                               will be written to the buffer.
 * @param size                   the amount of storage at *ppBuffer,
 *                               zero if ppBuffer points to NULL.
 * @param timeoutMs              how long to wait for the [first]
 *                               message to arrive in milliseconds.
 * @param[in] pKeepGoingCallback a function that will be called
 *                               while waiting.  As long as
 *                               pKeepGoingCallback returns true this
 *                               function will continue to wait until
 *                               a matching message has arrived or
 *                               timeoutSeconds have elapsed. If
 *                               pKeepGoingCallback returns false
 *                               then this function will return.
 *                               pKeepGoingCallback can also be used to
 *                               feed any application watchdog timer that
 *                               might be running.  May be NULL, in
 *                               which case this function will wait
 *                               until the [first] message has arrived
 *                               or timeoutSeconds have elapsed.
 * @return                       the number of bytes copied into
 *                               *ppBuffer else negative error code.
 */
int32_t uGnssMsgReceive(uDeviceHandle_t gnssHandle,
                        const uGnssMessageId_t *pMessageId,
                        char **ppBuffer, size_t size,
                        int32_t timeoutMs,
                        bool (*pKeepGoingCallback)(uDeviceHandle_t gnssHandle));

/** Monitor the output of the GNSS chip for the given message,
 * non-blocking (see uGnssMsgReceive() for the blocking version).
 * This may be called multiple times; to stop listening for a given
 * message type, call uGnssMsgReceiveStop() with the handle returned by
 * this function, or alternatively call uGnssMsgReceiveStopAll()
 * to stop them all and free memory.  There can be a maximum of
 * #U_GNSS_MSG_RECEIVER_MAX_NUM of these running at any one time.
 * Message handlers callbacks are called mostly-recently-added first.
 *
 * IMPORTANT: this currently only works for GNSS chips directly
 * connected to this MCU, it does NOT work for GNSS chips connected
 * via an intermediate [e.g. cellular] module.
 *
 * @param gnssHandle             the handle of the GNSS instance.
 * @param[in] pMessageId         a pointer to the message ID to capture;
 *                               a copy will be taken so this may be
 *                               on the stack; cannot be NULL.
 * @param[in] pCallback          the callback to be called when a
 *                               matching message arrives.  It is up to
 *                               pCallback to read the message with a
 *                               call to uGnssMsgReceiveCallbackRead();
 *                               this should be done as quickly as possible
 *                               so that the callback can return as quickly
 *                               as possible, otherwise there is a chance
 *                               of data loss as the internal buffer fills
 *                               up. The entire message, with any header, $,
 *                               checksum, etc. will be included.
 *                               IMPORTANT: the ONLY GNSS API calls that
 *                               pCallback may make are
 *                               uGnssMsgReceiveCallbackRead() and
 *                               uGnssMsgIsGood(), no others or you risk
 *                               getting mutex-locked. pCallback is run in
 *                               the context of a task with a stack of size
 *                               #U_GNSS_MSG_RECEIVE_TASK_STACK_SIZE_BYTES;
 *                               you may you may call uGnssMsgReceiveStackMinFree()
 *                               just before calling uGnssMsgReceiveStop()
 *                               to check if the remaining stack margin was
 *                               big enough.  pCallback cannot be NULL.
 * @param[in] pCallbackParam     will be passed to pCallback as its last
 *                               parameter.
 * @return                       a handle for this asynchronous reader on
 *                               success, else negative error code.
 */
int32_t uGnssMsgReceiveStart(uDeviceHandle_t gnssHandle,
                             const uGnssMessageId_t *pMessageId,
                             uGnssMsgReceiveCallback_t pCallback,
                             void *pCallbackParam);

/** To be called from the pCallback of uGnssMsgReceiveStart() to take
 * a peek at the message data from the internal ring buffer, copying it
 * into your buffer but NOT REMOVING IT from the internal ring buffer,
 * so that it is still there to be passed to any other of your pCallbacks.
 * This is the function you would normally use; if you have a long message
 * of specific interest you may wish to use uGnssMsgReceiveCallbackExtract()
 * instead.
 *
 * IMPORTANT: this function can ONLY be called from the message receive
 * pCallback, it is NOT thread-safe to call it from anywhere else.
 *
 * @param gnssHandle   the handle of the GNSS instance.
 * @param[out] pBuffer a place to put the message; cannot be NULL.
 * @param size         the amount of storage at pBuffer, should be
 *                     the size of the message, as indicated by the
 *                     pCallback "size" parameter.
 * @return             on success the number of bytes read,
 *                     else negative error code.
 */
int32_t uGnssMsgReceiveCallbackRead(uDeviceHandle_t gnssHandle,
                                    char *pBuffer, size_t size);

/** To be called from the pCallback of uGnssMsgReceiveStart()
 * to REMOVE a message from the internal ring buffer into your buffer;
 * once this is called the message will not be available to any of your
 * other pCallbacks.  Use this if the message you wish to read is very
 * large, larger than this codes' internal ring buffer
 * (#U_GNSS_MSG_RING_BUFFER_LENGTH_BYTES) and so you need to read
 * it directly in this way, in other words when the internal buffer simply
 * isn't big enough to store the msssage and pass it around; normally
 * you would use uGnssMsgReceiveCallbackRead().
 *
 * IMPORTANT: this function can ONLY be called from the message
 * receive pCallback, it is NOT thread-safe to call it from anywhere else.
 *
 * @param gnssHandle   the handle of the GNSS instance.
 * @param[out] pBuffer a place to put the message; cannot be NULL.
 * @param size         the amount of storage at pBuffer, should be
 *                     the size of the message, as indicated by the
 *                     pCallback "size" parameter.
 * @return             on success the number of bytes read,
 *                     else negative error code.
 */
int32_t uGnssMsgReceiveCallbackExtract(uDeviceHandle_t gnssHandle,
                                       char *pBuffer, size_t size);

/** Stop monitoring the output of the GNSS chip for a message.
 * Once this function returns the pCallback function passed to the
 * associated uGnssMsgReceiveStart() will no longer be called.
 *
 * @param gnssHandle   the handle of the GNSS instance.
 * @param asyncHandle  the handle originally returned by
 *                     uGnssMsgReceiveStart().
 * @return             zero on success else negative error code.
 */
int32_t uGnssMsgReceiveStop(uDeviceHandle_t gnssHandle,
                            int32_t asyncHandle);

/** Stop monitoring all uGnssMsgReceiveStart() instances and free
 * memory.
 *
 * @param gnssHandle   the handle of the GNSS instance.
 * @return             zero on success else negative error code.
 */
int32_t uGnssMsgReceiveStopAll(uDeviceHandle_t gnssHandle);

/** Return the minimum number of bytes of stack free in the task
 * that is running the message receive.  Will return a valid
 * number only if at least one uGnssMsgReceiveStart() is running.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @return            the minimum amount of free stack for the task
 *                    running the current asynchronous message
 *                    receive, else negative error code.
 */
int32_t uGnssMsgReceiveStackMinFree(uDeviceHandle_t gnssHandle);

/** Check if any message data bytes from a streaming source (for
 * example I2C or UART) have been lost to the non-blocking message
 * receive handler as a result of it not keeping up with the data flow
 * out of the ring buffer; if this returns non-zero then you may be doing
 * too much in your callback or you may have too many callbacks active.
 * See also uGnssMsgReceiveStatStreamLoss() for a count of the bytes lost at
 * the other end of the ring-buffer (more serious as that is a system-wide
 * loss of messages to all destinations).
 *
 * @param gnssHandle   the handle of the GNSS instance.
 * @return             the number of bytes lost.
 */
size_t uGnssMsgReceiveStatReadLoss(uDeviceHandle_t gnssHandle);

/** Check the number of bytes lost between a streaming source (for
 * instance I2C or UART) and the input of the ring buffer as a result
 * of the ring buffer not being emptied fast enough.  This is a more
 * serious loss than uGnssMsgReceiveStatReadLoss() since the data is
 * lost to all destinations.
 *
 * @param gnssHandle   the handle of the GNSS instance.
 * @return             the number of bytes lost.
 */
size_t uGnssMsgReceiveStatStreamLoss(uDeviceHandle_t gnssHandle);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_GNSS_MSG_H_

// End of file
