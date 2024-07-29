/*
 * Copyright 2019-2024 u-blox
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
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_GNSS_MSG_RING_BUFFER_LENGTH_BYTES
/** The size of the ring buffer that is used to hold messages
 * streamed (e.g. over I2C or UART or SPI) from the GNSS chip.
 * Should be big enough to hold a few long messages from the device
 * while these are read asynchronously in task-space by the
 * application.
 */
# define U_GNSS_MSG_RING_BUFFER_LENGTH_BYTES 2048
#endif

#ifndef U_GNSS_MSG_TEMPORARY_BUFFER_LENGTH_BYTES
/** A temporary buffer, used as a staging post to get stuff
 * from a streaming source (e.g. I2C or UART or SPI) into the
 * ring buffer; must be less than
 * #U_GNSS_MSG_RING_BUFFER_LENGTH_BYTES - 1 but, since this
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

#ifndef U_GNSS_MSG_DATA_READY_THRESHOLD_BYTES
/** The default threshold at which a GNSS device should signal
 * Data Ready if uGnssMsgSetDataReady() is to be used.  Best to
 * make this a multiple of 8 as the GNSS module only takes
 * a multiple of 8.
 */
# define U_GNSS_MSG_DATA_READY_THRESHOLD_BYTES 8
#endif

#ifndef U_GNSS_MSG_DATA_READY_FILL_TIMEOUT_MS
/** The maximum time to wait for Data Ready (AKA TX-Ready)
 * to go active, only used if a Data Ready pin is connected.
  */
# define U_GNSS_MSG_DATA_READY_FILL_TIMEOUT_MS 10000
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** A callback which will be called by uGnssMsgReceiveStart()
 * when a matching message has been received from the GNSS chip.
 * This callback should be executed as quickly as possible to
 * avoid data loss.  The ONLY GNSS API calls that pCallback may make
 * are uGnssMsgReceiveCallbackRead() / uGnssMsgReceiveCallbackExtract(),
 * and potentially pUGnssDecAlloc() / uGnssDecFree(), no others or
 * you risk getting mutex-locked.
 *
 * If you are checking for a specific UBX-format message (i.e. no
 * wild-cards) and a NACK is received for that message then
 * errorCodeOrLength will be set to #U_GNSS_ERROR_NACK and there
 * will be no message to read, otherwise errorCodeOrLength will
 * indicate the length of the message.
 *
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
 * @param errorCodeOrLength      the size of the message, including
 *                               headers and checksums etc. or, if
 *                               pMessageId specifies a particular
 *                               UBX-format message (i.e. no wild-cards)
 *                               and a NACK was received for that
 *                               message, then #U_GNSS_ERROR_NACK will
 *                               be returned (and there will be no
 *                               message to read).
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
 * then true will always be returned, if pMessageIdWanted has the
 * protocol type #U_GNSS_PROTOCOL_NMEA and an empty pNmea then
 * all NMEA message IDs will match, if pNmea contains "G" then
 * "GX" and "GAZZN" would match, if pMessageIdWanted has the
 * protocol type #U_GNSS_PROTOCOL_UBX, message class (upper byte)
 * 0x01 (UBX-NAV) and message ID (lower byte) set to
 * #U_GNSS_UBX_MESSAGE_ID_ALL then all UBX format messages of
 * the UBX-NAV class will match, etc.
 *
 * @param[in] pMessageId       the message ID to check.
 * @param[in] pMessageIdWanted the wanted message ID.
 * @return                     true if pMessageId is inside
 *                             pMessageIdWanted, else false.
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
 * IMPORTANT: this does not work for modules connected via an AT
 * transport, please instead open a Virtual Serial connection for
 * that case (see uCellMuxAddChannel()).
 *
 * @param gnssHandle      the handle of the GNSS instance.
 * @param[in] pBuffer     the message to send; cannot be NULL.
 * @param size            the amount of data at pBuffer; if
 *                        you are using SPI then size should
 *                        not be greater than
 *                        #U_GNSS_SPI_BUFFER_LENGTH_BYTES or there
 *                        is a risk that you will lose some of
 *                        the SPI data that is inevitably received
 *                        while you are sending.
 * @return                on success the number of bytes sent, else
 *                        negative error code.
 */
int32_t uGnssMsgSend(uDeviceHandle_t gnssHandle,
                     const char *pBuffer, size_t size);

/** Monitor the output of the GNSS chip for a given message, blocking
 * (see uGnssMsgReceiveStart() for a non-blocking version).
 *
 * Note: if the message ID is set to a particular UBX-format message (i.e.
 * no wild-cards) and a NACK is received for that message then the
 * error code #U_GNSS_ERROR_NACK will be returned.
 *
 * This function does not pass back the message ID it has decoded;
 * if you used a wildcard in pMessageId and you don't want to decode
 * the message ID from the message yourself (e.g. in the case of a
 * UBX protocol message by using uUbxProtocolDecode()), then you
 * can use pUGnssDecAlloc() / uGnssDecFree() which will always
 * give you the protocol type and message ID (though it may give the
 * error #U_ERROR_COMMON_NOT_SUPPORTED if pUGnssDecAlloc() happens not
 * to support decoding the body of that kind of message), or you
 * could instead use uGnssMsgReceiveStart(), which does pass back
 * the decoded message ID to the pCallback.
 *
 * IMPORTANT: this does not work for modules connected via an AT
 * transport, please instead open a Virtual Serial connection for
 * that case (see uCellMuxAddChannel()).
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
 *                               in which the whole message, including
 *                               headers and checksums etc., will be
 *                               placed, cannot be NULL.  If ppBuffer
 *                               points to NULL (i.e *ppBuffer is NULL)
 *                               then this function will allocate a
 *                               buffer of the correct size and populate
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
 * Message handler callbacks are called mostly-recently-added first.
 *
 * IMPORTANT: this does not work for modules connected via an AT
 * transport, please instead open a Virtual Serial connection for
 * that case (see uCellMuxAddChannel()).
 *
 * Note: if you wish to capture multiple message IDs, e.g. "G?GGA"
 * and "G?RMC", then you should make a call to uGnssMsgReceiveStart()
 * for both message IDs, which could be with the same pCallback, (don't
 * worry about resources, internally the same monitoring task will be
 * used for both) and either call uGnssMsgReceiveStop() for both when
 * done or call uGnssMsgReceiveStopAll() to stop both of them.
 * Alternatively, you could set a wider filter (in this case
 * messageId.type = #U_GNSS_PROTOCOL_NMEA; messageId.id.pNmea = "";
 * (i.e. all NMEA messages)) and do the filtering yourself in your
 * #uGnssMsgReceiveCallback_t callback by checking the contents of
 * the pMessageId pointer it is passed, e.g. with uGnssMsgIdIsWanted()
 * in a loop for a list of wanted message IDs, or by hand in your
 * own way if you prefer: this might be a very slightly higher
 * processor load but, in the end, the filtering is always going to
 * be in C code on this MCU and so whether you do it in your
 * application or this API does it internally is a moot point.
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
 *                               uGnssMsgReceiveCallbackRead(),
 *                               uGnssMsgReceiveCallbackExtract(), and
 *                               potentially pUGnssDecAlloc() / uGnssDecFree(),
 *                               no others or you risk getting mutex-locked.
 *                               pCallback is run in the context of a task with
 *                               a stack of size
 *                               #U_GNSS_MSG_RECEIVE_TASK_STACK_SIZE_BYTES;
 *                               you may call uGnssMsgReceiveStackMinFree()
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
 * (including any headers and checksums) into your buffer but NOT REMOVING
 * IT from the internal ring buffer, so that it is still there to be passed
 * to any other of your pCallbacks. This is the function you would normally
 * use; if you have a long message of specific interest to a single reader
 * you may wish to use uGnssMsgReceiveCallbackExtract() instead to get it
 * out of the way.
 *
 * IMPORTANT: this function can ONLY be called from the message receive
 * pCallback, it is NOT thread-safe to call it from anywhere else.
 *
 * @param gnssHandle   the handle of the GNSS instance.
 * @param[out] pBuffer a place to put the message; cannot be NULL.
 * @param size         the amount of storage at pBuffer, should be
 *                     the size of the message, as indicated by the
 *                     pCallback "errorCodeOrLength" parameter.
 * @return             on success the number of bytes read,
 *                     else negative error code.
 */
int32_t uGnssMsgReceiveCallbackRead(uDeviceHandle_t gnssHandle,
                                    char *pBuffer, size_t size);

/** To be called from the pCallback of uGnssMsgReceiveStart()
 * to REMOVE a whole message (including any headers and checksums) from
 * the internal ring buffer into your buffer; once this is called the
 * message will not be available to any of your other pCallbacks.  Use
 * this if the message you wish to read is very large and you want to
 * get it out of the way; normally you would use
 * uGnssMsgReceiveCallbackRead().
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

/** Set the pin of this MCU that is connected to a GPIO pin of the
 * GNSS device that is to act as a Data Ready (AKA TX-Ready) indication.
 * This may allow your MCU to save power by sleeping while waiting for
 * a response from the GNSS device.  Only works if interrupts are
 * accessible on your platform (so not supported on Windows or Linux) and
 * is only supported by GNSS devices over I2C and SPI interfaces.  If
 * you have your own porting layer, for this to work the "interrupt"
 * portion of the GPIO porting layer must be implemented and note that
 * some platforms may require additional compile-time configuration for
 * interrupts to work, e.g. for STM32Cube the correct HW interrupts
 * must be made available to this code.
 *
 * The GNSS device must have already been powered-on for this to work since
 * communication with the GNSS device is required during the setup.
 *
 * Note: if you are using uDeviceOpen() to bring up the GNSS device and
 * have set the pinDataReady and devicePioDataReady fields in #uDeviceCfgGnss_t
 * then uDeviceOpen() will call this function for you, you do not need to do so;
 * you _may_ still call it if you wish to add your own callback, or if you
 * wish to change the timeout from the default, but if you do that it is
 * best to leave thresholdBytes at -1, i.e. the default, since that is what
 * uDeviceOpen() did and is how the ubxlib code is tested.
 *
 * @param gnssHandle         the handle of the GNSS instance.
 * @param pinMcu             the pin of this MCU that is connected to the
 *                           GPIO pin of the GNSS device that is to be used
 *                           as Data Ready (AKA TX-Ready); if there is an
 *                           inverter between the two pins, so that 0
 *                           indicates "data ready" rather than 1, the
 *                           value should be ORed with #U_GNSS_PIN_INVERTED
 *                           (defined in u_gnss_type.h).
 * @param devicePio          the PIO of the GNSS device that is to be
 *                           used for Data Ready (AKA TX-Ready).  This is
 *                           the PIO, _NOT_ the pin number, they are different;
 *                           the PIO can usually be found in the data sheet
 *                           for the GNSS device.
 *                           IMPORTANT: this PIO must not already be in use
 *                           for some other peripheral function within the
 *                           GNSS device; should that be the case the error
 *                           #U_GNSS_ERROR_PIO_IN_USE will be returned.
 * @param thresholdBytes     the threshold, in bytes of data queued to be
 *                           sent, at which the GNSS device should assert
 *                           Data Ready.  Usually you should set -1 in which
 *                           case the [tested] ubxlib default value of
 *                           #U_GNSS_MSG_DATA_READY_THRESHOLD_BYTES will be used.
 * @param timeoutMs          the time to wait in milliseconds; if you use
 *                           -1 here then the default value of
 *                           #U_GNSS_MSG_DATA_READY_FILL_TIMEOUT_MS will apply.
 * @param[in] pCallback      an OPTIONAL function, that the application may
 *                           provide, which will be called when Data Ready
 *                           is detected.  THIS WILL BE CALLED IN INTERRUPT
 *                           CONTEXT so be _very_ careful what you do in the
 *                           function.  Whether the callback is called after
 *                           or before ubxlib performs any of its own
 *                           operations (e.g. to receive and process any data)
 *                           is OS timing dependent and should not be relied
 *                           upon.  Note that no callback is required for the
 *                           beneficial effect of waiting for Data Ready to be
 *                           realised; that is all done internally within this
 *                           code, the callback is _purely_ for the application
 *                           should it find a need.
 * @param[in] pCallbackParam optional parameter that will be passed to
 *                           pCallback as its last parameter; ignored
 *                           if pCallback is NULL
 * @return                   zero on success, else negative error code.
 */
int32_t uGnssMsgSetDataReady(uDeviceHandle_t gnssHandle, int32_t pinMcu,
                             int32_t devicePio, int32_t thresholdBytes,
                             int32_t timeoutMs,
                             void (*pCallback) (uDeviceHandle_t, void *),
                             void *pCallbackParam);

/** Get the pin of this MCU that is connected to a GPIO pin of the
 * GNSS device that is to act as a Data Ready (AKA TX-Ready) indication.
 * If pDevicePio or pThresholdBytes are non-NULL the GNSS device must
 * be powered on for this to work, since the values are read from
 * the GNSS device.
 *
 * @param gnssHandle           the handle of the GNSS instance.
 * @param[out] pDevicePio      a pointer to a place to put the PIO of the
 *                             GNSS device that is being used for Data Ready
 *                             (AKA TX-Ready); may be NULL.
 * @param[out] pThresholdBytes a pointer to a place to put the threshold,
 *                             in bytes, at which the GNSS device should
 *                             assert Data Ready; may be NULL.
 * @param[out] pTimeoutMs      a pointer to a place to put the timeout,
 *                             to wait for Data Reay in milliseconds; may
 *                             be NULL.
 * @return                     on success the pin of this MCU that is
 *                             expected to be connected to the PIO of the
 *                             GNSS device that is used for Data Ready
 *                             (AKA TX-Ready), else negative error code.
 *                             If the pin is marked as inverted then
 *                             the returned value will have been ORed
 *                             with #U_GNSS_PIN_INVERTED, so to get just
 *                             the pin you must AND it with NOT
 *                             #U_GNSS_PIN_INVERTED.
 */
int32_t uGnssMsgGetDataReady(uDeviceHandle_t gnssHandle, int32_t *pDevicePio,
                             int32_t *pThresholdBytes, int32_t *pTimeoutMs);

/** Wait for the Data Ready (AKA TX-Ready) pin to indicate that data is
 * present.  If the pin already indicates that data is present this will
 * return true immediately, otherwise it will wait (on a semaphore) until
 * Data Ready becomes active and, should that happen within the timeout,
 * (set by uGnssMsgSetDataReady()) true will be returned and any callback
 * set in the call to uGnssMsgSetDataReady() will be called.  Returns false
 * if uGnssMsgSetDataReady() has not been called.
 *
 * @param gnssHandle     the handle of the GNSS instance.
 * @return               true if the data ready pin is active, else false.
 */
bool uGnssMsgIsDataReady(uDeviceHandle_t gnssHandle);

/** Remove a Data Ready (AKA TX-Ready) indication that was previously
 * set.  Note that there is normally no reason to call this function;
 * any Data Ready pin will be removed by uGnssRemove().
 *
 * @param gnssHandle         the handle of the GNSS instance.
 * @return                   zero on success, else negative error code.
 */
int32_t uGnssMsgRemoveDataReady(uDeviceHandle_t gnssHandle);

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
 * example I2C or UART or SPI) have been lost to the non-blocking message
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
 * instance I2C or UART or SPI) and the input of the ring buffer as a
 * result of the ring buffer not being emptied fast enough.  This is a
 * more serious loss than uGnssMsgReceiveStatReadLoss() since the data
 * is lost to all destinations.
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
