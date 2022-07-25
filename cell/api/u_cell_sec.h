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

#ifndef _U_CELL_SEC_H_
#define _U_CELL_SEC_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h"

/** \addtogroup _cell
 *  @{
 */

/** @file
 * @brief This header file defines the APIs for u-blox security on a
 * cellular module.  Note that these functions are not intended to be
 * called directly: please use the common/security API.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_CELL_SEC_TRANSACTION_TIMEOUT_SECONDS
/** Security transactions which may require a heartbeat
 * to be completed can take, worst case, 150 seconds to
 * complete.  If you wish you may set this to a smaller
 * number (e.g. 10 seconds) and just retry the security
 * transaction at application level on failure.
 */
# define U_CELL_SEC_TRANSACTION_TIMEOUT_SECONDS 150
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS: INFORMATION
 * -------------------------------------------------------------- */

/** Get whether a cellular module supports u-blox security services
 * or not.
 *
 * @param cellHandle the handle of the cellular instance.
 * @return           true if the cellular module supports u-blox
 *                   security services else false.
 */
bool uCellSecIsSupported(uDeviceHandle_t cellHandle);

/** Get the security bootstrap status of a cellular module.  A
 * cellular module that supports u-blox security should bootstrap
 * the first time it is able to contact u-blox security services
 * over the cellular network.  Once the module is bootrapped it may be
 * sealed with a call to uCellSecSealSet().
 *
 * @param cellHandle the handle of the cellular instance.
 * @return           true if the module has been successfully
 *                   boot-strapped with u-blox security services,
 *                   else false.
 */
bool uCellSecIsBootstrapped(uDeviceHandle_t cellHandle);

/** Get the cellular module's serial number string, which is
 * the 16 digit IMEI, as a null-terminated string.
 *
 * @param cellHandle         the handle of the cellular instance.
 * @param[out] pSerialNumber pointer to storage of length at least
 *                           #U_CELL_INFO_IMEI_SIZE bytes plus an
 *                           additional byte for the terminator,
 *                           where the IMEI string will be
 *                           placed; cannot be NULL.
 * @return                   the length of the string copied into
 *                           pSerialNumber (as strlen() would
 *                           return) or negative error code.
 */
int32_t uCellSecGetSerialNumber(uDeviceHandle_t cellHandle,
                                char *pSerialNumber);

/** Get the root of trust UID from the cellular module.  This may
 * be required if the device is to be sealed using the u-blox
 * security REST API.  The request may time-out if the module has only
 * just booted, in which case please try again.
 *
 * @param cellHandle           the handle of the cellular instance.
 * @param[out] pRootOfTrustUid pointer to storage of at least
 *                             #U_SECURITY_ROOT_OF_TRUST_UID_LENGTH_BYTES
 *                             where the root of trust UID will be placed
 *                             encoded as binary, for example
 *                             [0x0a, 0x04, 0xf0, 0x08, 0x00, 0x3c, 0x96, 0x23],
 *                             *not* ASCII; cannot be NULL.
 * @return                     the number of bytes copied into
 *                             pRootOfTrustUid or negative error code.
 */
int32_t uCellSecGetRootOfTrustUid(uDeviceHandle_t cellHandle,
                                  char *pRootOfTrustUid);

/* ----------------------------------------------------------------
 * FUNCTIONS: CHIP TO CHIP SECURITY
 * -------------------------------------------------------------- */

/** Pair a cellular module's AT interface with this MCU for chip to
 * chip security.  This feature is available by arrangement with
 * u-blox.  The pairing process is expected to be carried out in a
 * secure production environment *before* the device is boostrapped,
 * i.e. before the module is allowed to contact the u-blox security
 * services over the network.  Only if a special feature,
 * "LocalC2CKeyPairing", is enabled in the u-blox security service
 * can pairing be carried out after a device has been sealed, since
 * this represents a potential attack vector.
 *
 * Once this function returns successfully the values of the locally
 * generated pTESecret and the pKey and pHMac parameters returned must
 * be stored securely on this MCU by the caller.  Later, after the
 * module has bootstrapped and been sealed the parameters may be used
 * in a call to uSecurityC2cOpen() to encrypt communication over the
 * AT interface between this MCU and the module.
 *
 * @param cellHandle     the handle of the instance to be used.
 * @param[in] pTESecret  a pointer to the fixed-length 16 byte
 *                       secret generated by this MCU (the
 *                       "Terminal Equipment") to be used in the
 *                       pairing process; cannot be NULL.
 * @param[out] pKey      a place to store the fixed-length 16 byte
 *                       encryption key that must be used when a
 *                       secure AT session is opened.  It is up to
 *                       the caller to store this securely in
 *                       non-volatile memory for future use.
 *                       Cannot be NULL.
 * @param[out] pHMacKey  a place to store the fixed-length
 *                       16 byte HMAC key that must be used when a
 *                       secure AT session is opened.  It is up
 *                       to the caller to store this securely in
 *                       non-volatile memory for future use.
 *                       Cannot be NULL.
 * @return               zero on success else negative error code.
 */
int32_t uCellSecC2cPair(uDeviceHandle_t cellHandle,
                        const char *pTESecret,
                        char *pKey, char *pHMacKey);

/** Open a secure AT session.  Once this has returned
 * successfully the AT client will encrypt the outgoing data
 * stream to the cellular module and decrypt data received back from
 * the cellular module using the keys provided.  pTeSecret, pKey,
 * and pHMax are provided from non-volatile storage on the
 * MCU, the latter two resulting from the C2C pairing process
 * carried out earlier. Once this function returns successfully
 * all AT communications will be encrypted by the AT client until
 * uSecurityC2cClose() is called or the cellular module is powered
 * off or rebooted.  If a chip to chip security session is
 * already open when this is called it will do nothing and
 * return success.
 *
 * Should chip to chip security have somehow failed the cellular
 * module will appear as though it is unresponsive.  If this
 * happen use hard power off, uCellPwrOffHard() (but no need
 * for "trulyHard"), which uses electrical rather than AT-command
 * means to power the module down, and then restart it to try again.
 *
 * @param cellHandle     the handle of the instance to be used.
 * @param[in] pTESecret  a pointer to the fixed-length 16 byte
 *                       secret key that was used during pairing;
 *                       cannot be NULL.
 * @param[in] pKey       a pointer to the fixed-length 16 byte
 *                       encryption key that was returned during
 *                       pairing; cannot be NULL.
 * @param[in] pHMacKey   a pointer to the fixed-length 16 byte
 *                       HMAC key that was returned during pairing;
 *                       cannot be NULL.
 * @return               zero on success else negative error code.
 */
int32_t uCellSecC2cOpen(uDeviceHandle_t cellHandle,
                        const char *pTESecret,
                        const char *pKey,
                        const char *pHMacKey);

/** Close a secure AT session.  Once this has returned
 * successfully the AT exchange with the cellular module will
 * once more be unencrypted.  If there is no open C2C session
 * this function will do nothing and return success.
 *
 * @param cellHandle     the handle of the instance to be used.
 * @return               zero on success else negative error code.
 */
int32_t uCellSecC2cClose(uDeviceHandle_t cellHandle);

/* ----------------------------------------------------------------
 * FUNCTIONS: SEAL
 * -------------------------------------------------------------- */

/** Request security sealing of a cellular module.  The module must
 * have an active connection for the sealing process to succeed (e.g. by
 * calling uCellNetConnect() on the given handle).  Sealing may take some
 * time, hence pKeepGoingCallback is provided as a mean for the caller
 * to stop waiting for the outcome.  This function will return an error
 * if the module is already security sealed; use uCellSecIsSealed()
 * to check whether this is the case.
 *
 * @param cellHandle                 the handle of the cellular instance.
 * @param[in] pDeviceProfileUid      the null-terminated device profile
 *                                   UID string provided by u-blox.
 *                                   Note: if you have activated your module
 *                                   via the Thingstream portal
 *                                   (https://portal.thingstream.io) then the
 *                                   device profile UID string is visible
 *                                   once you have created a device profile
 *                                   for your module; it will look something
 *                                   like "AgbCtixjwqLjwV3VWpfPyz".
 * @param[in] pDeviceSerialNumberStr the null-terminated device serial
 *                                   number string; you may chose what this
 *                                   is, noting that there may be an upper
 *                                   length limit. It is usual to use the
 *                                   IMEI here; that can be obtained as a
 *                                   string by calling uCellSecGetSerialNumber().
 * @param[in] pKeepGoingCallback     a callback function that will be called
 *                                   periodically while waiting for
 *                                   security sealing to complete.  The
 *                                   callback should return true to
 *                                   continue waiting, else this function
 *                                   will return.  Note that this does
 *                                   not necessarily terminate the sealing
 *                                   process: that may continue in the
 *                                   background if there is a connection.
 *                                   This callback function may also be used
 *                                   to feed an application's watchdog timer.
 *                                   May be NULL, in which case this function
 *                                   will not return until a succesful
 *                                   security seal has been achieved or
 *                                   an error has occurred.
 * @return                           zero on success, else negative
 *                                   error code.
 */
int32_t uCellSecSealSet(uDeviceHandle_t cellHandle,
                        const char *pDeviceProfileUid,
                        const char *pDeviceSerialNumberStr,
                        bool (*pKeepGoingCallback) (void));

/** Get whether a cellular module is sealed with u-blox security
 * services or not.  The module does NOT need an active connection
 * for this to work.
 *
 * @param cellHandle    the handle of the instance to  be used.
 * @return              true if the module has been successfully
 *                      security sealed, else false.
 */
bool uCellSecIsSealed(uDeviceHandle_t cellHandle);

/* ----------------------------------------------------------------
 * FUNCTIONS: ZERO TOUCH PROVISIONING
 * -------------------------------------------------------------- */

/** Read the device X.509 public certificate that was generated
 * during the sealing process.  If the certificate does not [yet]
 * exist an error will be returned.  This feature is only
 * supported if the Zero Touch Provisioning feature is enabled
 * in your Thingstream portal for the module.
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
 * Note that if the chip-to-chip security feature is enabled
 * in the Thingstream portal for a module then a chip-to-chip
 * security session must have been opened before this function is
 * called, otherwise it will return an error.
 *
 * @param cellHandle      the handle of the cellular instance.
 * @param[out] pData      a pointer to somewhere to store
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
int32_t uCellSecZtpGetDeviceCertificate(uDeviceHandle_t cellHandle,
                                        char *pData,
                                        size_t dataSizeBytes);

/** Read the device private key that was generated during the
 * sealing process.  If the key does not [yet] exist an error
 * will be returned.  This feature is only supported if the Zero
 * Touch Provisioning feature is enabled in your Thingstream
 * portal for the module.
 *
 * If pData is set to NULL then the number of bytes required to
 * store the key, including a null terminator, will still be
 * returned, allowing this API to be called once to find out
 * the length and then a second time with the correct amount of
 * storage allocated.  The key is returned in PEM format and
 * will include a null terminator.
 *
 * In order to avoid character loss it is recommended that
 * flow control lines are connected on the interface to the
 * module.
 *
 * Note that if the chip-to-chip security feature is enabled
 * in the Thingstream portal for a module then a chip-to-chip
 * security session must have been opened before this function is
 * called, otherwise it will return an error.
 *
 * @param cellHandle      the handle of the cellular instance.
 * @param[out] pData      a pointer to somewhere to store
 *                        the key; use NULL to just get the
 *                        size required without any actual data
 *                        being returned.
 * @param dataSizeBytes   the number of bytes of storage at
 *                        pData; ignored if pData is NULL.
 * @return                on success the number of bytes read
 *                        (or, if pData is NULL the number of
 *                        bytes that would be read) INCLUDING
 *                        the null terminator (strlen() + 1),
 *                        else negative error code on failure.
 */
int32_t uCellSecZtpGetPrivateKey(uDeviceHandle_t cellHandle,
                                 char *pData,
                                 size_t dataSizeBytes);

/** Read the X.509 certificate authorities that were used during
 * the sealing process.  If the certificate(s) do not [yet]
 * exist an error will be returned.  This feature is only
 * supported if the Zero Touch Provisioning feature is enabled
 * in your Thingstream portal for the module.
 *
 * If pData is set to NULL then the number of bytes required to
 * store the certificates, including a null terminator, will still
 * be returned, allowing this API to be called once to find out
 * the length and then a second time with the correct amount of
 * storage allocated.  The certificate(s) are returned in PEM
 * format and will include a null terminator.
 *
 * In order to avoid character loss it is recommended that
 * flow control lines are connected on the interface to the
 * module.
 *
 * Note that if the chip-to-chip security feature is enabled
 * in the Thingstream portal for a module then a chip-to-chip
 * security session must have been opened before this function is
 * called, otherwise it will return an error.
 *
 * @param cellHandle      the handle of the cellular instance.
 * @param[out] pData      a pointer to somewhere to store
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
int32_t uCellSecZtpGetCertificateAuthorities(uDeviceHandle_t cellHandle,
                                             char *pData,
                                             size_t dataSizeBytes);

/* ----------------------------------------------------------------
 * FUNCTIONS: END TO END ENCRYPTION
 * -------------------------------------------------------------- */

/** Set the E2E encryption version to be used.  Not all cellular
 * module types support all versions: refer to the AT manual for your
 * cellular module to determine what's what.  If a cellular module
 * only supports a single E2E encryption type then it probably won't
 * support setting the E2E encryption version.
 *
 * @param cellHandle     the handle of the instance to be used.
 * @param version        the version to use; use 1 for version 1,
 *                       etc. (so there is no version 0).
 * @return               zero on success else negative error code.
 */
int32_t uCellSecE2eSetVersion(uDeviceHandle_t cellHandle, int32_t version);

/** Get the E2E encryption version.  If a cellular module only supports
 * a single E2E encryption type then it may not support getting the
 * E2E encryption version.  Note that while the AT+USECOPCMD="e2e_enc"
 * command returns 0 for version 1 etc., this function will return
 * 1 for version 1, i.e. there is no version 0.
 *
 * @param cellHandle  the handle of the instance to be used.
 * @return            on success the E2E encryption version,
 *                    else negative error code.
 */
int32_t uCellSecE2eGetVersion(uDeviceHandle_t cellHandle);

/** Ask a cellular module to encrypt a block of data.  For this to
 * work the module must have previously been security sealed but no
 * current connection is required.  Data encrypted in this way
 * can be decrypted on arrival at its destination by requesting
 * the relevant security keys from u-blox via the security services
 * web API.
 *
 * @param cellHandle     the handle of the instance to be used.
 * @param[in] pDataIn    a pointer to dataSizeBytes of data to be
 *                       encrypted, may be NULL, in which case this
 *                       function does nothing.
 * @param[out] pDataOut  a pointer to a location to store the
 *                       encrypted data that MUST BE at least of
 *                       size dataSizeBytes +
 *                       #U_SECURITY_E2E_V1_HEADER_LENGTH_BYTES for
 *                       E2E encryption version 1 or dataSizeBytes +
 *                       #U_SECURITY_E2E_V2_HEADER_LENGTH_BYTES for
 *                       E2E encryption version 2 (or you can
 *                       just use #U_SECURITY_E2E_HEADER_LENGTH_MAX_BYTES
 *                       for both cases); can only be NULL if pDataIn
 *                       is NULL.
 * @param dataSizeBytes  the number of bytes of data to encrypt;
 *                       must be zero if pDataIn is NULL.
 * @return               on success the number of bytes in the
 *                       encrypted data block else negative error
 *                       code.
 */
int32_t uCellSecE2eEncrypt(uDeviceHandle_t cellHandle,
                           const void *pDataIn,
                           void *pDataOut, size_t dataSizeBytes);

/* ----------------------------------------------------------------
 * FUNCTIONS: PRE-SHARED KEY GENERATION
 * -------------------------------------------------------------- */

/** Generate a PSK and accompanying PSK ID.
 *
 * @param cellHandle     the handle of the instance to be used.
 * @param pskSizeBytes   the size of PSK to be generated: can be
 *                       16 bytes or 32 bytes.
 * @param[out] pPsk      a pointer to storage for 16 or 32 bytes
 *                       of generated PSK, encoded as binary, for
 *                       example [0x0a, 0x04, 0xf0... etc], *not*
 *                       ASCII; cannot be NULL.
 * @param[out] pPskId    a pointer to storage for the PSK ID to go
 *                       to go with the PSK, again encoded as binary,
 *                       *not* ASCII; cannot be NULL, can be up to
 *                       32 bytes in size.
 * @return               the number of bytes copied into pPskId, so
 *                       the *PSK ID*, not the PSK (which will always
 *                       be the number of bytes requested), or negative
 *                       error code.
 */
int32_t uCellSecPskGenerate(uDeviceHandle_t cellHandle,
                            size_t pskSizeBytes, char *pPsk,
                            char *pPskId);

/* ----------------------------------------------------------------
 * FUNCTIONS: MISC
 * -------------------------------------------------------------- */

/** Trigger a security heartbeat: this is useful if modifications
 * have been made to the security profile of the device in the
 * u-blox security services REST API (or through the Thingstream
 * service) and the device needs to be updated with these changes.
 * HOWEVER, note that rate limiting is applied to these adhoc security
 * hearbeats and hence if requested too frequently (e.g. more than
 * once every 24 hours) the trigger request may return an error.
 *
 * @param cellHandle the handle of the instance to  be used, for
 *                   example obtained through uDeviceOpen().
 * @return           zero on success else negative error code.
 */
int32_t uCellSecHeartbeatTrigger(uDeviceHandle_t cellHandle);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_CELL_SEC_H_

// End of file
