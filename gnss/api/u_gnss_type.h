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

#ifndef _U_GNSS_TYPE_H_
#define _U_GNSS_TYPE_H_

/* No #includes allowed here */

/** @file
 * @brief This header file defines the types for GNSS that are used
 * in the API.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_GNSS_UART_BAUD_RATE
/** The baud rate for UART comms with the GNSS chip.
 */
# define U_GNSS_UART_BAUD_RATE 9600
#endif

#ifndef U_GNSS_UART_BUFFER_LENGTH_BYTES
/** The recommended UART buffer length for the GNSS driver.
 */
# define U_GNSS_UART_BUFFER_LENGTH_BYTES 256
#endif

#ifndef U_GNSS_DEFAULT_TIMEOUT_MS
/** The default time-out to use on the GNSS interface in
 * milliseconds; note that the separate U_GNSS_POS_TIMEOUT_SECONDS
 * is used for the GNSS position establishment calls.
 */
# define U_GNSS_DEFAULT_TIMEOUT_MS 10000
#endif

#ifndef U_GNSS_PIN_ENABLE_POWER_ON_STATE
/** Which way up the GNSS_ENABLE_POWER pin ON state is.
 */
# define U_GNSS_PIN_ENABLE_POWER_ON_STATE 1
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The types of transport that the GNSS API can support.
 */
//lint -estring(788, uGnssTransportType_t::U_GNSS_TRANSPORT_NONE)
//  Suppress not used within defaulted switch
//lint -estring(788, uGnssTransportType_t::U_GNSS_TRANSPORT_MAX_NUM)
//  Suppress not used within defaulted switch
typedef enum {
    U_GNSS_TRANSPORT_NONE,
    U_GNSS_TRANSPORT_UBX_UART, /**< the transport handle should be a UART handle
                                    over which ubx commands will be transferred;
                                    NMEA will be switched off. */
    U_GNSS_TRANSPORT_UBX_AT,   /**< the transport handle should be an AT client
                                    handle over which ubx commands will be
                                    transferred. */
    U_GNSS_TRANSPORT_NMEA_UART, /**< the transport handle should be a UART handle
                                     over which NMEA commands will be received;
                                     ubx commands will still be used by this code. */
    U_GNSS_TRANSPORT_MAX_NUM
} uGnssTransportType_t;

/** The handle for the transport with types implied by
 * uGnssTransportType_t.
 */
typedef union {
    void *pAt;
    int32_t uart;
} uGnssTransportHandle_t;

/** The types of dynamic platform model.
 */
typedef enum {
    U_GNSS_DYNAMIC_PORTABLE = 0,
    U_GNSS_DYNAMIC_STATIONARY = 2,
    U_GNSS_DYNAMIC_PEDESTRIAN = 3,
    U_GNSS_DYNAMIC_AUTOMOTIVE = 4,
    U_GNSS_DYNAMIC_SEA = 5,
    U_GNSS_DYNAMIC_AIRBORNE_1G = 6,
    U_GNSS_DYNAMIC_AIRBORNE_2G = 7,
    U_GNSS_DYNAMIC_AIRBORNE_4G = 8,
    U_GNSS_DYNAMIC_WRIST = 9,
    U_GNSS_DYNAMIC_BIKE = 10
} uGnssDynamic_t;

/** The fix modes.
 */
typedef enum {
    U_GNSS_FIX_MODE_2D = 1,
    U_GNSS_FIX_MODE_3D = 2,
    U_GNSS_FIX_MODE_AUTO = 3
} uGnssFixMode_t;

#endif // _U_GNSS_TYPE_H_

// End of file
