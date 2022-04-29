/*
 * Copyright 2022 u-blox
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

#ifndef _U_DEVICE_H_
#define _U_DEVICE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This is a high-level API for initializing an u-blox device (chip or module)
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The u-blox device handle.
 */
typedef void *uDeviceHandle_t;

/** Device types.
 */
typedef enum {
    U_DEVICE_TYPE_NONE,
    U_DEVICE_TYPE_CELL,
    U_DEVICE_TYPE_GNSS,
    U_DEVICE_TYPE_SHORT_RANGE,
    U_DEVICE_TYPE_SHORT_RANGE_OPEN_CPU,
    U_DEVICE_TYPE_MAX_NUM
} uDeviceType_t;

/** Device transport types.
 */
typedef enum {
    U_DEVICE_TRANSPORT_TYPE_NONE,
    U_DEVICE_TRANSPORT_TYPE_UART,
    U_DEVICE_TRANSPORT_TYPE_I2C,
    U_DEVICE_TRANSPORT_TYPE_MAX_NUM
} uDeviceTransportType_t;

/** Uart transport configuration.
 */
typedef struct {
    int32_t uart;     /**< The UART HW block to use. */
    int32_t baudRate; /**< Uart speed value
                           Currently only applicable for short-range modules. */
    int32_t pinTxd;   /**< The output pin that sends UART data to
                           the module. */
    int32_t pinRxd;   /**< The input pin that receives UART data from
                           the module. */
    int32_t pinCts;   /**< The input pin that the module
                           will use to indicate that data can be sent
                           to it; use -1 if there is no such connection. */
    int32_t pinRts;   /**< The output pin output pin that tells the
                           module that it can send more UART
                           data; use -1 if there is no such connection. */
} uDeviceConfigUart_t;

/** I2C transport configuration.
 */
typedef struct {
    int32_t pinSda; /** I2C data pin. */
    int32_t pinScl; /** I2C clock pin. */
} uDeviceConfigI2c_t;

/** Cellular device configuration.
*/
typedef struct {
    int32_t moduleType;     /**< The module type that is connected,
                                 see uCellModuleType_t in u_cell_module_type.h. */
    int32_t pinEnablePower; /**< The output pin that enables power
                                 to the cellular module; use -1 if
                                 there is no such connection. */
    int32_t pinPwrOn;       /**< The output pin that is connected to the
                                 PWR_ON pin of the cellular module; use -1
                                 if there is no such connection. */
    int32_t pinVInt;        /**< The input pin that is connected to the
                                 VINT pin of the cellular module; use -1
                                 if there is no such connection. */
} uDeviceConfigCell_t;

/** GNSS device configuration.
 */
typedef struct {
    int32_t moduleType;         /**< The module type that is connected,
                                     see uGnssModuleType_t in u_gnss_module_type.h. */
    int32_t pinGnssEnablePower; /**< The output pin that is used to power-on
                                     the GNSS module; use -1 if there is no
                                     such connection. */
    int32_t transportType;      /**< The transport type to use,
                                     chosen from uGnssTransportType_t
                                     in gnss.h. */
    int32_t gnssAtPinPwr;       /**< Only relevant if transportType
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
} uDeviceConfigGnss_t;

/** Short-range device configuration.
 */
typedef struct {
    int32_t module; /**< The module type that is connected,
                         see uShortRangeModuleType_t in u_short_range_module_type.h. */
} uDeviceConfigShortRange_t;

/** The complete device configuration.
 */
typedef struct {
    uDeviceType_t deviceType;
    union {
        uDeviceConfigCell_t cellCfg;
        uDeviceConfigGnss_t gnssCfg;
        uDeviceConfigShortRange_t shoCfg;
    } deviceCfg;
    uDeviceTransportType_t transport;
    union {
        uDeviceConfigUart_t uartCfg;
        uDeviceConfigI2c_t i2cCfg;
    } transportCfg;
} uDeviceConfig_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Open a device instance.
 *
 * @param pDevCfg              Device configuration
 * @param[out] pUDeviceHandle  A new device handle.
 *
 * @return                     0 on success else a negative error code.
 */
int32_t uDeviceOpen(const uDeviceConfig_t *pDevCfg, uDeviceHandle_t *pUDeviceHandle);

/** Close an open device instance.
 *
 * @param pUDeviceHandle  Handle to a previously opened device.
 *
 * @return                0 on success else a negative error code.
 */
int32_t uDeviceClose(uDeviceHandle_t pUDeviceHandle);

#ifdef __cplusplus
}
#endif

#endif // _U_DEVICE_H_

// End of file
