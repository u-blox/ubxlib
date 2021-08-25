/*
 * Copyright 2020 u-blox Cambourne Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _U_NETWORK_CONFIG_GNSS_H_
#define _U_NETWORK_CONFIG_GNSS_H_

/* No #includes allowed here */

/* This header file defines the configuration structure for the
 * network API for GNSS.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* Note: try, wherever possible, to use only basic types in this
 * structure (i.e. int32_t, const char, bool, etc.) since otherwise
 * your GNSS headers will have to be brought in to all files that
 * need this config type, so into all network examples, etc.
 * irrespective of whether GNSS is used there.
 */

/** The network configuration for GNSS.  Note that the pin
 * numbers are those of the MCU: if you are using an MCU inside
 * a u-blox module the IO pin numbering for the module is likely
 * different to that from the MCU: check the data sheet for the
 * module to determine the mapping.
 */
typedef struct {
    uNetworkType_t type; /**< All uNetworkConfigurationXxx structures
                              must begin with this for error checking
                              purposes. */
    int32_t moduleType; /**< The module type that is connected,
                             see uGnssModuleType_t in u_gnss_module_type.h. */
    int32_t pinGnssEnablePower; /**< The output pin that is used to power-on
                                     the GNSS module; use -1 if there is no
                                     such connection. */
    int32_t transportType; /**< The transport type to use,
                                chosen from uGnssTransportType_t
                                in gnss.h. */
    int32_t uart; /**< The UART HW block to use; ignored if transportType
                       does not indicate a UART connection. */
    int32_t pinTxd; /** The output pin that sends UART data to
                        the GNSS module; ignored if transportType
                        does not indicate a UART connection. */
    int32_t pinRxd; /** The input pin that receives UART data from
                        the GNSS module; ignored if transportType
                        does not indicate a UART connection. */
    int32_t pinCts; /**< The input pin that the GNSS module
                         will use to indicate that data can be sent
                         to it; use -1 if there is no such connection,
                         ignored if transportType does not indicate a
                         UART connection. */
    int32_t pinRts; /**< The output pin output pin that tells the
                         GNSS module that it can send more UART
                         data; use -1 if there is no such connection,
                         ignored if transportType does not indicate a
                         UART connection. */
    int32_t networkHandleAt;  /**< If transportType is set to
                                   U_GNSS_TRANSPORT_UBX_AT, set
                                   this to the handle of the
                                   network which provides the
                                   AT interface, i.e. the module
                                   through which the GNSS module is
                                   connected; ignored if transportType
                                   is set to anything else. */
    int32_t gnssAtPinPwr; /**< Only relevant if transportType
                               is set to U_GNSS_TRANSPORT_UBX_AT:
                               set this to the pin of the intermediate
                               (e.g. cellular) module that powers
                               the GNSS chip.  For instance, in the
                               case of a cellular module, GPIO2
                               is module pin 23 and hence 23 would be
                               used here. If there is no such
                               functionality then use -1. */
    int32_t gnssAtPinDataReady; /**< Only relevant if transportType is set
                                     to U_GNSS_TRANSPORT_UBX_AT: set this to
                                     the pin of the intermediate (e.g. cellular
                                     module that is connected to the Data Ready
                                     pin of the GNSS chip.  For instance, in
                                     the case of cellular, GPIO3 is cellular
                                     module pin 24 and hence 24 would be used here.
                                     If no Data Ready signalling is required then
                                     specify -1. */
} uNetworkConfigurationGnss_t;

#endif // _U_NETWORK_CONFIG_GNSS_H_

// End of file
