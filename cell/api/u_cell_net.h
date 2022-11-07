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

#ifndef _U_CELL_NET_H_
#define _U_CELL_NET_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h"

/** \addtogroup _cell
 *  @{
 */

/** @file
 * @brief This header file defines the APIs that control the network
 * connectivity of a cellular module. These functions are thread-safe
 * unless otherwise specified with the proviso that a cellular instance
 * should not be accessed before it has been added or after it has been
 * removed.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The number of digits in an IP address, which could be IPV6
 * and includes room for a null terminator.
 */
#define U_CELL_NET_IP_ADDRESS_SIZE 64

#ifndef U_CELL_NET_CONTEXT_ID
/** The PDP context ID to use.
 */
# define U_CELL_NET_CONTEXT_ID 1
#endif

#ifndef U_CELL_NET_PROFILE_ID
/** The module profile ID to use: has to be zero for SARA-R4.
 */
# define U_CELL_NET_PROFILE_ID 0
#endif

/** The maximum number of PDP contexts that can be exist
 * (3GPP defined).
 */
#define U_CELL_NET_MAX_NUM_CONTEXTS 7

/** The number of bytes required to represent an MCC/MNC string
 * with null terminator, enough for the 3-digit MNC case,
 * for example "722320".
 */
#define U_CELL_NET_MCC_MNC_LENGTH_BYTES 7

#ifndef U_CELL_NET_MAX_NAME_LENGTH_BYTES
/** The number of bytes required to store a network name,
 * including terminator.
 */
# define U_CELL_NET_MAX_NAME_LENGTH_BYTES 64
#endif

#ifndef U_CELL_NET_MAX_APN_LENGTH_BYTES
/** The number of bytes required to store an APN, including
 * terminator.
 */
# define U_CELL_NET_MAX_APN_LENGTH_BYTES 101
#endif

#ifndef U_CELL_NET_CONNECT_TIMEOUT_SECONDS
/** The time in seconds allowed for a connection to complete.
 * This is a long time since, in the worst case, deep scan
 * on an NB1 network could take this long.  To shorten the
 * connection time, pass a pKeepGoingCallback() parameter
 * to the connection function.
 */
# define U_CELL_NET_CONNECT_TIMEOUT_SECONDS (60 * 30)
#endif

#ifndef U_CELL_NET_UPSD_CONTEXT_ACTIVATION_TIME_SECONDS
/** Where a module uses the AT+UPSD command to activate
 * a context for the internal IP stack of the module,
 * we have to just wait on the "OK" being returned;
 * there is no other feedback and we can't abort.
 * This sets the amount of time to wait at each attempt.
 * Should not be less than 30 seconds.
 */
# define U_CELL_NET_UPSD_CONTEXT_ACTIVATION_TIME_SECONDS 30
#endif

#ifndef U_CELL_NET_SCAN_RETRIES
/** How many times to retry a network scan if there is no
 * response at all within #U_CELL_NET_SCAN_TIME_SECONDS.
 */
# define U_CELL_NET_SCAN_RETRIES 2
#endif

#ifndef U_CELL_NET_SCAN_TIME_SECONDS
/** How long to allow for a network scan; note that this is
 * the scan time but the uCellNetScanGetFirst() function
 * may retry up to #U_CELL_NET_SCAN_RETRIES times if the module
 * offers no response at all within this time.
 */
# define U_CELL_NET_SCAN_TIME_SECONDS (60 * 3)
#endif

/** Determine if a given cellular network status value means that
 * we're registered with the network.
 */
#define U_CELL_NET_STATUS_MEANS_REGISTERED(status)                 \
   (((status) == U_CELL_NET_STATUS_REGISTERED_HOME) ||             \
    ((status) == U_CELL_NET_STATUS_REGISTERED_ROAMING) ||          \
    ((status) == U_CELL_NET_STATUS_REGISTERED_SMS_ONLY_HOME) ||    \
    ((status) == U_CELL_NET_STATUS_REGISTERED_SMS_ONLY_ROAMING) || \
    ((status) == U_CELL_NET_STATUS_REGISTERED_NO_CSFB_HOME) ||     \
    ((status) == U_CELL_NET_STATUS_REGISTERED_NO_CSFB_ROAMING))

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The current network status.  Note that these are
 * offset by one in value from the 3GPP values since
 * zero is used to mean "unknown" (and negative values
 * are used to indicate errors).
 */
//lint -estring(788, uCellNetStatus_t::U_CELL_NET_STATUS_MAX_NUM)
//  Suppress not used within defaulted switch
//lint -estring(788, uCellNetStatus_t::U_CELL_NET_STATUS_DUMMY)
// Suppress not used within defaulted switch
typedef enum {
    U_CELL_NET_STATUS_DUMMY = -1, /**< added to ensure that the
                                       compiler treats values of
                                       this type as signed in case
                                       an error code is to be returned
                                       as this type. Otherwise the enum
                                       could, in some cases, have an
                                       underlying type of unsigned and
                                       hence < 0 checks will always be
                                       false and you might not be warned
                                       of this. */
    U_CELL_NET_STATUS_UNKNOWN,
    U_CELL_NET_STATUS_NOT_REGISTERED,              /**< +CEREG: 0. */
    U_CELL_NET_STATUS_REGISTERED_HOME,             /**< +CEREG: 1. */
    U_CELL_NET_STATUS_SEARCHING,                   /**< +CEREG: 2. */
    U_CELL_NET_STATUS_REGISTRATION_DENIED,         /**< +CEREG: 3. */
    U_CELL_NET_STATUS_OUT_OF_COVERAGE,             /**< +CEREG: 4. */
    U_CELL_NET_STATUS_REGISTERED_ROAMING,          /**< +CEREG: 5. */
    U_CELL_NET_STATUS_REGISTERED_SMS_ONLY_HOME,    /**< +CEREG: 6. */
    U_CELL_NET_STATUS_REGISTERED_SMS_ONLY_ROAMING, /**< +CEREG: 7. */
    U_CELL_NET_STATUS_EMERGENCY_ONLY,              /**< +CEREG: 8. */
    U_CELL_NET_STATUS_REGISTERED_NO_CSFB_HOME,     /**< +CEREG: 9. */
    U_CELL_NET_STATUS_REGISTERED_NO_CSFB_ROAMING,  /**< +CEREG: 10. */
    U_CELL_NET_STATUS_TEMPORARY_NETWORK_BARRING,
    U_CELL_NET_STATUS_MAX_NUM
} uCellNetStatus_t;

/** The possible radio access technologies.  Note that
 * these are offset by one in value from the 3GPP values
 * returned in the AT+COPS or AT+CxREG commands since
 * zero is used to mean "unknown/not used" (and negative
 * values are used to indicate errors).
 */
//lint -estring(788, uCellNetRat_t::U_CELL_NET_RAT_MAX_NUM)
// Suppress not used within defaulted switch
//lint -estring(788, uCellNetRat_t::U_CELL_NET_RAT_DUMMY)
// Suppress not used within defaulted switch
typedef enum {
    U_CELL_NET_RAT_DUMMY = -1, /**< added to ensure that the
                                    compiler treats values of
                                    this type as signed in case
                                    an error code is to be returned
                                    as this type. Otherwise the enum
                                    could, in some cases, have an
                                    underlying type of unsigned and
                                    hence < 0 checks will always be
                                    false and you might not be warned
                                    of this. */
    U_CELL_NET_RAT_UNKNOWN_OR_NOT_USED = 0,
    U_CELL_NET_RAT_GSM_GPRS_EGPRS = 1,
    U_CELL_NET_RAT_GSM_COMPACT = 2, /**< this RAT is not supported
                                         by any of the supported
                                         u-blox modules. */
    U_CELL_NET_RAT_UTRAN = 3,
    U_CELL_NET_RAT_EGPRS = 4, /**< this RAT can be detected as active
                                   but cannot be individually configured
                                   using uCellCfgSetRat() or
                                   uCellCfgSetRatRank(). */
    U_CELL_NET_RAT_HSDPA = 5, /**< this RAT can be detected as active
                                   but cannot be individually configured
                                   using uCellCfgSetRat() or
                                   uCellCfgSetRatRank(). */
    U_CELL_NET_RAT_HSUPA = 6, /**< this RAT can be detected as active
                                   but cannot be individually configured
                                   using uCellCfgSetRat() or
                                   uCellCfgSetRatRank(). */
    U_CELL_NET_RAT_HSDPA_HSUPA = 7, /**< this RAT can be detected as active
                                         but cannot be individually configured
                                         using uCellCfgSetRat() or
                                         uCellCfgSetRatRank(). */
    U_CELL_NET_RAT_LTE = 8, /**< supported by LARA-R6. */
    U_CELL_NET_RAT_EC_GSM = 9, /**< this RAT is not supported
                                    by any of the supported
                                    u-blox modules. */
    U_CELL_NET_RAT_CATM1 = 10,
    U_CELL_NET_RAT_NB1 = 11,
    U_CELL_NET_RAT_MAX_NUM
} uCellNetRat_t;

/** The possible registration types.
 */
//lint -estring(788, uCellNetRegDomain_t::U_CELL_NET_REG_DOMAIN_MAX_NUM)
// Suppress not used within defaulted switch
typedef enum {
    U_CELL_NET_REG_DOMAIN_CS, /**< circuit switched (AT+CREG). */
    U_CELL_NET_REG_DOMAIN_PS, /**< packet switched (AT+CGREG/AT+CEREG). */
    U_CELL_NET_REG_DOMAIN_MAX_NUM
} uCellNetRegDomain_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Register with the cellular network and activate a PDP context.
 * This function provides the registration and activation of the
 * PDP context in one call. To split these operations up use the
 * uCellNetRegister() and uCellNetActivate() functions instead.
 * If a connection is already active this function will simply
 * return unless the requested APN is different from the APN of
 * the current connection, in which case that PDP context will be
 * deactivated (and potentially deregistration may occur) then
 * [registration will occur and] the new context will be activated.
 *
 * @param cellHandle             the handle of the cellular instance.
 * @param[in] pMccMnc            pointer to a string giving the MCC and
 *                               MNC of the PLMN to use (for example "23410")
 *                               for manual connection; set to NULL if
 *                               automatic PLMN selection (AT+COPS=0)
 *                               is required.
 * @param[in] pApn               pointer to a string giving the APN to
 *                               use; set to NULL if no APN is specified
 *                               by the service provider, in which
 *                               case the APN database in u_cell_apn_db.h
 *                               will be used to determine a default APN.
 *                               To force an empty APN to be used, specify
 *                               "" for pApn.
 * @param[in] pUsername          pointer to a string giving the user name
 *                               for PPP authentication; may be set to
 *                               NULL if no user name or password is
 *                               required.
 * @param[in] pPassword          pointer to a string giving the password
 *                               for PPP authentication; must be
 *                               non-NULL if pUsername is non-NULL, ignored
 *                               if pUsername is NULL.
 * @param[in] pKeepGoingCallback a callback function that governs how
 *                               long a connection attempt will continue
 *                               for. This function is called once a second
 *                               while waiting for a connection attempt
 *                               to complete; the connection attempt
 *                               will only continue while it returns
 *                               true.  This allows the caller to
 *                               terminate the connection attempt at their
 *                               convenience. This function may also be
 *                               used to feed any watchdog timer that
 *                               might be running during longer cat-M1/NB1
 *                               network search periods.  The single
 *                               int32_t parameter is the cell handle.
 *                               May be NULL, in which case the connection
 *                               attempt will eventually time out on
 *                               failure.
 * @return                       zero on success or negative error code on
 *                               failure.
 */
int32_t uCellNetConnect(uDeviceHandle_t cellHandle,
                        const char *pMccMnc,
                        const char *pApn, const char *pUsername,
                        const char *pPassword,
                        bool (*pKeepGoingCallback) (uDeviceHandle_t));

/** Register with the cellular network.  Note that on EUTRAN (LTE)
 * networks, registration and context activation are done at the same
 * time and hence, if you want to specify an APN rather than rely
 * on the default APN provided by the network, you should use
 * uCellConnect() instead.
 *
 * @param cellHandle             the handle of the cellular instance.
 * @param[in] pMccMnc            pointer to a string giving the MCC and
 *                               MNC of the PLMN to use (for example "23410")
 *                               for manual connection; set to NULL if
 *                               automatic PLMN selection (AT+COPS=0)
 *                               is required.
 * @param[in] pKeepGoingCallback a callback function that governs how
 *                               long registration will continue for.
 *                               This function is called once a second
 *                               while waiting for registration to finish;
 *                               registration will only continue while it
 *                               returns true.  This allows the caller to
 *                               terminate registration at their convenience.
 *                               This function may also be used to feed
 *                               any watchdog timer that might be running
 *                               during longer cat-M1/NB1 network search
 *                               periods.  The single int32_t parameter
 *                               is the cell handle. May be NULL, in which
 *                               case the registration attempt will
 *                               eventually time-out on failure.
 * @return                       zero on success or negative error code on
 *                               failure.
 */
int32_t uCellNetRegister(uDeviceHandle_t cellHandle,
                         const char *pMccMnc,
                         bool (*pKeepGoingCallback) (uDeviceHandle_t));

/** Activate the PDP context.  If a PDP context is already active
 * this function will simply return unless the requested APN
 * is different from the APN of the current PDP context,
 * in which case the current PDP context will be deactivated and
 * the new one activated.  Note that on EUTRAN (LTE) networks and
 * on SARA-R4 modules the APN is set during registration and so
 * this will result in de-registration and re-registration with the
 * network.
 *
 * @param cellHandle             the handle of the cellular instance.
 * @param[in] pApn               pointer to a string giving the APN to
 *                               use; set to NULL if no APN is specified
 *                               by the service provider, in which
 *                               case the APN database in u_cell_apn_db.h
 *                               will be used to determine a default APN.
 *                               To force an empty APN to be used, specify
 *                               "" for pApn.
 * @param[in] pUsername          pointer to a string giving the user name
 *                               for PPP authentication; may be set to
 *                               NULL if no user name or password is
 *                               required.
 * @param[in] pPassword          pointer to a string giving the password
 *                               for PPP authentication; ignored if pUsername
 *                               is NULL, must be non-NULL if pUsername is
 *                               non-NULL.
 * @param[in] pKeepGoingCallback a callback function that governs how
 *                               long an activation attempt will continue
 *                               for. This function is called once a second
 *                               while waiting for an activation attempt
 *                               to complete; the activation attempt
 *                               will only continue while it returns
 *                               true.  This allows the caller to
 *                               terminate the activation attempt at their
 *                               convenience. This function may also be
 *                               used to feed any watchdog timer that
 *                               might be running during longer cat-M1/NB1
 *                               network search periods.  May be NULL,
 *                               in which case the activation attempt
 *                               will eventually time out on failure.
 * @return                       zero on success or negative error code on
 *                               failure.
 */
int32_t uCellNetActivate(uDeviceHandle_t cellHandle,
                         const char *pApn, const char *pUsername,
                         const char *pPassword,
                         bool (*pKeepGoingCallback) (uDeviceHandle_t));

/** Deactivate the PDP context.  On EUTRAN (LTE) networks and on
 * SARA-R4 modules irrespective of the radio access technology, it is
 * not permitted to have no context and therefore this function
 * will also result in deregistration from the network.
 *
 * @param cellHandle             the handle of the cellular instance.
 * @param[in] pKeepGoingCallback a call-back function that governs how
 *                               long deactivation will continue for.
 *                               This function is called once a second
 *                               while waiting for deactivation to
 *                               finish; deactivation will only
 *                               continue while it returns true. This
 *                               allows the caller to terminate
 *                               activation at their convenience.
 *                               May be NULL.  The single int32_t
 *                               parameter is the cell handle.
 * @return                       zero on success or negative error code
 *                               on failure.
 */
int32_t uCellNetDeactivate(uDeviceHandle_t cellHandle,
                           bool (*pKeepGoingCallback) (uDeviceHandle_t));

/** Disconnect from the network. If there is an active PDP Context it
 * will be deactivated. The state of the module will be that the
 * radio is in airplane mode (AT+CFUN=4).
 *
 * @param cellHandle             the handle of the cellular instance.
 * @param[in] pKeepGoingCallback a call-back function that governs how
 *                               long de-registration will continue for.
 *                               This function is called once a second
 *                               while waiting for de-registration to
 *                               finish; de-registration will only
 *                               continue while it returns true. This
 *                               allows the caller to terminate
 *                               registration at their convenience.
 *                               May be NULL.  The single int32_t
 *                               parameter is the cell handle.
 * @return                       zero on success or negative error code on
 *                               failure.
 */
int32_t uCellNetDisconnect(uDeviceHandle_t cellHandle,
                           bool (*pKeepGoingCallback) (uDeviceHandle_t));

/** Initiate a network scan and return the first result after
 * it has completed; uCellNetScanGetNext() should be called
 * repeatedly to iterate through subsequent results from the
 * scan.  This function is not thread-safe in that there is a
 * single scan list for any given cellHandle.
 *
 * For instance, to print out the MCC/MNC's of all the visible
 * networks:
 *
 * ```
 * char buffer[U_CELL_NET_MCC_MNC_LENGTH_BYTES];
 *
 * for (int32_t x = uCellNetScanGetFirst(handle, NULL, 0,
                                         buffer, NULL, NULL);
 *      x >= 0;
 *      x = uCellNetScanGetNext(handle, NULL, 0, buffer, NULL)) {
 *     printf("%s\n", buffer);
 * }
 * ```
 *
 * @param cellHandle             the handle of the cellular instance.
 * @param[out] pName             a place to put the name of the first
 *                               network found; may be NULL.
 * @param nameSize               the amount of storage at pName, must
 *                               be non-zero if pName is non-NULL.
 *                               No more than #U_CELL_NET_MAX_NAME_LENGTH_BYTES
 *                               (which includes room for a terminator)
 *                               are required.
 * @param[out] pMccMnc           a pointer to #U_CELL_NET_MCC_MNC_LENGTH_BYTES
 *                               of storage in which the MCC/MNC
 *                               string representing the first network
 *                               will be stored; may be NULL.
 * @param[out] pRat              pointer to a place to put the radio
 *                               access technology of the network;
 *                               may be NULL.
 * @param[in] pKeepGoingCallback network scanning can take some time,
 *                               this call-back is called once a second
 *                               during the scan, allowing a watch-dog
 *                               function to be called if required; may
 *                               be NULL.  The function should return
 *                               true; if it returns false the network
 *                               scan will be aborted.  The single
 *                               int32_t parameter is the cell handle.
 * @return                       the number of networks found or negative
 *                               error code.  If
 *                               #U_CELL_ERROR_TEMPORARY_FAILURE is returned
 *                               then the module is currently in a state
 *                               where it is unable to perform a network
 *                               search (e.g. if it is already doing one
 *                               for other reasons) and in this case it
 *                               is worth waiting a little while (e.g. 10
 *                               seconds) and trying again.
 */
int32_t uCellNetScanGetFirst(uDeviceHandle_t cellHandle,
                             char *pName, size_t nameSize,
                             char *pMccMnc, uCellNetRat_t *pRat,
                             bool (*pKeepGoingCallback) (uDeviceHandle_t));

/** Return subsequent results from a network scan.  Use
 * uCellNetScanGetFirst() to get the number of results and
 * return the first result and then call this "number of
 * results" times to read out all of the search results.
 * Calling this "number of results" times will free
 * the memory that held the search results after the final
 * call (otherwise it will be freed when the cellular
 * instance is removed or another scan is initiated, or
 * can be freed with a call to uCellNetScanGetLast()).
 * This function is not thread-safe in that there is a
 * single scan list for all threads.
 *
 * @param cellHandle   the handle of the cellular instance.
 * @param[out] pName   a place to put the name of the next
 *                     network found; may be NULL.
 * @param nameSize     the amount of storage at pName, must
 *                     be non-zero if pName is non-NULL.
 *                     No more than #U_CELL_NET_MAX_NAME_LENGTH_BYTES
 *                     (which includes room for a terminator)
 *                     are required.
 * @param[out] pMccMnc a pointer to #U_CELL_NET_MCC_MNC_LENGTH_BYTES
 *                     of storage in which the MCC/MNC
 *                     string representing the next network
 *                     will be stored; may be NULL.
 * @param[out] pRat    pointer to a place to put the radio
 *                     access technology of the network;
 *                     may be NULL.
 * @return             the number of networks remaining *after*
 *                     this one has been read or negative error
 *                     code.
 */
int32_t uCellNetScanGetNext(uDeviceHandle_t cellHandle,
                            char *pName, size_t nameSize,
                            char *pMccMnc, uCellNetRat_t *pRat);

/** It is good practice to call this to clear up memory
 * from uCellNetScanGetFirst() if you are not going to
 * iterate through the whole list with uCellNetScanGetNext().
 *
 * @param cellHandle  the handle of the cellular instance.
 */
void uCellNetScanGetLast(uDeviceHandle_t cellHandle);

/** Enable or disable the registration status call-back. This
 * call-back allows the application to know the various
 * states of the network scanning, registration and rejections
 * from the networks.
 * You may use the #U_CELL_NET_STATUS_MEANS_REGISTERED macro
 * with the second parameter passed to the callback to
 * determine if the status value means that the module is
 * currently registered with the network or not.
 *
 * @param cellHandle                  the handle of the cellular
 *                                    instance.
 * @param[in] pCallback               pointer to the function to
 *                                    handle any registration
 *                                    state changes. Use NULL to
 *                                    deactivate a previously
 *                                    active registration status
 *                                    callback.
 * @param[in] pCallbackParameter      a pointer to be passed to
 *                                    the call-back as its third
 *                                    parameter; may be NULL.
 * @return                            zero on success or negative
*                                     error code on failure.
 */
int32_t uCellNetSetRegistrationStatusCallback(uDeviceHandle_t cellHandle,
                                              void (*pCallback) (uCellNetRegDomain_t,
                                                                 uCellNetStatus_t,
                                                                 void *),
                                              void *pCallbackParameter);

/** Enable or disable the module's base station connection
 * call-back. The callback will be called with the Boolean
 * parameter set to true when it enters connected state and
 * false when it leaves connected state.  It is module
 * dependent as to whether such an indication is supported:
 * for instance SARA-U201 and SARA-R410M-02B do NOT support
 * such an indication; if the module does not support such
 * an indication under any circumstances an error will be
 * returned by this function.
 *
 * Note that the state of the base station connection and
 * that of registration are not the same: the
 * base station connection will be active while the module
 * is communicating with, or maintaining readiness to
 * communicate with, the base station.  It is possible to
 * be connected but not registered and vice-versa.
 *
 * @param cellHandle                the handle of the
 *                                  cellular instance.
 * @param[in] pCallback             pointer to the
 *                                  function to handle
 *                                  any connection state
 *                                  changes.  Use NULL
 *                                  to deactivate a previously
 *                                  active connection status
 *                                  call-back.
 * @param[in] pCallbackParameter    a pointer to be passed
 *                                  to the call-back as its
 *                                  second parameter; may be
 *                                  NULL.
 * @return                          zero on success or negative
 *                                  error code on failure.
 */
int32_t uCellNetSetBaseStationConnectionStatusCallback(uDeviceHandle_t cellHandle,
                                                       void (*pCallback) (bool,
                                                                          void *),
                                                       void *pCallbackParameter);

/** Get the current network registration status.  If you
 * simply want to confirm that registration has been
 * achieved, use uCellNetIsRegistered() instead.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param domain      you may be registered on a cellular
 *                    network for circuit switched or
 *                    packet switched access, where the one
 *                    you will get depends upon the
 *                    subscription you have purchased or
 *                    possibly the roaming agreement your
 *                    home operator has with a visited
 *                    network.  99% of the time you will
 *                    only care about #U_CELL_NET_REG_DOMAIN_PS.
 *                    but you may set #U_CELL_NET_REG_DOMAIN_CS
 *                    to specifically check the status for
 *                    circuit switched service only.
 * @return            the current status.
 */
uCellNetStatus_t uCellNetGetNetworkStatus(uDeviceHandle_t cellHandle,
                                          uCellNetRegDomain_t domain);

/** Get a value indicating whether the module is registered on
 * the network, roaming or home networks.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            true if registered either on roaming or home
 *                    networks, false otherwise.
 */
bool uCellNetIsRegistered(uDeviceHandle_t cellHandle);

/** Return the RAT that is currently in use.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            the current RAT or -1 on failure (which means
 *                    that the module is not registered on any RAT).
 */
uCellNetRat_t uCellNetGetActiveRat(uDeviceHandle_t cellHandle);

/** Get the name of the operator on which the cellular module is
 * registered.  An error will be returned if the module is not
 * registered on the network at the time this is called.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param[out] pStr   a pointer to size bytes of storage into which
 *                    the operator name will be copied.  Room
 *                    should be allowed for a null terminator, which
 *                    will be added to terminate the string.  This
 *                    pointer cannot be NULL.
 * @param size        the number of bytes available at pStr, including
 *                    room for a null terminator. Must be greater
 *                    than zero.
 * @return            on success, the number of characters copied into
 *                    pStr NOT including the terminator (as strlen()
 *                    would return), on failure negative error code.
 */
int32_t uCellNetGetOperatorStr(uDeviceHandle_t cellHandle,
                               char *pStr, size_t size);

/** Get the MCC/MNC of the network on which the cellular module is
 * registered.  An error will be returned if the module is not
 * registered on the network at the time this is called.
 * To get the returned values into the same form as the
 * pMccMnc strings used elsewhere in this API, snprintf() them
 * into a buffer of length #U_CELL_NET_MCC_MNC_LENGTH_BYTES with
 * the formatter "%03d%02d".
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param[out] pMcc   pointer to a place to store the MCC; cannot
 *                    be NULL.
 * @param[out] pMnc   pointer to a place to store the MNC; cannot
 *                    be NULL.
 * @return            zero on success else negative error code.
 */
int32_t uCellNetGetMccMnc(uDeviceHandle_t cellHandle,
                          int32_t *pMcc, int32_t *pMnc);

/** Return the IP address of the currently active connection.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param[out] pStr   should point to storage of length at least
 *                    #U_CELL_NET_IP_ADDRESS_SIZE bytes in size.
 *                    On return the IP address will be written to
 *                    pStr as a string and a null terminator will
 *                    be added.
 *                    May be set to NULL for a simple test as to
 *                    whether an IP address has been allocated or not.
 * @return            on success, the number of characters that would
 *                    be copied into into pStr if it is not NULL,
 *                    NOT including the terminator (as strlen()
 *                    would return), on failure negative error code.
 */
int32_t uCellNetGetIpAddressStr(uDeviceHandle_t cellHandle, char *pStr);

/** Return the IP addresses of the first and second DNS assigned
 * by the network.  Without a DNS the module is unable to
 * use hostnames in these API functions, only IP addresses.
 *
 * @param cellHandle    the handle of the cellular instance.
 * @param v6            set this to true if IPV6 DNS addresses
 *                      should be returned, else IPV4 addresses
 *                      will be returned.  In some cases it is
 *                      not possible to return IPV6 addresses
 *                      (e.g. the IP stack inside SARA-U201 is
 *                      IPV4 only), in which case IPV4 addresses
 *                      may be returned even when IPV6 addresses
 *                      have been requested: the user should
 *                      expect either.
 * @param[out] pStrDns1 a pointer to storage of length at least
 *                      #U_CELL_NET_IP_ADDRESS_SIZE bytes in size.
 *                      On return the primary DNS address will be
 *                      written to pStr as a string and a null
 *                      terminator will be added.
 *                      May be set to NULL for a simple test as to
 *                      whether a DNS address has been allocated or
 *                      not.
 * @param[out] pStrDns2 a pointer to storage of length at least
 *                      #U_CELL_NET_IP_ADDRESS_SIZE bytes in size.
 *                      On return the secondary DNS address will be
 *                      written to pStr as a string and a null
 *                      terminator will be added.  May be set to NULL.
 * @return              zero if at least one DNS address has been
 *                      assigned (either v4 or v6, irrespective
 *                      of the setting of the v6 parameter) else
 *                      negative error code.
 */
int32_t uCellNetGetDnsStr(uDeviceHandle_t cellHandle, bool v6,
                          char *pStrDns1, char *pStrDns2);

/** Get the APN currently in use.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param[out] pStr   a pointer to size bytes of storage into which
 *                    the APN string will be copied.  Room should be
 *                    allowed for a null terminator, which will be
 *                    added to terminate the string; to ensure
 *                    the maximum number of characters for an APN
 *                    can be stored, allocate
 *                    #U_CELL_NET_MAX_APN_LENGTH_BYTES.  This pointer
 *                    cannot be NULL.
 * @param size        the number of bytes available at pStr, including
 *                    room for a null terminator.  Must be greater
 *                    than zero.
 * @return            on success, the number of characters copied into
 *                    pStr NOT including the terminator (as strlen()
 *                    would return), on failure negative error code.
 */
int32_t uCellNetGetApnStr(uDeviceHandle_t cellHandle, char *pStr, size_t size);

/* ----------------------------------------------------------------
 * FUNCTIONS: DATA COUNTERS
 * -------------------------------------------------------------- */

/** Get the current value of the transmit data counter.  Only
 * available when a connection is active.
 *
 * @param cellHandle     the handle of the cellular instance.
 * @return               the number of data bytes transmitted, since
 *                       the cellular connection was made, or negative
 *                       error code.  The count resets to zero when
 *                       the connection is dropped.
 */
int32_t uCellNetGetDataCounterTx(uDeviceHandle_t cellHandle);

/** Get the current value of the receive data counter.  Only
 * available when a connection is active.
 *
 * @param cellHandle     the handle of the cellular instance.
 * @return               the number of data bytes received, since
 *                       the cellular connection was made, or negative
 *                       error code.  The count resets to zero when
 *                       the connection is dropped.
 */
int32_t uCellNetGetDataCounterRx(uDeviceHandle_t cellHandle);

/** Reset the transmit and receive data counters.  Only
 * available when a connection is active.
 *
 * @param cellHandle     the handle of the cellular instance.
 * @return               zero on success, else negative error code.
 */
int32_t uCellNetResetDataCounters(uDeviceHandle_t cellHandle);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_CELL_NET_H_

// End of file
