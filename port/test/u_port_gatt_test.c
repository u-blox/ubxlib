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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Test for the port API: these should pass on all platforms.
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the U_PORT_TEST_FUNCTION()
 * macro.
 */

#ifdef U_CFG_BLE_MODULE_INTERNAL

//lint -e845 "The right argument to operator '&&' is certain to be 0"
// lint does not understand that the continue statement inside for-loops
// might cause the final testOK = true inside the loop not to execute

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "stdlib.h"    // rand()
#include "string.h"    // strtok() and strcmp()
#include "stdio.h"     // snprintf()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_port_clib_platform_specific.h" /* Integer stdio, must be included
                                              before the other port files if
                                              any print or scan function is used. */
#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_gatt.h"
#include "u_error_common.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_PORT_GATT_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#define NBR_OF_CONNECTION_RETRIES 3
#define CONNECTION_SETUP_TIMEOUT 6000
#define WAIT_FOR_CALLBACK_TIMEOUT 10000
#define WAIT_FOR_CALLBACK_FINISH_DELAY 100

#define U_PORT_GATT_TEST_NBR_OF_SERVICES 4

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef struct {
    int32_t connHandle;
    uPortGattGapConnStatus_t status;
    void *pCallbackParam;
} connStatusEvt_t;

typedef struct {
    int32_t connHandle;
    uPortGattUuid128_t uuid;
    uint16_t attrHandle;
    uint16_t endHandle;
} serviceEvt_t;

typedef struct {
    int32_t connHandle;
    uPortGattUuid128_t uuid;
    uint16_t attrHandle;
    uint16_t valHandle;
    uint8_t properties;
} characteristicEvt_t;

typedef struct {
    int32_t connHandle;
    uPortGattUuid128_t uuid;
    uint16_t attrHandle;
} descriptorEvt_t;

typedef struct {
    int32_t connHandle;
    uint16_t length;
    uint8_t data[4];
    uPortGattSubscribeParams_t *pParams;
} notifyEvt_t;

typedef struct {
    int32_t connHandle;
    uint8_t err;
} writeCccEvt_t;

typedef struct {
    int32_t connHandle;
    uint16_t length;
    uint8_t data[4];
    uint16_t offset;
    uint8_t flags;
} spsWriteEvt_t;

typedef enum {
    GATT_EVT_CONN_STATUS,
    GATT_EVT_SERVICE,
    GATT_EVT_CHARACTERISTIC,
    GATT_EVT_DESCRIPTOR,
    GATT_EVT_NOTIFY,
    GATT_EVT_WRITE_CCC,
    GATT_EVT_SPS_WRITE_FIFO_CCC,
    GATT_EVT_SPS_WRITE_FIFO_CHAR,
    GATT_EVT_SPS_WRITE_CREDIT_CCC,
    GATT_EVT_SPS_WRITE_CREDIT_CHAR
} gattEvtId_t;

typedef struct {
    gattEvtId_t id;
    union {
        connStatusEvt_t conn;
        serviceEvt_t svc;
        characteristicEvt_t ch;
        descriptorEvt_t desc;
        notifyEvt_t notify;
        writeCccEvt_t writeCcc;
        spsWriteEvt_t spsWrite;
    };
} gattEvt_t;


/* ----------------------------------------------------------------
 * STATIC PROTOTYPES
 * -------------------------------------------------------------- */

static void createEvtQueue(void);
static void deleteEvtQueue(void);
static bool enqueueEvt(const gattEvt_t *evt);
static bool waitForEvt(gattEvtId_t id, gattEvt_t *evt, int32_t timeoutMs);

static int32_t hexToInt(const char *pIn, uint8_t *pOut);
static int32_t addrStringToArray(const char *pAddrIn, uint8_t *pAddrOut,
                                 uPortBtLeAddressType_t *pType);
static void printUuid(const uPortGattUuid_t *pUuid);
static void copyUuid(const uPortGattUuid_t *pUuidSrc, uPortGattUuid_t *pUuidDest);
static bool cmpUuidStrict(const uPortGattUuid_t *pUuidSrc, const uPortGattUuid_t *pUuidDest);
static void gapConnStatusCallback(int32_t connHandle,
                                  uPortGattGapConnStatus_t status,
                                  void *pCallbackParam);
static uPortGattIter_t gattServiceDiscoveryCallback(int32_t connHandle,
                                                    uPortGattUuid_t *pUuid,
                                                    uint16_t attrHandle,
                                                    uint16_t endHandle);
static uPortGattIter_t gattCharDiscoveryCallback(int32_t connHandle,
                                                 uPortGattUuid_t *pUuid,
                                                 uint16_t attrHandle,
                                                 uint16_t valHandle,
                                                 uint8_t properties);
static uPortGattIter_t gattDescriptorDiscoveryCallback(int32_t connHandle,
                                                       uPortGattUuid_t *pUuid,
                                                       uint16_t attrHandle);
static uPortGattIter_t gattNotifyFunc(int32_t connHandle,
                                      struct uPortGattSubscribeParams_s *pParams,
                                      const void *pData, uint16_t length);
static void gattCccWriteResp(int32_t connHandle, uint8_t err);
static int32_t remoteWritesFifoChar(int32_t gapConnHandle, const void *pBuf, uint16_t len,
                                    uint16_t offset, uint8_t flags);
static int32_t remoteWritesFifoCcc(int32_t gapConnHandle, const void *pBuf, uint16_t len,
                                   uint16_t offset, uint8_t flags);
static int32_t remoteWritesCreditChar(int32_t gapConnHandle, const void *pBuf, uint16_t len,
                                      uint16_t offset, uint8_t flags);
static int32_t remoteWritesCreditCcc(int32_t gapConnHandle, const void *pBuf, uint16_t len,
                                     uint16_t offset, uint8_t flags);

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */
/** For tracking heap lost to memory  lost by the C library.
 */
static size_t gSystemHeapLost = 0;

//lint -esym(843, gGattCallbackParamIn) "could be declared as const"
static void *gGattCallbackParamIn = (void *)123456;
static uint8_t gInvalidAddress[] = {0xde, 0xad, 0x99, 0x88, 0x77, 0x55};
static const char gRemoteSpsPeripheralStr[] =
    U_PORT_STRINGIFY_QUOTED(U_BLE_TEST_CFG_REMOTE_SPS_PERIPHERAL);
static uint8_t gRemoteSpsPeripheral[6];
static const char gRemoteSpsCentralStr[] =
    U_PORT_STRINGIFY_QUOTED(U_BLE_TEST_CFG_REMOTE_SPS_CENTRAL);
static uint8_t gRemoteSpsCentral[6];
static uPortBtLeAddressType_t gRemoteSpsPeripheralType;
static uPortBtLeAddressType_t gRemoteSpsCentralType;
static volatile uPortGattIter_t gGattIterReturnValue;
static uPortQueueHandle_t gEvtQueue = NULL;

static uPortGattUuid16_t gAppearanceCharUuid = {
    .type = U_PORT_GATT_UUID_TYPE_16,
    .val = 0x2a01
};

//lint -esym(843, gClientCharCfgUuid) "could be declared as const"
static uPortGattUuid16_t gClientCharCfgUuid = {
    .type = U_PORT_GATT_UUID_TYPE_16,
    .val = 0x2902
};

static uPortGattUuid128_t gSpsCreditsCharUuid = {
    .type = U_PORT_GATT_UUID_TYPE_128,
    .val = {0x04, 0xd7, 0xe9, 0x01, 0x4f, 0xf3, 0x44, 0xe7, 0x83, 0x8f, 0xe2, 0x26, 0xb9, 0xe1, 0x56, 0x24}
};

static uPortGattUuid128_t gSpsFifoCharUuid = {
    .type = U_PORT_GATT_UUID_TYPE_128,
    .val = {0x03, 0xd7, 0xe9, 0x01, 0x4f, 0xf3, 0x44, 0xe7, 0x83, 0x8f, 0xe2, 0x26, 0xb9, 0xe1, 0x56, 0x24},
};

static uPortGattUuid128_t gSpsServiceUuid = {
    .type = U_PORT_GATT_UUID_TYPE_128,
    .val = {0x01, 0xd7, 0xe9, 0x01, 0x4f, 0xf3, 0x44, 0xe7, 0x83, 0x8f, 0xe2, 0x26, 0xb9, 0xe1, 0x56, 0x24},
};

static const uPortGattCharDescriptor_t gSpsFifoClientConf = {
    .descriptorType = U_PORT_GATT_CHRC_DESC_CLIENT_CHAR_CONF,
    .att = {
        .permissions = U_PORT_GATT_ATT_PERM_WRITE,
        .write = remoteWritesFifoCcc,
        .read = NULL,
    },
    .pNextDescriptor = NULL,
};

static const uPortGattCharDescriptor_t gSpsCreditsClientConf = {
    .descriptorType = U_PORT_GATT_CHRC_DESC_CLIENT_CHAR_CONF,
    .att = {
        .permissions = U_PORT_GATT_ATT_PERM_WRITE,
        .write = remoteWritesCreditCcc,
        .read = NULL,
    },
    .pNextDescriptor = NULL,
};

static const uPortGattCharacteristic_t gSpsCreditsChar = {
    .pUuid = (uPortGattUuid_t *) &gSpsCreditsCharUuid,
    .properties = U_PORT_GATT_CHRC_NOTIFY | U_PORT_GATT_CHRC_WRITE | U_PORT_GATT_CHRC_WRITE_WITHOUT_RESP,
    .valueAtt = {
        .permissions = U_PORT_GATT_ATT_PERM_WRITE,
        .write = remoteWritesCreditChar,
        .read = NULL,
    },
    .pFirstDescriptor = &gSpsCreditsClientConf,
    .pNextChar = NULL,
};

static const uPortGattCharacteristic_t gSpsFifoChar = {
    .pUuid = (uPortGattUuid_t *) &gSpsFifoCharUuid,
    .properties = U_PORT_GATT_CHRC_NOTIFY | U_PORT_GATT_CHRC_WRITE | U_PORT_GATT_CHRC_WRITE_WITHOUT_RESP,
    .valueAtt = {
        .permissions = U_PORT_GATT_ATT_PERM_WRITE,
        .write = remoteWritesFifoChar,
        .read = NULL,
    },
    .pFirstDescriptor = &gSpsFifoClientConf,
    .pNextChar = &gSpsCreditsChar,
};

static const uPortGattService_t gTestSpsService = {
    .pUuid = (uPortGattUuid_t *) &gSpsServiceUuid,
    .pFirstChar = &gSpsFifoChar,
};

// lint doesn't notice that the union members are in fact referenced in the
// constant declarations below
//lint -e754 "local union member not referenced"
typedef struct {
    union {
        uPortGattUuid_t    uuid;
        uPortGattUuid16_t  uuid16;
        uPortGattUuid32_t  uuid32;
        uPortGattUuid128_t uuid128;
    } uuid;
    uint16_t attrHandle;
    uint16_t endHandle;
} gattService_t;

static const gattService_t gNinaW15GenericAttrService = {
    .uuid = {
        .uuid16 = {
            .type = U_PORT_GATT_UUID_TYPE_16,
            .val = 0x1801
        }
    },
    .attrHandle = 1,
    .endHandle = 4
};

static const gattService_t gNinaW15GenericAccessService = {
    .uuid = {
        .uuid16 = {
            .type = U_PORT_GATT_UUID_TYPE_16,
            .val = 0x1800
        }
    },
    .attrHandle = 5,
    .endHandle = 11
};

static const gattService_t gNinaW15DeviceInfoService = {
    .uuid = {
        .uuid16 = {
            .type = U_PORT_GATT_UUID_TYPE_16,
            .val = 0x180A
        }
    },
    .attrHandle = 12,
    .endHandle = 20
};

static const gattService_t gNinaW15SpsService = {
    .uuid = {
        .uuid128 = {
            .type = U_PORT_GATT_UUID_TYPE_128,
            .val = {0x01, 0xd7, 0xe9, 0x01, 0x4f, 0xf3, 0x44, 0xe7, 0x83, 0x8f, 0xe2, 0x26, 0xb9, 0xe1, 0x56, 0x24},
        }
    },
    .attrHandle = 21,
    .endHandle = 27
};
//lint +e75

static const gattService_t *const gExpectedServices[U_PORT_GATT_TEST_NBR_OF_SERVICES] = {
    &gNinaW15GenericAttrService,
    &gNinaW15GenericAccessService,
    &gNinaW15DeviceInfoService,
    &gNinaW15SpsService
};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static void createEvtQueue(void)
{
    if (gEvtQueue) {
        // If the queue already exists we first delete it to get a "fresh" queue
        deleteEvtQueue();
    }
    U_PORT_TEST_ASSERT(uPortQueueCreate(1, sizeof(gattEvt_t), &gEvtQueue) == 0);
}

static void deleteEvtQueue(void)
{
    if (gEvtQueue) {
        U_PORT_TEST_ASSERT(uPortQueueDelete(gEvtQueue) == 0);
        gEvtQueue = NULL;
    }
}

static bool enqueueEvt(const gattEvt_t *evt)
{
    U_PORT_TEST_ASSERT(gEvtQueue);
    return (uPortQueueSend(gEvtQueue, evt) == 0);
}

static bool waitForEvt(gattEvtId_t id, gattEvt_t *evt, int32_t timeoutMs)
{
    U_PORT_TEST_ASSERT(gEvtQueue);
    if (uPortQueueTryReceive(gEvtQueue, timeoutMs, evt) != 0) {
        return false;
    }
    return (evt->id == id);
}

static int32_t hexToInt(const char *pIn, uint8_t *pOut)
{
    uint32_t i;
    int32_t errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;

    *pOut = 0;
    for (i = 0; i < 2; i++) {
        char inChar = *pIn;
        uint8_t nibbleVal;

        if (inChar >= '0' && inChar <= '9') {
            nibbleVal = inChar - '0';
        } else if (inChar >= 'a' && inChar <= 'f') {
            nibbleVal = inChar + 10 - 'a';
        } else if (inChar >= 'A' && inChar <= 'F') {
            nibbleVal = inChar + 10 - 'A';
        } else {
            errorCode = (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
            break;
        }
//lint -save -e701 -e734
        *pOut |= nibbleVal << (4 * (1 - i));
//lint -restore
        pIn++;
    }

    return errorCode;
}

static int32_t addrStringToArray(const char *pAddrIn, uint8_t *pAddrOut,
                                 uPortBtLeAddressType_t *pType)
{
    int32_t errorCode = (int32_t)U_ERROR_COMMON_SUCCESS;
    uint32_t i;
    char lastChar = pAddrIn[12];

    for (i = 0; i < 6; i++) {
//lint -save -e679
        if (hexToInt(&pAddrIn[2 * i], &pAddrOut[5 - i]) != (int32_t)U_ERROR_COMMON_SUCCESS) {
//lint -restore
            errorCode = (int32_t)U_ERROR_COMMON_INVALID_ADDRESS;
            break;
        }
    }
    if (lastChar == 'p' || lastChar == 'P' || lastChar == '\0') {
        *pType = U_PORT_BT_LE_ADDRESS_TYPE_PUBLIC;
    } else if (lastChar == 'r' || lastChar == 'R') {
        *pType = U_PORT_BT_LE_ADDRESS_TYPE_RANDOM;
    } else {
        errorCode = (int32_t)U_ERROR_COMMON_INVALID_ADDRESS;
    }

    return errorCode;
}

//lint -efunc(826, printUuid) "Suspicious pointer-to-pointer conversion (area too small)"
static void printUuid(const uPortGattUuid_t *pUuid)
{
    if (pUuid != NULL) {
        switch (pUuid->type) {
            case U_PORT_GATT_UUID_TYPE_16:
                uPortLog("UUID16: 0x%04X", ((const uPortGattUuid16_t *)pUuid)->val);
                break;
            case U_PORT_GATT_UUID_TYPE_32:
                uPortLog("UUID32: 0x%08X", ((const uPortGattUuid32_t *)pUuid)->val);
                break;
            case U_PORT_GATT_UUID_TYPE_128:
                uPortLog("UUID128: 0x");
                for (int ii = 0; ii < 16; ii++) {
                    uPortLog("%02X", ((const uPortGattUuid128_t *)pUuid)->val[ii]);
                }
                uPortLog("");
                break;
            default:
                uPortLog("UUID: invalid");
                break;
        }
    } else {
        uPortLog("UUID:  NULL");
    }
}

static void copyUuid(const uPortGattUuid_t *pUuidSrc, uPortGattUuid_t *pUuidDest)
{
    switch (pUuidSrc->type) {
        case U_PORT_GATT_UUID_TYPE_16:
            memcpy(pUuidDest, pUuidSrc, sizeof(uPortGattUuid16_t));
            break;
        case U_PORT_GATT_UUID_TYPE_32:
            memcpy(pUuidDest, pUuidSrc, sizeof(uPortGattUuid32_t));
            break;
        case U_PORT_GATT_UUID_TYPE_128:
            memcpy(pUuidDest, pUuidSrc, sizeof(uPortGattUuid128_t));
            break;
        default:
            break;
    }
}

//lint -efunc(826, cmpUuidStrict) "Suspicious pointer-to-pointer conversion (area too small)"
static bool cmpUuidStrict(const uPortGattUuid_t *pUuidSrc, const uPortGattUuid_t *pUuidDest)
{
    if (pUuidSrc->type == pUuidDest->type) {
        switch (pUuidSrc->type) {
            case U_PORT_GATT_UUID_TYPE_16:
                return ((const uPortGattUuid16_t *)pUuidDest)->val == ((const uPortGattUuid16_t *)pUuidSrc)->val;
            case U_PORT_GATT_UUID_TYPE_32:
                return ((const uPortGattUuid32_t *)pUuidDest)->val == ((const uPortGattUuid32_t *)pUuidSrc)->val;
            case U_PORT_GATT_UUID_TYPE_128:
                return memcmp(((const uPortGattUuid128_t *)pUuidDest)->val,
                              ((const uPortGattUuid128_t *)pUuidSrc)->val, 16) == 0;
            default:
                break;
        }
    }
    return false;
}

//lint -efunc(785, gapConnStatusCallback) "Too few initializers for aggregate 'evt' of type 'gattEvt_t'"
static void gapConnStatusCallback(int32_t connHandle,
                                  uPortGattGapConnStatus_t status,
                                  void *pCallbackParam)
{
    gattEvt_t evt = { .id = GATT_EVT_CONN_STATUS };
    connStatusEvt_t *conn = &evt.conn;
    conn->connHandle = connHandle;
    conn->status = status;
    conn->pCallbackParam = pCallbackParam;
    U_TEST_PRINT_LINE("BT connect status(connHandle=%d, status=%d, pCallbackParam=%d)",
                      connHandle, status, pCallbackParam);
    if (!enqueueEvt(&evt)) {
        U_TEST_PRINT_LINE("ERROR: failed to queue GATT conn status evt.");
    }
}

//lint -efunc(785, gattServiceDiscoveryCallback) "Too few initializers for aggregate 'evt' of type 'gattEvt_t'"
static uPortGattIter_t gattServiceDiscoveryCallback(int32_t connHandle,
                                                    uPortGattUuid_t *pUuid,
                                                    uint16_t attrHandle,
                                                    uint16_t endHandle)
{
    gattEvt_t evt = { .id = GATT_EVT_SERVICE };
    serviceEvt_t *svc = &evt.svc;
    uPortLog(U_TEST_PREFIX "callback(connHandle=%d, attrHandle=%d, endHandle=%d, ",
             connHandle, attrHandle, endHandle);
    if (pUuid != NULL) {
        copyUuid(pUuid, (uPortGattUuid_t *)&svc->uuid);
        printUuid(pUuid);
    } else {
        memset(&svc->uuid, 0, sizeof(uPortGattUuid128_t));
        uPortLog("UUID: NULL");
    }
    uPortLog(").\n");

    svc->connHandle = connHandle;
    svc->attrHandle = attrHandle;
    svc->endHandle = endHandle;

    if (!enqueueEvt(&evt)) {
        U_TEST_PRINT_LINE("ERROR: failed to queue GATT service evt.");
        return U_PORT_GATT_ITER_STOP;
    }

    return gGattIterReturnValue;
}

//lint -efunc(785, gattCharDiscoveryCallback) "Too few initializers for aggregate 'evt' of type 'gattEvt_t'"
static uPortGattIter_t gattCharDiscoveryCallback(int32_t connHandle,
                                                 uPortGattUuid_t *pUuid,
                                                 uint16_t attrHandle,
                                                 uint16_t valHandle,
                                                 uint8_t  properties)
{
    gattEvt_t evt = { .id = GATT_EVT_CHARACTERISTIC };
    characteristicEvt_t *ch = &evt.ch;

    uPortLog(U_TEST_PREFIX
             "callback(connHandle=%d, attrHandle=%d, valueHandle=%d, properties=0x%02x,\n                  ",
             connHandle, attrHandle, valHandle, properties);

    if (pUuid != NULL) {
        copyUuid(pUuid, (uPortGattUuid_t *)&ch->uuid);
        printUuid(pUuid);
    } else {
        memset(&ch->uuid, 0, sizeof(uPortGattUuid128_t));
        uPortLog("UUID: NULL");
    }
    uPortLog(").\n");
    ch->connHandle = connHandle;
    ch->attrHandle = attrHandle;
    ch->valHandle = valHandle;
    ch->properties = properties;

    if (!enqueueEvt(&evt)) {
        U_TEST_PRINT_LINE("ERROR: failed to queue GATT characteristic evt.");
        return U_PORT_GATT_ITER_STOP;
    }

    return gGattIterReturnValue;
}

//lint -efunc(785, gattDescriptorDiscoveryCallback) "Too few initializers for aggregate 'evt' of type 'gattEvt_t'"
static uPortGattIter_t gattDescriptorDiscoveryCallback(int32_t connHandle,
                                                       uPortGattUuid_t *pUuid,
                                                       uint16_t  attrHandle)
{
    gattEvt_t evt = { .id = GATT_EVT_DESCRIPTOR };
    descriptorEvt_t *desc = &evt.desc;

    uPortLog(U_TEST_PREFIX "callback(connHandle=%d, attrHandle=%d, ", connHandle, attrHandle);
    if (pUuid != NULL) {
        copyUuid(pUuid, (uPortGattUuid_t *)&desc->uuid);
        printUuid(pUuid);
    } else {
        memset(&desc->uuid, 0, sizeof(uPortGattUuid128_t));
        uPortLog("UUID: NULL");
    }
    uPortLog(").\n");
    desc->connHandle = connHandle;
    desc->attrHandle = attrHandle;

    if (!enqueueEvt(&evt)) {
        U_TEST_PRINT_LINE("ERROR: failed to queue GATT descriptor evt.");
        return U_PORT_GATT_ITER_STOP;
    }

    return gGattIterReturnValue;
}

//lint -efunc(785, gattNotifyFunc) "Too few initializers for aggregate 'evt' of type 'gattEvt_t'"
static uPortGattIter_t gattNotifyFunc(int32_t connHandle,
                                      struct uPortGattSubscribeParams_s *pParams,
                                      const void *pData, uint16_t length)
{
    gattEvt_t evt = { .id = GATT_EVT_NOTIFY };
    notifyEvt_t *notify = &evt.notify;

    notify->connHandle = connHandle;
    notify->length = length;
    notify->pParams = pParams;
    if (pData) {
        U_TEST_PRINT_LINE("notified with %d bytes of data.", length);
        if (length <= sizeof(notify->data)) {
            memcpy(notify->data, (const uint8_t *)pData, sizeof(notify->data));
        }
    } else {
        U_TEST_PRINT_LINE("notification removed.");
    }

    if (!enqueueEvt(&evt)) {
        U_TEST_PRINT_LINE("ERROR: failed to queue GATT notify evt.");
        return U_PORT_GATT_ITER_STOP;
    }

    return gGattIterReturnValue;
}

//lint -efunc(785, gattCccWriteResp) "Too few initializers for aggregate 'evt' of type 'gattEvt_t'"
static void gattCccWriteResp(int32_t connHandle, uint8_t err)
{
    gattEvt_t evt = { .id = GATT_EVT_WRITE_CCC };
    writeCccEvt_t *writeCcc = &evt.writeCcc;

    writeCcc->connHandle = connHandle;
    writeCcc->err = err;
    uPortLog(U_TEST_PREFIX "Characteristics Client Configuration write ");
    if (err == 0) {
        uPortLog("successful!\n");
    } else {
        uPortLog("failed!\n");
    }

    if (!enqueueEvt(&evt)) {
        U_TEST_PRINT_LINE("ERROR: failed to queue GATT write CCC evt.");
    }
}

//lint -efunc(785, enqueueSpsWrite) "Too few initializers for aggregate 'evt' of type 'gattEvt_t'"
static bool enqueueSpsWrite(gattEvtId_t id, int32_t gapConnHandle, const void *pBuf,
                            uint16_t len, uint16_t offset, uint8_t flags)
{
    gattEvt_t evt = { .id = id };
    spsWriteEvt_t *spsWrite = &evt.spsWrite;

    spsWrite->connHandle = gapConnHandle;
    spsWrite->length = len;
    spsWrite->flags = flags;
    spsWrite->offset = offset;

    if (len <= sizeof(spsWrite->data)) {
        memcpy(spsWrite->data, pBuf, len);
    }

    return enqueueEvt(&evt);
}

static int32_t remoteWritesFifoChar(int32_t gapConnHandle, const void *pBuf, uint16_t len,
                                    uint16_t offset, uint8_t flags)
{
    U_TEST_PRINT_LINE("remote writes to FIFO characteristics.");
    if (!enqueueSpsWrite(GATT_EVT_SPS_WRITE_FIFO_CHAR, gapConnHandle, pBuf, len, offset, flags)) {
        U_TEST_PRINT_LINE("ERROR: failed to queue SPS write FIFO char evt.");
    }
    return len;
}

static int32_t remoteWritesFifoCcc(int32_t gapConnHandle, const void *pBuf, uint16_t len,
                                   uint16_t offset, uint8_t flags)
{
    U_TEST_PRINT_LINE("remote writes to FIFO CCC.");
    if (!enqueueSpsWrite(GATT_EVT_SPS_WRITE_FIFO_CCC, gapConnHandle, pBuf, len, offset, flags)) {
        U_TEST_PRINT_LINE("ERROR: failed to queue SPS write FIFO CCC evt.");
    }
    return len;
}

static int32_t remoteWritesCreditChar(int32_t gapConnHandle, const void *pBuf, uint16_t len,
                                      uint16_t offset, uint8_t flags)
{
    U_TEST_PRINT_LINE("remote writes to credit characteristics.");
    if (!enqueueSpsWrite(GATT_EVT_SPS_WRITE_CREDIT_CHAR, gapConnHandle, pBuf, len, offset, flags)) {
        U_TEST_PRINT_LINE("ERROR: failed to queue SPS write credit char evt.");
    }
    return len;
}

static int32_t remoteWritesCreditCcc(int32_t gapConnHandle, const void *pBuf, uint16_t len,
                                     uint16_t offset, uint8_t flags)
{
    U_TEST_PRINT_LINE("remote writes to credit CCC.");
    if (!enqueueSpsWrite(GATT_EVT_SPS_WRITE_CREDIT_CCC, gapConnHandle, pBuf, len, offset, flags)) {
        U_TEST_PRINT_LINE("ERROR: failed to queue SPS write credit CCC evt.");
    }
    return len;
}

static bool parseSpsCccWriteData(spsWriteEvt_t *evt, uint16_t *data)
{
    if ((evt->length == 2) && (evt->offset == 0)) {
        *data = evt->data[0] | (evt->data[1] << 8);
        return true;
    }
    return false;
}

/* ----------------------------------------------------------------
 * TESTS
 * -------------------------------------------------------------- */

U_PORT_TEST_FUNCTION("[portGatt]", "portGattInitTests")
{
    int32_t errorCode;

    errorCode = addrStringToArray(gRemoteSpsPeripheralStr, gRemoteSpsPeripheral,
                                  &gRemoteSpsPeripheralType);
    U_TEST_PRINT_LINE("using %s as remote peripheral.", gRemoteSpsPeripheralStr);
    U_PORT_TEST_ASSERT_EQUAL(errorCode, 0);
    errorCode = addrStringToArray(gRemoteSpsCentralStr, gRemoteSpsCentral, &gRemoteSpsCentralType);
    U_TEST_PRINT_LINE("using %s as remote central.", gRemoteSpsCentralStr);
    U_PORT_TEST_ASSERT_EQUAL(errorCode, 0);
}

// Test misc functions like:
//   - uPortGattInit
//   - uPortGattAdd
//   - uPortGattUp
//   - uPortGattDown
//   - uPortGattConnectGap
//   - uPortGattDisconnectGap
//   - uPortGattGetRemoteAddress
U_PORT_TEST_FUNCTION("[portGatt]", "portGattMisc")
{
    gattEvt_t evt;
    int32_t errorCode;
    int32_t heapUsed;
    int32_t heapClibLossOffset = (int32_t) gSystemHeapLost;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();
    U_PORT_TEST_ASSERT_EQUAL(uPortInit(), 0);

    // Test cases

    createEvtQueue();

    U_TEST_PRINT_LINE("GATT init.");
    U_PORT_TEST_ASSERT_EQUAL(uPortGattInit(), 0);
    U_PORT_TEST_ASSERT_EQUAL(uPortGattAdd(), 0);
    U_PORT_TEST_ASSERT_EQUAL(uPortGattUp(true), 0);
    U_PORT_TEST_ASSERT(uPortGattIsAdvertising());
    uPortGattDown();
    uPortGattDeinit();
    U_PORT_TEST_ASSERT_EQUAL(uPortGattAdd(), 0);
    U_PORT_TEST_ASSERT_EQUAL(uPortGattUp(false), 0);
    U_PORT_TEST_ASSERT(!uPortGattIsAdvertising());

    uPortGattSetGapConnStatusCallback(gapConnStatusCallback, gGattCallbackParamIn);

    U_TEST_PRINT_LINE("uPortGattConnectGap() to unavailable device.");
    int32_t connHandle = uPortGattConnectGap(gInvalidAddress, gRemoteSpsPeripheralType, NULL);
    U_PORT_TEST_ASSERT(connHandle != U_PORT_GATT_GAP_INVALID_CONNHANDLE);
    U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_CONN_STATUS, &evt, CONNECTION_SETUP_TIMEOUT));
    U_PORT_TEST_ASSERT_EQUAL(evt.conn.status, U_PORT_GATT_GAP_DISCONNECTED);

    // Retry this a couple of times if connection setup fails
    bool testOK = false;
    for (int i = 0; i < NBR_OF_CONNECTION_RETRIES && !testOK; i++) {
        uPortGattGapParams_t gapParams;

        gapParams.scanInterval = 100;
        gapParams.scanWindow = 100;
        gapParams.createConnectionTmo = 3000;

        gapParams.connIntervalMin = 10;
        gapParams.connIntervalMax = 20;
        gapParams.connLatency = 0;
        gapParams.linkLossTimeout = 3000;

        U_TEST_PRINT_LINE("uPortGattConnectGap() to device with conn params.");
        connHandle = uPortGattConnectGap(gRemoteSpsPeripheral, gRemoteSpsPeripheralType, &gapParams);
        U_PORT_TEST_ASSERT(connHandle != U_PORT_GATT_GAP_INVALID_CONNHANDLE);

        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_CONN_STATUS, &evt, CONNECTION_SETUP_TIMEOUT));
        // Verify values
        U_PORT_TEST_ASSERT_EQUAL(gGattCallbackParamIn, evt.conn.pCallbackParam);
        if (evt.conn.status != U_PORT_GATT_GAP_CONNECTED) {
            // Block for small amount of time to let callback finish so we can connect again
            uPortTaskBlock(WAIT_FOR_CALLBACK_FINISH_DELAY);
            continue;
        }
        U_PORT_TEST_ASSERT_EQUAL(evt.conn.connHandle, connHandle);

        U_TEST_PRINT_LINE("uPortGattGetRemoteAddress() - NULL address.");
        uint8_t addr[6];
        uPortBtLeAddressType_t addrType;
        errorCode = uPortGattGetRemoteAddress(connHandle, NULL, &addrType);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_UNKNOWN);

        U_TEST_PRINT_LINE("uPortGattGetRemoteAddress() - NULL address type.");
        errorCode = uPortGattGetRemoteAddress(connHandle, addr, NULL);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_UNKNOWN);

        U_TEST_PRINT_LINE("uPortGattGetRemoteAddress() - invalid conn handle.");
        errorCode = uPortGattGetRemoteAddress(U_PORT_GATT_GAP_INVALID_CONNHANDLE, addr, &addrType);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_UNKNOWN);

        U_TEST_PRINT_LINE("uPortGattGetRemoteAddress()");
        errorCode = uPortGattGetRemoteAddress(connHandle, addr, &addrType);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, 0);
        U_PORT_TEST_ASSERT(memcmp(addr, gRemoteSpsPeripheral, 6) == 0);
        U_PORT_TEST_ASSERT_EQUAL(addrType, gRemoteSpsPeripheralType);

        U_PORT_TEST_ASSERT_EQUAL(uPortGattDisconnectGap(connHandle), 0);
        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_CONN_STATUS, &evt, WAIT_FOR_CALLBACK_TIMEOUT));
        // Verify values
        U_PORT_TEST_ASSERT_EQUAL(gGattCallbackParamIn, evt.conn.pCallbackParam);
        U_PORT_TEST_ASSERT_EQUAL(evt.conn.status, U_PORT_GATT_GAP_DISCONNECTED);

        U_TEST_PRINT_LINE("uPortGattDisconnectGap() when not connected.");
        U_PORT_TEST_ASSERT_EQUAL(uPortGattDisconnectGap(connHandle), (int32_t)U_ERROR_COMMON_UNKNOWN);
        uPortGattDown();

        testOK = true;
    }
    U_PORT_TEST_ASSERT(testOK);

    deleteEvtQueue();

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("%d byte(s) of heap were lost to the C library"
                      " during this test and we have leaked %d byte(s).",
                      gSystemHeapLost - heapClibLossOffset,
                      heapUsed - (gSystemHeapLost - heapClibLossOffset));
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed < 0) ||
                       (heapUsed <= ((int32_t) gSystemHeapLost) - heapClibLossOffset));
}
// Test Primary service search.
U_PORT_TEST_FUNCTION("[portGatt]", "portGattPrimDisc")
{
    int32_t errorCode;
    int32_t heapUsed;
    int32_t heapClibLossOffset = (int32_t) gSystemHeapLost;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();
    U_PORT_TEST_ASSERT_EQUAL(uPortInit(), 0);

    // Test cases
    createEvtQueue();

    U_TEST_PRINT_LINE("GATT primary service search.");
    U_PORT_TEST_ASSERT_EQUAL(uPortGattInit(), 0);
    U_PORT_TEST_ASSERT_EQUAL(uPortGattAdd(), 0);
    U_PORT_TEST_ASSERT_EQUAL(uPortGattUp(false), 0);
    uPortGattSetGapConnStatusCallback(gapConnStatusCallback, gGattCallbackParamIn);

    // Retry this a couple of times if connection setup fails
    bool testOK = false;
    for (int i = 0; i < NBR_OF_CONNECTION_RETRIES && !testOK; i++) {
        gattEvt_t evt;
        int32_t connHandle = uPortGattConnectGap(gRemoteSpsPeripheral, gRemoteSpsPeripheralType, NULL);
        U_PORT_TEST_ASSERT(connHandle != U_PORT_GATT_GAP_INVALID_CONNHANDLE);

        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_CONN_STATUS, &evt, CONNECTION_SETUP_TIMEOUT));
        // Verify values
        U_PORT_TEST_ASSERT_EQUAL(gGattCallbackParamIn, evt.conn.pCallbackParam);
        if (evt.conn.status != U_PORT_GATT_GAP_CONNECTED) {
            // Block for small amount of time to let callback finish so we can connect again
            uPortTaskBlock(WAIT_FOR_CALLBACK_FINISH_DELAY);
            continue;
        }
        U_PORT_TEST_ASSERT_EQUAL(evt.conn.connHandle, connHandle);

        U_TEST_PRINT_LINE("uPortGattStartPrimaryServiceDiscovery() - invalid conn handle.");
        errorCode = uPortGattStartPrimaryServiceDiscovery(-1, NULL,
                                                          gattServiceDiscoveryCallback);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);

        U_TEST_PRINT_LINE("uPortGattStartPrimaryServiceDiscovery() - callback is NULL.");
        errorCode = uPortGattStartPrimaryServiceDiscovery(connHandle, NULL, NULL);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);

        U_TEST_PRINT_LINE("uPortGattStartPrimaryServiceDiscovery() - get all services.");
        gGattIterReturnValue = U_PORT_GATT_ITER_CONTINUE;
        errorCode = uPortGattStartPrimaryServiceDiscovery(connHandle, NULL,
                                                          gattServiceDiscoveryCallback);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_SUCCESS);

        // Discover all available services and compare to expected result
        int32_t serviceIndex = 0;
        serviceEvt_t *svc = &evt.svc;
        do {
            U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_SERVICE, &evt, WAIT_FOR_CALLBACK_TIMEOUT));
            U_PORT_TEST_ASSERT_EQUAL(svc->connHandle, connHandle);
            if (svc->attrHandle != 0) {
                U_PORT_TEST_ASSERT(serviceIndex < U_PORT_GATT_TEST_NBR_OF_SERVICES);
                //lint -e661 "Possible access of out-of-bounds pointer"
                U_PORT_TEST_ASSERT(cmpUuidStrict((uPortGattUuid_t *)&svc->uuid,
                                                 &gExpectedServices[serviceIndex]->uuid.uuid));
                U_PORT_TEST_ASSERT_EQUAL(svc->attrHandle, gExpectedServices[serviceIndex]->attrHandle);
                U_PORT_TEST_ASSERT_EQUAL(svc->endHandle, gExpectedServices[serviceIndex]->endHandle);
                //lint +e661
            }
            serviceIndex++;
        } while ((svc->attrHandle != 0) && (serviceIndex <= U_PORT_GATT_TEST_NBR_OF_SERVICES + 1));
        // We might need to retry service discovery in case of
        // both central and peripheral devices not moved to connected state
        if (serviceIndex != U_PORT_GATT_TEST_NBR_OF_SERVICES + 1) {
            // Disconnect existing link and start reconnection and service discovery
            if (uPortGattDisconnectGap(connHandle) == (int32_t)U_ERROR_COMMON_SUCCESS) {
                if (waitForEvt(GATT_EVT_CONN_STATUS, &evt, WAIT_FOR_CALLBACK_TIMEOUT)) {
                    if (evt.conn.status == U_PORT_GATT_GAP_DISCONNECTED) {
                        U_TEST_PRINT_LINE("disconnected GAP for retry.");
                    }
                }
            }
            if (i == (NBR_OF_CONNECTION_RETRIES - 1)) {
                U_TEST_PRINT_LINE("muliple retries uPortGattStartPrimaryServiceDiscovery() - get all services failed.");
            } else {
                U_TEST_PRINT_LINE("retry uPortGattStartPrimaryServiceDiscovery() - get all services.");
            }
            continue;
        }

        U_TEST_PRINT_LINE("uPortGattStartPrimaryServiceDiscovery() - get all services, no continue.");
        gGattIterReturnValue = U_PORT_GATT_ITER_STOP;
        errorCode = uPortGattStartPrimaryServiceDiscovery(connHandle, NULL,
                                                          gattServiceDiscoveryCallback);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_SUCCESS);
        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_SERVICE, &evt, WAIT_FOR_CALLBACK_TIMEOUT));
        U_PORT_TEST_ASSERT_EQUAL(svc->connHandle, connHandle);
        U_PORT_TEST_ASSERT_EQUAL(svc->attrHandle, gExpectedServices[0]->attrHandle);
        U_PORT_TEST_ASSERT_EQUAL(svc->endHandle, gExpectedServices[0]->endHandle);
        U_PORT_TEST_ASSERT(cmpUuidStrict((uPortGattUuid_t *)&svc->uuid,
                                         (uPortGattUuid_t *) & (gExpectedServices[0]->uuid)));
        // Timeout here, we should not get any more callbacks
        U_PORT_TEST_ASSERT(!waitForEvt(GATT_EVT_SERVICE, &evt, WAIT_FOR_CALLBACK_TIMEOUT));

        U_TEST_PRINT_LINE("uPortGattStartPrimaryServiceDiscovery() - get specific service.");
        errorCode =
            uPortGattStartPrimaryServiceDiscovery(connHandle,
                                                  &gNinaW15SpsService.uuid.uuid,
                                                  gattServiceDiscoveryCallback);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_SUCCESS);
        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_SERVICE, &evt, WAIT_FOR_CALLBACK_TIMEOUT));
        U_PORT_TEST_ASSERT_EQUAL(svc->connHandle, connHandle);
        U_PORT_TEST_ASSERT_EQUAL(svc->attrHandle, gNinaW15SpsService.attrHandle);
        U_PORT_TEST_ASSERT_EQUAL(svc->endHandle, gNinaW15SpsService.endHandle);
        U_PORT_TEST_ASSERT(cmpUuidStrict((uPortGattUuid_t *)&svc->uuid,
                                         &gNinaW15SpsService.uuid.uuid)); // DIS

        U_PORT_TEST_ASSERT(uPortGattDisconnectGap(connHandle) == 0);
        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_CONN_STATUS, &evt, WAIT_FOR_CALLBACK_TIMEOUT));
        // Verify values
        U_PORT_TEST_ASSERT_EQUAL(gGattCallbackParamIn, evt.conn.pCallbackParam);
        U_PORT_TEST_ASSERT_EQUAL(evt.conn.status, U_PORT_GATT_GAP_DISCONNECTED);
        uPortGattDown();

        testOK = true;
    }
    U_PORT_TEST_ASSERT(testOK);

    deleteEvtQueue();

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("%d byte(s) of heap were lost to the C library during this"
                      " test and we have leaked %d byte(s).",
                      gSystemHeapLost - heapClibLossOffset,
                      heapUsed - (gSystemHeapLost - heapClibLossOffset));
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed < 0) ||
                       (heapUsed <= ((int32_t) gSystemHeapLost) - heapClibLossOffset));
}

U_PORT_TEST_FUNCTION("[portGatt]", "portGattCharDisc")
{
    gattEvt_t evt;
    int32_t errorCode;
    int32_t heapUsed;
    int32_t heapClibLossOffset = (int32_t) gSystemHeapLost;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();
    U_PORT_TEST_ASSERT(uPortInit() == 0);

    // Test cases
    createEvtQueue();

    U_TEST_PRINT_LINE("GATT characteristic discovery.");
    U_PORT_TEST_ASSERT_EQUAL(uPortGattInit(), 0);
    U_PORT_TEST_ASSERT_EQUAL(uPortGattAdd(), 0);
    U_PORT_TEST_ASSERT_EQUAL(uPortGattUp(false), 0);
    uPortGattSetGapConnStatusCallback(gapConnStatusCallback, gGattCallbackParamIn);

    // Retry this a couple of times if connection setup fails
    bool testOK = false;
    for (int i = 0; i < NBR_OF_CONNECTION_RETRIES && !testOK; i++) {

        int32_t connHandle = uPortGattConnectGap(gRemoteSpsPeripheral, gRemoteSpsPeripheralType, NULL);
        U_PORT_TEST_ASSERT(connHandle != U_PORT_GATT_GAP_INVALID_CONNHANDLE);

        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_CONN_STATUS, &evt, CONNECTION_SETUP_TIMEOUT));
        // Verify values
        U_PORT_TEST_ASSERT_EQUAL(gGattCallbackParamIn, evt.conn.pCallbackParam);
        if (evt.conn.status != U_PORT_GATT_GAP_CONNECTED) {
            // Block for small amount of time to let callback finish so we can connect again
            uPortTaskBlock(WAIT_FOR_CALLBACK_FINISH_DELAY);
            continue;
        }
        U_PORT_TEST_ASSERT_EQUAL(evt.conn.connHandle, connHandle);

        U_TEST_PRINT_LINE("uPortGattStartCharacteristicDiscovery() - invalid conn handle.");
        errorCode = uPortGattStartCharacteristicDiscovery(-1, NULL, 0, gattCharDiscoveryCallback);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);

        U_TEST_PRINT_LINE("uPortGattStartCharacteristicDiscovery() - callback is NULL.");
        errorCode = uPortGattStartCharacteristicDiscovery(connHandle, NULL, 0, NULL);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);

        U_TEST_PRINT_LINE("uPortGattStartCharacteristicDiscovery() - get all characteristics of SPS service.");
        gGattIterReturnValue = U_PORT_GATT_ITER_CONTINUE;
        errorCode = uPortGattStartCharacteristicDiscovery(connHandle, NULL,
                                                          gNinaW15SpsService.attrHandle,
                                                          gattCharDiscoveryCallback);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_SUCCESS);

        characteristicEvt_t *ch = &evt.ch;
        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_CHARACTERISTIC, &evt, WAIT_FOR_CALLBACK_TIMEOUT));
        U_PORT_TEST_ASSERT_EQUAL(ch->connHandle, connHandle);
        U_PORT_TEST_ASSERT_EQUAL(ch->attrHandle, gNinaW15SpsService.attrHandle + 1);
        U_PORT_TEST_ASSERT_EQUAL(ch->valHandle, gNinaW15SpsService.attrHandle + 2);
        U_PORT_TEST_ASSERT_EQUAL(ch->properties,
                                 (U_PORT_GATT_CHRC_READ | U_PORT_GATT_CHRC_WRITE_WITHOUT_RESP |
                                  U_PORT_GATT_CHRC_WRITE | U_PORT_GATT_CHRC_NOTIFY));
        U_PORT_TEST_ASSERT(cmpUuidStrict((uPortGattUuid_t *)&ch->uuid,
                                         (uPortGattUuid_t *)&gSpsFifoCharUuid));

        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_CHARACTERISTIC, &evt, WAIT_FOR_CALLBACK_TIMEOUT));
        U_PORT_TEST_ASSERT_EQUAL(ch->connHandle, connHandle);
        U_PORT_TEST_ASSERT_EQUAL(ch->attrHandle, gNinaW15SpsService.attrHandle + 4);
        U_PORT_TEST_ASSERT_EQUAL(ch->valHandle, gNinaW15SpsService.attrHandle + 5);
        U_PORT_TEST_ASSERT_EQUAL(ch->properties,
                                 (U_PORT_GATT_CHRC_WRITE_WITHOUT_RESP | U_PORT_GATT_CHRC_WRITE | U_PORT_GATT_CHRC_NOTIFY));
        U_PORT_TEST_ASSERT(cmpUuidStrict((uPortGattUuid_t *)&ch->uuid,
                                         (uPortGattUuid_t *)&gSpsCreditsCharUuid));

        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_CHARACTERISTIC, &evt, WAIT_FOR_CALLBACK_TIMEOUT));
        U_PORT_TEST_ASSERT_EQUAL(ch->connHandle, connHandle);
        U_PORT_TEST_ASSERT_EQUAL(ch->valHandle, 0);
        U_PORT_TEST_ASSERT_EQUAL(ch->properties, 0);

        U_TEST_PRINT_LINE("uPortGattStartCharacteristicDiscovery() - get all characteristics, no continue.");
        gGattIterReturnValue = U_PORT_GATT_ITER_STOP;
        errorCode = uPortGattStartCharacteristicDiscovery(connHandle, NULL,
                                                          gNinaW15SpsService.attrHandle,
                                                          gattCharDiscoveryCallback);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_SUCCESS);
        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_CHARACTERISTIC, &evt, WAIT_FOR_CALLBACK_TIMEOUT));
        U_PORT_TEST_ASSERT_EQUAL(ch->connHandle, connHandle);
        U_PORT_TEST_ASSERT_EQUAL(ch->attrHandle, gNinaW15SpsService.attrHandle + 1);
        U_PORT_TEST_ASSERT_EQUAL(ch->valHandle, gNinaW15SpsService.attrHandle + 2);
        U_PORT_TEST_ASSERT_EQUAL(ch->properties,
                                 (U_PORT_GATT_CHRC_READ | U_PORT_GATT_CHRC_WRITE_WITHOUT_RESP |
                                  U_PORT_GATT_CHRC_WRITE | U_PORT_GATT_CHRC_NOTIFY));
        U_PORT_TEST_ASSERT(cmpUuidStrict((uPortGattUuid_t *)&ch->uuid,
                                         (uPortGattUuid_t *)&gSpsFifoCharUuid));

        // timeout here, we should not get any more callbacks
        U_PORT_TEST_ASSERT(!waitForEvt(GATT_EVT_CHARACTERISTIC, &evt, WAIT_FOR_CALLBACK_TIMEOUT));
        gGattIterReturnValue = U_PORT_GATT_ITER_CONTINUE;

        U_TEST_PRINT_LINE("uPortGattStartCharacteristicDiscovery() - get specific char by UUID, appearance char.");
        errorCode = uPortGattStartCharacteristicDiscovery(connHandle,
                                                          (uPortGattUuid_t *)&gAppearanceCharUuid, 1,
                                                          gattCharDiscoveryCallback);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_SUCCESS);
        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_CHARACTERISTIC, &evt, WAIT_FOR_CALLBACK_TIMEOUT));
        U_PORT_TEST_ASSERT_EQUAL(ch->connHandle, connHandle);
        U_PORT_TEST_ASSERT_EQUAL(ch->attrHandle, gNinaW15GenericAccessService.attrHandle + 3);
        U_PORT_TEST_ASSERT_EQUAL(ch->valHandle, gNinaW15GenericAccessService.attrHandle + 4);
        U_PORT_TEST_ASSERT_EQUAL(ch->properties, U_PORT_GATT_CHRC_READ);
        U_PORT_TEST_ASSERT_EQUAL(ch->uuid.type, U_PORT_GATT_UUID_TYPE_16);
        U_PORT_TEST_ASSERT(cmpUuidStrict((uPortGattUuid_t *)&ch->uuid,
                                         (uPortGattUuid_t *)&gAppearanceCharUuid));

        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_CHARACTERISTIC, &evt, WAIT_FOR_CALLBACK_TIMEOUT));
        U_PORT_TEST_ASSERT_EQUAL(ch->connHandle, connHandle);
        U_PORT_TEST_ASSERT_EQUAL(ch->valHandle, 0);
        U_PORT_TEST_ASSERT_EQUAL(ch->properties, 0);

        // timeout here, we should not get any more callbacks
        U_PORT_TEST_ASSERT(!waitForEvt(GATT_EVT_CHARACTERISTIC, &evt, WAIT_FOR_CALLBACK_TIMEOUT));

        U_PORT_TEST_ASSERT_EQUAL(uPortGattDisconnectGap(connHandle), 0);
        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_CONN_STATUS, &evt, CONNECTION_SETUP_TIMEOUT));

        // Verify values
        U_PORT_TEST_ASSERT_EQUAL(evt.conn.pCallbackParam, gGattCallbackParamIn);
        U_PORT_TEST_ASSERT_EQUAL(evt.conn.status, U_PORT_GATT_GAP_DISCONNECTED);

        uPortTaskBlock(WAIT_FOR_CALLBACK_FINISH_DELAY);
        uPortGattDown();

        testOK = true;
    }
    U_PORT_TEST_ASSERT(testOK);

    deleteEvtQueue();

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("%d byte(s) of heap were lost to the C library during"
                      " this test and we have leaked %d byte(s).",
                      gSystemHeapLost - heapClibLossOffset,
                      heapUsed - (gSystemHeapLost - heapClibLossOffset));
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed < 0) ||
                       (heapUsed <= ((int32_t) gSystemHeapLost) - heapClibLossOffset));
}

U_PORT_TEST_FUNCTION("[portGatt]", "portGattDescDisc")
{
    gattEvt_t evt;
    int32_t errorCode;
    int32_t heapUsed;
    int32_t heapClibLossOffset = (int32_t) gSystemHeapLost;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();
    U_PORT_TEST_ASSERT_EQUAL(uPortInit(), 0);

    // Test cases
    createEvtQueue();

    U_TEST_PRINT_LINE("GATT descriptors discovery.");
    U_PORT_TEST_ASSERT_EQUAL(uPortGattInit(), 0);
    U_PORT_TEST_ASSERT_EQUAL(uPortGattAdd(), 0);
    U_PORT_TEST_ASSERT_EQUAL(uPortGattUp(false), 0);
    uPortGattSetGapConnStatusCallback(gapConnStatusCallback, gGattCallbackParamIn);

    // Retry this a couple of times if connection setup fails
    bool testOK = false;
    for (int i = 0; i < NBR_OF_CONNECTION_RETRIES && !testOK; i++) {

        int32_t connHandle = uPortGattConnectGap(gRemoteSpsPeripheral, gRemoteSpsPeripheralType, NULL);
        U_PORT_TEST_ASSERT(connHandle != U_PORT_GATT_GAP_INVALID_CONNHANDLE);

        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_CONN_STATUS, &evt, CONNECTION_SETUP_TIMEOUT));
        // Verify values
        U_PORT_TEST_ASSERT_EQUAL(gGattCallbackParamIn, evt.conn.pCallbackParam);
        if (evt.conn.status != U_PORT_GATT_GAP_CONNECTED) {
            // Block for small amount of time to let callback finish so we can connect again
            uPortTaskBlock(WAIT_FOR_CALLBACK_FINISH_DELAY);
            continue;
        }
        U_PORT_TEST_ASSERT_EQUAL(evt.conn.connHandle, connHandle);

        U_TEST_PRINT_LINE("uPortGattStartDescriptorDiscovery() - invalid conn handle.");
        errorCode = uPortGattStartDescriptorDiscovery(-1, U_PORT_GATT_CHRC_DESC_CLIENT_CHAR_CONF, 0,
                                                      gattDescriptorDiscoveryCallback);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);

        U_TEST_PRINT_LINE("uPortGattStartDescriptorDiscovery() - callback is NULL.");
        errorCode = uPortGattStartDescriptorDiscovery(connHandle, U_PORT_GATT_CHRC_DESC_CLIENT_CHAR_CONF, 0,
                                                      NULL);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);

        U_TEST_PRINT_LINE("uPortGattStartDescriptorDiscovery() - get all CCC descriptors of SPS service characteristics.");
        gGattIterReturnValue = U_PORT_GATT_ITER_CONTINUE;
        errorCode = uPortGattStartDescriptorDiscovery(connHandle, U_PORT_GATT_CHRC_DESC_CLIENT_CHAR_CONF,
                                                      gNinaW15SpsService.attrHandle + 1, // SPS FIFO char value
                                                      gattDescriptorDiscoveryCallback);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_SUCCESS);
        U_TEST_PRINT_LINE("uPortGattStartDescriptorDiscovery() errorCode %d.", errorCode);

        descriptorEvt_t *desc = &evt.desc;
        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_DESCRIPTOR, &evt, WAIT_FOR_CALLBACK_TIMEOUT));
        U_PORT_TEST_ASSERT_EQUAL(desc->connHandle, connHandle);
        U_PORT_TEST_ASSERT_EQUAL(desc->attrHandle, gNinaW15SpsService.attrHandle + 3); // FIFO char CCC
        U_PORT_TEST_ASSERT(cmpUuidStrict((uPortGattUuid_t *)&desc->uuid,
                                         (uPortGattUuid_t *)&gClientCharCfgUuid));

        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_DESCRIPTOR, &evt, WAIT_FOR_CALLBACK_TIMEOUT));
        U_PORT_TEST_ASSERT_EQUAL(desc->connHandle, connHandle);
        U_PORT_TEST_ASSERT_EQUAL(desc->attrHandle, gNinaW15SpsService.attrHandle +
                                 6); // Credits char CCC is also found since we have not stopped the discovery
        U_PORT_TEST_ASSERT(cmpUuidStrict((uPortGattUuid_t *)&desc->uuid,
                                         (uPortGattUuid_t *)&gClientCharCfgUuid));

        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_DESCRIPTOR, &evt, WAIT_FOR_CALLBACK_TIMEOUT));
        U_PORT_TEST_ASSERT_EQUAL(desc->connHandle, connHandle);
        U_PORT_TEST_ASSERT_EQUAL(desc->attrHandle, 0);

        U_TEST_PRINT_LINE("uPortGattStartDescriptorDiscovery() - get all CCC descriptors of SPS FIFO char, no continue.");
        gGattIterReturnValue = U_PORT_GATT_ITER_STOP;
        errorCode = uPortGattStartDescriptorDiscovery(connHandle, U_PORT_GATT_CHRC_DESC_CLIENT_CHAR_CONF,
                                                      gNinaW15SpsService.attrHandle + 1,
                                                      gattDescriptorDiscoveryCallback);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_SUCCESS);
        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_DESCRIPTOR, &evt, WAIT_FOR_CALLBACK_TIMEOUT));
        U_PORT_TEST_ASSERT_EQUAL(desc->connHandle, connHandle);
        U_PORT_TEST_ASSERT_EQUAL(desc->attrHandle, gNinaW15SpsService.attrHandle + 3); // FIFIO char CCC
        U_PORT_TEST_ASSERT(cmpUuidStrict((uPortGattUuid_t *)&desc->uuid,
                                         (uPortGattUuid_t *)&gClientCharCfgUuid));

        // timeout here, we should not get any more callbacks
        U_PORT_TEST_ASSERT(!waitForEvt(GATT_EVT_DESCRIPTOR, &evt, WAIT_FOR_CALLBACK_TIMEOUT));
        gGattIterReturnValue = U_PORT_GATT_ITER_CONTINUE;

        U_PORT_TEST_ASSERT(uPortGattDisconnectGap(connHandle) == 0);
        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_CONN_STATUS, &evt, CONNECTION_SETUP_TIMEOUT));
        // Verify values
        U_PORT_TEST_ASSERT_EQUAL(gGattCallbackParamIn, evt.conn.pCallbackParam);
        U_PORT_TEST_ASSERT_EQUAL(evt.conn.status, U_PORT_GATT_GAP_DISCONNECTED);

        uPortTaskBlock(WAIT_FOR_CALLBACK_FINISH_DELAY);
        uPortGattDown();

        testOK = true;
    }
    U_PORT_TEST_ASSERT(testOK);

    deleteEvtQueue();

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("%d byte(s) of heap were lost to the C library during"
                      " this test and we have leaked %d byte(s).",
                      gSystemHeapLost - heapClibLossOffset,
                      heapUsed - (gSystemHeapLost - heapClibLossOffset));
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed < 0) ||
                       (heapUsed <= ((int32_t) gSystemHeapLost) - heapClibLossOffset));
}

U_PORT_TEST_FUNCTION("[portGatt]", "portGattSubscribeAttrWrite")
{
    gattEvt_t evt;
    int32_t errorCode;
    int32_t heapUsed;
    int32_t heapClibLossOffset = (int32_t) gSystemHeapLost;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();
    U_PORT_TEST_ASSERT(uPortInit() == 0);

    // Test cases
    createEvtQueue();

    U_TEST_PRINT_LINE("GATT notification subscription and attribute write.");
    U_PORT_TEST_ASSERT_EQUAL(uPortGattInit(), 0);
    U_PORT_TEST_ASSERT_EQUAL(uPortGattAdd(), 0);
    U_PORT_TEST_ASSERT_EQUAL(uPortGattUp(false), 0);
    uPortGattSetGapConnStatusCallback(gapConnStatusCallback, gGattCallbackParamIn);

    // Retry this a couple of times if connection setup fails
    bool testOK = false;
    for (int i = 0; i < NBR_OF_CONNECTION_RETRIES && !testOK; i++) {
        int32_t connHandle = uPortGattConnectGap(gRemoteSpsPeripheral, gRemoteSpsPeripheralType, NULL);
        U_PORT_TEST_ASSERT(connHandle != U_PORT_GATT_GAP_INVALID_CONNHANDLE);

        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_CONN_STATUS, &evt, CONNECTION_SETUP_TIMEOUT));
        // Verify values
        U_PORT_TEST_ASSERT_EQUAL(gGattCallbackParamIn, evt.conn.pCallbackParam);
        if (evt.conn.status != U_PORT_GATT_GAP_CONNECTED) {
            // Block for small amount of time to let callback finish so we can connect again
            uPortTaskBlock(WAIT_FOR_CALLBACK_FINISH_DELAY);
            continue;
        }
        U_PORT_TEST_ASSERT_EQUAL(evt.conn.connHandle, connHandle);

        static uPortGattSubscribeParams_t subParams = {
            .notifyCb = gattNotifyFunc,
            .cccWriteRespCb = gattCccWriteResp,
            .valueHandle = 0, // Set separately below
            .cccHandle = 0,   // Set separately below
            .receiveNotifications = true,
            .receiveIndications = false,
        };
        subParams.valueHandle = gNinaW15SpsService.attrHandle + 2, // SPS FIFO
        subParams.cccHandle = gNinaW15SpsService.attrHandle + 3,

        U_TEST_PRINT_LINE("uPortGattSubscribe() - invalid conn handle.");
        errorCode = uPortGattSubscribe(-1, &subParams);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);

        U_TEST_PRINT_LINE("uPortGattSubscribe() - pParams is NULL.");
        errorCode = uPortGattSubscribe(connHandle, NULL);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);

        gGattIterReturnValue = U_PORT_GATT_ITER_CONTINUE;
        U_TEST_PRINT_LINE("uPortGattSubscribe() - SPS FIFO.");
        errorCode = uPortGattSubscribe(connHandle, &subParams);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_SUCCESS);
        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_WRITE_CCC, &evt, WAIT_FOR_CALLBACK_TIMEOUT));
        U_PORT_TEST_ASSERT_EQUAL(evt.writeCcc.err, 0);

        U_TEST_PRINT_LINE("uPortGattWriteAttribute() - invalid connection handle.");
        errorCode = uPortGattWriteAttribute(-1, gNinaW15SpsService.attrHandle + 2,
                                            "abcd", 4);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);

        U_TEST_PRINT_LINE("uPortGattWriteAttribute() - invalid attribute handle.");
        errorCode = uPortGattWriteAttribute(connHandle, 0, "abcd", 4);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);

        U_TEST_PRINT_LINE("uPortGattWriteAttribute() - NULL data.");
        errorCode = uPortGattWriteAttribute(connHandle,
                                            gNinaW15SpsService.attrHandle + 2,
                                            NULL, 4);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);

        // Since we have subscribed to the FIFO characteristics, but not the Credit characteristics
        // the remote server will echo data without any given credits. So writing to the FIFO
        // should produce a notification to us when the data is echoed.
        U_TEST_PRINT_LINE("uPortGattWriteAttribute() - write attribute on GATT server.");
        errorCode = uPortGattWriteAttribute(connHandle,
                                            gNinaW15SpsService.attrHandle + 2,
                                            "abcd", 4);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_SUCCESS);

        U_TEST_PRINT_LINE("get notified from GATT server.");
        notifyEvt_t *notify = &evt.notify;
        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_NOTIFY, &evt, WAIT_FOR_CALLBACK_TIMEOUT));
        U_PORT_TEST_ASSERT_EQUAL(notify->length, 4);
        U_PORT_TEST_ASSERT(memcmp(notify->data, "abcd", 4) == 0);
        U_PORT_TEST_ASSERT_EQUAL(notify->pParams, &subParams);

        gGattIterReturnValue = U_PORT_GATT_ITER_STOP; // Stop subscription on next notification
        U_TEST_PRINT_LINE("write attribute on GATT server again.");
        errorCode = uPortGattWriteAttribute(connHandle,
                                            gNinaW15SpsService.attrHandle + 2,
                                            "efgh", 4);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_SUCCESS);

        U_TEST_PRINT_LINE("get notified from GATT server and stop subscription.");
        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_NOTIFY, &evt, WAIT_FOR_CALLBACK_TIMEOUT));
        U_PORT_TEST_ASSERT_EQUAL(notify->length, 4);
        U_PORT_TEST_ASSERT(memcmp(notify->data, "efgh", 4) == 0);
        U_PORT_TEST_ASSERT_EQUAL(notify->pParams, &subParams);

        U_TEST_PRINT_LINE("write attribute on GATT server yet one more time.");
        errorCode = uPortGattWriteAttribute(connHandle,
                                            gNinaW15SpsService.attrHandle + 2,
                                            "ijkl", 4);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_SUCCESS);

        // There could be a last notification with 0 bytes data length
        // Don't assert on waitForCallback since we don't care weather we
        // get this last notification or not
        if (waitForEvt(GATT_EVT_NOTIFY, &evt, WAIT_FOR_CALLBACK_TIMEOUT)) {
            // Just make sure that if there was a notification it did not have any data
            U_PORT_TEST_ASSERT_EQUAL(notify->length, 0);
        }

        U_TEST_PRINT_LINE("write attribute on GATT server one last time.");
        errorCode = uPortGattWriteAttribute(connHandle,
                                            gNinaW15SpsService.attrHandle + 2,
                                            "mnop", 4);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_SUCCESS);
        // There should be no more notifications
        U_PORT_TEST_ASSERT(!waitForEvt(GATT_EVT_NOTIFY, &evt, WAIT_FOR_CALLBACK_TIMEOUT));

        U_TEST_PRINT_LINE("disconnect.");
        U_PORT_TEST_ASSERT_EQUAL(uPortGattDisconnectGap(connHandle), 0);
        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_CONN_STATUS, &evt, WAIT_FOR_CALLBACK_TIMEOUT));
        // Verify values
        U_PORT_TEST_ASSERT_EQUAL(gGattCallbackParamIn, evt.conn.pCallbackParam);
        U_PORT_TEST_ASSERT_EQUAL(evt.conn.status, U_PORT_GATT_GAP_DISCONNECTED);

        uPortTaskBlock(WAIT_FOR_CALLBACK_FINISH_DELAY);
        uPortGattDown();

        testOK = true;
    }
    U_PORT_TEST_ASSERT(testOK);

    deleteEvtQueue();

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("%d byte(s) of heap were lost to the C library during"
                      " this test and we have leaked %d byte(s).",
                      gSystemHeapLost - heapClibLossOffset,
                      heapUsed - (gSystemHeapLost - heapClibLossOffset));
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed < 0) ||
                       (heapUsed <= ((int32_t) gSystemHeapLost) - heapClibLossOffset));
}

U_PORT_TEST_FUNCTION("[portGatt]", "portGattServerConf")
{
    gattEvt_t evt;
    int32_t errorCode;
    int32_t heapUsed;
    int32_t heapClibLossOffset = (int32_t) gSystemHeapLost;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();
    U_PORT_TEST_ASSERT_EQUAL(uPortInit(), 0);

    // Test cases
    createEvtQueue();

    U_TEST_PRINT_LINE("GATT server registration and functionality.");
    U_PORT_TEST_ASSERT_EQUAL(uPortGattInit(), 0);
    U_PORT_TEST_ASSERT_EQUAL(uPortGattAdd(), 0);
    U_TEST_PRINT_LINE("uPortGattAddPrimaryService() - NULL service.");
    errorCode = uPortGattAddPrimaryService(NULL);
    U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);
    U_TEST_PRINT_LINE("uPortGattAddPrimaryService()");
    U_PORT_TEST_ASSERT_EQUAL(uPortGattAddPrimaryService(&gTestSpsService), 0);
    U_PORT_TEST_ASSERT_EQUAL(uPortGattUp(true), 0);
    uPortGattSetGapConnStatusCallback(gapConnStatusCallback, gGattCallbackParamIn);

    // Retry this a couple of times if connection setup fails
    bool testOK = false;
    for (int i = 0; i < NBR_OF_CONNECTION_RETRIES && !testOK; i++) {
        int32_t connHandle = uPortGattConnectGap(gRemoteSpsCentral, gRemoteSpsCentralType, NULL);
        U_PORT_TEST_ASSERT(connHandle != U_PORT_GATT_GAP_INVALID_CONNHANDLE);

        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_CONN_STATUS, &evt, CONNECTION_SETUP_TIMEOUT));
        // Verify values
        U_PORT_TEST_ASSERT_EQUAL(gGattCallbackParamIn, evt.conn.pCallbackParam);
        if (evt.conn.status != U_PORT_GATT_GAP_CONNECTED) {
            // Block for small amount of time to let callback finish so we can connect again
            uPortTaskBlock(WAIT_FOR_CALLBACK_FINISH_DELAY);
            continue;
        }
        U_PORT_TEST_ASSERT_EQUAL(evt.conn.connHandle, connHandle);

        spsWriteEvt_t *spsWrite = &evt.spsWrite;
        uint16_t cccValue = 0;
        U_TEST_PRINT_LINE("wait for Credit CCC write.");
        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_SPS_WRITE_CREDIT_CCC, &evt, CONNECTION_SETUP_TIMEOUT));
        U_PORT_TEST_ASSERT(parseSpsCccWriteData(spsWrite, &cccValue));
        U_PORT_TEST_ASSERT_EQUAL(cccValue, 1);

        U_TEST_PRINT_LINE("wait for FIFO CCC write.");
        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_SPS_WRITE_FIFO_CCC, &evt, CONNECTION_SETUP_TIMEOUT));
        U_PORT_TEST_ASSERT(parseSpsCccWriteData(spsWrite, &cccValue));
        U_PORT_TEST_ASSERT_EQUAL(cccValue, 1);

        U_TEST_PRINT_LINE("wait for Credit write.");
        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_SPS_WRITE_CREDIT_CHAR, &evt, CONNECTION_SETUP_TIMEOUT));
        U_PORT_TEST_ASSERT_EQUAL(spsWrite->offset, 0);
        U_PORT_TEST_ASSERT_EQUAL(spsWrite->length, 1);
        U_PORT_TEST_ASSERT(spsWrite->data[0] > 1);

        uint8_t credits = 10;
        U_TEST_PRINT_LINE("uPortGattNotify() - invalid connection handle.");
        errorCode = uPortGattNotify(-1, &gSpsCreditsChar, &credits, 1);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);
        U_TEST_PRINT_LINE("uPortGattNotify() - NULL characteristics.");
        errorCode = uPortGattNotify(connHandle, NULL, &credits, 1);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);
        U_TEST_PRINT_LINE("uPortGattNotify() - NULL data.");
        errorCode = uPortGattNotify(connHandle, &gSpsCreditsChar, NULL, 1);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);
        U_TEST_PRINT_LINE("uPortGattNotify() - data length = 0.");
        errorCode = uPortGattNotify(connHandle, &gSpsCreditsChar, &credits, 0);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);

        U_TEST_PRINT_LINE("notify credits to remote client.");
        errorCode = uPortGattNotify(connHandle, &gSpsCreditsChar, &credits, 1);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_SUCCESS);
        // We have no way of verifying directly that the credits reached the remote side
        // Indirectly it is verified if we get data back in the next step since remote
        // side should not send unless it has credits

        // If we send data before first credits has been processed on remote side it will be
        // dropped, so we have to wait a little
        uPortTaskBlock(200);

        notifyEvt_t *notify = &evt.notify;
        U_TEST_PRINT_LINE("notify data to remote client.");
        errorCode = uPortGattNotify(connHandle, &gSpsFifoChar, "abcd", 4);
        U_PORT_TEST_ASSERT_EQUAL(errorCode, (int32_t)U_ERROR_COMMON_SUCCESS);
        U_TEST_PRINT_LINE("wait for data to echo back.");
        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_SPS_WRITE_FIFO_CHAR, &evt, CONNECTION_SETUP_TIMEOUT));
        U_PORT_TEST_ASSERT_EQUAL(notify->length, sizeof(notify->data));
        U_PORT_TEST_ASSERT(memcmp(notify->data, "abcd", sizeof(notify->data)) == 0);

        U_TEST_PRINT_LINE("disconnect.");
        U_PORT_TEST_ASSERT_EQUAL(uPortGattDisconnectGap(connHandle), 0);
        U_PORT_TEST_ASSERT(waitForEvt(GATT_EVT_CONN_STATUS, &evt, WAIT_FOR_CALLBACK_TIMEOUT));
        // Verify values
        U_PORT_TEST_ASSERT_EQUAL(gGattCallbackParamIn, evt.conn.pCallbackParam);
        U_PORT_TEST_ASSERT_EQUAL(evt.conn.status, U_PORT_GATT_GAP_DISCONNECTED);
        uPortTaskBlock(WAIT_FOR_CALLBACK_FINISH_DELAY);
        uPortGattDown();

        testOK = true;
    }
    U_PORT_TEST_ASSERT(testOK);

    deleteEvtQueue();

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("%d byte(s) of heap were lost to the C library during"
                      " this test and we have leaked %d byte(s).",
                      gSystemHeapLost - heapClibLossOffset,
                      heapUsed - (gSystemHeapLost - heapClibLossOffset));
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed < 0) ||
                       (heapUsed <= ((int32_t) gSystemHeapLost) - heapClibLossOffset));
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[portGatt]", "portGattCleanUp")
{
    int32_t x;

    x = uPortTaskStackMinFree(NULL);
    if (x != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_TEST_PRINT_LINE("main task stack had a minimum of %d byte(s) free"
                          "at the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);
    }

    uPortDeinit();

    x = uPortGetHeapMinFree();
    if (x >= 0) {
        U_TEST_PRINT_LINE("heap had a minimum of %d byte(s) free at"
                          " the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

#endif
// End of file
