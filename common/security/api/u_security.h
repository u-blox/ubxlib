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

#ifndef _U_SECURITY_H_
#define _U_SECURITY_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h"

/** \addtogroup security Security
 *  @{
 */

/** @file
 * @brief This header file defines the u-blox security API; these
 * functions are thead-safe.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The maximum length of the serial number string obtained from
 * the module.
 */
#define U_SECURITY_SERIAL_NUMBER_MAX_LENGTH_BYTES 32

/** The space required to store the root of trust UID.
 */
#define U_SECURITY_ROOT_OF_TRUST_UID_LENGTH_BYTES 8

/** The maximum amount of storage required for a generated
 * pre-shared key.
 */
#define U_SECURITY_PSK_MAX_LENGTH_BYTES 32

/** The maximum amount of storage required for a generated
 * pre-shared key ID.
 */
#define U_SECURITY_PSK_ID_MAX_LENGTH_BYTES 32

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS: INFORMATION
 * -------------------------------------------------------------- */

/** Get whether a module supports u-blox security services or not.
 *
 * @param devHandle     the handle of the instance to  be used,
 *                      for example obtained using uDeviceOpen().
 * @return              true if the module supports u-blox security
 *                      services else false.
 */
bool uSecurityIsSupported(uDeviceHandle_t devHandle);

/** Get the security bootstrap status of a module.  A module that
 * supports u-blox security should bootstrap the first time it
 * is able to contact u-blox security services over the network.
 * Once the module is bootrapped it may be sealed with a call
 * to uSecuritySealSet().
 *
 * @param devHandle     the handle of the instance to  be used,
 *                      for example obtained using uDeviceOpen().
 * @return              true if the module has been successfully
 *                      boot-strapped with u-blox security services,
 *                      else false.
 */
bool uSecurityIsBootstrapped(uDeviceHandle_t devHandle);

/** Get the module serial number string.  This may be used, e.g.,
 * when performing security sealing of the module.  The nature of the
 * serial number, and its length, will depend upon the underlying
 * module type.  All that is guaranteed is that it will be a
 * null-terminated string.
 *
 * @param devHandle       the handle of the instance to be used,
 *                        for example obtained using uDeviceOpen().
 * @param pSerialNumber   pointer to storage of at least
 *                        #U_SECURITY_SERIAL_NUMBER_MAX_LENGTH_BYTES
 *                        where the serial number string will be
 *                        placed; cannot be NULL.
 * @return                the length of the string copied into
 *                        pSerialNumber (as strlen() would return)
 *                        or negative error code.
 */
int32_t uSecurityGetSerialNumber(uDeviceHandle_t devHandle,
                                 char *pSerialNumber);

/** Get the root of trust UID from the module.  This may be required
 * if the device is to be sealed using the u-blox security web
 * API.  If pRootOfTrustUid is NULL then this function provides a
 * simple mechanism to check that the u-blox security services inside
 * the module are ready for use (i.e. if it returns non-negative).
 *
 * @param devHandle       the handle of the instance to be used,
 *                        for example obtained using uDeviceOpen().
 * @param pRootOfTrustUid pointer to storage of at least
 *                        #U_SECURITY_ROOT_OF_TRUST_UID_LENGTH_BYTES
 *                        where the root of trust UID will be placed,
 *                        encoded as binary, for example
 *                        [0x0a, 0x04, 0xf0, 0x08, 0x00, 0x3c, 0x96, 0x23],
 *                        *not* ASCII; may be NULL.
 * @return                the number of bytes copied into
 *                        pRootOfTrustUid or negative error code.
 */
int32_t uSecurityGetRootOfTrustUid(uDeviceHandle_t devHandle,
                                   char *pRootOfTrustUid);

/* ----------------------------------------------------------------
 * FUNCTIONS: SEAL
 * -------------------------------------------------------------- */

/** Request security sealing of a module.  The module must have an
 * active connection for the sealing process to succeed (e.g. by
 * calling uNetworkInterfaceUp() on the given handle).  Sealing may
 * take some time, hence pKeepGoingCallback is provided as a mean for
 * the caller to stop waiting for the outcome.  This function will
 * return an error if the module is already security sealed; use
 * uSecuritySealGet() to check whether this is the case.
 *
 * @param devHandle              the handle of the instance to
 *                               be used, for example obtained using
 *                               uDeviceOpen().
 * @param pDeviceProfileUid      the null-terminated device
 *                               profile UID string provided by
 *                               u-blox, something like
 *                               "AgbCtixjwqLjwV3VWpfPyz".
 * @param pDeviceSerialNumberStr the null-terminated device serial
 *                               number string; you may chose what this
 *                               is, noting that there may be an upper
 *                               length limit (for example 16 characters
 *                               for the SARA-R4/SARA-R5 cellular modules
 *                               where it is usual to use the IMEI
 *                               as the device serial number). The device
 *                               serial number may be obtainable by calling
 *                               uSecurityGetSerialNumber().
 * @param pKeepGoingCallback     a callback function that will be called
 *                               periodically while waiting for
 *                               security sealing to complete.  The
 *                               callback should return true to
 *                               continue waiting, else this function
 *                               will return.  Note that this does
 *                               not necessarily terminate the sealing
 *                               process: that may continue in the
 *                               background if there is a connection.
 *                               This callback function may also be used
 *                               to feed an application's watchdog timer.
 *                               May be NULL, in which case this function
 *                               will not return until a succesful
 *                               security seal has been achieved or
 *                               an error has occurred.
 * @return                       zero on success, else negative
 *                               error code.
 */
int32_t uSecuritySealSet(uDeviceHandle_t devHandle,
                         const char *pDeviceProfileUid,
                         const char *pDeviceSerialNumberStr,
                         bool (*pKeepGoingCallback) (void));

/** Get the security seal status of a module.
 *
 * @param devHandle     the handle of the instance to  be used,
 *                      for example obtained using uDeviceOpen().
 * @return              true if the module has been successfully
 *                      security sealed, else false.
 */
bool uSecurityIsSealed(uDeviceHandle_t devHandle);

/* ----------------------------------------------------------------
 * FUNCTIONS: ZERO TOUCH PROVISIONING
 * -------------------------------------------------------------- */

/** Read the device X.509 public certificate that was generated
 * during the sealing process. If the certificate does not [yet]
 * exist an error will be returned.  This feature is only supported
 * if the Zero Touch Provisioning feature for your module.  For
 * certificates stored in the module by the application, or for
 * certificates pre-stored in the module, see instead the
 * uSecurityCredential*() functions in u_security_credential.h.
 *
 * If pData is set to NULL then the number of bytes required to
 * store the certificate, including a null terminator, will still
 * be returned, allowing this API to be called once to find out
 * the length and then a second time with the correct amount of
 * storage allocated.  The certificate is returned in PEM format
 * and will include a null terminator.
 *
 * In order to avoid character loss it is recommended that
 * flow control lines are connected on the interface to the
 * module.
 *
 * @param devHandle       the handle of the instance to be used,
 *                        for example obtained using
 *                        uDeviceOpen().
 * @param pData           a pointer to somewhere to store
 *                        the certificate; use NULL to
 *                        just get the size required without
 *                        any actual data being returned.
 * @param dataSizeBytes   the number of bytes of storage at
 *                        pData; ignored if pData is NULL.
 * @return                on success the number of bytes read
 *                        (or, if pData is NULL the number of
 *                        bytes that would be read) INCLUDING
 *                        the null terminator (strlen() + 1),
 *                        else negative error code on failure.
 */
int32_t uSecurityZtpGetDeviceCertificate(uDeviceHandle_t devHandle,
                                         char *pData,
                                         size_t dataSizeBytes);

/** Read the device private key that was generated during
 * the sealing process.  If the key does not [yet] exist an
 * error will be returned.  This feature is only supported
 * if the Zero Touch Provisioning feature is for your module.
 * For keys stored in the module by the application see instead
 * the uSecurityCredential*() functions in u_security_credential.h.
 *
 * If pData is set to NULL then the number of bytes required to
 * store the key, including a null terminator, will still be
 * returned, allowing this API to be called once to find out
 * the length and then a second time with the correct amount of
 * storage allocated.  The key is returned in PEM format and will
 * include a null terminator.
 *
 * In order to avoid character loss it is recommended that
 * flow control lines are connected on the interface to the
 * module.
 *
 * @param devHandle       the handle of the instance to be used,
 *                        for example obtained using uDeviceOpen().
 * @param pData           a pointer to somewhere to store
 *                        the key; use NULL to just get the size
 *                        required without any actual data being
 *                        returned.
 * @param dataSizeBytes   the number of bytes of storage at
 *                        pData; ignored if pData is NULL.
 * @return                on success the number of bytes read
 *                        (or, if pData is NULL the number of
 *                        bytes that would be read) INCLUDING
 *                        the null terminator (strlen() + 1),
 *                        else negative error code on failure.
 */
int32_t uSecurityZtpGetPrivateKey(uDeviceHandle_t devHandle,
                                  char *pData,
                                  size_t dataSizeBytes);

/** Read the X.509 certificate authorities that were used during
 * the sealing process. If the certificate(s) do not [yet] exist
 * an error will be returned.  This feature is only supported if
 * the Zero Touch Provisioning feature is for your module.  For
 * certificate authorities stored in the module by the application,
 * or for certificate authorities pre-stored in the module, see
 * instead the uSecurityCredential*() functions in
 * u_security_credential.h.
 *
 * If pData is set to NULL then the number of bytes required to
 * store the certificates, including a null terminator, will still
 * be returned, allowing this API to be called once to find out
 * the length and then a second time with the correct amount of
 * storage allocated.  The certificate(s) are returned in PEM format
 * and will include a null terminator.
 *
 * In order to avoid character loss it is recommended that
 * flow control lines are connected on the interface to the
 * module.
 *
 * @param devHandle       the handle of the instance to be used,
 *                        for example obtained using
 *                        uDeviceOpen().
 * @param pData           a pointer to somewhere to store
 *                        the certificate authorities; use
 *                        NULL to just get the size required
 *                        without any actual data being returned.
 * @param dataSizeBytes   the number of bytes of storage at
 *                        pData; ignored if pData is NULL.
 * @return                on success the number of bytes read
 *                        (or, if pData is NULL the number of
 *                        bytes that would be read) INCLUDING
 *                        the null terminator (strlen() + 1),
 *                        else negative error code on failure.
 */
int32_t uSecurityZtpGetCertificateAuthorities(uDeviceHandle_t devHandle,
                                              char *pData,
                                              size_t dataSizeBytes);

/* ----------------------------------------------------------------
 * FUNCTIONS: PRE-SHARED KEY GENERATION
 * -------------------------------------------------------------- */

/** Generate a PSK and accompanying PSK ID.
 * Note: if you simply wish the module to generate the PSK and
 * PSK ID for a TLS session, you should instead set
 * pskGeneratedByRoT to true in the security settings structure;
 * there is no need to generate the PSK and PSK ID here and pass
 * them into the security settings structure.
 *
 * @param devHandle       the handle of the instance to be used,
 *                        for example obtained using uDeviceOpen().
 * @param pskSizeBytes    the size of PSK to be generated: can be
 *                        16 bytes or 32 bytes.
 * @param pPsk            a pointer to storage for 16 or 32 bytes
 *                        of generated PSK, encoded as binary, for
 *                        example [0x0a, 0x04, 0xf0... etc], *not*
 *                        ASCII; cannot be NULL.
 * @param pPskId          a pointer to storage for the PSK ID to go
 *                        to go with the PSK, again encoded as binary,
 *                        *not* ASCII; cannot be NULL, can be up to
 *                        32 bytes in size.
 * @return                the number of bytes copied into pPskId, so
 *                        the *PSK ID*, not the PSK (which will always
 *                        be the number of bytes requested), or negative
 *                        error code.
 */
int32_t uSecurityPskGenerate(uDeviceHandle_t devHandle,
                             size_t pskSizeBytes, char *pPsk,
                             char *pPskId);

/* ----------------------------------------------------------------
 * FUNCTIONS: MISC
 * -------------------------------------------------------------- */

/** Trigger a security heartbeat: this is useful if modifications
 * have been made to the security profile of the device in the
 * u-blox security services REST API and the device needs to be updated
 * with these changes. HOWEVER, note that rate limiting is applied to
 * these adhoc security hearbeats and hence if requested too frequently
 * (e.g. more than once every 24 hours) the trigger request may return
 * an error.
 *
 * @param devHandle     the handle of the instance to be used,
 *                      for example obtained using uDeviceOpen().
 * @return              zero on success else negative error code.
 */
int32_t uSecurityHeartbeatTrigger(uDeviceHandle_t devHandle);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_SECURITY_H_

// End of file
