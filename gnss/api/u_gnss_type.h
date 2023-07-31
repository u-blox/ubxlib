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

#ifndef _U_GNSS_TYPE_H_
#define _U_GNSS_TYPE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup _GNSS
 *  @{
 */

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
/** The recommended UART buffer length for the GNSS driver;
 * 256 bytes is OK for a 9600 baud UART but on Windows/Linux
 * with a USB interface it needs to be more like 1024.
 * Note also that, if you are going to access a GNSS module
 * that is inside or connected-via a cellular module using
 * the CMUX multiplexing protocol rather than the AT interface,
 * then this number should be twice or more the frame size on
 * that protocol #U_CELL_MUX_PRIVATE_INFORMATION_LENGTH_MAX_BYTES,
 * to avoid the multiplexer stalling; it likely will be since
 * the frame size is generally small.
 */
# define U_GNSS_UART_BUFFER_LENGTH_BYTES 1024
#endif

#ifndef U_GNSS_SPI_BUFFER_LENGTH_BYTES
/** The recommended SPI buffer length for the GNSS driver;
 * just go with the UART one for consistency, and since SPI
 * is much faster than a 9600 baud UART a "USB-sized" receive
 * buffer, which #U_GNSS_UART_BUFFER_LENGTH_BYTES is, is
 * probably about right.
 */
# define U_GNSS_SPI_BUFFER_LENGTH_BYTES U_GNSS_UART_BUFFER_LENGTH_BYTES
#endif

#ifndef U_GNSS_DEFAULT_TIMEOUT_MS
/** The default time-out to use on the GNSS interface in
 * milliseconds; note that the separate #U_GNSS_POS_TIMEOUT_SECONDS
 * is used for the GNSS position establishment calls.
 */
# define U_GNSS_DEFAULT_TIMEOUT_MS 10000
#endif

#ifndef U_GNSS_DEFAULT_SPI_FILL_THRESHOLD
/** The default number of 0xFF bytes which, if received on an
 * SPI transport, constitute fill rather than valid data.
 */
# define U_GNSS_DEFAULT_SPI_FILL_THRESHOLD 48
#endif

#ifndef U_GNSS_SPI_FILL_THRESHOLD_MAX
/** The maximum value for the SPI fill threshold; note that an
 * array of this size is placed on the stack.
 */
# define U_GNSS_SPI_FILL_THRESHOLD_MAX 128
#endif

/** There can be an inverter in-line between an MCU pin
 * and whatever enables power to the GNSS chip; OR this value
 * with the value of the pin passed into uGnssAdd() and the sense of
 * that pin will be assumed to be inverted, so "asserted" will be
 * 0 and "deasserted" 1.
 */
#define U_GNSS_PIN_INVERTED 0x80

#ifndef U_GNSS_PIN_ENABLE_POWER_ON_STATE
/** Which way up the U_CFG_APP_PIN_GNSS_ENABLE_POWER pin ON state is.
 * If you wish to indicate that 0 is the "on" state then you
 * should do that by ORing the value of pinGnssEnablePower with
 * #U_GNSS_PIN_INVERTED in the call to uGnssAdd() rather
 * than changing this value.  And certainly don't do both or the
 * sense of the pin will be inverted twice.
 */
# define U_GNSS_PIN_ENABLE_POWER_ON_STATE 1
#endif

#ifndef U_GNSS_UBX_MESSAGE_CLASS_ALL
/** Value used in the most significant byte of the .ubx field of
 * uGnssMessageId_t to indicate "all UBX message classes".
 */
# define U_GNSS_UBX_MESSAGE_CLASS_ALL 0xFF
#endif

#ifndef U_GNSS_UBX_MESSAGE_ID_ALL
/** Value used in the least significant byte of the .ubx field of
 * uGnssMessageId_t to indicate "all UBX message IDs".
 */
# define U_GNSS_UBX_MESSAGE_ID_ALL 0xFF
#endif

#ifndef U_GNSS_UBX_MESSAGE_ALL
/** Value that can be used in the .ubx field of uGnssMessageId_t
 * to indicate "all UBX messages".
 */
# define U_GNSS_UBX_MESSAGE_ALL 0xFFFF
#endif

#ifndef U_GNSS_RTCM_MESSAGE_ID_ALL
/** Value used in uGnssMessageId_t to indicate "all RTCM message IDs".
 */
# define U_GNSS_RTCM_MESSAGE_ID_ALL 0xFFFF
#endif

#ifndef U_GNSS_NMEA_MESSAGE_MATCH_LENGTH_CHARACTERS
/** The maximum number of characters of an NMEA message header
 * (i.e. talker/sentence) to include when performing a match
 * against NMEA message types.
  */
# define U_GNSS_NMEA_MESSAGE_MATCH_LENGTH_CHARACTERS 8
#endif

/** Make a UBX message type from a message class and message ID.
 */
#define U_GNSS_UBX_MESSAGE(class, id) ((((uint16_t) (class)) << 8) | ((uint8_t) (id)))

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
    U_GNSS_TRANSPORT_UART,      /**< the transport handle should be a UART handle;
                                     see also #U_GNSS_TRANSPORT_UART_1 and
                                     #U_GNSS_TRANSPORT_UART_2. */
    U_GNSS_TRANSPORT_AT,        /**< the transport handle should be an AT client
                                     handle; currently only UBX-format messages may
                                     be received when this transport type is in use.
                                     It is better to enable CMUX on the cellular
                                     module with uCellMuxEnable() and then open
                                     a virtual serial port to the GNSS device with
                                     uCellMuxAddChannel(), then any GNSS message
                                     format may be used and position will be streamed. */
    U_GNSS_TRANSPORT_I2C,       /**< the transport handle should be an I2C handle. */
    U_GNSS_TRANSPORT_SPI,       /**< the transport handle should be an SPI handle. */
    U_GNSS_TRANSPORT_VIRTUAL_SERIAL, /**< the transport handle should be a virtual serial,
                                          port handle, for example as returned by
                                          uCellMuxAddChannel() if you are talking to
                                          a GNSS device either inside or connected
                                          via a cellular module. */
    U_GNSS_TRANSPORT_UART_2, /**< the transport handle should be a UART handle;
                                  use this if your GNSS chip has two UART ports
                                  and you are connected to the second one. */
    U_GNSS_TRANSPORT_MAX_NUM,
    U_GNSS_TRANSPORT_UART_1 = U_GNSS_TRANSPORT_UART  /**< the transport handle should
                                                          be a UART handle; equivalent
                                                          to #U_GNSS_TRANSPORT_UART
                                                          but you may wish to use this
                                                          value if your GNSS chip has
                                                          two UART ports and you want
                                                          to distinguish between the
                                                          two in your code. */
} uGnssTransportType_t;

/** The handle for the transport with types implied by
 * uGnssTransportType_t.
 */
typedef union {
    void *pAt;      /**< for transport type #U_GNSS_TRANSPORT_AT. */
    int32_t uart;   /**< for transport types #U_GNSS_TRANSPORT_UART, #U_GNSS_TRANSPORT_UART_1 and  #U_GNSS_TRANSPORT_UART_2). */
    int32_t i2c;    /**< for transport type #U_GNSS_TRANSPORT_I2C. */
    int32_t spi;    /**< for transport type #U_GNSS_TRANSPORT_SPI. */
    void *pDeviceSerial; /**< for transport type #U_GNSS_TRANSPORT_VIRTUAL_SERIAL. */
} uGnssTransportHandle_t;

/** The port type on the GNSS chip itself; this is different
 * from the uGnssTransportType_t since, for instance, a USB port
 * on the MCU might be connected to a UART port on the GNSS chip,
 * and some GNSS chips have two UART ports which need to be
 * identified separately; effectively this is the GNSS chip's own
 * internal port ID, which needs to be used in some messages
 * (for example those querying the communications state).
 */
typedef enum {
    U_GNSS_PORT_I2C = 0,
    U_GNSS_PORT_UART = 1,
    U_GNSS_PORT_UART1 = 1,
    U_GNSS_PORT_UART2 = 2,
    U_GNSS_PORT_USB = 3,
    U_GNSS_PORT_SPI = 4,
    U_GNSS_PORT_MAX_NUM
} uGnssPort_t;

/** The protocol types for exchanges with a GNSS chip.
 */
typedef enum {
    U_GNSS_PROTOCOL_UBX = 0,     // Value chosen to match encoded version for the GNSS chip
    U_GNSS_PROTOCOL_NMEA = 1,    // Value chosen to match encoded version for the GNSS chip
    U_GNSS_PROTOCOL_RTCM = 2,
    U_GNSS_PROTOCOL_UNKNOWN = 3, // Must have this value as it is used to mark
    U_GNSS_PROTOCOL_MAX_NUM,     // the end of the known output protocols
    U_GNSS_PROTOCOL_NONE,
    U_GNSS_PROTOCOL_ALL,
    U_GNSS_PROTOCOL_ANY
} uGnssProtocol_t;

/** Structure to hold a message ID.
 * Note: if you change this structure then uGnssPrivateMessageId_t
 * will probably need changing also
 */
typedef struct {
    uGnssProtocol_t type;
    union {
        uint16_t ubx; /**< formed of the message class in the most significant byte
                           and the message ID in the least significant byte; where
                           this is employed for matching you may use
                           #U_GNSS_UBX_MESSAGE_CLASS_ALL in the most significant byte
                           for all classes, #U_GNSS_UBX_MESSAGE_ID_ALL in the least
                           significant byte for all IDs, or just
                           #U_GNSS_UBX_MESSAGE_ALL for all UBX format messages. */
        char *pNmea;  /**< "GPGGA", "GNZDA": a null-terminated string;
                           where this is used for matching it is done on a
                           per character basis for up to the first
                           #U_GNSS_NMEA_MESSAGE_MATCH_LENGTH_CHARACTERS: set
                           this to NULL or an empty string to match all NMEA
                           messages, "G" to match both "GPGGA" and "GNZDA",
                           "GP" to match all sentences of the "GP" talker,
                           etc.  Any matching is done in a case-sensitive way.
                           Use of a "?" indicates a wildcard, matching any
                           character at that position, so for instance "G?GSV"
                           would match "GPGSV", "GLGSV", "GAGSV", etc. */
        uint16_t rtcm;
    } id;
} uGnssMessageId_t;

/** The types of dynamic platform model; not all types are available on all modules.
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
    U_GNSS_DYNAMIC_BIKE = 10,
    U_GNSS_DYNAMIC_MOWER = 11,
    U_GNSS_DYNAMIC_ESCOOTER = 12
} uGnssDynamic_t;

/** The fix modes.
 */
typedef enum {
    U_GNSS_FIX_MODE_2D = 1,
    U_GNSS_FIX_MODE_3D = 2,
    U_GNSS_FIX_MODE_AUTO = 3
} uGnssFixMode_t;

/** The possible GNSS UTC standards; not all types are available on all modules.
 */
typedef enum {
    U_GNSS_UTC_STANDARD_AUTOMATIC = 0,  /**< automatic. */
    U_GNSS_UTC_STANDARD_USNO = 3,  /**< derived from GPS. */
    U_GNSS_UTC_STANDARD_GALILEO = 5,
    U_GNSS_UTC_STANDARD_GLONASS = 6,
    U_GNSS_UTC_STANDARD_NTSC = 7, /**< National Time Service Center (NTSC), China; derived from BeiDou time. */
    U_GNSS_UTC_STANDARD_NPLI = 8, /**< National Physics Laboratory India. */
    U_GNSS_UTC_STANDARD_NICT = 9 /**< National Institute of Information and Communications Technology, Japan;
                                      derived from QZSS time. */
} uGnssUtcStandard_t;

/** The possible time systems; not all time systems are supported by all modules.
 */
typedef enum {
    U_GNSS_TIME_SYSTEM_NONE = -1,
    U_GNSS_TIME_SYSTEM_UTC = 0,
    U_GNSS_TIME_SYSTEM_GPS = 1,
    U_GNSS_TIME_SYSTEM_GLONASS = 2,
    U_GNSS_TIME_SYSTEM_BEIDOU = 3,
    U_GNSS_TIME_SYSTEM_GALILEO = 4,
    U_GNSS_TIME_SYSTEM_NAVIC = 5
} uGnssTimeSystem_t;


/** The possible modes for RRLP capture; modes beyond
 * U_GNSS_RRLP_MODE_MEASX are only supported on M10 modules or later.
 * For guidance on how these modes may be used, see:
 * https://developer.thingstream.io/guides/location-services/cloudlocate-getting-started
 */
typedef enum {
    U_GNSS_RRLP_MODE_MEASX   = 0,
    U_GNSS_RRLP_MODE_MEAS50  = 1,
    U_GNSS_RRLP_MODE_MEAS20  = 2,
    U_GNSS_RRLP_MODE_MEASC12 = 3,
    U_GNSS_RRLP_MODE_MEASD12 = 4
} uGnssRrlpMode_t;

/** The possible GNSS systems; not all GNSS systems are supported by all modules.
 */
typedef enum {
    U_GNSS_SYSTEM_NONE = -1,
    U_GNSS_SYSTEM_GPS = 0,
    U_GNSS_SYSTEM_SBAS = 1,
    U_GNSS_SYSTEM_GALILEO = 2,
    U_GNSS_SYSTEM_BEIDOU = 3,
    U_GNSS_SYSTEM_IMES = 4,
    U_GNSS_SYSTEM_QZSS = 5,
    U_GNSS_SYSTEM_GLONASS = 6
} uGnssSystem_t;

/** A GNSS space vehicle ID.
 */
typedef struct {
    uGnssSystem_t system;
    int32_t svId;
} uGnssSvId_t;

/** @}*/

#endif // _U_GNSS_TYPE_H_

// End of file
