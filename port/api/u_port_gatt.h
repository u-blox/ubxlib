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
 * distr�ibuted under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _U_PORT_GATT_H_
#define _U_PORT_GATT_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief Porting layer for GATT functions.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

// ATT permissions
#define U_PORT_GATT_ATT_PERM_READ          0x01
#define U_PORT_GATT_ATT_PERM_WRITE         0x02
#define U_PORT_GATT_ATT_PERM_READ_ENCRYPT  0x04
#define U_PORT_GATT_ATT_PERM_WRITE_ENCRYPT 0x08
#define U_PORT_GATT_ATT_PERM_READ_AUTHEN   0x10
#define U_PORT_GATT_ATT_PERM_WRITE_AUTHEN  0x20
#define U_PORT_GATT_ATT_PERM_PREPARE_WRITE 0x40

// GATT Characteristic Properties
#define U_PORT_GATT_CHRC_BROADCAST          0x01
#define U_PORT_GATT_CHRC_READ               0x02
#define U_PORT_GATT_CHRC_WRITE_WITHOUT_RESP 0x04
#define U_PORT_GATT_CHRC_WRITE              0x08
#define U_PORT_GATT_CHRC_NOTIFY             0x10
#define U_PORT_GATT_CHRC_INDICATE           0x20
#define U_PORT_GATT_CHRC_AUTH               0x40
#define U_PORT_GATT_CHRC_EXT_PROP           0x80

#define U_PORT_GATT_GAP_INVALID_CONNHANDLE     -1

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** UUID bit length types
 */
enum {
    U_PORT_GATT_UUID_TYPE_16,
    U_PORT_GATT_UUID_TYPE_32,
    U_PORT_GATT_UUID_TYPE_128,
};

/** General UUID type
 *  Used only to cast when passing pointers to
 *  UUIDs as arguments to functions
 *
 *  @param type UUID bit length (U_PORT_GATT_UUID_TYPE_*)
 */
typedef struct {
    uint8_t type;
} uPortGattUuid_t;

/** 16 bit UUID type
 */
typedef struct {
    uint8_t type;
    uint16_t val;
} uPortGattUuid16_t;

/** 32 bit UUID type
 */
typedef struct {
    uint8_t type;
    uint32_t val;
} uPortGattUuid32_t;

/** 128 bit UUID type
 */
typedef struct {
    uint8_t type;
    uint8_t val[16];
} uPortGattUuid128_t;

/** GAP connection parameters
 *
 *  @param scanInterval Scan interval (N*0.625 ms)
 *  @param scanWindow   Scan window (N*0.625 ms)
 *  @param createConnectionTmo Timeout before giving up if
 *                             remote device is not found in ms
 *  @param connIntervalMin Connection interval (N*1.25 ms)
 *  @param connIntervalMax Connection interval (N*1.25 ms)
 *  @param connLatency Connection lantency, nbr of connection intervals
 *  @param linkLossTimeout Link loss timeout in ms
 */
typedef struct {
    // For central
    uint16_t scanInterval;
    uint16_t scanWindow;
    uint32_t createConnectionTmo;
    uint16_t connIntervalMin;
    uint16_t connIntervalMax;
    uint16_t connLatency;
    uint32_t linkLossTimeout;
} uPortGattGapParams_t;

/** GATT Characteristic Descriptor type
 */
typedef enum {
    U_PORT_GATT_CHRC_DESC_EXT_PROP,
    U_PORT_GATT_CHRC_DESC_USER_DESCR,
    U_PORT_GATT_CHRC_DESC_CLIENT_CHAR_CONF,
    U_PORT_GATT_CHRC_DESC_SERVER_CHAR_CONF,
    U_PORT_GATT_CHRC_DESC_CHAR_PRESENTATION_FORMAT,
    U_PORT_GATT_CHRC_DESC_CHAR_AGGREGATE_FORMAT,
    U_PORT_GATT_NBR_OF_CHRC_DESC_TYPES,
} uPortGattCharDescriptorType_t;

/** GAP Connection status
 */
typedef enum {
    U_PORT_GATT_GAP_CONNECTED = 0,
    U_PORT_GATT_GAP_DISCONNECTED = 1,
} uPortGattGapConnStatus_t;

/** Connection status change callback
 *
 *  @param connHandle      Handle for GAP connection
 *  @param status         New status of connection
 *  @param pCallbackParam  Pointer to context given when setting callback
 *                         in uPortGattSetGapConnStatusCallback
 */
typedef void (*uPortGattGapConnStatusCallback_t)(int32_t connHandle,
                                                 uPortGattGapConnStatus_t status, void *pCallbackParam);

/** MTU exchanged callback
 *
 *  @param connHandle      Handle for GAP connection
 *  @param err             Equal to 0 if MTU exchange was ok
 */
typedef void (*mtuXchangeRespCallback_t)(int32_t connHandle, uint8_t err);

/** GATT attribute write callback type
 *
 *  @param pBuf    Pointer to buffer with values to write
 *  @param len     Number of bytes to write
 *  @param offset  Where to start to write
 *  @param flags   Indicates if this is a prepare write (bit 0,
 *                 only check authorization, do not write)
 *                 or a CMD, i.e. write without response (bit 1).
 */
typedef int32_t (*uPortGattAttWriteCallback_t)(int32_t connHandle, const void *pBuf, uint16_t len,
                                               uint16_t offset, uint8_t flags);

/** GATT read callback type
 *
 *  @param pBuf    Pointer to buffer where to put read values
 *  @param len     Number of bytes to read
 *  @param offset  Where to start to read
 */
typedef int32_t (*uPortGattAttReadCallback_t)(int32_t connHandle, const void *buf, uint16_t len,
                                              uint16_t offset);

/** GATT Attribute
 *
 *  @param permissions  Attribute permissions bit field
 *                      (U_PORT_GATT_ATT_PERM_*)
 *  @param write        Attribute write callback
 *  @param read         Attribute read callback
 */
typedef struct {
    uint8_t                     permissions;
    uPortGattAttWriteCallback_t write;
    uPortGattAttReadCallback_t  read;
} uPortGattAtt_t;

/** GATT Characteristic Descriptor configuration struct
 *
 *  @param descriptorType  Select one of 6 types from enum
 *  @param att             The descriptor attribute
 *  @param pNextDescriptor Pointer to next descriptor for
 *                         this characteristic, NULL if
 *                         this is the last descriptor
 */
typedef struct uPortGattCharDescriptor {
    uPortGattCharDescriptorType_t         descriptorType;
    uPortGattAtt_t                        att;
    const struct uPortGattCharDescriptor  *pNextDescriptor;
} uPortGattCharDescriptor_t;

/** GATT Characteristic configuration struct
 *
 *  @param pUuid            Pointer to characteristic UUID
 *  @param properties       Bit field with characteristic
 *                          properties (U_PORT_GATT_CHRC_*)
 *  @param att              The characteristic value attribute
 *  @param pFirstDescriptor Pointer to first characteristic descriptor, if any.
 *  @param pNextChar        Pointer to next characteristic in this service, NULL
 *                          if this is the last characteristic
 */
typedef struct uPortGattCharacteristic_s {
    uPortGattUuid_t                        *pUuid;
    uint8_t                                 properties;
    uPortGattAtt_t                          valueAtt;
    const uPortGattCharDescriptor_t        *pFirstDescriptor;
    const struct uPortGattCharacteristic_s *pNextChar;
} uPortGattCharacteristic_t;

/** GATT Service configuration struct
 *
 *  @param pUuid            Pointer to Service UUID
 *  @param pFirstChar       Pointer to first characteristic
 *                          in service
 */
typedef struct {
    uPortGattUuid_t                 *pUuid;
    const uPortGattCharacteristic_t *pFirstChar;
} uPortGattService_t;

/** Bluetooth address type
 */
typedef enum {
    U_PORT_BT_LE_ADDRESS_TYPE_RANDOM,
    U_PORT_BT_LE_ADDRESS_TYPE_PUBLIC,
    U_PORT_BT_LE_ADDRESS_TYPE_UNKNOWN,
} uPortBtLeAddressType_t;

/** GATT Iteration continue or stop
 */
typedef enum {
    U_PORT_GATT_ITER_STOP = 0,
    U_PORT_GATT_ITER_CONTINUE = 1,
} uPortGattIter_t;

/** GATT notify callback
 *
 * @param connHandle Connection handle
 * @param pParams    Pointer to subscription parameters
 * @param pData      Pointer to notification data
 * @param length     Size of notification data
 *
 * @return           Returning U_PORT_GATT_ITER_STOP will stop subscription
 */
struct uPortGattSubscribeParams_s;
typedef uPortGattIter_t (*uPortGattNotifyFunc_t)(int32_t connHandle,
                                                 struct uPortGattSubscribeParams_s *pParams,
                                                 const void *pData, uint16_t length);

/** GATT CCC write response callback
 *
 * @param connHandle Connection handle
 * @param err        Indicates if write went ok (0) or not
 */
typedef void (*uPortGattCccWriteResp_t)(int32_t connHandle, uint8_t err);

/** GATT Subscription parameters
 *
 * @param notifyCb             Callback which will be called on notifications from GATT server
 * @param cccWriteRespCb       Callback which will be called on CCC write response
 * @param valueHandle          Attribute handle for characteristic value
 * @param cccHandle            Attribute handle for Client Characteristic Config (CCC) value
 * @param receiveNotifications Set to true if you want to subscribe to notifications
 * @param receiveIndications   Set to true if you want to subscribe to indications
 */
typedef struct uPortGattSubscribeParams_s {
    uPortGattNotifyFunc_t    notifyCb;
    uPortGattCccWriteResp_t  cccWriteRespCb;
    uint16_t                 valueHandle;
    uint16_t                 cccHandle;
    bool                     receiveNotifications;
    bool                     receiveIndications;
} uPortGattSubscribeParams_t;

/** GATT Discovery Callback
 *
 * @param connHandle  Connection handle
 * @param pUuid       Pointer to UUID for discovered attribute
 *                    Is NULL if no more services were found
 * @param attrHandle  Service attribute handle
 *                    Is 0 if if no more services were found
 * @param endHandle   End attribute handle for discovered service
 *
 * @return           Return U_PORT_GATT_ITER_STOP to stop current discovery
 */
typedef uPortGattIter_t (*uPortGattServiceDiscoveryCallback_t)(
    int32_t connHandle,
    uPortGattUuid_t *pUuid,
    uint16_t attrHandle,
    uint16_t endHandle);

/** GATT characterstic discovery Callback
 *
 * @param connHandle Connection handle
 * @param pUuid      Pointer to UUID for discovered characterstic
 *                   Is NULL if no more characterstics were found
 * @param valHandle  Value handle for discovered characterstic
 *                   Is 0 if no more characterstics were found
 * @param properties Properties for discovered characterstic
 *
 * @return           Return U_PORT_GATT_ITER_STOP to stop current discovery
 */
typedef uPortGattIter_t (*uPortGattCharDiscoveryCallback_t)(
    int32_t connHandle,
    uPortGattUuid_t *pUuid,
    uint16_t attrHandle,
    uint16_t valueHandle,
    uint8_t  properties);

/** GATT descriptor discovery Callback
 *
 * @param connHandle Connection handle
 * @param pUuid      Pointer to UUID for discovered attribute
 *                   Is NULL if no more descriptors were found
 * @param attHandle  Attribute handle for discovered attribute
 *                   Is 0 if no more descriptors were found
 *
 * @return           Return U_PORT_GATT_ITER_STOP to stop current discovery
 */
typedef uPortGattIter_t (*uPortGattDescriptorDiscoveryCallback_t)(int32_t connHandle,
                                                                  uPortGattUuid_t *pUuid,
                                                                  uint16_t attrHandle);


extern const uPortGattGapParams_t uPortGattGapParamsDefault;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Initialise GATT.
 *
 * @return  zero on success else negative error code.
 */
int32_t uPortGattInit(void);

/** Shutdown GATT handling.
 */
void uPortGattDeinit(void);

/** Add a GATT instance
 *
 * @return                       a GATT handle else negative
 *                               error code.
 */
int32_t uPortGattAdd(void);

/** Add primary GATT service
 *
 * @param pService the struct defining the service
 *
 * @return a service handle if successfull
 *         otherwise negative error code
 */
int32_t uPortGattAddPrimaryService(const uPortGattService_t *pService);

/** Remove all registered services
 *
 * should not be done while GATT is up
 *
 * @return errorCode
 */
int32_t uPortGattRemoveAllServices(void);

/** Start GATT services
 *
 * @param startAdv Start advertising
 *
 * @return errorCode
 */
int32_t uPortGattUp(bool startAdv);

/** Check if device is advertising
 *
 * @return true if advertising
 */
bool uPortGattIsAdvertising(void);

/** End GATT services
 */
void uPortGattDown(void);

/** Set connection status callback
 *
 * @param pCallback       callback
 * @param pCallbackParam  Context pointer that will be sent as
 *                        argument when callback is called
 */
void uPortGattSetGapConnStatusCallback(uPortGattGapConnStatusCallback_t pCallback,
                                       void *pCallbackParam);

/** Get MTU for connection
 *
 * @param connHandle Connection handle
 *
 * @return MTU or error code
 */
int32_t uPortGattGetMtu(int32_t connHandle);

/** Exchange MTU with remote device
 *
 * @param connHandle   Connection handle
 * @param respCallback Callback that will be called when MTU is exchanged
 *
 * @return MTU or error code
 */
int32_t uPortGattExchangeMtu(int32_t connHandle, mtuXchangeRespCallback_t respCallback);

/** Send characteristic notification
 *
 * @param connHandle Connection handle
 * @param pChar      Pointer to characteristic
 * @param data       Pointer to notification data to send
 * @param len        Length of data to send
 */
int32_t uPortGattNotify(int32_t connHandle, const uPortGattCharacteristic_t *pChar,
                        const void *data, uint16_t len);

/** Connect GAP
 *
 * @param pAddress    Pointer to array with address (6 bytes)
 * @param addressType Public or random address
 * @param pGapParams  GAP connection parameters. Use NULL for default values.
 *
 * @return            connection handle
 */
int32_t uPortGattConnectGap(uint8_t *pAddress, uPortBtLeAddressType_t addressType,
                            const uPortGattGapParams_t *pGapParams);

/** Disconnect GAP
 *
 * @param connHandle  Connection handle
 *
 * @return            Error code
 */
int32_t uPortGattDisconnectGap(int32_t connHandle);

/** Read remote address
 *
 * @param connHandle  Connection handle
 * @param pAddr       Buffer where to write address, should be 6 bytes
 * @param pAddrType   Address type
 *
 * @return            Error code
 */
int32_t uPortGattGetRemoteAddress(int32_t connHandle, uint8_t *pAddr,
                                  uPortBtLeAddressType_t *pAddrType);

/** Write attribute on remote GATT server
 *
 * @param connHandle  Connection handle
 * @param handle      Characteristics handle
 * @param pData       Data to write
 * @param len         Length of data
 *
 * @return            Error code
 */
int32_t uPortGattWriteAttribute(int32_t connHandle, uint16_t handle, const void *pData,
                                uint16_t len);

/** Initiate subscription to notifications or indications from characteristic
 *
 * @param connHandle  Connection handle
 * @param pParams     Pointer to subscription parameters
 *                    the subscription parameters must be valid during whole
 *                    subscription
 *
 * @return            Error code
 */
int32_t uPortGattSubscribe(int32_t connHandle, uPortGattSubscribeParams_t *pParams);


/** Start discovery of primary GATT service
 *
 * @param connHandle  Connection handle
 * @param pUuid       Pointer to UUID of service to discover
 * @param callback    Callback that will be called in discovery
 *
 * @return            Error code
 */
int32_t uPortGattStartPrimaryServiceDiscovery(int32_t connHandle, const uPortGattUuid_t *pUuid,
                                              uPortGattServiceDiscoveryCallback_t callback);

/** Start discovery of GATT characteristic
 *
 * @param connHandle  Connection handle
 * @param pUuid       Pointer to UUID of characteristic to discover
 * @param startHandle Search from this handle and onwards
 * @param callback    Callback that will be called in discovery
 *
 * @return            Error code
 */
int32_t uPortGattStartCharacteristicDiscovery(int32_t connHandle, uPortGattUuid_t *pUuid,
                                              uint16_t startHandle, uPortGattCharDiscoveryCallback_t callback);

/** Start discovery of GATT characteristics descriptors
 *
 * @param connHandle  Connection handle
 * @param type        Type of descriptor
 * @param startHandle Search from this handle and onwards
 * @param callback    Callback that will be called in discovery
 *                    callback should return U_PORT_GATT_ITER_CONTINUE until
 *                    all descriptors in service is found, instead of restarting
 *                    descriptor discovery with this function.
 *
 * @return            Error code
 */
int32_t uPortGattStartDescriptorDiscovery(int32_t connHandle, uPortGattCharDescriptorType_t type,
                                          uint16_t startHandle, uPortGattDescriptorDiscoveryCallback_t callback);

#ifdef __cplusplus
}
#endif

#endif // _U_PORT_GATT_H_
