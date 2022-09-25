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

#ifndef _U_DEVICE_H_
#define _U_DEVICE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup device Device
 *  @{
 */

/** @file
 * @brief This is a high-level API for initializing a u-blox device
 * (chip or module). These functions are generally used in conjunction
 * with those in the network API, see u_network.h for further information.
 * These functions are thread-safe.
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

/** The u-blox device handle; this is intended to be anonymous,
 * the contents should never be referenced by the application.
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

/** A version number for the device configuration structure. In
 * general you should allow the compiler to initialise any variable
 * of this type to zero and ignore it.  It is only set to a value
 * other than zero when variables in a new and extended version of
 * the structure it is a part of are being used, the version number
 * being employed by this code to detect that and, more importantly,
 * to adopt default values for any new elements when the version
 * number is STILL ZERO, maintaining backwards compatibility with
 * existing application code.  The structure this is a part of will
 * include instructions as to when a non-zero version number should
 * be set.
 */
typedef int32_t uDeviceVersion_t;

/* Note: try, wherever possible, to use only basic types in the
 * configuration structures in this file (i.e. int32_t, const char,
 * bool, etc.) since otherwise you'll end up dragging
 * device/transport-type-specific headers into every application,
 * irrespective of whether that device/transport-type is used there.
 * Never use any types that are network-specific here; if you find
 * you need to do so there's something wrong, the device should
 * know nothing about the network.
 */

/** UART transport configuration.
 */
typedef struct {
    uDeviceVersion_t version; /**< Version of this structure; allow your
                                   compiler to initialise this to zero
                                   unless otherwise specified below. */
    int32_t uart;             /**< The UART HW block to use. */
    int32_t baudRate;         /**< Uart speed value
                                   Currently only applicable for short-range modules. */
    int32_t pinTxd;           /**< The output pin that sends UART data to
                                   the module. */
    int32_t pinRxd;           /**< The input pin that receives UART data from
                                   the module. */
    int32_t pinCts;           /**< The input pin that the module
                                   will use to indicate that data can be sent
                                   to it; use -1 if there is no such connection. */
    int32_t pinRts;          /**< The output pin output pin that tells the
                                  module that it can send more UART
                                  data; use -1 if there is no such connection. */
    /* This is the end of version 0 of this structure:
       should any fields be added to this structure in
       future they must be added AFTER this point and
       instructions must be given against each one
       as to how to set the version field if any of
       the new fields are populated. For example,
       if int32_t pinMagic were added, the comment
       against it might and with the clause"; if this
       field is populated then the version field of
       this structure must be set to 1 or higher". */
} uDeviceCfgUart_t;

/** I2C transport configuration.
 */
typedef struct {
    uDeviceVersion_t version;  /**< Version of this structure; allow your
                                    compiler to initialise this to zero
                                    unless otherwise specified below. */
    int32_t i2c;               /**< The I2C HW block to use. */
    int32_t pinSda;            /**< I2C data pin. */
    int32_t pinScl;            /**< I2C clock pin. */
    int32_t clockHertz;        /**< To use the default I2C clock frequency
                                    of #U_PORT_I2C_CLOCK_FREQUENCY_HERTZ
                                    then do NOT set this field, simply
                                    let the compiler initialise it to zero,
                                    and the default clock frequence will be
                                    employed; however, if you wish to set a
                                    different clock frequency you may set it
                                    here.  Note that it alreadyOpen is set
                                    to true then this will be IGNORED. */
    bool alreadyOpen;          /**< Set this to true if the application code
                                    has already opened the I2C port and
                                    hence the device layer should not touch
                                    the I2C HW configuration; if this is
                                    true then pinSda, pinScl and clockHertz
                                    will be ignored. */
    /* This is the end of version 0 of this structure:
       should any fields be added to this structure in
       future they must be added AFTER this point and
       instructions must be given against each one
       as to how to set the version field if any of
       the new fields are populated. For example,
       if int32_t pinMagic were added, the comment
       against it might end with the clause "; if this
       field is populated then the version field of
       this structure must be set to 1 or higher". */
} uDeviceCfgI2c_t;

/** Cellular device configuration.
*/
typedef struct {
    uDeviceVersion_t version;  /**< Version of this structure; allow your
                                    compiler to initialise this to zero
                                    unless otherwise specified below. */
    int32_t moduleType;        /**< The module type that is connected,
                                    see #uCellModuleType_t in u_cell_module_type.h. */
    const char *pSimPinCode;   /**< The PIN of the SIM. */
    int32_t pinEnablePower;    /**< The output pin that enables power
                                    to the cellular module; use -1 if
                                    there is no such connection. */
    int32_t pinPwrOn;          /**< The output pin that is connected to the
                                    PWR_ON pin of the cellular module; use -1
                                    if there is no such connection. */
    int32_t pinVInt;           /**< The input pin that is connected to the
                                    VINT pin of the cellular module; use -1
                                    if there is no such connection. */
    int32_t pinDtrPowerSaving; /**< If you have a GPIO pin of this MCU
                                    connected to the DTR pin of the cellular
                                    module because you intend to use the DTR
                                    pin to tell the module whether it can enter
                                    power-saving or not then put that pin number
                                    here, else set it to -1. */
    /* This is the end of version 0 of this structure:
       should any fields be added to this structure in
       future they must be added AFTER this point and
       instructions must be given against each one
       as to how to set the version field if any of
       the new fields are populated. For example,
       if int32_t pinMagic were added, the comment
       against it might end with the clause "; if this
       field is populated then the version field
       of this structure must be set to 1 or higher". */
} uDeviceCfgCell_t;

/** GNSS device configuration.
 */
typedef struct {
    uDeviceVersion_t version;    /**< Version of this structure; allow your
                                      compiler to initialise this to zero
                                      unless otherwise specified below. */
    int32_t moduleType;          /**< The module type that is connected,
                                      see #uGnssModuleType_t in
                                      u_gnss_module_type.h. */
    int32_t pinEnablePower;      /**< The output pin that is used to control
                                      power to the GNSS module; use -1 if
                                      there is no such connection, or if the
                                      connection is via an intermediate
                                      (e.g. cellular) module that does the
                                      controlling (in which case the
                                      devicePinPwr field of the network
                                      configuration structure for GNSS,
                                      #uNetworkCfgGnss_t, should be
                                      populated instead). */
    int32_t pinDataReady;        /**< The input pin that is used to receive
                                      the Data Ready state of the GNSS module;
                                      this field is present for
                                      forwards-compatibility only; it is
                                      currently ignored. */
    bool includeNmea;            /**< \deprecated This field used to
                                      permit NMEA messages to be included
                                      when they were normally excluded by
                                      default; it is now ignored and may
                                      be removed in future: instead NMEA
                                      messages are now included by default.
                                      If you wish to disable them please use
                                      #uGnssCfgSetProtocolOut() once you have
                                      opened your GNSS device. */
    uint16_t i2cAddress;         /**< Only required if the GNSS device is
                                      connected via I2C and the I2C address that
                                      the GNSS device is using is NOT the default
                                      #U_GNSS_I2C_ADDRESS; otherwise let the
                                      compiler initialise this to 0. */
    /* This is the end of version 0 of this structure:
       should any fields be added to this structure in
       future they must be added AFTER this point and
       instructions must be given against each one
       as to how to set the version field if any of
       the new fields are populated. For example,
       if int32_t pinMagic were added, the comment
       against it might end with the clause "; if this
       field is populated then the version field of
       this structure must be set to 1 or higher". */
} uDeviceCfgGnss_t;

/** Short-range device configuration.
 */
typedef struct {
    uDeviceVersion_t version; /**< Version of this structure; allow your
                                   compiler to initialise this to zero
                                   unless otherwise specified below. */
    int32_t moduleType;       /**< The module type that is connected,
                                   see #uShortRangeModuleType_t in
                                   u_short_range_module_type.h. */
    /* This is the end of version 0 of this structure:
       should any fields be added to this structure in
       future they must be added AFTER this point and
       instructions must be given against each one
       as to how to set the version field if any of
       the new fields are populated. For example,
       if int32_t pinMagic were added, the comment
       against it might end with the clause "; if this
       field is populated then the version field of
       this structure must be set to 1 or higher". */
} uDeviceCfgShortRange_t;

/** The complete device configuration.
 */
typedef struct {
    uDeviceVersion_t version; /**< Version of this structure; allow your
                                   compiler to initialise this to zero
                                   unless otherwise specified below. */
    uDeviceType_t deviceType;
    union {
        uDeviceCfgCell_t cfgCell;
        uDeviceCfgGnss_t cfgGnss;
        uDeviceCfgShortRange_t cfgSho;
    } deviceCfg;
    uDeviceTransportType_t transportType;
    union {
        uDeviceCfgUart_t cfgUart;
        uDeviceCfgI2c_t cfgI2c;
    } transportCfg;
    /* This is the end of version 0 of this structure:
       should any fields be added to this structure in
       future they must be added AFTER this point and
       instructions must be given against each one
       as to how to set the version field if any of
       the new fields are populated. For example,
       if int32_t magic were added, the comment
       against it might end with the clause "; if this
       field is populated then the version field of
       this structure must be set to 1 or higher". */
} uDeviceCfg_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Initialise the device API.  If the device API has already
 * been initialised this function returns success without doing
 * anything.
 *
 * @return  zero on success else negative error code.
 */
int32_t uDeviceInit();

/** Deinitialise the device API.  Note that this performs no
 * clean-up, it is up to the caller to ensure that all devices
 * have been closed with a call to uDeviceClose().
 *
 * @return  zero on success else negative error code.
 */
int32_t uDeviceDeinit();

/** Fill a device configuration with recommended defaults.
 * These defaults come from the port specific settings or
 * possible compile time external defines.
 * This is a voluntary convenience routine.
 * Please note that the module type field may have to be filled in
 * manually after this call as there is currently no applicable default
 * unless it has been specified externally via U_CFG_..._MODULE_TYPE
 *
 * @param[in] deviceType      type of the device
 * @param[in] pDeviceCfg      device configuration to be filled,
 *                            cannot be NULL.
 * @return                    zero on success else a negative error code.
 */
int32_t uDeviceGetDefaults(uDeviceType_t deviceType,
                           uDeviceCfg_t *pDeviceCfg);

/** Open a device instance; if this function returns successfully
 * the device is powered-up and ready to be configured.
 *
 * @param[in] pDeviceCfg      device configuration, cannot be NULL.
 * @param[out] pDeviceHandle  a place to put the device handle;
 *                            cannot be NULL.
 * @return                    zero on success else a negative error code.
 */
int32_t uDeviceOpen(const uDeviceCfg_t *pDeviceCfg,
                    uDeviceHandle_t *pDeviceHandle);

/** Close an open device instance, optionally powering it down.
 *
 * @param devHandle handle to a previously opened device.
 * @param powerOff  if true then also power the device off; leave
 *                  this as false to simply logically disconnect
 *                  the device, in which case the device will be
 *                  able to return to a useful state on
 *                  uDeviceOpen() very quickly.  Note that Short
 *                  Range devices do not support powering off;
 *                  setting this parameter to true will result in
 *                  an error.
 * @return          zero on success else a negative error code.
 */
int32_t uDeviceClose(uDeviceHandle_t devHandle, bool powerOff);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_DEVICE_H_

// End of file
