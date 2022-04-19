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

#ifndef _U_GNSS_PRIVATE_H_
#define _U_GNSS_PRIVATE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file defines types, functions and inclusions that
 * are common and private to the GNSS API.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_GNSS_MAX_UBX_PROTOCOL_MESSAGE_BODY_LENGTH_BYTES
/** The maximum size of ubx-format message body to be read using
 * these functions.  The maximum length of an RRLP message
 * (UBX-RXM-MEASX) is the governing factor here.
 */
# define U_GNSS_MAX_UBX_PROTOCOL_MESSAGE_BODY_LENGTH_BYTES 1024
#endif

/** Determine if the given feature is supported or not
 * by the pointed-to module.
 */
//lint --emacro((774), U_GNSS_PRIVATE_HAS) Suppress left side always
// evaluates to True
//lint -esym(755, U_GNSS_PRIVATE_HAS) Suppress macro not
// referenced as this is for future expansion and, in any case,
// references may be conditionally compiled-out.
#define U_GNSS_PRIVATE_HAS(pModule, feature) \
    ((pModule != NULL) && ((pModule->featuresBitmap) & (1UL << (int32_t) (feature))))

/** Flag to indicate that the post task has run (for synchronisation
 * purposes. */
#define U_GNSS_POS_TASK_FLAG_HAS_RUN    0x01

/** Flag to indicate that the post task should continue running.
 */
#define U_GNSS_POS_TASK_FLAG_KEEP_GOING 0x02

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Features of a module that require different compile-time
 * behaviours in this implementation.
 */
//lint -esym(756, uGnssPrivateFeature_t) Suppress not referenced,
// Lint can't seem to find it inside macros.
//lint -esym(769, uGnssPrivateFeature_t::U_GNSS_PRIVATE_FEATURE_DUMMY)
// Suppress not referenced, just a placeholder.
typedef enum {
    // This feature selector is included for future expansion:
    // there are currently no optional features and hence
    // U_GNSS_PRIVATE_FEATURE_DUMMY is used simply to permit
    // compilation; it should be removed when the first
    // optional feature is added.
    U_GNSS_PRIVATE_FEATURE_DUMMY
} uGnssPrivateFeature_t;

/** The characteristics that may differ between GNSS modules.
 * Note: order is important since this is statically initialised.
 */
typedef struct {
//lint -esym(768, uGnssPrivateModule_t::moduleType) Suppress not referenced,
// this is for the future.
    uGnssModuleType_t moduleType; /**< the module type. */
//lint -esym(768, uGnssPrivateModule_t::featuresBitmap) Suppress not referenced,
// this is for the future.
    uint32_t featuresBitmap; /**< a bit-map of the uGnssPrivateFeature_t
                                  characteristics of this module. */
} uGnssPrivateModule_t;

/** Definition of a GNSS instance.
 * Note: a pointer to this structure is passed to the asynchronous
 * "get position" function (posGetTask()) which does NOT lock the
 * GNSS mutex, hence it is important that no elements that it cares
 * about are modified while it is active (unlikely since it looks
 * at none of note) but, more importantly, posGetTask() is stopped
 * before an instance is removed.
 */
typedef struct uGnssPrivateInstance_t {
    int32_t handle; /**< the handle for this instance. */
    const uGnssPrivateModule_t *pModule; /**< pointer to the module type. */
    uGnssTransportType_t transportType; /**< the type of transport to use. */
    uGnssTransportHandle_t transportHandle; /**< the handle of the transport to use. */
    int32_t timeoutMs; /**< the timeout for responses from the GNSS chip in milliseconds. */
    bool printUbxMessages; /**< whether debug printing of ubx messages is on or off. */
    int32_t pinGnssEnablePower; /**< the pin of the MCU that enables power to the GNSS module. */
    int32_t pinGnssEnablePowerOnState; /**< the value to set pinGnssEnablePower to for "on". */
    int32_t atModulePinPwr; /**< the pin of the AT module that enables power to the GNSS chip (only relevant for transport type AT). */
    int32_t atModulePinDataReady; /**< the pin of the AT module that is connected to the Data Ready pin of the GNSS chip (only relevant for transport type AT). */
    int32_t portNumber; /**< the internal port number of the GNSS device that we are connected on. */
    uPortMutexHandle_t
    transportMutex; /**< mutex so that we can have an asynchronous task use the transport. */
    uPortTaskHandle_t
    posTask; /**< handle for a task associated with non-blocking position establishment. */
    uPortMutexHandle_t
    posMutex; /**< handle for mutex associated with non-blocking position establishment. */
    volatile uint8_t posTaskFlags; /**< flags to synchronisation the pos task. */
    struct uGnssPrivateInstance_t *pNext;
} uGnssPrivateInstance_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** The characteristics of the supported module types, compiled
 * into the driver.
 */
extern const uGnssPrivateModule_t gUGnssPrivateModuleList[];

/** Number of items in the gUGnssPrivateModuleList array.
 */
extern const size_t gUGnssPrivateModuleListSize;

/** Root for the linked list of instances.
 */
extern uGnssPrivateInstance_t *gpUGnssPrivateInstanceList;

/** Mutex to protect the linked list.
 */
extern uPortMutexHandle_t gUGnssPrivateMutex;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Find a GNSS instance in the list by instance handle.
 * Note: gUGnssPrivateMutex should be locked before this is called.
 *
 * @param handle  the instance handle.
 * @return        a pointer to the instance.
 */
uGnssPrivateInstance_t *pUGnssPrivateGetInstance(int32_t handle);

/** Get the module characteristics for a given instance.
 * Note: gUGnssPrivateMutex should be locked before this is called.
 *
 * @param handle  the instance handle.
 * @return        a pointer to the module characteristics.
 */
//lint -esym(714, pUGnssPrivateGetModule) Suppress lack of a reference
//lint -esym(759, pUGnssPrivateGetModule) etc. since use of this function
//lint -esym(765, pUGnssPrivateGetModule) may be compiled-out in various ways
const uGnssPrivateModule_t *pUGnssPrivateGetModule(int32_t handle);

/** Send a buffer as hex.
 *
 * @param pBuffer           the buffer to print; cannot be NULL.
 * @param bufferLengthBytes the number of bytes to print.
 */
void uGnssPrivatePrintBuffer(const char *pBuffer,
                             size_t bufferLengthBytes);

/** Send a ubx format message over the UART (do not wait for the response).
 * Note: gUGnssPrivateMutex should be locked before this is called.
 *
 * @param pInstance                  a pointer to the GNSS instance, cannot
 *                                   be NULL.
 * @param messageClass               the ubx message class to send with.
 * @param messageId                  the ubx message ID to end with.
 * @param pMessageBody               the body of the message to send; may be
 *                                   NULL.
 * @param messageBodyLengthBytes     the amount of data at pMessageBody; must
 *                                   be non-zero if pMessageBody is non-NULL.
 * @return                           the number of bytes sent, including
 *                                   ubx protocol coding overhead, else negative
 *                                   error code.
 */
int32_t uGnssPrivateSendOnlyUartUbxMessage(const uGnssPrivateInstance_t *pInstance,
                                           int32_t messageClass,
                                           int32_t messageId,
                                           const char *pMessageBody,
                                           size_t messageBodyLengthBytes);

/** Wait for a ubx format message with the given message class and ID to
 * arrive on a UART.
 * Note: gUGnssPrivateMutex should be locked before this is called.
 *
 * @param pInstance             a pointer to the GNSS instance, cannot
 *                              be NULL.
 * @param messageClass          the ubx message class expected.
 * @param messageId             the ubx message ID expected.
 * @param pMessageBody          a place to put the body of the received
  *                             message; may be NULL.
 * @param maxBodyLengthBytes    the amount of room at pMessageBody; must
 *                              be non-zero if pMessageBody is non-NULL.
 * @return                      the number of bytes copied into pMessageBody,
 *                              else negative error code.
 */
int32_t uGnssPrivateReceiveOnlyUartUbxMessage(const uGnssPrivateInstance_t *pInstance,
                                              int32_t messageClass,
                                              int32_t messageId,
                                              char *pMessageBody,
                                              size_t maxBodyLengthBytes);

/** Send a ubx format message to the GNSS module and, optionally, receive
 * the response.  If the message only illicites a simple Ack/Nack from the
 * module then uGnssPrivateSendUbxMessage() must be used instead.
 * May be used with any transport.
 * Note: gUGnssPrivateMutex should be locked before this is called.
 *
 * @param pInstance                  a pointer to the GNSS instance, cannot
 *                                   be NULL.
 * @param messageClass               the ubx message class.
 * @param messageId                  the ubx message ID.
 * @param pMessageBody               the body of the message to send; may be
 *                                   NULL.
 * @param messageBodyLengthBytes     the amount of data at pMessageBody; must
 *                                   be non-zero if pMessageBody is non-NULL.
 * @param pResponseBody              a pointer to somewhere to store the
 *                                   response body, if one is expected; may
 *                                   be NULL.
 * @param maxResponseBodyLengthBytes the amount of storage at pResponseBody;
 *                                   must be non-zero if pResponseBody is non-NULL.
 * @return                           the number of bytes in the body of the response
 *                                   from the GNSS module (irrespective of the value
 *                                   of maxResponseBodyLengthBytes), else negative
 *                                   error code.
 */
int32_t uGnssPrivateSendReceiveUbxMessage(const uGnssPrivateInstance_t *pInstance,
                                          int32_t messageClass,
                                          int32_t messageId,
                                          const char *pMessageBody,
                                          size_t messageBodyLengthBytes,
                                          char *pResponseBody,
                                          size_t maxResponseBodyLengthBytes);

/** Send a ubx format message to the GNSS module that only has an Ack
 * response and check that it is Acked.  May be used with any transport.
 * Note: gUGnssPrivateMutex should be locked before this is called.
 *
 * @param pInstance                  a pointer to the GNSS instance, cannot
 *                                   be NULL.
 * @param messageClass               the ubx message class.
 * @param messageId                  the ubx message ID.
 * @param pMessageBody               the body of the message to send; may be
 *                                   NULL.
 * @param messageBodyLengthBytes     the amount of data at pMessageBody; must
 *                                   be non-zero if pMessageBody is non-NULL.
 * @return                           zero on success else negative error code;
 *                                   if the message has been nacked by the GNSS
 *                                   module U_GNSS_ERROR_NACK will be returned.
 */
int32_t uGnssPrivateSendUbxMessage(const uGnssPrivateInstance_t *pInstance,
                                   int32_t messageClass,
                                   int32_t messageId,
                                   const char *pMessageBody,
                                   size_t messageBodyLengthBytes);

/** Shut down and free memory from a [potentially] running pos task.
 * Note: gUGnssPrivateMutex should be locked before this is called.
 *
 * @param pInstance  a pointer to the GNSS instance, cannot  be NULL.
 */
void uGnssPrivateCleanUpPosTask(uGnssPrivateInstance_t *pInstance);

/** Check whether a GNSS chip that we are using via a cellular module
 * is on-board the cellular module, in which case the AT+GPIOC
 * comands are not used.
 *
 * @param pInstance  a pointer to the GNSS instance, cannot  be NULL.
 * @return           true if there is a GNSS chip inside the cellular
 *                   module, else false.
*/
bool uGnssPrivateIsInsideCell(const uGnssPrivateInstance_t *pInstance);

#ifdef __cplusplus
}
#endif

#endif // _U_GNSS_PRIVATE_H_

// End of file
