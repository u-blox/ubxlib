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

#ifndef _U_SHORT_RANGE_H_
#define _U_SHORT_RANGE_H_

/* No #includes allowed here */

/** @file
 * @brief This header file defines the ShortRange APIs,
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_SHORT_RANGE_AT_BUFFER_LENGTH_BYTES
/** The buffer length required in the AT client by the ShortRange driver.
 */
# define U_SHORT_RANGE_AT_BUFFER_LENGTH_BYTES U_AT_CLIENT_BUFFER_LENGTH_BYTES
#endif

#ifndef U_SHORT_RANGE_UART_BUFFER_LENGTH_BYTES
/** Corresponds to the large posible short range EDM packet
 */
# define U_SHORT_RANGE_UART_BUFFER_LENGTH_BYTES 4000
#endif

#ifndef U_SHORT_RANGE_UART_BAUD_RATE
/** The default baud rate to communicate with short range module.
 */
# define U_SHORT_RANGE_UART_BAUD_RATE 115200
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Error codes specific to short range.
 */
typedef enum {
    U_SHORT_RANGE_ERROR_FORCE_32_BIT = 0x7FFFFFFF,  /**< Force this enum to be 32 bit as it can be
                                                  used as a size also. */
    U_SHORT_RANGE_ERROR_AT = U_ERROR_SHORT_RANGE_MAX,      /**< -4096 if U_ERROR_BASE is 0. */
    U_SHORT_RANGE_ERROR_NOT_CONFIGURED = U_ERROR_SHORT_RANGE_MAX - 1, /**< -4097 if U_ERROR_BASE is 0. */
    U_SHORT_RANGE_ERROR_VALUE_OUT_OF_RANGE = U_ERROR_SHORT_RANGE_MAX - 2, /**< -4098 if U_ERROR_BASE is 0. */
    U_SHORT_RANGE_ERROR_INVALID_MODE = U_ERROR_SHORT_RANGE_MAX - 3, /**< -4099 if U_ERROR_BASE is 0. */
    U_SHORT_RANGE_ERROR_NOT_DETECTED = U_ERROR_SHORT_RANGE_MAX - 4, /**< -4100 if U_ERROR_BASE is 0. */
    U_SHORT_RANGE_ERROR_WRONG_TYPE = U_ERROR_SHORT_RANGE_MAX - 5, /**< -4101 if U_ERROR_BASE is 0. */
} uShortRangeErrorCode_t;

/** The possible types of short range module.
 * Note: if you add a new module type here, check the
 * U_SHORT_RANGE_PRIVATE_MODULE_xxx macros in u_short_range_private.h
 * to see if they need updating (amongst other things).
 * Note: order is important as these are used to index
 * into a statically defined array in u_short_range_cfg.c.
 */
//lint -estring(788, uShortRangeModuleType_t::U_SHORT_RANGE_MODULE_TYPE_MAX_NUM) Suppress not used within defaulted switch
typedef enum {
    U_SHORT_RANGE_MODULE_TYPE_NINA_B1 = 0, /**< Modules NINA-B1. BLE only*/
    U_SHORT_RANGE_MODULE_TYPE_ANNA_B1, /**< Modules ANNA-B1. BLE only */
    U_SHORT_RANGE_MODULE_TYPE_NINA_B3, /**< Modules NINA-B3. BLE only */
    U_SHORT_RANGE_MODULE_TYPE_NINA_B4, /**< Modules NINA-B4. BLE only */
    U_SHORT_RANGE_MODULE_TYPE_NINA_B2, /**< Modules NINA-B2. BLE and Classic */
    U_SHORT_RANGE_MODULE_TYPE_NINA_W13, /**< Modules NINA-W13. Wifi */
    U_SHORT_RANGE_MODULE_TYPE_NINA_W15, /**< Modules NINA-W15. Wifi, BLE and Classic */
    U_SHORT_RANGE_MODULE_TYPE_ODIN_W2, /**< Modules NINA-B1. Wifi, BLE and Classic */
    U_SHORT_RANGE_MODULE_TYPE_MAX_NUM,
    U_SHORT_RANGE_MODULE_TYPE_INVALID = U_SHORT_RANGE_MODULE_TYPE_MAX_NUM,
} uShortRangeModuleType_t;

typedef enum {
    U_SHORT_RANGE_BLE_ROLE_DISABLED = 0, /**< BLE disabled. */
    U_SHORT_RANGE_BLE_ROLE_CENTRAL, /**< Central only mode. */
    U_SHORT_RANGE_BLE_ROLE_PERIPHERAL, /**< Peripheral only mode. */
    U_SHORT_RANGE_BLE_ROLE_CENTRAL_AND_PERIPHERAL, /**< Simutanious central and peripheral mode. */
} uShortRangeBleRole_t;

typedef enum {
    U_SHORT_RANGE_SERVER_DISABLED = 0, /**< Disabled status. */
    U_SHORT_RANGE_SERVER_SPS = 6 /**< SPS server. */
} uShortRangeServerType_t;

typedef struct {
    uShortRangeBleRole_t role;
    bool spsServer;
} uShortRangeBleCfg_t;


/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Initialise the short range driver.  If the driver is already
 * initialised then this function returns immediately.
 *
 * @return zero on success or negative error code on failure.
 */
int32_t uShortRangeInit();

/** Shut-down the short range driver.  All short range instances
 * will be removed internally with calls to uShortRangeRemove().
 */
void uShortRangeDeinit();

/** Add a short range instance.
 *
 * @param moduleType       the short range module type.
 * @param atHandle         the handle of the AT client to use.  This must
 *                         already have been created by the caller with
 *                         a buffer of size U_SHORT_RANGE_AT_BUFFER_LENGTH_BYTES.
 *                         If a short range instance has already been added
 *                         for this atHandle an error will be returned.
 * @return                 on success the handle of the short range instance,
 *                         else negative error code.
 */
int32_t uShortRangeAdd(uShortRangeModuleType_t moduleType,
                       uAtClientHandle_t atHandle);

/** Remove a short range instance.  It is up to the caller to ensure
 * that the short range module for the given instance has been disconnected
 * and/or powered down etc.; all this function does is remove the logical
 * instance.
 *
 * @param shortRangeHandle  the handle of the short range instance to remove.
 */
void uShortRangeRemove(int32_t shortRangeHandle);

/** Set a callback for incoming data.
  *
 * @param shortRangeHandle   the handle of the short range instance.
 * @param pCallback          callback function.
 * @param pCallbackParameter parameter included with the callback.
 * @return                   zero on success or negative error code
 *                           on failure.
 */
int32_t uShortRangeSetDataCallback(int32_t shortRangeHandle,
                                   void (*pCallback) (int32_t, size_t, char *, void *),
                                   void *pCallbackParameter);

/** Detect the module connected to the handle. Will attempt to change the mode on
 * the module to communicate with it. No change to UART configuration is done,
 * so even if this fails, as last attempt to recover, it could work to  re-init
 * the UART on a different baud rate. This sould recover that module if another
 * rate than the default one has been used.
 *
 * @param shortRangeHandle   the handle of the short range instance.
 * @return                   Module on success, U_SHORT_RANGE_MODULE_TYPE_INVALID
 *                           on failure.
 */
uShortRangeModuleType_t uShortRangeDetectModule(int32_t shortRangeHandle);

/** Send data
 * By design of the module, in command/data mode it will broacast on all connections
 * and in extended data mode (EDM) it only sends on the given channel. This is controlled
 * from ubx-lib with the choice of stream type provided to the at client (see uAtClientStream_t).
 * If u_network.h was used to set this up, EDM is used.
 *
 * If UART stream is used, uShortRangeDataMode() must be called before using this command.
 *
 * @param shortRangeHandle the handle of the short range instance.
 * @param channel          channel id. EDM only.
 * @param pData            pointer to the data.
 * @param length           length of data.
 * @return                 zero on success or negative error code
 *                         on failure.
 */
int32_t uShortRangeData(int32_t shortRangeHandle,
                        int32_t channel,
                        const char *pData,
                        int32_t length);

/** Sends "AT" to the short range module on which it should repond with "OK"
 * but takes no action. This checks that the module is ready to respond to commands.
 *
 * @param shortRangeHandle  the handle of the short range instance.
 * @return                  zero on success or negative error code
 *                          on failure.
 */
int32_t uShortRangeAttention(int32_t shortRangeHandle);

/** Configure the short range module.
 * Function is blocking and might require a module re-boot, this can mean
 * up to 500ms before it returns.
 *
 * @param shortRangeHandle  the handle of the short range instance.
 * @param pShortRangeBleCfg pointer to the struct holding the configuration.
 * @return                  zero on success or negative error code
 *                          on failure.
 */
int32_t uShortRangeConfigure(int32_t shortRangeHandle,
                             const uShortRangeBleCfg_t *pShortRangeBleCfg);

/** Checks ble role.
 *
 * @param shortRangeHandle the handle of the short range instance.
 * @param pRole            pointer to a variable that will get the role,
 *                         must not be NULL.
 * @return                 zero on success or negative error code
 *                         on failure.
 */
int32_t uShortRangeCheckBleRole(int32_t shortRangeHandle, uShortRangeBleRole_t *pRole);

/** Change to command mode by sending a escape sequence, can be used at
 * startup if uShortRangeAttention is unresponsive.
 *
 * @param shortRangeHandle  the handle of the short range instance.
 * @param pAtHandle         the place to put the new atHandle, cannot be NULL.
 * @return                  zero on success or negative error code
 *                          on failure.
 */
int32_t uShortRangeCommandMode(int32_t shortRangeHandle, uAtClientHandle_t *pAtHandle);


/** Change to data mode, no commands will be accepted in this mode and
 * the caller can send, and must handle the incoming, data directly on
 * the stream.
 *
 * @note: A delay of 50 ms is required before start of data transmission
 * @note: The original atHandle is no longer valid after this is called, at client is
 *        re-added when calling uShortRangeCommandMode.
 *
 * @param shortRangeHandle  the handle of the short range instance.
 * @return                  zero on success or negative error code
 *                          on failure.
 */
int32_t uShortRangeDataMode(int32_t shortRangeHandle);

/** Change to extended data mode
 * @note: A delay of 50 ms is required before start of data transmission
 *
 * @param shortRangeHandle  the handle of the short range instance.
 * @return                  zero on success or negative error code
 *                          on failure.
 */
int32_t uShortRangeSetEdm(int32_t shortRangeHandle);

/** Set a callback for bluetooth connection status.
 *
 * @param shortRangeHandle   the handle of the short range instance.
 * @param pCallback          callback function.
 * @param pCallbackParameter parameter included with the callback.
 * @return                   zero on success or negative error code
 *                           on failure.
 */
int32_t uShortRangeBtConnectionStatusCallback(int32_t shortRangeHandle,
                                              void (*pCallback) (int32_t, char *, void *),
                                              void *pCallbackParameter);

/** Set a callback for SPS connection status.
  *
 * @param shortRangeHandle   the handle of the short range instance.
 * @param pCallback          callback function.
 * @param pCallbackParameter parameter included with the callback.
 * @return                   zero on success or negative error code
 *                           on failure.
 */
int32_t uShortRangeSpsConnectionStatusCallback(int32_t shortRangeHandle,
                                               void (*pCallback) (int32_t, char *, int32_t, int32_t, int32_t, void *),
                                               void *pCallbackParameter);


/** Connect to a remote device
 *
 * @param shortRangeHandle the handle of the short range instance.
 * @param pAddress         address in 0012F398DD12p format

 * @return            zero on successful connection attempt. There is no
 *                    actual connection until the SPS callback reports
 *                    connected, else negative error code.
 */
int32_t uShortRangeConnectSps(int32_t shortRangeHandle, const char *pAddress);


/** Disconnect a connection
 *
 * @param shortRangeHandle  the handle of the short range instance.
 * @param connHandle  the handle of the connection.
 * @return            zero on success or negative error code
 *                    on failure.
 */
int32_t uShortRangeDisconnect(int32_t shortRangeHandle, int32_t connHandle);



#ifdef __cplusplus
}
#endif

#endif // _U_SHORT_RANGE_H_

// End of file
