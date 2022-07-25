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

/** \addtogroup __port
 *  @{
 */

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

/** UUID bit length types.
 */
enum {
    U_PORT_GATT_UUID_TYPE_16,
    U_PORT_GATT_UUID_TYPE_32,
    U_PORT_GATT_UUID_TYPE_128,
};

/** General UUID type.
 *  Used only to cast when passing pointers to
 *  UUIDs as arguments to functions.
 *
 *  @param type UUID bit length (U_PORT_GATT_UUID_TYPE_*)
 */
typedef struct {
    uint8_t type;
} uPortGattUuid_t;

/** 16 bit UUID type.
 */
typedef struct {
    uint8_t type;
    uint16_t val;
} uPortGattUuid16_t;

/** 32 bit UUID type.
 */
typedef struct {
    uint8_t type;
    uint32_t val;
} uPortGattUuid32_t;

/** 128 bit UUID type.
 */
typedef struct {
    uint8_t type;
    uint8_t val[16];
} uPortGattUuid128_t;

/** GAP connection parameters
 *
 *  @param scanInterval        scan interval (N*0.625 ms).
 *  @param scanWindow          scan window (N*0.625 ms).
 *  @param createConnectionTmo timeout before giving up if
 *                             remote device is not found in ms.
 *  @param connIntervalMin     connection interval (N*1.25 ms).
 *  @param connIntervalMax     connection interval (N*1.25 ms).
 *  @param connLatency         connection lantency, nbr of connection intervals.
 *  @param linkLossTimeout     link loss timeout in ms.
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

/** GATT characteristic descriptor type
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

/** GAP connection status
 */
typedef enum {
    U_PORT_GATT_GAP_CONNECTED = 0,
    U_PORT_GATT_GAP_DISCONNECTED = 1,
} uPortGattGapConnStatus_t;

/** Connection status change callback.
 *
 * @param connHandle              handle for GAP connection.
 * @param status                  new status of connection.
 * @param[in,out] pCallbackParam  pointer to context given when setting callback
 *                                in uPortGattSetGapConnStatusCallback().
 */
typedef void (*uPortGattGapConnStatusCallback_t)(int32_t connHandle,
                                                 uPortGattGapConnStatus_t status,
                                                 void *pCallbackParam);

/** MTU exchanged callback.
 *
 * @param connHandle      handle for GAP connection.
 * @param err             equal to 0 if MTU exchange was OK.
 */
typedef void (*mtuXchangeRespCallback_t)(int32_t connHandle, uint8_t err);

/** GATT attribute write callback type.
 *
 * @param connHandle handle for GAP connection.
 * @param[in] pBuf   pointer to buffer with values to write.
 * @param len        number of bytes to write.
 * @param offset     where to start to write.
 * @param flags      indicates if this is a prepare write (bit 0,
 *                   only check authorization, do not write)
 *                   or a CMD, write without response (bit 1).
 */
typedef int32_t (*uPortGattAttWriteCallback_t)(int32_t connHandle, const void *pBuf, uint16_t len,
                                               uint16_t offset, uint8_t flags);

/** GATT read callback type.
 *
 * @param connHandle handle for GAP connection.
 * @param[in] pBuf   pointer to buffer where to put read values.
 * @param len        number of bytes to read.
 * @param offset     where to start to read.
 */
typedef int32_t (*uPortGattAttReadCallback_t)(int32_t connHandle, const void *pBuf, uint16_t len,
                                              uint16_t offset);

/** GATT attribute.
 *
 * @param permissions  attribute permissions bit field
 *                     (U_PORT_GATT_ATT_PERM_*).
 * @param write        attribute write callback.
 * @param read         attribute read callback.
 */
typedef struct {
    uint8_t                     permissions;
    uPortGattAttWriteCallback_t write;
    uPortGattAttReadCallback_t  read;
} uPortGattAtt_t;

/** GATT characteristic descriptor configuration struct.
 *
 * @param descriptorType  select one of 6 types from enum.
 * @param att             the descriptor attribute.
 * @param pNextDescriptor pointer to next descriptor for
 *                        this characteristic, NULL if
 *                        this is the last descriptor.
 */
typedef struct uPortGattCharDescriptor {
    uPortGattCharDescriptorType_t         descriptorType;
    uPortGattAtt_t                        att;
    const struct uPortGattCharDescriptor  *pNextDescriptor;
} uPortGattCharDescriptor_t;

/** GATT characteristic configuration struct.
 *
 * @param pUuid            pointer to characteristic UUID.
 * @param properties       bit field with characteristic
 *                         properties (U_PORT_GATT_CHRC_*).
 * @param att              the characteristic value attribute.
 * @param pFirstDescriptor pointer to first characteristic descriptor, if any.
 * @param pNextChar        pointer to next characteristic in this service, NULL
 *                         if this is the last characteristic.
 */
typedef struct uPortGattCharacteristic_s {
    uPortGattUuid_t                        *pUuid;
    uint8_t                                 properties;
    uPortGattAtt_t                          valueAtt;
    const uPortGattCharDescriptor_t        *pFirstDescriptor;
    const struct uPortGattCharacteristic_s *pNextChar;
} uPortGattCharacteristic_t;

/** GATT service configuration struct.
 *
 * @param pUuid            pointer to Service UUID.
 * @param pFirstChar       pointer to first characteristic in service.
 */
typedef struct {
    uPortGattUuid_t                 *pUuid;
    const uPortGattCharacteristic_t *pFirstChar;
} uPortGattService_t;

/** Bluetooth address type.
 */
typedef enum {
    U_PORT_BT_LE_ADDRESS_TYPE_RANDOM,
    U_PORT_BT_LE_ADDRESS_TYPE_PUBLIC,
    U_PORT_BT_LE_ADDRESS_TYPE_UNKNOWN,
} uPortBtLeAddressType_t;

/** GATT iteration continue or stop.
 */
typedef enum {
    U_PORT_GATT_ITER_STOP = 0,
    U_PORT_GATT_ITER_CONTINUE = 1,
} uPortGattIter_t;

/** GATT notify callback.
 *
 * @param connHandle     connection handle.
 * @param[in] pParams    pointer to subscription parameters.
 * @param[in] pData      pointer to notification data.
 * @param length         size of notification data.
 * @return               returning #U_PORT_GATT_ITER_STOP will stop subscription.
 */
struct uPortGattSubscribeParams_s;
typedef uPortGattIter_t (*uPortGattNotifyFunc_t)(int32_t connHandle,
                                                 struct uPortGattSubscribeParams_s *pParams,
                                                 const void *pData, uint16_t length);

/** GATT CCC write response callback.
 *
 * @param connHandle connection handle.
 * @param err        indicates if write went ok (0) or not.
 */
typedef void (*uPortGattCccWriteResp_t)(int32_t connHandle, uint8_t err);

/** GATT Subscription parameters.
 *
 * @param notifyCb             callback which will be called on notifications from GATT server.
 * @param cccWriteRespCb       callback which will be called on CCC write response.
 * @param valueHandle          attribute handle for characteristic value.
 * @param cccHandle            attribute handle for Client Characteristic Config (CCC) value.
 * @param receiveNotifications set to true if you want to subscribe to notifications.
 * @param receiveIndications   set to true if you want to subscribe to indications.
 */
typedef struct uPortGattSubscribeParams_s {
    uPortGattNotifyFunc_t    notifyCb;
    uPortGattCccWriteResp_t  cccWriteRespCb;
    uint16_t                 valueHandle;
    uint16_t                 cccHandle;
    bool                     receiveNotifications;
    bool                     receiveIndications;
} uPortGattSubscribeParams_t;

/** GATT discovery callback.
 *
 * @param connHandle   connection handle.
 * @param[in] pUuid    pointer to UUID for discovered attribute;
 *                     NULL if no more services were found.
 * @param attrHandle   service attribute handle;
 *                     0 if no more services were found.
 * @param endHandle    end attribute handle for discovered service.
 * @return             #U_PORT_GATT_ITER_STOP to stop current discovery.
 */
typedef uPortGattIter_t (*uPortGattServiceDiscoveryCallback_t)(
    int32_t connHandle,
    uPortGattUuid_t *pUuid,
    uint16_t attrHandle,
    uint16_t endHandle);

/** GATT characterstic discovery callback.
 *
 * @param connHandle   connection handle.
 * @param pUuid        pointer to UUID for discovered characterstic;
 *                     NULL if no more characterstics were found.
 * @param attrHandle   service attribute handle;
 *                     0 if no more services were found.
 * @param valueHandle  value handle for discovered characterstic;
 *                     0 if no more characterstics were found.
 * @param properties   properties for discovered characteristic.
 * @return             #U_PORT_GATT_ITER_STOP to stop current discovery.
 */
typedef uPortGattIter_t (*uPortGattCharDiscoveryCallback_t)(
    int32_t connHandle,
    uPortGattUuid_t *pUuid,
    uint16_t attrHandle,
    uint16_t valueHandle,
    uint8_t  properties);

/** GATT descriptor discovery callback.
 *
 * @param connHandle connection handle.
 * @param[in] pUuid  pointer to UUID for discovered attribute;
 *                   NULL if no more descriptors were found.
 * @param attrHandle attribute handle for discovered attribute;
 *                   0 if no more descriptors were found.
 * @return           #U_PORT_GATT_ITER_STOP to stop current discovery.
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

/** Add a GATT instance.
 *
 * @return a GATT handle else negative error code.
 */
int32_t uPortGattAdd(void);

/** Add primary GATT service.
 *
 * @param[in] pService the struct defining the service.
 * @return             a service handle if successful
 *                     otherwise negative error code.
 */
int32_t uPortGattAddPrimaryService(const uPortGattService_t *pService);

/** Remove all registered services; should not be
 * called while GATT is up.
 *
 * @return zero on success else negative error code.
 */
int32_t uPortGattRemoveAllServices(void);

/** Start GATT services.
 *
 * @param startAdv start advertising.
 * @return         zero on success else negative error code.
 */
int32_t uPortGattUp(bool startAdv);

/** Check if device is advertising.
 *
 * @return true if advertising.
 */
bool uPortGattIsAdvertising(void);

/** End GATT services.
 */
void uPortGattDown(void);

/** Set connection status callback.
 *
 * @param[in] pCallback       callback.
 * @param[in] pCallbackParam  context pointer that will be sent as
 *                            argument when callback is called.
 */
void uPortGattSetGapConnStatusCallback(uPortGattGapConnStatusCallback_t pCallback,
                                       void *pCallbackParam);

/** Get MTU for connection.
 *
 * @param connHandle connection handle.
 * @return           MTU or negative error code.
 */
int32_t uPortGattGetMtu(int32_t connHandle);

/** Exchange MTU with remote device.
 *
 * @param connHandle   connection handle.
 * @param respCallback callback that will be called when MTU is exchanged.
 * @return             MTU or negative error code.
 */
int32_t uPortGattExchangeMtu(int32_t connHandle,
                             mtuXchangeRespCallback_t respCallback);

/** Send characteristic notification.
 *
 * @param connHandle     connection handle.
 * @param[in] pChar      pointer to characteristic.
 * @param[in] data       pointer to notification data to send.
 * @param len            length of data to send.
 */
int32_t uPortGattNotify(int32_t connHandle,
                        const uPortGattCharacteristic_t *pChar,
                        const void *data, uint16_t len);

/** Connect GAP.
 *
 * @param[in] pAddress    pointer to array with address (6 bytes).
 * @param addressType     public or random address.
 * @param[in] pGapParams  GAP connection parameters; use NULL for default values.
 * @return               connection handle.
 */
int32_t uPortGattConnectGap(uint8_t *pAddress,
                            uPortBtLeAddressType_t addressType,
                            const uPortGattGapParams_t *pGapParams);

/** Disconnect GAP.
 *
 * @param connHandle  connection handle.
 * @return            zero on success else negative error code.
 */
int32_t uPortGattDisconnectGap(int32_t connHandle);

/** Read remote address.
 *
 * @param connHandle     connection handle.
 * @param[out] pAddr     buffer where to write address, should be 6 bytes.
 * @param[out] pAddrType address type.
 * @return               zero on success else negative error code.
 */
int32_t uPortGattGetRemoteAddress(int32_t connHandle, uint8_t *pAddr,
                                  uPortBtLeAddressType_t *pAddrType);

/** Write attribute on remote GATT server.
 *
 * @param connHandle  connection handle.
 * @param handle      characteristics handle.
 * @param[in] pData   data to write.
 * @param len         length of data.
 * @return            zero on success else negative error code.
 */
int32_t uPortGattWriteAttribute(int32_t connHandle, uint16_t handle, const void *pData,
                                uint16_t len);

/** Initiate subscription to notifications or indications
 * from characteristic.
 *
 * @param connHandle  connection handle.
 * @param[in] pParams pointer to subscription parameters; the subscription
 *                    parameters must be valid during whole subscription.
 * @return            zero on success else negative error code.
 */
int32_t uPortGattSubscribe(int32_t connHandle, uPortGattSubscribeParams_t *pParams);

/** Start discovery of primary GATT service.
 *
 * @param connHandle  connection handle.
 * @param[in] pUuid   pointer to UUID of service to discover.
 * @param callback    callback that will be called in discovery.
 * @return            zero on success else negative error code.
 */
int32_t uPortGattStartPrimaryServiceDiscovery(int32_t connHandle,
                                              const uPortGattUuid_t *pUuid,
                                              uPortGattServiceDiscoveryCallback_t callback);

/** Start discovery of GATT characteristic.
 *
 * @param connHandle  connection handle.
 * @param[in] pUuid   pointer to UUID of characteristic to discover.
 * @param startHandle search from this handle and onwards.
 * @param callback    callback that will be called in discovery.
 * @return            zero on success else negative error code.
 */
int32_t uPortGattStartCharacteristicDiscovery(int32_t connHandle,
                                              uPortGattUuid_t *pUuid,
                                              uint16_t startHandle,
                                              uPortGattCharDiscoveryCallback_t callback);

/** Start discovery of GATT characteristics descriptors.
 *
 * @param connHandle  connection handle.
 * @param type        type of descriptor.
 * @param startHandle search from this handle and onwards.
 * @param callback    callback that will be called in discovery; the
 *                    callback should return #U_PORT_GATT_ITER_CONTINUE
 *                    until all descriptors in service is found, instead
 *                    of restarting descriptor discovery with this function.
 * @return            zero on success else negative error code.
 */
int32_t uPortGattStartDescriptorDiscovery(int32_t connHandle,
                                          uPortGattCharDescriptorType_t type,
                                          uint16_t startHandle,
                                          uPortGattDescriptorDiscoveryCallback_t callback);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_PORT_GATT_H_
