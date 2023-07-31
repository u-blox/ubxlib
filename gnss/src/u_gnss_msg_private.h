/*
 * Copyright 2019-2023 u-blox
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

#ifndef _U_GNSS_MSG_PRIVATE_H_
#define _U_GNSS_MSG_PRIVATE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file defines a few message functions that
 * are needed in internal form inside the GNSS API.  These few
 * functions are made available this way in order to avoid
 * dragging the whole of the msg part of the GNSS API into
 * u_gnss_pos.c for streamed position.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Monitor the output of the GNSS chip for the given message,
 * non-blocking.  This may be called multiple times; to stop listening
 * for a given message type, call uGnssMsgPrivateReceiveStop() with
 * the handle returned by this function.  There can be a maximum of
 * #U_GNSS_MSG_RECEIVER_MAX_NUM of these running at any one time.
 * Message handler callbacks are called mostly-recently-added first.
 *
 * IMPORTANT: this does not work for modules connected via an AT
 * transport, please instead open a Virtual Serial connection for
 * that case (see uCellMuxAddChannel()).
 *
 * @param[in] pInstance          a pointer to the GNSS instance, cannot
 *                               be NULL.
 * @param[in] pPrivateMessageId  a pointer to the message ID to capture;
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
 *                               uGnssMsgReceiveCallbackExtract(), no others
 *                               or you risk getting mutex-locked.  pCallback
 *                               cannot be NULL.
 * @param[in] pCallbackParam     will be passed to pCallback as its last
 *                               parameter.
 * @return                       a handle for this asynchronous reader on
 *                               success, else negative error code.
 */
int32_t uGnssMsgPrivateReceiveStart(uGnssPrivateInstance_t *pInstance,
                                    const uGnssPrivateMessageId_t *pPrivateMessageId,
                                    uGnssMsgReceiveCallback_t pCallback,
                                    void *pCallbackParam);

/** Stop monitoring the output of the GNSS chip for a message.
 * Once this function returns the pCallback function passed to the
 * associated uGnssMsgPrivateReceiveStart() will no longer be called.
 *
 * @param[in] pInstance a pointer to the GNSS instance, cannot be NULL.
 * @param asyncHandle   the handle originally returned by
 *                      uGnssMsgPrivateReceiveStart().
 * @return              zero on success else negative error code.
 */
int32_t uGnssMsgPrivateReceiveStop(uGnssPrivateInstance_t *pInstance,
                                   int32_t asyncHandle);

#ifdef __cplusplus
}
#endif

#endif // _U_GNSS_MSG_PRIVATE_H_

// End of file
