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

#ifndef _U_GNSS_PRIVATE_H_
#define _U_GNSS_PRIVATE_H_

/* No #includes allowed here */

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

/** Determine if the given feature is supported or not
 * by the pointed-to module.
 */
//lint --emacro((774), U_GNSS_PRIVATE_HAS) Suppress left side always
// evaluates to True
//lint -esym(755, U_GNSS_PRIVATE_HAS) Suppress not referenced, this
// is for future expansion
#define U_GNSS_PRIVATE_HAS(pModule, feature) \
    ((pModule != NULL) && ((pModule->featuresBitmap) & (1UL << (int32_t) (feature))))

#ifndef U_GNSS_MAX_UBX_MESSAGE_BODY_LENGTH_BYTES
/** The maximum size of ubx-format message body to be read using
 * these functions.
 */
# define U_GNSS_MAX_UBX_MESSAGE_BODY_LENGTH_BYTES 256
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Features of a module that require different compile-time
 * behaviours in this implementation.
 */
//lint -esym(756, uGnssPrivateFeature_t) Suppress not referenced,
// Lint can't seem to find it inside macros.
//lint -esym(769, uGnssPrivateFeature_t::U_GNSS_PRIVATE_FEATURE_NONE)
// Suppress not referenced, just a placeholder.
typedef enum {
    // This feature selector is included for future expansion:
    // there are currently no optional features and hence
    // U_GNSS_PRIVATE_FEATURE_NONE is used simply to permit
    // compilation; it should be removed when the first
    // optional feature is added.
    U_GNSS_PRIVATE_FEATURE_NONE
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
 */
typedef struct uGnssPrivateInstance_t {
    int32_t handle; /**< The handle for this instance. */
    const uGnssPrivateModule_t *pModule; /**< Pointer to the module type. */
    uGnssTransportType_t transportType; /**< The type of transport to use. */
    uGnssTransportHandle_t transportHandle; /**< The handle of the transport to use. */
    int32_t timeoutMs; /**< The timeout for responses from the GNSS chip in milliseconds. */
    int32_t pinGnssEn; /**< The pin of the MCU that is connected to GNSSEN of the module. */
    int32_t portNumber; /**< The internal port number of the GNSS device that we are connected on. */
    bool pendingSwitchOffNmea; /**< Flag to permit NMEA to be switched off once the module is powered. */
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
 *
 * @param handle  the instance handle.
 * @return        a pointer to the module characteristics.
 */
//lint -esym(714, pUGnssPrivateGetModule) Suppress lack of a reference
//lint -esym(759, pUGnssPrivateGetModule) etc. since use of this function
//lint -esym(765, pUGnssPrivateGetModule) may be compiled-out in various ways
const uGnssPrivateModule_t *pUGnssPrivateGetModule(int32_t handle);

/** Send a ubx format message to the GNSS module and, optionally, receive
 * the response.  If the message only illicites a simple Ack/Nack from the
 * module then uGnssPrivateSendUbxMessage() must be used instead.
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
 * response and check that it is Acked.
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

#ifdef __cplusplus
}
#endif

#endif // _U_GNSS_PRIVATE_H_

// End of file
