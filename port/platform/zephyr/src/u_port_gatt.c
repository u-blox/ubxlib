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

/** @file
 * @brief Implementation of the port UART API for the NRF53 platform.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include <zephyr/types.h>
#include <zephyr.h>

#include <device.h>
#include <soc.h>

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdio.h"

#include "u_cfg_sw.h"
#include "u_cfg_hw_platform_specific.h"
#include "u_error_common.h"
#include "u_port_debug.h"
#include "u_port.h"
#include "u_port_gatt.h"
#include "u_port_os.h"
#include "u_port_event_queue.h"

#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>

#include "string.h" // For memcpy()

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** @brief Maximum number of user services. **/
#ifndef U_PORT_GATT_MAX_NBR_OF_USER_SERVICES
#define U_PORT_GATT_MAX_NBR_OF_USER_SERVICES 1
#endif

/** @brief Maximum total number of ATT attributes in services.
 * A service declaration uses one attribute.
 * A characteristic definition uses two attributes, one for
 * the declaration and one for the value.
 * A characteristic descriptor uses one attribute. **/
#ifndef U_PORT_GATT_MAX_NBR_OF_ATTRIBUTES
#define U_PORT_GATT_MAX_NBR_OF_ATTRIBUTES 10
#endif

/** @brief Maximum total number of GATT characteristics in services. **/
#ifndef U_PORT_GATT_MAX_NBR_OF_CHARACTERISTICS
#define U_PORT_GATT_MAX_NBR_OF_CHARACTERISTICS 3
#endif

#ifndef U_PORT_GATT_MAX_NBR_OF_SUBSCRIBTIONS
#define U_PORT_GATT_MAX_NBR_OF_SUBSCRIBTIONS 4
#endif

#define U_PORT_GATT_CHRC_DESC_EXT_PROP_UUID                 0x2900
#define U_PORT_GATT_CHRC_DESC_USER_DESCR_UUID               0x2901
#define U_PORT_GATT_CHRC_DESC_CLIENT_CHAR_CONF_UUID         0x2902
#define U_PORT_GATT_CHRC_DESC_SERVER_CHAR_CONF_UUID         0x2903
#define U_PORT_GATT_CHRC_DESC_CHAR_PRESENTATION_FORMAT_UUID 0x2904
#define U_PORT_GATT_CHRC_DESC_CHAR_AGGREGATE_FORMAT_UUID    0x2905

#ifndef U_PORT_BLE_DEVICE_NAME
#define U_PORT_BLE_DEVICE_NAME CONFIG_BT_DEVICE_NAME
#endif

#define INVALID_HANDLE 0xffffffff

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */
typedef struct {
    int32_t connHandle;
    struct bt_gatt_subscribe_params zParams;
    uPortGattSubscribeParams_t     *pUparams;
} subscribeParams_t;

typedef struct {
    struct bt_conn                *pConn;
    subscribeParams_t             *pOngoingSubscribe;
    mtuXchangeRespCallback_t       mtuXchangeCallback;
    void                          *discoveryCallback;
    struct bt_gatt_discover_params discoverParams;
} gattConnection_t;

/* ----------------------------------------------------------------
 * Static Prototypes
 * -------------------------------------------------------------- */
static ssize_t onAttRead(struct bt_conn *conn,
                         const struct bt_gatt_attr *attr,
                         void *buf, uint16_t len,
                         uint16_t offset);

static ssize_t onAttWrite(struct bt_conn *conn,
                          const struct bt_gatt_attr *attr,
                          const void *buf, uint16_t len,
                          uint16_t offset, uint8_t flags);
static subscribeParams_t *pFindFreeSubscription(void);
static subscribeParams_t *pFindSubscription(struct bt_gatt_subscribe_params *pZsub);
static void deleteAllSubscriptions(int32_t connHandle);
static int32_t findConnHandle(struct bt_conn *pConn);
static int32_t findFreeConnHandle(void);
static bool validConnHandle(int32_t connHandle);
static void gapConnected(struct bt_conn *conn, uint8_t err);
static void gapDisconnected(struct bt_conn *conn, uint8_t reason);
static void countGattNodes(const uPortGattService_t *pService, uint32_t *pNbrOfAttr,
                           uint32_t *pNbrOfChrc);
static void writeServiceDeclaration(struct bt_gatt_attr **ppAttr,
                                    struct bt_uuid *pTypeUuid,
                                    struct bt_uuid *pServiceUuid);
static void writeCharDeclaration(struct bt_gatt_attr **ppAttr,
                                 struct bt_gatt_chrc **ppChrc,
                                 const uPortGattCharacteristic_t *pPortChar);
static int32_t addServiceInternal(struct bt_uuid *pTypeUuid, const uPortGattService_t *pService);
static uint8_t portAddrTypeToZephyrAddrType(uPortBtLeAddressType_t portAddrType);
static uint8_t notifyCallback(struct bt_conn *conn,
                              struct bt_gatt_subscribe_params *params,
                              const void *data, uint16_t length);
static void cccWriteResponseCb(struct bt_conn *conn, uint8_t err,
                               struct bt_gatt_write_params *params);
static uint8_t onDiscovery(struct bt_conn *conn,
                           const struct bt_gatt_attr *attr,
                           struct bt_gatt_discover_params *params);
static int32_t startDiscovery(int32_t connHandle, const uPortGattUuid_t *pUuid,
                              uint16_t startHandle, uint16_t endHandle,
                              void *callback, uint8_t type);
static void gattXchangeMtuRsp(struct bt_conn *conn, uint8_t err,
                              struct bt_gatt_exchange_params *params);

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */
static bool gGattUp = false;
static gattConnection_t gCurrentConnections[CONFIG_BT_MAX_CONN];

// Server variables
static struct bt_gatt_service gService[U_PORT_GATT_MAX_NBR_OF_USER_SERVICES];
static uint32_t gNextFreeServiceIndex = 0;
static struct bt_gatt_attr gAttrPool[U_PORT_GATT_MAX_NBR_OF_ATTRIBUTES];
static struct bt_gatt_attr *gpNextFreeAttr = gAttrPool;
static struct bt_gatt_chrc gChrcPool[U_PORT_GATT_MAX_NBR_OF_CHARACTERISTICS];
static struct bt_gatt_chrc *gpNextFreeChrc = gChrcPool;
static struct bt_data gScanResponseData[U_PORT_GATT_MAX_NBR_OF_USER_SERVICES];

// Client variables
static subscribeParams_t gSubscribeParams[U_PORT_GATT_MAX_NBR_OF_SUBSCRIBTIONS];

static uint32_t gAdvIndex = 0;
static bool gAdvertising = false;

static struct bt_conn_cb conn_callbacks = {
    .connected    = gapConnected,
    .disconnected = gapDisconnected,
};

static uPortGattGapConnStatusCallback_t pGapConnStatusCallback;
static void *pGapConnStatusParam;


static const struct bt_uuid_16 primaryServiceUuid   = {{BT_UUID_TYPE_16}, 0x2800};
// not used for the moment: static const struct bt_uuid_16 secondaryServiceUuid = {{BT_UUID_TYPE_16}, 0x2801};
// not used for the moment: static const struct bt_uuid_16 includeUuid          = {{BT_UUID_TYPE_16}, 0x2802};
static const struct bt_uuid_16 charDeclUuid         = {{BT_UUID_TYPE_16}, 0x2803};

static const struct bt_uuid_16 charDescriptorsUuid[U_PORT_GATT_NBR_OF_CHRC_DESC_TYPES] = {
    [U_PORT_GATT_CHRC_DESC_EXT_PROP]                 = {{BT_UUID_TYPE_16}, U_PORT_GATT_CHRC_DESC_EXT_PROP_UUID},
    [U_PORT_GATT_CHRC_DESC_USER_DESCR]               = {{BT_UUID_TYPE_16}, U_PORT_GATT_CHRC_DESC_USER_DESCR_UUID},
    [U_PORT_GATT_CHRC_DESC_CLIENT_CHAR_CONF]         = {{BT_UUID_TYPE_16}, U_PORT_GATT_CHRC_DESC_CLIENT_CHAR_CONF_UUID},
    [U_PORT_GATT_CHRC_DESC_SERVER_CHAR_CONF]         = {{BT_UUID_TYPE_16}, U_PORT_GATT_CHRC_DESC_SERVER_CHAR_CONF_UUID},
    [U_PORT_GATT_CHRC_DESC_CHAR_PRESENTATION_FORMAT] = {{BT_UUID_TYPE_16}, U_PORT_GATT_CHRC_DESC_CHAR_PRESENTATION_FORMAT_UUID},
    [U_PORT_GATT_CHRC_DESC_CHAR_AGGREGATE_FORMAT]    = {{BT_UUID_TYPE_16}, U_PORT_GATT_CHRC_DESC_CHAR_AGGREGATE_FORMAT_UUID},
};

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, U_PORT_BLE_DEVICE_NAME, sizeof U_PORT_BLE_DEVICE_NAME - 1),
};

const uPortGattGapParams_t uPortGattGapParamsDefault = {48, 48, 5000, 24, 30, 0, 2000};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */
static ssize_t onAttRead(struct bt_conn *conn,
                         const struct bt_gatt_attr *attr,
                         void *buf, uint16_t len,
                         uint16_t offset)
{
    ssize_t returnValue = -1;
    int32_t connHandle = findConnHandle(conn);

    if ((attr->user_data != NULL) && (connHandle != U_PORT_GATT_GAP_INVALID_CONNHANDLE)) {
        uPortGattAtt_t *pPortAtt = (uPortGattAtt_t *)attr->user_data;
        if (pPortAtt->read != NULL) {
            returnValue = (ssize_t)pPortAtt->read(connHandle, buf, len, offset);
        }
    }

    return returnValue;
}

static ssize_t onAttWrite(struct bt_conn *conn,
                          const struct bt_gatt_attr *attr,
                          const void *buf, uint16_t len,
                          uint16_t offset, uint8_t flags)
{
    ssize_t returnValue = -1;
    int32_t connHandle = findConnHandle(conn);

    if ((attr->user_data != NULL) && (connHandle != U_PORT_GATT_GAP_INVALID_CONNHANDLE)) {
        uPortGattAtt_t *pPortAtt = (uPortGattAtt_t *)attr->user_data;
        if (pPortAtt->write != NULL) {
            returnValue = (ssize_t)pPortAtt->write(connHandle, buf, len, offset, flags);
        }
    }

    return returnValue;
}

static subscribeParams_t *pFindFreeSubscription(void)
{
    uint32_t i;
    subscribeParams_t *pSub = NULL;

    for (i = 0; i < U_PORT_GATT_MAX_NBR_OF_SUBSCRIBTIONS; i++) {
        if (gSubscribeParams[i].pUparams == NULL) {
            pSub = &gSubscribeParams[i];
            break;
        }
    }
    if (pSub == NULL) {
        uPortLog("U_PORT_GATT: Out of subscriptions!\n");
    }

    return pSub;
}

static subscribeParams_t *pFindSubscription(struct bt_gatt_subscribe_params *pZsub)
{
    uint32_t i;
    subscribeParams_t *pSub = NULL;

    for (i = 0; i < U_PORT_GATT_MAX_NBR_OF_SUBSCRIBTIONS; i++) {
        if (pZsub == &gSubscribeParams[i].zParams) {
            pSub = &gSubscribeParams[i];
            break;
        }
    }

    return pSub;
}

static void deleteAllSubscriptions(int32_t connHandle)
{
    for (int ii = 0; ii < U_PORT_GATT_MAX_NBR_OF_SUBSCRIBTIONS; ii++) {
        if (gSubscribeParams[ii].connHandle == connHandle) {
            gSubscribeParams[ii].pUparams = NULL;
        }
    }
}

static int32_t findConnHandle(struct bt_conn *pConn)
{
    uint32_t i;
    int32_t connHandle = U_PORT_GATT_GAP_INVALID_CONNHANDLE;

    for (i = 0; i < CONFIG_BT_MAX_CONN; i++) {
        if (gCurrentConnections[i].pConn == pConn) {
            connHandle = i;
        }
    }

    return connHandle;
}

static int32_t findFreeConnHandle(void)
{
    int32_t i;
    int32_t connHandle = U_PORT_GATT_GAP_INVALID_CONNHANDLE;

    for (i = 0; i < CONFIG_BT_MAX_CONN; i++) {
        if (gCurrentConnections[i].pConn == NULL) {
            connHandle = i;
            break;
        }
    }
    return connHandle;
}

static bool validConnHandle(int32_t connHandle)
{
    if ((connHandle < CONFIG_BT_MAX_CONN) &&
        (connHandle != U_PORT_GATT_GAP_INVALID_CONNHANDLE) &&
        (gCurrentConnections[connHandle].pConn != NULL)) {
        return true;
    } else {
        return false;
    }
}

static void gapConnected(struct bt_conn *conn, uint8_t err)
{
    char addr[BT_ADDR_LE_STR_LEN];
    int32_t connHandle = findConnHandle(conn);

    if (err) {
        if (connHandle != U_PORT_GATT_GAP_INVALID_CONNHANDLE) {
            bt_conn_unref(conn);
            gCurrentConnections[connHandle].pConn = NULL;
        }
        uPortLog("U_PORT_GATT: GAP Connection failed (err %u)\n", err);
        if (pGapConnStatusCallback != NULL) {
            pGapConnStatusCallback(connHandle, 1, pGapConnStatusParam);
        }
        if (gAdvertising) {
            (void)bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), gScanResponseData, gAdvIndex);
        }
        return;
    }

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    uPortLog("U_PORT_GATT: GAP Connected %s\n", addr);

    if (connHandle == U_PORT_GATT_GAP_INVALID_CONNHANDLE) {
        // Since there was no handle the connection must have been
        // initiated by the remote device
        connHandle = findFreeConnHandle();
        // If we initiate the connection with bt_conn_le_create the reference
        // is incremented automatically
        // When the remote has initiated the connection
        // we have to increment the reference count manually
        conn = bt_conn_ref(conn);
    }

    if (connHandle != U_PORT_GATT_GAP_INVALID_CONNHANDLE) {
        gCurrentConnections[connHandle].pConn = conn;
        if (pGapConnStatusCallback != NULL) {
            pGapConnStatusCallback(connHandle, U_PORT_GATT_GAP_CONNECTED, pGapConnStatusParam);
        }
    }
}

static void gapDisconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];
    int32_t connHandle;

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    uPortLog("U_PORT_GATT: GAP Disconnected: %s (reason %u)\n", addr, reason);

    connHandle = findConnHandle(conn);

    if (connHandle != U_PORT_GATT_GAP_INVALID_CONNHANDLE) {
        if (pGapConnStatusCallback != NULL) {
            pGapConnStatusCallback(connHandle, U_PORT_GATT_GAP_DISCONNECTED, pGapConnStatusParam);
        }
        bt_conn_unref(conn);
        gCurrentConnections[connHandle].pConn = NULL;
        deleteAllSubscriptions(connHandle);
        if (gAdvertising) {
            (void)bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), gScanResponseData, gAdvIndex);
        }
    }
}

static void countGattNodes(const uPortGattService_t *pService, uint32_t *pNbrOfAttr,
                           uint32_t *pNbrOfChrc)
{
    const uPortGattCharacteristic_t *pChar;
    uint32_t nbrOfChrc = 0;
    uint32_t nbrOfAttr = 1; // There is always the service declaration attribute

    pChar = pService->pFirstChar;
    while (pChar != NULL) {
        const uPortGattCharDescriptor_t *pCharDesc = pChar->pFirstDescriptor;
        // One attribute for each characteristic declaration,
        // and one for each characteristic value
        nbrOfAttr += 2;
        nbrOfChrc++;

        while (pCharDesc != NULL) {
            nbrOfAttr++;
            pCharDesc = pCharDesc->pNextDescriptor;
        }
        pChar = pChar->pNextChar;
    }

    *pNbrOfAttr = nbrOfAttr;
    *pNbrOfChrc = nbrOfChrc;
}

static void writeServiceDeclaration(struct bt_gatt_attr **ppAttr,
                                    struct bt_uuid *pTypeUuid,
                                    struct bt_uuid *pServiceUuid)
{
    struct bt_gatt_attr *pAttr = *ppAttr;

    pAttr->uuid = pTypeUuid;
    pAttr->handle = 0;
    pAttr->perm = BT_GATT_PERM_READ;
    pAttr->read = bt_gatt_attr_read_service;
    pAttr->write = NULL;
    pAttr->user_data = pServiceUuid;
    pAttr++;

    *ppAttr = pAttr;
}


static void writeCharDeclaration(struct bt_gatt_attr **ppAttr,
                                 struct bt_gatt_chrc **ppChrc,
                                 const uPortGattCharacteristic_t *pPortChar)
{
    struct bt_gatt_attr *pAttr = *ppAttr;
    struct bt_gatt_chrc *pChrc = *ppChrc;
    const uPortGattCharDescriptor_t *pCharDesc = pPortChar->pFirstDescriptor;

    // Set up the user_data struct for the
    // characteristic declaration attribute
    pChrc->uuid = (struct bt_uuid *)pPortChar->pUuid;
    pChrc->properties = pPortChar->properties;
    pChrc->value_handle = 0;

    // Add and set up the characteristic declaration attribute
    pAttr->uuid = (struct bt_uuid *)&charDeclUuid;
    pAttr->handle = 0;
    pAttr->perm = BT_GATT_PERM_READ;
    pAttr->read = bt_gatt_attr_read_chrc;
    pAttr->write = NULL;
    pAttr->user_data = pChrc;
    pAttr++;

    // Add and set up the characteristic value attribute
    pAttr->uuid = (struct bt_uuid *)(pPortChar->pUuid);
    pAttr->handle = 0;
    pAttr->perm = pPortChar->valueAtt.permissions;
    pAttr->read = onAttRead;
    pAttr->write = onAttWrite;
    pAttr->user_data = (void *) & (pPortChar->valueAtt);
    pAttr++;

    // Add and set up any characteristic descriptor attributes
    while (pCharDesc != NULL) {
        pAttr->uuid = (struct bt_uuid *)&charDescriptorsUuid[pCharDesc->descriptorType];
        pAttr->handle = 0;
        pAttr->perm = pCharDesc->att.permissions;
        pAttr->read = onAttRead;
        pAttr->write = onAttWrite;
        pAttr->user_data = (void *) & (pCharDesc->att);
        pAttr++;
        pCharDesc = pCharDesc->pNextDescriptor;
    }

    pChrc++;
    *ppAttr = pAttr;
    *ppChrc = pChrc;
}

static int32_t addServiceInternal(struct bt_uuid *pTypeUuid, const uPortGattService_t *pService)
{
    struct bt_gatt_attr *pAttr = gpNextFreeAttr;
    struct bt_gatt_chrc *pChrc = gpNextFreeChrc;
    uint32_t nbrOfAttr;
    uint32_t nbrOfChrc;
    uint32_t serviceIndex = gNextFreeServiceIndex;
    const uPortGattCharacteristic_t *pChar;

    countGattNodes(pService, &nbrOfAttr, &nbrOfChrc);

    if ((serviceIndex >= U_PORT_GATT_MAX_NBR_OF_USER_SERVICES) ||
        (pAttr + nbrOfAttr > gAttrPool + U_PORT_GATT_MAX_NBR_OF_ATTRIBUTES) ||
        (pChrc + nbrOfChrc > gChrcPool + U_PORT_GATT_MAX_NBR_OF_CHARACTERISTICS)) {
        return U_ERROR_COMMON_NO_MEMORY;
    }

    gService[serviceIndex].attrs = pAttr;
    writeServiceDeclaration(&pAttr, pTypeUuid, (struct bt_uuid *)pService->pUuid);
    pChar = pService->pFirstChar;
    while (pChar != NULL) {
        writeCharDeclaration(&pAttr, &pChrc, pChar);
        pChar = pChar->pNextChar;
    }
    gpNextFreeAttr = pAttr;
    gpNextFreeChrc = pChrc;
    gNextFreeServiceIndex++;

    gService[serviceIndex].attr_count = nbrOfAttr;

    return serviceIndex;
}

static uint8_t portAddrTypeToZephyrAddrType(uPortBtLeAddressType_t portAddrType)
{
    switch (portAddrType) {
        case U_PORT_BT_LE_ADDRESS_TYPE_RANDOM:
            return BT_ADDR_LE_RANDOM;
        case U_PORT_BT_LE_ADDRESS_TYPE_PUBLIC:
            return BT_ADDR_LE_PUBLIC;
        default:
            return BT_ADDR_LE_PUBLIC;
    }
}

static uint8_t notifyCallback(struct bt_conn *conn,
                              struct bt_gatt_subscribe_params *params,
                              const void *data, uint16_t length)
{
    uint8_t returnValue = BT_GATT_ITER_STOP;
    int32_t connHandle = findConnHandle(conn);
    subscribeParams_t *pSub = pFindSubscription(params);

    if ((pSub != NULL) && (connHandle != U_PORT_GATT_GAP_INVALID_CONNHANDLE)) {
        switch (pSub->pUparams->notifyCb(connHandle, pSub->pUparams, data, length)) {
            case U_PORT_GATT_ITER_CONTINUE:
                returnValue = BT_GATT_ITER_CONTINUE;
                break;
            case U_PORT_GATT_ITER_STOP:
            default:
                returnValue = BT_GATT_ITER_STOP;
                break;
        }
    }

    return returnValue;
}

static void cccWriteResponseCb(struct bt_conn *conn, uint8_t err,
                               struct bt_gatt_write_params *params)
{
    int32_t connHandle = findConnHandle(conn);
    (void)params;
    // Make sure that we are currently setting up a subscription,
    // that it is on the same connection and that there is a callback given
    if ((connHandle != U_PORT_GATT_GAP_INVALID_CONNHANDLE) &&
        (gCurrentConnections[connHandle].pOngoingSubscribe != NULL) &&
        (gCurrentConnections[connHandle].pOngoingSubscribe->pUparams->cccWriteRespCb != NULL)) {
        // Save the pointer to the current ongoing subscription
        // When we call the callback it could start a new subscription which
        // would alter the global pointer
        subscribeParams_t *pSub = gCurrentConnections[connHandle].pOngoingSubscribe;
        pSub->pUparams->cccWriteRespCb(connHandle, err);
        if (gCurrentConnections[connHandle].pOngoingSubscribe == pSub) {
            // The callback did not change the ongoing subscription which means
            // it did not start a new one
            gCurrentConnections[connHandle].pOngoingSubscribe = NULL;
        }
    }
}

static uint8_t onDiscovery(struct bt_conn *conn,
                           const struct bt_gatt_attr *attr,
                           struct bt_gatt_discover_params *params)
{
    uint8_t returnValue = BT_GATT_ITER_STOP;
    int32_t connHandle = findConnHandle(conn);

    if (connHandle != U_PORT_GATT_GAP_INVALID_CONNHANDLE) {

        switch (params->type) {

            case BT_GATT_DISCOVER_PRIMARY: {
                uPortGattServiceDiscoveryCallback_t callback =
                    (uPortGattServiceDiscoveryCallback_t)gCurrentConnections[connHandle].discoveryCallback;
                if ((callback != NULL) && (params != NULL)) {
                    if (attr == NULL) {
                        (void)callback(connHandle, NULL, 0, 0);
                    } else {
                        struct bt_gatt_service_val *p = (struct bt_gatt_service_val *)attr->user_data;
                        returnValue = callback(connHandle, (uPortGattUuid_t *)(p->uuid), attr->handle, p->end_handle);
                    }
                }
                break;
            }

            case BT_GATT_DISCOVER_CHARACTERISTIC: {
                uPortGattCharDiscoveryCallback_t callback =
                    (uPortGattCharDiscoveryCallback_t)gCurrentConnections[connHandle].discoveryCallback;
                if ((callback != NULL) && (params != NULL)) {
                    if (attr == NULL) {
                        (void)callback(connHandle, NULL, 0, 0, 0);
                    } else {
                        struct bt_gatt_chrc *p = (struct bt_gatt_chrc *)attr->user_data;
                        returnValue = callback(connHandle, (uPortGattUuid_t *)(p->uuid), attr->handle, p->value_handle,
                                               p->properties);
                    }
                }
                break;
            }

            case BT_GATT_DISCOVER_DESCRIPTOR: {
                uPortGattDescriptorDiscoveryCallback_t callback =
                    (uPortGattDescriptorDiscoveryCallback_t)gCurrentConnections[connHandle].discoveryCallback;
                if ((callback != NULL) && (params != NULL)) {
                    if (attr == NULL) {
                        (void)callback(connHandle, NULL, 0);
                    } else {
                        returnValue = callback(connHandle, (uPortGattUuid_t *)(attr->uuid), attr->handle);
                    }
                }
                break;
            }

            default:
                break;
        }
    }

    return returnValue;
}

static int32_t startDiscovery(int32_t connHandle, const uPortGattUuid_t *pUuid,
                              uint16_t startHandle, uint16_t endHandle,
                              void *callback, uint8_t type)
{
    int32_t errorCode = U_ERROR_COMMON_UNKNOWN;
    gattConnection_t *pConn;

    if ((callback == NULL) || !validConnHandle(connHandle)) {
        return U_ERROR_COMMON_INVALID_PARAMETER;
    }

    pConn = &gCurrentConnections[connHandle];
    pConn->discoverParams.uuid = (struct bt_uuid *)pUuid;
    pConn->discoverParams.func = onDiscovery;
    pConn->discoverParams.start_handle = startHandle;
    pConn->discoverParams.end_handle = endHandle;
    pConn->discoverParams.type = type;
    gCurrentConnections[connHandle].discoveryCallback = callback;
    if (bt_gatt_discover(gCurrentConnections[connHandle].pConn, &(pConn->discoverParams)) == 0) {
        errorCode = U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

static void gattXchangeMtuRsp(struct bt_conn *conn, uint8_t err,
                              struct bt_gatt_exchange_params *params)
{
    int32_t connHandle = findConnHandle(conn);
    (void)params;
    if (connHandle != U_PORT_GATT_GAP_INVALID_CONNHANDLE) {
        if (gCurrentConnections[connHandle].mtuXchangeCallback != NULL) {
            gCurrentConnections[connHandle].mtuXchangeCallback(connHandle, err);
        }
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */
int32_t uPortGattInit(void)
{
    return 0;
}

void uPortGattDeinit(void)
{
    uPortGattRemoveAllServices();
}

int32_t uPortGattAdd(void)
{
    static bool cbRegistered = false;

    if (!cbRegistered) {
        // We only register callbacks once, since doing it again
        // will add them to a list and they will be called
        // once more on every event
        bt_conn_cb_register(&conn_callbacks);
        cbRegistered = true;
    }

    return U_ERROR_COMMON_SUCCESS;
}

int32_t uPortGattAddPrimaryService(const uPortGattService_t *pService)
{
    int32_t err = U_ERROR_COMMON_UNKNOWN;

    if (pService == NULL) {
        return U_ERROR_COMMON_INVALID_PARAMETER;
    }

    if (!gGattUp) {
        int32_t errOrServiceIndex = addServiceInternal((struct bt_uuid *)&primaryServiceUuid, pService);

        if (errOrServiceIndex >= 0) {
            if (pService->pUuid->type == U_PORT_GATT_UUID_TYPE_128) {
                gScanResponseData[gAdvIndex].type = BT_DATA_UUID128_ALL;
                gScanResponseData[gAdvIndex].data_len = 16;
                gScanResponseData[gAdvIndex].data = ((struct bt_uuid_128 *)(pService->pUuid))->val;
            }
            gAdvIndex++;
            err = U_ERROR_COMMON_SUCCESS;
        } else {
            err = errOrServiceIndex;
        }
    }

    return err;
}

int32_t uPortGattRemoveAllServices(void)
{
    int32_t errorCode = U_ERROR_COMMON_SUCCESS;

    if (!gGattUp) {
        memset(gService, 0x00, sizeof gService);
        gNextFreeServiceIndex = 0;
        gpNextFreeAttr = gAttrPool;
        gpNextFreeChrc = gChrcPool;
        gAdvIndex = 0;
    } else {
        errorCode = U_ERROR_COMMON_UNKNOWN;
    }

    return errorCode;
}

int32_t uPortGattUp(bool startAdv)
{
    uint32_t serviceIndex = 0;
    int32_t err = 0;

    if (!gGattUp) {
        while (serviceIndex < gNextFreeServiceIndex) {
            err = bt_gatt_service_register(&gService[serviceIndex]);
            if (err) {
                break;
            }
            serviceIndex++;
        }

        if (err == 0) {
            err = bt_enable(NULL);
            if ((err == 0) || (err == -EALREADY)) {
                err = 0;
                if (startAdv) {
                    err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), gScanResponseData, gAdvIndex);
                    gAdvertising = true;
                }

                if (err == 0) {
                    gGattUp = true;
                }
            }
        }
    }

    return err;
}

bool uPortGattIsAdvertising(void)
{
    return (gGattUp && gAdvertising);
}

void uPortGattDown(void)
{
    uint32_t serviceIndex = 0;
    int32_t err = 0;

    if (gGattUp) {
        err = bt_le_adv_stop();
        gAdvertising = false;

        if (err == 0) {
            while (serviceIndex < gNextFreeServiceIndex) {
                bt_gatt_service_unregister(&gService[serviceIndex]);
                serviceIndex++;
            }

            gGattUp = false;
        }
    }
}

void uPortGattSetGapConnStatusCallback(uPortGattGapConnStatusCallback_t pCallback,
                                       void *pCallbackParam)
{
    pGapConnStatusCallback = pCallback;
    pGapConnStatusParam = pCallbackParam;
}

int32_t uPortGattGetMtu(int32_t connHandle)
{
    int32_t mtu = U_ERROR_COMMON_UNKNOWN;

    if (validConnHandle(connHandle)) {
        mtu = bt_gatt_get_mtu(gCurrentConnections[connHandle].pConn);
    }

    return mtu;
}

int32_t uPortGattExchangeMtu(int32_t connHandle, mtuXchangeRespCallback_t respCallback)
{
    int32_t errorCode = U_ERROR_COMMON_INVALID_PARAMETER;

    if (validConnHandle(connHandle)) {
        static struct bt_gatt_exchange_params xParms;
        xParms.func = gattXchangeMtuRsp;
        gCurrentConnections[connHandle].mtuXchangeCallback = respCallback;
        if (bt_gatt_exchange_mtu(gCurrentConnections[connHandle].pConn, &xParms) == 0) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return errorCode;
}

int32_t uPortGattNotify(int32_t connHandle, const uPortGattCharacteristic_t *pChar,
                        const void *data, uint16_t len)
{
    int32_t returnValue = U_ERROR_COMMON_UNKNOWN;
    struct bt_gatt_attr *pAtt = gAttrPool;

    if (!validConnHandle(connHandle) || (pChar == NULL) || (data == NULL) || (len == 0)) {
        return U_ERROR_COMMON_INVALID_PARAMETER;
    }

    if (gCurrentConnections[connHandle].pConn == NULL) {
        return returnValue;
    }

    // We are given a pointer to the porting layer characteristic struct
    // but we need to find the corresponding zephyr attribute in the attribute pool.
    while ((pAtt != gpNextFreeAttr) && (pAtt->user_data != &(pChar->valueAtt))) {
        pAtt++;
    }

    if (pAtt != gpNextFreeAttr) {
        returnValue = bt_gatt_notify(gCurrentConnections[connHandle].pConn, pAtt, data, len);
    }

    return returnValue;
}

static struct bt_conn *connectGapAsPeripheral(const bt_addr_le_t *peer, int32_t *pErrorCode)
{
    struct bt_conn *pConn;
    struct bt_le_adv_param param =
        BT_LE_ADV_PARAM_INIT(BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_ONE_TIME, 0, 0, peer);

    bt_le_adv_stop();
    *pErrorCode = bt_le_adv_start(&param, NULL, 0, NULL, 0);

    if (*pErrorCode != 0) {
        pConn = NULL;
    } else {
        pConn = bt_conn_lookup_addr_le(param.id, peer);
    }

    return pConn;
}

static struct bt_conn *connectGapAsCentral(const bt_addr_le_t *pPeer, int32_t *pErrorCode,
                                           const uPortGattGapParams_t *pGapParams)
{
    struct bt_conn_le_create_param createParam;
    struct bt_le_conn_param connParam;
    struct bt_conn *pConn;

    createParam.options = BT_CONN_LE_OPT_NONE;
    createParam.window_coded = 0;
    createParam.interval_coded = 0;

    if (pGapParams == NULL) {
        createParam.interval = uPortGattGapParamsDefault.scanInterval;
        createParam.window = uPortGattGapParamsDefault.scanWindow;
        createParam.timeout = uPortGattGapParamsDefault.createConnectionTmo / 10;
        connParam.interval_min = uPortGattGapParamsDefault.connIntervalMin;
        connParam.interval_max = uPortGattGapParamsDefault.connIntervalMax;
        connParam.latency = uPortGattGapParamsDefault.connLatency;
        connParam.timeout = uPortGattGapParamsDefault.linkLossTimeout;
    } else {
        createParam.interval = pGapParams->scanInterval;
        createParam.window = pGapParams->scanWindow;
        createParam.timeout = pGapParams->createConnectionTmo / 10;

        connParam.interval_min = pGapParams->connIntervalMin;
        connParam.interval_max = pGapParams->connIntervalMax;
        connParam.latency = pGapParams->connLatency;
        connParam.timeout = pGapParams->linkLossTimeout;
    }

    *pErrorCode = bt_conn_le_create(pPeer, &createParam, &connParam, &pConn);
    if (*pErrorCode != 0) {
        pConn = NULL;
    }

    return pConn;
}

int32_t uPortGattConnectGap(uint8_t *pAddress, uPortBtLeAddressType_t addressType,
                            const uPortGattGapParams_t *pGapParams)
{
    bt_addr_le_t peer;
    int32_t connHandle;

    peer.type = portAddrTypeToZephyrAddrType(addressType);
    memcpy(peer.a.val, pAddress, sizeof peer.a.val);

    connHandle = findFreeConnHandle();

    if (connHandle != U_PORT_GATT_GAP_INVALID_CONNHANDLE) {
        int32_t errorCode;
        struct bt_conn *pConn;

        if (gAdvertising) {
            uPortLog("U_PORT_GATT: connecting as peripheral\n");
            pConn = connectGapAsPeripheral(&peer, &errorCode);
        } else {
            uPortLog("U_PORT_GATT: connecting as central\n");
            pConn = connectGapAsCentral(&peer, &errorCode, pGapParams);
        }
        if (pConn != 0) {
            gCurrentConnections[connHandle].pConn = pConn;
        } else {
            uPortLog("U_PORT_GATT: GAP Connection error %d\n", errorCode);
            connHandle = U_PORT_GATT_GAP_INVALID_CONNHANDLE;
        }
    } else {
        uPortLog("U_PORT_GATT: No room for more connections!\n");
    }

    return connHandle;
}

int32_t uPortGattDisconnectGap(int32_t connHandle)
{
    int32_t errorCode = U_ERROR_COMMON_UNKNOWN;

    if (validConnHandle(connHandle)) {
        if (bt_conn_disconnect(gCurrentConnections[connHandle].pConn,
                               BT_HCI_ERR_REMOTE_USER_TERM_CONN) == 0) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }
    }

    return errorCode;
}

int32_t uPortGattGetRemoteAddress(int32_t connHandle, uint8_t *pAddr,
                                  uPortBtLeAddressType_t *pAddrType)
{
    int32_t errorCode = U_ERROR_COMMON_UNKNOWN;

    if ((pAddr == NULL) || (pAddrType == NULL)) {
        return errorCode;
    }

    if (validConnHandle(connHandle)) {
        errorCode = U_ERROR_COMMON_SUCCESS;
        const bt_addr_le_t *pZaddr = bt_conn_get_dst(gCurrentConnections[connHandle].pConn);

        memcpy(pAddr, pZaddr->a.val, sizeof pZaddr->a.val);
        switch (pZaddr->type) {
            case BT_ADDR_LE_RANDOM:
            case BT_ADDR_LE_RANDOM_ID:
                *pAddrType = U_PORT_BT_LE_ADDRESS_TYPE_RANDOM;
                break;
            case BT_ADDR_LE_PUBLIC:
            case BT_ADDR_LE_PUBLIC_ID:
                *pAddrType = U_PORT_BT_LE_ADDRESS_TYPE_PUBLIC;
                break;
        }
    }

    return errorCode;
}

int32_t uPortGattWriteAttribute(int32_t connHandle, uint16_t handle, const void *pData,
                                uint16_t len)
{
    int32_t errorCode = U_ERROR_COMMON_UNKNOWN;

    if ((handle == 0) || !validConnHandle(connHandle) || (pData == NULL)) {
        return U_ERROR_COMMON_INVALID_PARAMETER;
    }

    if (bt_gatt_write_without_response(gCurrentConnections[connHandle].pConn,
                                       handle, pData, len, false) == 0) {
        errorCode = U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

int32_t uPortGattSubscribe(int32_t connHandle, uPortGattSubscribeParams_t *pParams)
{
    int32_t errorCode = U_ERROR_COMMON_NO_MEMORY;
    subscribeParams_t *pSub = pFindFreeSubscription();

    if ((pParams == NULL) || !validConnHandle(connHandle)) {
        return U_ERROR_COMMON_INVALID_PARAMETER;
    }

    if ((pSub != NULL) && (pParams->notifyCb != NULL)) {
        pSub->connHandle = connHandle;
        pSub->pUparams = pParams;
        pSub->zParams.notify = notifyCallback;
        pSub->zParams.write = cccWriteResponseCb;
        pSub->zParams.value_handle = pParams->valueHandle;
        pSub->zParams.ccc_handle = pParams->cccHandle;
        pSub->zParams.value = 0;
        if (pParams->receiveNotifications) {
            pSub->zParams.value |= 1;
        }
        if (pParams->receiveIndications) {
            pSub->zParams.value |= 2;
        }
        gCurrentConnections[connHandle].pOngoingSubscribe = pSub;
        if (bt_gatt_subscribe(gCurrentConnections[connHandle].pConn, &(pSub->zParams)) == 0) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        } else {
            errorCode = U_ERROR_COMMON_UNKNOWN;
        }
    } else {
        errorCode = U_ERROR_COMMON_INVALID_PARAMETER;
    }

    return errorCode;
}

int32_t uPortGattStartPrimaryServiceDiscovery(int32_t connHandle, const uPortGattUuid_t *pUuid,
                                              uPortGattServiceDiscoveryCallback_t callback)
{
    return startDiscovery(connHandle, pUuid, 0x0001, 0xffff, (void *)callback,
                          BT_GATT_DISCOVER_PRIMARY);
}

int32_t uPortGattStartCharacteristicDiscovery(int32_t connHandle, uPortGattUuid_t *pUuid,
                                              uint16_t startHandle, uPortGattCharDiscoveryCallback_t callback)
{
    return startDiscovery(connHandle, pUuid, startHandle, 0xffff, callback,
                          BT_GATT_DISCOVER_CHARACTERISTIC);
}

int32_t uPortGattStartDescriptorDiscovery(int32_t connHandle, uPortGattCharDescriptorType_t type,
                                          uint16_t startHandle, uPortGattDescriptorDiscoveryCallback_t callback)
{
    return startDiscovery(connHandle, (uPortGattUuid_t *)&charDescriptorsUuid[type], startHandle,
                          0xffff, callback, BT_GATT_DISCOVER_DESCRIPTOR);
}

// End of file
