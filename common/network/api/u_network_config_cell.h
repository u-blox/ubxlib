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

#ifndef _U_NETWORK_CONFIG_CELL_H_
#define _U_NETWORK_CONFIG_CELL_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup network
 *  @{
 */

/** @file
 * @brief This header file defines the configuration structure for the
 * network API for cellular.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* Note: try, wherever possible, to use only basic types in this
 * structure (i.e. int32_t, const char, bool, etc.) since otherwise
 * your cellular headers will have to be brought in to all files that
 * need this config type, so into all network examples, etc.
 * irrespective of whether cellular is used there.
 */

/* NOTE TO MAINTAINERS: if you change this structure you will
 * need to change u-blox,ubxlib-network-cellular.yaml over in
 * /port/platform/zephyr/dts/bindings to match and you may also
 * need to change the code in the Zephyr u_port_board_cfg.c file
 * that parses the values.
 */
/** The network configuration for cellular.
 */
typedef struct {
    uNetworkCfgVersion_t version; /**< version of this network
                                       configuration; allow your
                                       compiler to initialise this
                                       to zero unless otherwise
                                       specified below. */
    uNetworkType_t type; /**< for error checking purposes. */
    const char *pApn;    /**< the APN to use; if left as NULL
                              a database look-up will be used. */
    int32_t timeoutSeconds; /**< timeout when connecting, in seconds. */
    bool (*pKeepGoingCallback) (uDeviceHandle_t); /**< if set, this function
                                                       will be called
                                                       periodically during
                                                       an "abortable"
                                                       operation; while the
                                                       function returns true
                                                       the operation will
                                                       continue, else it will
                                                       be stopped and this
                                                       code will return. If
                                                       this is set
                                                       timeoutSeconds will
                                                       be ignored.  If you do
                                                       not need this facility,
                                                       ignore it, i.e. allow
                                                       your compiler to set
                                                       the field to zero, and
                                                       timeoutSeconds will be
                                                       obeyed instead. */
    const char *pUsername; /** ONLY REQUIRED if you must use a user name
                               and password with the APN provided to you
                               by your service provider; let your compiler
                               initialise this to zero otherwise. */
    const char *pPassword; /** ONLY REQUIRED if you must use a user name
                               and password with the APN provided to you
                               by your service provider; let your compiler
                               initialise this to zero otherwise. */
    int32_t authenticationMode; /** ONLY REQUIRED if you must give a user name
                                    and password with the APN provided to
                                    you by your service provider and your
                                    cellular module does NOT support figuring
                                    out the authentication mode automatically;
                                    there is no harm in populating this field
                                    even if your module _does_ support figuring
                                    out the authentication mode automatically. */
    const char *pMccMnc; /** ONLY REQUIRED if you wish to connect to a specific
                             MCC/MNC rather than to the best available network;
                             should point to the null-terminated string giving
                             the MCC and MNC of the PLMN to use (for example
                             "23410").
                             NOTE: Cannot be used if asyncConnect is set to true.*/
    const uDeviceCfgUart_t *pUartPpp; /** ONLY REQUIRED if U_CFG_PPP_ENABLE is defined AND
                                          you wish to run the PPP interface to the cellular
                                          module over a DIFFERENT serial port to that which
                                          is already in use.  This is useful if you are
                                          using the USB interface of a cellular module,
                                          which does not support the CMUX protocol that
                                          is used to multiplex PPP with AT.  Otherwise,
                                          please let your compiler initialise this to zero. */
    bool asyncConnect; /** ONLY SET THIS to true if you wish uNetworkInterfaceUp() to return IMMEDIATELY,
    before the cellular network connection has been established, allowing the
    application to continue with other operations rather than waiting. */
    /* IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT IMPORTANT
     * See note above.
     */
    /* This is the end of version 0 of this
       structure: should any fields (that cannot
       be interpreted as absent by dint of being
       initialised to zero) be added to this
       structure in future they must be
       added AFTER this point and instructions
       must be given against each one as to how
       to set the version field if any of the
       new fields are populated. For example, if
       int32_t magic were added, the comment
       against it might end with the clause "; if this
       field is populated then the version field of
       this structure must be set to 1 or higher". */
} uNetworkCfgCell_t;

/** @}*/

#endif // _U_NETWORK_CONFIG_CELL_H_

// End of file
