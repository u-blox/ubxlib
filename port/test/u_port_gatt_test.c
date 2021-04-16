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
#define NBR_OF_CONNECTION_RETRIES 3
#define CONNECTION_SETUP_TIMEOUT 5000
#define WAIT_FOR_CALLBACK_TIMEOUT 1000
#define WAIT_FOR_CALLBACK_FINISH_DELAY 100

#define USE_REAL_SEMAPHORES

#define U_PORT_GATT_TEST_NBR_OF_SERVICES 4

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC PROTOTYPES
 * -------------------------------------------------------------- */
static bool waitForCallback(int32_t timeoutMs);
static bool waitForTest(int32_t timeoutMs);
static void signalFromCallback(void);
static void signalFromTest(void);
static int32_t hexToInt(const char *pIn, uint8_t *pOut);
static int32_t addrStringToArray(const char *pAddrIn, uint8_t *pAddrOut,
                                 uPortBtLeAddressType_t *pType);
static void createSemaphores(void);
static void deleteSemaphores(void);
static void printUuid(uPortGattUuid_t *pUuid);
static void copyUuid(uPortGattUuid_t *pUuidSrc, uPortGattUuid_t *pUuidDest);
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
static int32_t remoteWritesFifoChar(int32_t gapConnHandle, const void *buf, uint16_t len,
                                    uint16_t offset, uint8_t flags);
static int32_t remoteWritesFifoCcc(int32_t gapConnHandle, const void *buf, uint16_t len,
                                   uint16_t offset, uint8_t flags);
static int32_t remoteWritesCreditChar(int32_t gapConnHandle, const void *buf, uint16_t len,
                                      uint16_t offset, uint8_t flags);
static int32_t remoteWritesCreditCcc(int32_t gapConnHandle, const void *buf, uint16_t len,
                                     uint16_t offset, uint8_t flags);

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */
/** For tracking heap lost to memory  lost by the C library.
 */
static size_t gSystemHeapLost = 0;

#ifdef USE_REAL_SEMAPHORES
static uPortSemaphoreHandle_t gGattCallbackSem;
static uPortSemaphoreHandle_t gGattTestSem;
#else
static volatile bool gGattCallbackSem;
static volatile bool gGattTestSem;
#endif
//lint -esym(843, gGattCallbackParamIn) "could be declared as const"
static void *gGattCallbackParamIn = (void *)123456;
static volatile void *gGattCallbackParamOut = 0;
static uPortGattGapConnStatus_t gGattConnStatus = U_PORT_GATT_GAP_DISCONNECTED;
static int32_t gGattConnHandle = 0;
static uint16_t gGattAttrHandle = 0;
static uint16_t gGattValueHandle = 0;
static uint8_t gGattProperties = 0;
static uint16_t gGattEndHandle = 0;
static uPortGattUuid128_t gGattUuid;
static uint8_t gGattReceivedData[4];
static uint8_t gGattCccWriteErr;
static uint16_t gGattNotificationLength;
//lint -esym(844, gpGattNotifyParams) "could be declared as pointing to const"
static uPortGattSubscribeParams_t *gpGattNotifyParams;
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
static uint16_t gGattCcc;
static uint8_t gGattSpsCredits;

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
static bool waitForCallback(int32_t timeoutMs)
{
#ifdef USE_REAL_SEMAPHORES
    return (uPortSemaphoreTryTake(gGattCallbackSem, timeoutMs) == 0);
#else
    while (!gGattCallbackSem && timeoutMs > 0) {
        uPortTaskBlock(100);
        timeoutMs -= 100;
    }
    gGattCallbackSem = false;
    return (timeoutMs > 0);
#endif
}

static bool waitForTest(int32_t timeoutMs)
{
#ifdef USE_REAL_SEMAPHORES
    U_PORT_TEST_ASSERT(uPortSemaphoreTryTake(gGattTestSem, timeoutMs) == 0);
#else
    while (!gGattTestSem && timeoutMs > 0) {
        uPortTaskBlock(100);
        timeoutMs -= 100;
    }
    gGattTestSem = false;
#endif
    return (timeoutMs > 0);
}

static void signalFromCallback(void)
{
#ifdef USE_REAL_SEMAPHORES
    U_PORT_TEST_ASSERT(uPortSemaphoreGive(gGattCallbackSem) == 0);
#else
    gGattCallbackSem = true;
#endif
}

static void signalFromTest(void)
{
#ifdef USE_REAL_SEMAPHORES
    U_PORT_TEST_ASSERT(uPortSemaphoreGive(gGattTestSem) == 0);
#else
    gGattTestSem = true;
#endif
}

// TODO This function was copied from u_ble_data_intmod.c. Make it common?
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

// TODO This function was copied from u_ble_data_intmod.c. Make it common?
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

static void createSemaphores(void)
{
#ifdef USE_REAL_SEMAPHORES
    U_PORT_TEST_ASSERT(uPortSemaphoreCreate(&gGattCallbackSem, 0, 1) == 0);
    U_PORT_TEST_ASSERT(uPortSemaphoreCreate(&gGattTestSem, 0, 1) == 0);
#else
    gGattCallbackSem = false;
    gGattTestSem = false;
#endif
}

static void deleteSemaphores(void)
{
#ifdef USE_REAL_SEMAPHORES
    U_PORT_TEST_ASSERT(uPortSemaphoreDelete(gGattCallbackSem) == 0);
    U_PORT_TEST_ASSERT(uPortSemaphoreDelete(gGattTestSem) == 0);
#endif
}

//lint -efunc(826, printUuid) "Suspicious pointer-to-pointer conversion (area too small)"
static void printUuid(uPortGattUuid_t *pUuid)
{
    if (pUuid != NULL) {
        switch (pUuid->type) {
            case U_PORT_GATT_UUID_TYPE_16:
                uPortLog("UUID16: 0x%04X", ((uPortGattUuid16_t *)pUuid)->val);
                break;
            case U_PORT_GATT_UUID_TYPE_32:
                uPortLog("UUID32: 0x%08X", ((uPortGattUuid32_t *)pUuid)->val);
                break;
            case U_PORT_GATT_UUID_TYPE_128:
                uPortLog("UUID128: 0x");
                for (int ii = 0; ii < 16; ii++) {
                    uPortLog("%02X", ((uPortGattUuid128_t *)pUuid)->val[ii]);
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

static void copyUuid(uPortGattUuid_t *pUuidSrc, uPortGattUuid_t *pUuidDest)
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

static void gapConnStatusCallback(int32_t connHandle,
                                  uPortGattGapConnStatus_t status,
                                  void *pCallbackParam)
{
    gGattConnHandle = connHandle;
    gGattConnStatus = status;
    gGattCallbackParamOut = pCallbackParam;
    uPortLog("U_PORT_TEST: BT connect status(connHandle=%d, status=%d, pCallbackParam=%d)\n",
             gGattConnHandle, gGattConnStatus, gGattCallbackParamOut);
    signalFromCallback();
}

static uPortGattIter_t gattServiceDiscoveryCallback(int32_t connHandle,
                                                    uPortGattUuid_t *pUuid,
                                                    uint16_t attrHandle,
                                                    uint16_t endHandle)
{
    gGattConnHandle = connHandle;
    gGattAttrHandle = attrHandle;
    gGattEndHandle = endHandle;
    uPortLog("U_PORT_TEST: callback(connHandle=%d, attrHandle=%d, endHandle=%d, ", connHandle,
             gGattAttrHandle, gGattEndHandle);
    if (pUuid != NULL) {
        copyUuid(pUuid, (uPortGattUuid_t *)&gGattUuid);
        printUuid((uPortGattUuid_t *)&gGattUuid);
    } else {
        uPortLog("UUID: NULL");
    }
    uPortLog(")\n");

    signalFromCallback();
    waitForTest(WAIT_FOR_CALLBACK_TIMEOUT);

    return gGattIterReturnValue;
}

static uPortGattIter_t gattCharDiscoveryCallback(int32_t connHandle,
                                                 uPortGattUuid_t *pUuid,
                                                 uint16_t attrHandle,
                                                 uint16_t valHandle,
                                                 uint8_t  properties)
{
    gGattConnHandle = connHandle;
    gGattAttrHandle = attrHandle;
    gGattValueHandle = valHandle;
    gGattProperties = properties;
    uPortLog("U_PORT_TEST: callback(connHandle=%d, attrHandle=%d, valueHandle=%d, properties=0x%02x,\n                      ",
             connHandle, gGattAttrHandle, gGattValueHandle, gGattProperties);
    if (pUuid != NULL) {
        copyUuid(pUuid, (uPortGattUuid_t *)&gGattUuid);
        printUuid((uPortGattUuid_t *)&gGattUuid);
    } else {
        uPortLog("UUID: NULL");
    }
    uPortLog(")\n");

    signalFromCallback();
    waitForTest(WAIT_FOR_CALLBACK_TIMEOUT);

    return gGattIterReturnValue;
}

static uPortGattIter_t gattDescriptorDiscoveryCallback(int32_t connHandle,
                                                       uPortGattUuid_t *pUuid,
                                                       uint16_t  attrHandle)
{
    gGattConnHandle = connHandle;
    gGattAttrHandle = attrHandle;
    uPortLog("U_PORT_TEST: callback(connHandle=%d, attrHandle=%d, ", connHandle, gGattAttrHandle);
    if (pUuid != NULL) {
        copyUuid(pUuid, (uPortGattUuid_t *)&gGattUuid);
        printUuid((uPortGattUuid_t *)&gGattUuid);
    } else {
        uPortLog("UUID: NULL");
    }
    uPortLog(")\n");

    signalFromCallback();
    waitForTest(WAIT_FOR_CALLBACK_TIMEOUT);

    return gGattIterReturnValue;
}

static uPortGattIter_t gattNotifyFunc(int32_t connHandle,
                                      struct uPortGattSubscribeParams_s *pParams,
                                      const void *pData, uint16_t length)
{
    gGattConnHandle = connHandle;
    gGattNotificationLength = length;
    gpGattNotifyParams = pParams;
    uPortLog("U_PORT_TEST: Notified with %d bytes of data\n", length);
    if (length == sizeof gGattReceivedData) {
        memcpy(gGattReceivedData, (const uint8_t *)pData, sizeof gGattReceivedData);
    }

    signalFromCallback();
    waitForTest(WAIT_FOR_CALLBACK_TIMEOUT);

    return gGattIterReturnValue;
}

static void gattCccWriteResp(int32_t connHandle, uint8_t err)
{
    gGattConnHandle = connHandle;
    gGattCccWriteErr = err;
    uPortLog("U_PORT_TEST: Characteristics Client Configuration write ");
    if (err == 0) {
        uPortLog("successful!\n");
    } else {
        uPortLog("failed!\n");
    }
    signalFromCallback();
}

static int32_t remoteWritesFifoChar(int32_t gapConnHandle, const void *buf, uint16_t len,
                                    uint16_t offset, uint8_t flags)
{
    (void)flags;
    if (gapConnHandle == gGattConnHandle) {
        uPortLog("U_PORT_TEST: remote writes to FIFO characteristics\n");
        if ((len == sizeof gGattReceivedData) && (offset == 0)) {
            memcpy(gGattReceivedData, (const uint8_t *)buf, len);
        }
        signalFromCallback();
        waitForTest(WAIT_FOR_CALLBACK_TIMEOUT);
    }

    return len;
}

static int32_t remoteWritesFifoCcc(int32_t gapConnHandle, const void *buf, uint16_t len,
                                   uint16_t offset, uint8_t flags)
{
    (void)flags;
    if (gapConnHandle == gGattConnHandle) {
        if ((len == 2) && (offset == 0)) {
            gGattCcc = *(const uint8_t *)buf | *((const uint8_t *)buf + 1) << 8;
        }
        uPortLog("U_PORT_TEST: remote writes to FIFO CCC to 0x%04x\n", gGattCcc);
        signalFromCallback();
        waitForTest(WAIT_FOR_CALLBACK_TIMEOUT);
    }

    return len;
}

static int32_t remoteWritesCreditChar(int32_t gapConnHandle, const void *buf, uint16_t len,
                                      uint16_t offset, uint8_t flags)
{
    (void)flags;
    if (gapConnHandle == gGattConnHandle) {
        if ((len == 1) && (offset == 0)) {
            gGattSpsCredits = *(const uint8_t *)buf;
        }
        uPortLog("U_PORT_TEST: remote writes %d credits to Credit characteristics!\n", gGattSpsCredits);
        signalFromCallback();
        waitForTest(WAIT_FOR_CALLBACK_TIMEOUT);
    }

    return len;
}

static int32_t remoteWritesCreditCcc(int32_t gapConnHandle, const void *buf, uint16_t len,
                                     uint16_t offset, uint8_t flags)
{
    (void)flags;
    if (gapConnHandle == gGattConnHandle) {
        if ((len == 2) && (offset == 0)) {
            gGattCcc = *(const uint8_t *)buf | *((const uint8_t *)buf + 1) << 8;
        }
        uPortLog("U_PORT_TEST: remote writes to Credit CCC to 0x%04x\n", gGattCcc);
        signalFromCallback();
        waitForTest(WAIT_FOR_CALLBACK_TIMEOUT);
    }

    return len;
}

/* ----------------------------------------------------------------
 * TESTS
 * -------------------------------------------------------------- */

U_PORT_TEST_FUNCTION("[portGatt]", "portGattInitTests")
{
    int32_t errorCode;

    errorCode = addrStringToArray(gRemoteSpsPeripheralStr, gRemoteSpsPeripheral,
                                  &gRemoteSpsPeripheralType);
    uPortLog("U_PORT_TEST: Using %s as remote peripheral\n", gRemoteSpsPeripheralStr);
    U_PORT_TEST_ASSERT(errorCode == 0);
    errorCode = addrStringToArray(gRemoteSpsCentralStr, gRemoteSpsCentral, &gRemoteSpsCentralType);
    uPortLog("U_PORT_TEST: Using %s as remote central\n", gRemoteSpsCentralStr);
    U_PORT_TEST_ASSERT(errorCode == 0);
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

    createSemaphores();

    uPortLog("U_PORT_TEST: GATT init\n");
    U_PORT_TEST_ASSERT(uPortGattInit() == 0);
    U_PORT_TEST_ASSERT(uPortGattAdd() == 0);
    U_PORT_TEST_ASSERT(uPortGattUp(true) == 0);
    U_PORT_TEST_ASSERT(uPortGattIsAdvertising());
    uPortGattDown();
    uPortGattDeinit();
    U_PORT_TEST_ASSERT(uPortGattAdd() == 0);
    U_PORT_TEST_ASSERT(uPortGattUp(false) == 0);
    U_PORT_TEST_ASSERT(!uPortGattIsAdvertising());

    uPortGattSetGapConnStatusCallback(gapConnStatusCallback, gGattCallbackParamIn);

    uPortLog("U_PORT_TEST: uPortGattConnectGap to unavailable device\n");
    int32_t connHandle = uPortGattConnectGap(gInvalidAddress, gRemoteSpsPeripheralType);
    U_PORT_TEST_ASSERT(connHandle != U_PORT_GATT_GAP_INVALID_CONNHANDLE);
    U_PORT_TEST_ASSERT(waitForCallback(CONNECTION_SETUP_TIMEOUT));
    U_PORT_TEST_ASSERT(gGattConnStatus == U_PORT_GATT_GAP_DISCONNECTED);

    // Retry this a couple of times if connection setup fails
    bool testOK = false;
    for (int i = 0; i < NBR_OF_CONNECTION_RETRIES && !testOK; i++) {

        uPortLog("U_PORT_TEST: uPortGattConnectGap to device\n");
        connHandle = uPortGattConnectGap(gRemoteSpsPeripheral, gRemoteSpsPeripheralType);
        U_PORT_TEST_ASSERT(connHandle != U_PORT_GATT_GAP_INVALID_CONNHANDLE);

        U_PORT_TEST_ASSERT(waitForCallback(CONNECTION_SETUP_TIMEOUT));
        // Verify values
        U_PORT_TEST_ASSERT(gGattCallbackParamIn == gGattCallbackParamOut);
        if (gGattConnStatus != U_PORT_GATT_GAP_CONNECTED) {
            // Block for small amount of time to let callback finish so we can connect again
            uPortTaskBlock(WAIT_FOR_CALLBACK_FINISH_DELAY);
            continue;
        }
        U_PORT_TEST_ASSERT(gGattConnHandle == connHandle);

        uPortLog("U_PORT_TEST: uPortGattGetRemoteAddress - NULL addr\n");
        uint8_t addr[6];
        uPortBtLeAddressType_t addrType;
        errorCode = uPortGattGetRemoteAddress(connHandle, NULL, &addrType);
        U_PORT_TEST_ASSERT(errorCode == (int32_t)U_ERROR_COMMON_UNKNOWN);

        uPortLog("U_PORT_TEST: uPortGattGetRemoteAddress - NULL addr type\n");
        errorCode = uPortGattGetRemoteAddress(connHandle, addr, NULL);
        U_PORT_TEST_ASSERT(errorCode == (int32_t)U_ERROR_COMMON_UNKNOWN);

        uPortLog("U_PORT_TEST: uPortGattGetRemoteAddress - invalid conn handle\n");
        errorCode = uPortGattGetRemoteAddress(U_PORT_GATT_GAP_INVALID_CONNHANDLE, addr, &addrType);
        U_PORT_TEST_ASSERT(errorCode == (int32_t)U_ERROR_COMMON_UNKNOWN);

        uPortLog("U_PORT_TEST: uPortGattGetRemoteAddress\n");
        errorCode = uPortGattGetRemoteAddress(connHandle, addr, &addrType);
        U_PORT_TEST_ASSERT(errorCode == 0);
        U_PORT_TEST_ASSERT(memcmp(addr, gRemoteSpsPeripheral, 6) == 0);
        U_PORT_TEST_ASSERT(addrType == gRemoteSpsPeripheralType);

        U_PORT_TEST_ASSERT(uPortGattDisconnectGap(connHandle) == 0);
        U_PORT_TEST_ASSERT(waitForCallback(WAIT_FOR_CALLBACK_TIMEOUT));
        // Verify values
        U_PORT_TEST_ASSERT(gGattCallbackParamIn == gGattCallbackParamOut);
        U_PORT_TEST_ASSERT(gGattConnStatus == U_PORT_GATT_GAP_DISCONNECTED);

        uPortLog("U_PORT_TEST: uPortGattDisconnectGap when not connected\n");
        U_PORT_TEST_ASSERT(uPortGattDisconnectGap(connHandle) == (int32_t)U_ERROR_COMMON_UNKNOWN);
        uPortGattDown();

        testOK = true;
    }
    U_PORT_TEST_ASSERT(testOK);

    deleteSemaphores();

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_PORT_TEST: %d byte(s) of heap were lost to"
             " the C library during this test and we have"
             " leaked %d byte(s).\n",
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
    U_PORT_TEST_ASSERT(uPortInit() == 0);

    // Test cases
    createSemaphores();

    uPortLog("U_PORT_TEST: GATT primary service search\n");
    U_PORT_TEST_ASSERT(uPortGattInit() == 0);
    U_PORT_TEST_ASSERT(uPortGattAdd() == 0);
    U_PORT_TEST_ASSERT(uPortGattUp(false) == 0);
    uPortGattSetGapConnStatusCallback(gapConnStatusCallback, gGattCallbackParamIn);

    // Retry this a couple of times if connection setup fails
    bool testOK = false;
    for (int i = 0; i < NBR_OF_CONNECTION_RETRIES && !testOK; i++) {

        int32_t connHandle = uPortGattConnectGap(gRemoteSpsPeripheral, gRemoteSpsPeripheralType);
        U_PORT_TEST_ASSERT(connHandle != U_PORT_GATT_GAP_INVALID_CONNHANDLE);

        U_PORT_TEST_ASSERT(waitForCallback(CONNECTION_SETUP_TIMEOUT));
        // Verify values
        U_PORT_TEST_ASSERT(gGattCallbackParamIn == gGattCallbackParamOut);
        if (gGattConnStatus != U_PORT_GATT_GAP_CONNECTED) {
            // Block for small amount of time to let callback finish so we can connect again
            uPortTaskBlock(WAIT_FOR_CALLBACK_FINISH_DELAY);
            continue;
        }
        U_PORT_TEST_ASSERT(gGattConnHandle == connHandle);
        gGattAttrHandle = 0xFFFF;

        uPortLog("U_PORT_TEST: uPortGattStartPrimaryServiceDiscovery - invalid conn handle\n");
        errorCode = uPortGattStartPrimaryServiceDiscovery(-1, NULL,
                                                          gattServiceDiscoveryCallback);
        U_PORT_TEST_ASSERT(errorCode == (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);

        uPortLog("U_PORT_TEST: uPortGattStartPrimaryServiceDiscovery - callback is NULL\n");
        errorCode = uPortGattStartPrimaryServiceDiscovery(connHandle, NULL, NULL);
        U_PORT_TEST_ASSERT(errorCode == (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);

        uPortLog("U_PORT_TEST: uPortGattStartPrimaryServiceDiscovery - get all services\n");
        gGattIterReturnValue = U_PORT_GATT_ITER_CONTINUE;
        errorCode = uPortGattStartPrimaryServiceDiscovery(connHandle, NULL,
                                                          gattServiceDiscoveryCallback);
        U_PORT_TEST_ASSERT(errorCode == (int32_t)U_ERROR_COMMON_SUCCESS);

        // Discover all available services and compare to expected result
        int32_t serviceIndex = 0;
        do {
            U_PORT_TEST_ASSERT(waitForCallback(WAIT_FOR_CALLBACK_TIMEOUT));
            U_PORT_TEST_ASSERT(gGattConnHandle == connHandle);
            if (gGattAttrHandle != 0) {
                U_PORT_TEST_ASSERT(serviceIndex < U_PORT_GATT_TEST_NBR_OF_SERVICES);
                //lint -e661 "Possible access of out-of-bounds pointer"
                U_PORT_TEST_ASSERT(cmpUuidStrict((uPortGattUuid_t *)&gGattUuid,
                                                 &gExpectedServices[serviceIndex]->uuid.uuid));
                U_PORT_TEST_ASSERT(gGattAttrHandle == gExpectedServices[serviceIndex]->attrHandle);
                U_PORT_TEST_ASSERT(gGattEndHandle == gExpectedServices[serviceIndex]->endHandle);
                //lint +e661
            }
            serviceIndex++;
            signalFromTest();
        } while ((gGattAttrHandle != 0) && (serviceIndex <= U_PORT_GATT_TEST_NBR_OF_SERVICES + 1));
        U_PORT_TEST_ASSERT(serviceIndex == U_PORT_GATT_TEST_NBR_OF_SERVICES + 1);

        uPortLog("U_PORT_TEST: uPortGattStartPrimaryServiceDiscovery - get all services, no continue\n");
        gGattIterReturnValue = U_PORT_GATT_ITER_STOP;
        errorCode = uPortGattStartPrimaryServiceDiscovery(connHandle, NULL,
                                                          gattServiceDiscoveryCallback);
        U_PORT_TEST_ASSERT(waitForCallback(WAIT_FOR_CALLBACK_TIMEOUT));
        U_PORT_TEST_ASSERT(gGattConnHandle == connHandle);
        U_PORT_TEST_ASSERT(gGattAttrHandle == gExpectedServices[0]->attrHandle);
        U_PORT_TEST_ASSERT(gGattEndHandle == gExpectedServices[0]->endHandle);
        U_PORT_TEST_ASSERT(cmpUuidStrict((uPortGattUuid_t *)&gGattUuid,
                                         (uPortGattUuid_t *) & (gExpectedServices[0]->uuid)));
        signalFromTest();
        // Timeout here, we should not get any more callbacks
        U_PORT_TEST_ASSERT(!waitForCallback(WAIT_FOR_CALLBACK_TIMEOUT));

        uPortLog("U_PORT_TEST: uPortGattStartPrimaryServiceDiscovery - get specific service\n");
        errorCode =
            uPortGattStartPrimaryServiceDiscovery(connHandle,
                                                  &gNinaW15SpsService.uuid.uuid,
                                                  gattServiceDiscoveryCallback);
        U_PORT_TEST_ASSERT(waitForCallback(WAIT_FOR_CALLBACK_TIMEOUT));
        U_PORT_TEST_ASSERT(gGattConnHandle == connHandle);
        U_PORT_TEST_ASSERT(gGattAttrHandle == gNinaW15SpsService.attrHandle);
        U_PORT_TEST_ASSERT(gGattEndHandle == gNinaW15SpsService.endHandle);
        U_PORT_TEST_ASSERT(cmpUuidStrict((uPortGattUuid_t *)&gGattUuid,
                                         &gNinaW15SpsService.uuid.uuid)); // DIS
        signalFromTest();

        U_PORT_TEST_ASSERT(uPortGattDisconnectGap(connHandle) == 0);
        U_PORT_TEST_ASSERT(waitForCallback(WAIT_FOR_CALLBACK_TIMEOUT));
        // Verify values
        U_PORT_TEST_ASSERT(gGattCallbackParamIn == gGattCallbackParamOut);
        U_PORT_TEST_ASSERT(gGattConnStatus == U_PORT_GATT_GAP_DISCONNECTED);
        uPortGattDown();

        testOK = true;
    }
    U_PORT_TEST_ASSERT(testOK);

    deleteSemaphores();

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_PORT_TEST: %d byte(s) of heap were lost to"
             " the C library during this test and we have"
             " leaked %d byte(s).\n",
             gSystemHeapLost - heapClibLossOffset,
             heapUsed - (gSystemHeapLost - heapClibLossOffset));
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed < 0) ||
                       (heapUsed <= ((int32_t) gSystemHeapLost) - heapClibLossOffset));
}

U_PORT_TEST_FUNCTION("[portGatt]", "portGattCharDisc")
{
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
    createSemaphores();

    uPortLog("U_PORT_TEST: GATT characteristic discovery\n");
    U_PORT_TEST_ASSERT(uPortGattInit() == 0);
    U_PORT_TEST_ASSERT(uPortGattAdd() == 0);
    U_PORT_TEST_ASSERT(uPortGattUp(false) == 0);
    uPortGattSetGapConnStatusCallback(gapConnStatusCallback, gGattCallbackParamIn);

    // Retry this a couple of times if connection setup fails
    bool testOK = false;
    for (int i = 0; i < NBR_OF_CONNECTION_RETRIES && !testOK; i++) {

        int32_t connHandle = uPortGattConnectGap(gRemoteSpsPeripheral, gRemoteSpsPeripheralType);
        U_PORT_TEST_ASSERT(connHandle != U_PORT_GATT_GAP_INVALID_CONNHANDLE);

        U_PORT_TEST_ASSERT(waitForCallback(CONNECTION_SETUP_TIMEOUT));
        // Verify values
        U_PORT_TEST_ASSERT(gGattCallbackParamIn == gGattCallbackParamOut);
        signalFromTest();
        if (gGattConnStatus != U_PORT_GATT_GAP_CONNECTED) {
            // Block for small amount of time to let callback finish so we can connect again
            uPortTaskBlock(WAIT_FOR_CALLBACK_FINISH_DELAY);
            continue;
        }
        U_PORT_TEST_ASSERT(gGattConnHandle == connHandle);
        gGattAttrHandle = 0xFFFF;

        uPortLog("U_PORT_TEST: uPortGattStartCharacteristicDiscovery - invalid conn handle\n");
        errorCode = uPortGattStartCharacteristicDiscovery(-1, NULL, 0, gattCharDiscoveryCallback);
        U_PORT_TEST_ASSERT(errorCode == (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);

        uPortLog("U_PORT_TEST: uPortGattStartCharacteristicDiscovery - callback is NULL\n");
        errorCode = uPortGattStartCharacteristicDiscovery(connHandle, NULL, 0, NULL);
        U_PORT_TEST_ASSERT(errorCode == (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);

        uPortLog("U_PORT_TEST: uPortGattStartCharacteristicDiscovery - get all characteristics of SPS service\n");
        gGattIterReturnValue = U_PORT_GATT_ITER_CONTINUE;
        errorCode = uPortGattStartCharacteristicDiscovery(connHandle, NULL,
                                                          gNinaW15SpsService.attrHandle,
                                                          gattCharDiscoveryCallback);
        U_PORT_TEST_ASSERT(errorCode == (int32_t)U_ERROR_COMMON_SUCCESS);

        U_PORT_TEST_ASSERT(waitForCallback(WAIT_FOR_CALLBACK_TIMEOUT));
        U_PORT_TEST_ASSERT(gGattConnHandle == connHandle);
        U_PORT_TEST_ASSERT(gGattAttrHandle == gNinaW15SpsService.attrHandle + 1);
        U_PORT_TEST_ASSERT(gGattValueHandle == gNinaW15SpsService.attrHandle + 2);
        U_PORT_TEST_ASSERT(gGattProperties == (U_PORT_GATT_CHRC_READ | U_PORT_GATT_CHRC_WRITE_WITHOUT_RESP |
                                               U_PORT_GATT_CHRC_WRITE | U_PORT_GATT_CHRC_NOTIFY));
        U_PORT_TEST_ASSERT(cmpUuidStrict((uPortGattUuid_t *)&gGattUuid,
                                         (uPortGattUuid_t *)&gSpsFifoCharUuid));
        signalFromTest();

        U_PORT_TEST_ASSERT(waitForCallback(WAIT_FOR_CALLBACK_TIMEOUT));
        U_PORT_TEST_ASSERT(gGattConnHandle == connHandle);
        U_PORT_TEST_ASSERT(gGattAttrHandle == gNinaW15SpsService.attrHandle + 4);
        U_PORT_TEST_ASSERT(gGattValueHandle == gNinaW15SpsService.attrHandle + 5);
        U_PORT_TEST_ASSERT(gGattProperties ==
                           (U_PORT_GATT_CHRC_WRITE_WITHOUT_RESP | U_PORT_GATT_CHRC_WRITE | U_PORT_GATT_CHRC_NOTIFY));
        U_PORT_TEST_ASSERT(cmpUuidStrict((uPortGattUuid_t *)&gGattUuid,
                                         (uPortGattUuid_t *)&gSpsCreditsCharUuid));
        signalFromTest();

        U_PORT_TEST_ASSERT(waitForCallback(WAIT_FOR_CALLBACK_TIMEOUT));
        U_PORT_TEST_ASSERT(gGattConnHandle == connHandle);
        U_PORT_TEST_ASSERT(gGattValueHandle == 0);
        U_PORT_TEST_ASSERT(gGattProperties == 0);
        signalFromTest();

        uPortLog("U_PORT_TEST: uPortGattStartCharacteristicDiscovery - get all characteristics, no continue\n");
        gGattIterReturnValue = U_PORT_GATT_ITER_STOP;
        errorCode = uPortGattStartCharacteristicDiscovery(connHandle, NULL,
                                                          gNinaW15SpsService.attrHandle,
                                                          gattCharDiscoveryCallback);
        U_PORT_TEST_ASSERT(waitForCallback(WAIT_FOR_CALLBACK_TIMEOUT));
        U_PORT_TEST_ASSERT(gGattConnHandle == connHandle);
        U_PORT_TEST_ASSERT(gGattAttrHandle == gNinaW15SpsService.attrHandle + 1);
        U_PORT_TEST_ASSERT(gGattValueHandle == gNinaW15SpsService.attrHandle + 2);
        U_PORT_TEST_ASSERT(gGattProperties == (U_PORT_GATT_CHRC_READ | U_PORT_GATT_CHRC_WRITE_WITHOUT_RESP |
                                               U_PORT_GATT_CHRC_WRITE | U_PORT_GATT_CHRC_NOTIFY));
        U_PORT_TEST_ASSERT(cmpUuidStrict((uPortGattUuid_t *)&gGattUuid,
                                         (uPortGattUuid_t *)&gSpsFifoCharUuid));
        signalFromTest();
        // timeout here, we should not get any more callbacks
        U_PORT_TEST_ASSERT(!waitForCallback(WAIT_FOR_CALLBACK_TIMEOUT));
        gGattIterReturnValue = U_PORT_GATT_ITER_CONTINUE;

        uPortLog("U_PORT_TEST: uPortGattStartCharacteristicDiscovery - get specific char by UUID, appearance char\n");
        errorCode = uPortGattStartCharacteristicDiscovery(connHandle,
                                                          (uPortGattUuid_t *)&gAppearanceCharUuid, 1,
                                                          gattCharDiscoveryCallback);
        U_PORT_TEST_ASSERT(waitForCallback(WAIT_FOR_CALLBACK_TIMEOUT));
        U_PORT_TEST_ASSERT(gGattConnHandle == connHandle);
        U_PORT_TEST_ASSERT(gGattAttrHandle == gNinaW15GenericAccessService.attrHandle + 3);
        U_PORT_TEST_ASSERT(gGattValueHandle == gNinaW15GenericAccessService.attrHandle + 4);
        U_PORT_TEST_ASSERT(gGattProperties == U_PORT_GATT_CHRC_READ);
        U_PORT_TEST_ASSERT(gGattUuid.type == U_PORT_GATT_UUID_TYPE_16);
        U_PORT_TEST_ASSERT(cmpUuidStrict((uPortGattUuid_t *)&gGattUuid,
                                         (uPortGattUuid_t *)&gAppearanceCharUuid));
        signalFromTest();

        U_PORT_TEST_ASSERT(waitForCallback(WAIT_FOR_CALLBACK_TIMEOUT));
        U_PORT_TEST_ASSERT(gGattConnHandle == connHandle);
        U_PORT_TEST_ASSERT(gGattValueHandle == 0);
        U_PORT_TEST_ASSERT(gGattProperties == 0);
        signalFromTest();
        // timeout here, we should not get any more callbacks
        U_PORT_TEST_ASSERT(!waitForCallback(WAIT_FOR_CALLBACK_TIMEOUT));

        U_PORT_TEST_ASSERT(uPortGattDisconnectGap(connHandle) == 0);
        U_PORT_TEST_ASSERT(waitForCallback(WAIT_FOR_CALLBACK_TIMEOUT));
        // Verify values
        U_PORT_TEST_ASSERT(gGattCallbackParamOut == gGattCallbackParamIn);
        U_PORT_TEST_ASSERT(gGattConnStatus == U_PORT_GATT_GAP_DISCONNECTED);
        signalFromTest();
        uPortTaskBlock(WAIT_FOR_CALLBACK_FINISH_DELAY);
        uPortGattDown();

        testOK = true;
    }
    U_PORT_TEST_ASSERT(testOK);

    deleteSemaphores();

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_PORT_TEST: %d byte(s) of heap were lost to"
             " the C library during this test and we have"
             " leaked %d byte(s).\n",
             gSystemHeapLost - heapClibLossOffset,
             heapUsed - (gSystemHeapLost - heapClibLossOffset));
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed < 0) ||
                       (heapUsed <= ((int32_t) gSystemHeapLost) - heapClibLossOffset));
}

U_PORT_TEST_FUNCTION("[portGatt]", "portGattDescDisc")
{
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
    createSemaphores();

    uPortLog("U_PORT_TEST: GATT descriptors discovery\n");
    U_PORT_TEST_ASSERT(uPortGattInit() == 0);
    U_PORT_TEST_ASSERT(uPortGattAdd() == 0);
    U_PORT_TEST_ASSERT(uPortGattUp(false) == 0);
    uPortGattSetGapConnStatusCallback(gapConnStatusCallback, gGattCallbackParamIn);

    // Retry this a couple of times if connection setup fails
    bool testOK = false;
    for (int i = 0; i < NBR_OF_CONNECTION_RETRIES && !testOK; i++) {

        int32_t connHandle = uPortGattConnectGap(gRemoteSpsPeripheral, gRemoteSpsPeripheralType);
        U_PORT_TEST_ASSERT(connHandle != U_PORT_GATT_GAP_INVALID_CONNHANDLE);

        U_PORT_TEST_ASSERT(waitForCallback(CONNECTION_SETUP_TIMEOUT));
        // Verify values
        U_PORT_TEST_ASSERT(gGattCallbackParamIn == gGattCallbackParamOut);
        signalFromTest();
        if (gGattConnStatus != U_PORT_GATT_GAP_CONNECTED) {
            // Block for small amount of time to let callback finish so we can connect again
            uPortTaskBlock(WAIT_FOR_CALLBACK_FINISH_DELAY);
            continue;
        }
        U_PORT_TEST_ASSERT(gGattConnHandle == connHandle);
        gGattAttrHandle = 0xFFFF;

        uPortLog("U_PORT_TEST: uPortGattStartDescriptorDiscovery - invalid conn handle\n");
        errorCode = uPortGattStartDescriptorDiscovery(-1, U_PORT_GATT_CHRC_DESC_CLIENT_CHAR_CONF, 0,
                                                      gattDescriptorDiscoveryCallback);
        U_PORT_TEST_ASSERT(errorCode == (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);

        uPortLog("U_PORT_TEST: uPortGattStartDescriptorDiscovery - callback is NULL\n");
        errorCode = uPortGattStartDescriptorDiscovery(connHandle, U_PORT_GATT_CHRC_DESC_CLIENT_CHAR_CONF, 0,
                                                      NULL);
        U_PORT_TEST_ASSERT(errorCode == (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);

        uPortLog("U_PORT_TEST: uPortGattStartDescriptorDiscovery - get all CCC descriptors of SPS service characteristics\n");
        gGattIterReturnValue = U_PORT_GATT_ITER_CONTINUE;
        errorCode = uPortGattStartDescriptorDiscovery(connHandle, U_PORT_GATT_CHRC_DESC_CLIENT_CHAR_CONF,
                                                      gNinaW15SpsService.attrHandle + 1, // SPS FIFO char value
                                                      gattDescriptorDiscoveryCallback);
        uPortLog("U_PORT_TEST: uPortGattStartDescriptorDiscovery errorCode %d\n", errorCode);

        U_PORT_TEST_ASSERT(waitForCallback(WAIT_FOR_CALLBACK_TIMEOUT));
        U_PORT_TEST_ASSERT(gGattConnHandle == connHandle);
        U_PORT_TEST_ASSERT(gGattAttrHandle == gNinaW15SpsService.attrHandle + 3); // FIFO char CCC
        U_PORT_TEST_ASSERT(cmpUuidStrict((uPortGattUuid_t *)&gGattUuid,
                                         (uPortGattUuid_t *)&gClientCharCfgUuid));
        signalFromTest();

        U_PORT_TEST_ASSERT(waitForCallback(WAIT_FOR_CALLBACK_TIMEOUT));
        U_PORT_TEST_ASSERT(gGattConnHandle == connHandle);
        U_PORT_TEST_ASSERT(gGattAttrHandle == gNinaW15SpsService.attrHandle +
                           6); // Credits char CCC is also found since we have not stopped the discovery
        U_PORT_TEST_ASSERT(cmpUuidStrict((uPortGattUuid_t *)&gGattUuid,
                                         (uPortGattUuid_t *)&gClientCharCfgUuid));
        signalFromTest();

        U_PORT_TEST_ASSERT(waitForCallback(WAIT_FOR_CALLBACK_TIMEOUT));
        U_PORT_TEST_ASSERT(gGattConnHandle == connHandle);
        U_PORT_TEST_ASSERT(gGattAttrHandle == 0);
        U_PORT_TEST_ASSERT(gGattProperties == 0);
        signalFromTest();

        uPortLog("U_PORT_TEST: uPortGattStartDescriptorDiscovery - get all CCC descriptors of SPS FIFO char, no continue\n");
        gGattIterReturnValue = U_PORT_GATT_ITER_STOP;
        errorCode = uPortGattStartDescriptorDiscovery(connHandle, U_PORT_GATT_CHRC_DESC_CLIENT_CHAR_CONF,
                                                      gNinaW15SpsService.attrHandle + 1,
                                                      gattDescriptorDiscoveryCallback);
        U_PORT_TEST_ASSERT(waitForCallback(WAIT_FOR_CALLBACK_TIMEOUT));
        U_PORT_TEST_ASSERT(gGattConnHandle == connHandle);
        U_PORT_TEST_ASSERT(gGattAttrHandle == gNinaW15SpsService.attrHandle + 3); // FIFIO char CCC
        U_PORT_TEST_ASSERT(cmpUuidStrict((uPortGattUuid_t *)&gGattUuid,
                                         (uPortGattUuid_t *)&gClientCharCfgUuid));
        signalFromTest();
        // timeout here, we should not get any more callbacks
        U_PORT_TEST_ASSERT(!waitForCallback(WAIT_FOR_CALLBACK_TIMEOUT));
        gGattIterReturnValue = U_PORT_GATT_ITER_CONTINUE;

        U_PORT_TEST_ASSERT(uPortGattDisconnectGap(connHandle) == 0);
        U_PORT_TEST_ASSERT(waitForCallback(WAIT_FOR_CALLBACK_TIMEOUT));
        // Verify values
        U_PORT_TEST_ASSERT(gGattCallbackParamIn == gGattCallbackParamOut);
        U_PORT_TEST_ASSERT(gGattConnStatus == U_PORT_GATT_GAP_DISCONNECTED);
        signalFromTest();
        uPortTaskBlock(WAIT_FOR_CALLBACK_FINISH_DELAY);
        uPortGattDown();

        testOK = true;
    }
    U_PORT_TEST_ASSERT(testOK);

    deleteSemaphores();

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_PORT_TEST: %d byte(s) of heap were lost to"
             " the C library during this test and we have"
             " leaked %d byte(s).\n",
             gSystemHeapLost - heapClibLossOffset,
             heapUsed - (gSystemHeapLost - heapClibLossOffset));
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed < 0) ||
                       (heapUsed <= ((int32_t) gSystemHeapLost) - heapClibLossOffset));
}

U_PORT_TEST_FUNCTION("[portGatt]", "portGattSubscribeAttrWrite")
{
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
    createSemaphores();

    uPortLog("U_PORT_TEST: GATT notification subscription and attribute write\n");
    U_PORT_TEST_ASSERT(uPortGattInit() == 0);
    U_PORT_TEST_ASSERT(uPortGattAdd() == 0);
    U_PORT_TEST_ASSERT(uPortGattUp(false) == 0);
    uPortGattSetGapConnStatusCallback(gapConnStatusCallback, gGattCallbackParamIn);

    // Retry this a couple of times if connection setup fails
    bool testOK = false;
    for (int i = 0; i < NBR_OF_CONNECTION_RETRIES && !testOK; i++) {
        int32_t connHandle = uPortGattConnectGap(gRemoteSpsPeripheral, gRemoteSpsPeripheralType);
        U_PORT_TEST_ASSERT(connHandle != U_PORT_GATT_GAP_INVALID_CONNHANDLE);

        U_PORT_TEST_ASSERT(waitForCallback(CONNECTION_SETUP_TIMEOUT));
        // Verify values
        U_PORT_TEST_ASSERT(gGattCallbackParamIn == gGattCallbackParamOut);
        signalFromTest();
        if (gGattConnStatus != U_PORT_GATT_GAP_CONNECTED) {
            // Block for small amount of time to let callback finish so we can connect again
            uPortTaskBlock(WAIT_FOR_CALLBACK_FINISH_DELAY);
            continue;
        }
        U_PORT_TEST_ASSERT(gGattConnHandle == connHandle);

        static uPortGattSubscribeParams_t subParams = {
            .notifyCb = gattNotifyFunc,
            .cccWriteRespCb = gattCccWriteResp,
            .valueHandle = gNinaW15SpsService.attrHandle + 2, // SPS FIFO
            .cccHandle = gNinaW15SpsService.attrHandle + 3,
            .receiveNotifications = true,
            .receiveIndications = false,
        };

        uPortLog("U_PORT_TEST: uPortGattSubscribe - invalid conn handle\n");
        errorCode = uPortGattSubscribe(-1, &subParams);
        U_PORT_TEST_ASSERT(errorCode == (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);

        uPortLog("U_PORT_TEST: uPortGattSubscribe - pParams is NULL\n");
        errorCode = uPortGattSubscribe(gGattConnHandle, NULL);
        U_PORT_TEST_ASSERT(errorCode == (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);

        gGattIterReturnValue = U_PORT_GATT_ITER_CONTINUE;
        uPortLog("U_PORT_TEST: uPortGattSubscribe - SPS FIFO\n");
        gGattCccWriteErr = 0xff;
        errorCode = uPortGattSubscribe(gGattConnHandle, &subParams);
        U_PORT_TEST_ASSERT(waitForCallback(WAIT_FOR_CALLBACK_TIMEOUT));
        U_PORT_TEST_ASSERT(gGattCccWriteErr == 0);

        uPortLog("U_PORT_TEST: uPortGattWriteAttribute - invalid connection handle\n");
        errorCode = uPortGattWriteAttribute(-1, gNinaW15SpsService.attrHandle + 2,
                                            "abcd", 4);
        U_PORT_TEST_ASSERT(errorCode == (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);

        uPortLog("U_PORT_TEST: uPortGattWriteAttribute - invalid attribute handle\n");
        errorCode = uPortGattWriteAttribute(gGattConnHandle, 0, "abcd", 4);
        U_PORT_TEST_ASSERT(errorCode == (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);

        uPortLog("U_PORT_TEST: uPortGattWriteAttribute - NULL data\n");
        errorCode = uPortGattWriteAttribute(gGattConnHandle,
                                            gNinaW15SpsService.attrHandle + 2,
                                            NULL, 4);
        U_PORT_TEST_ASSERT(errorCode == (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);

        // Since we have subscribed to the FIFO characteristics, but not the Credit characteristics
        // the remote server will echo data without any given credits. So writing to the FIFO
        // should produce a notification to us when the data is echoed.
        uPortLog("U_PORT_TEST: uPortGattWriteAttribute - write attribute on GATT server\n");
        memset(gGattReceivedData, 0x00, sizeof gGattReceivedData);
        errorCode = uPortGattWriteAttribute(gGattConnHandle,
                                            gNinaW15SpsService.attrHandle + 2,
                                            "abcd", 4);
        U_PORT_TEST_ASSERT(errorCode == (int32_t)U_ERROR_COMMON_SUCCESS);

        uPortLog("U_PORT_TEST: get notified from GATT server\n");
        U_PORT_TEST_ASSERT(waitForCallback(WAIT_FOR_CALLBACK_TIMEOUT));
        U_PORT_TEST_ASSERT(memcmp(gGattReceivedData, "abcd", 4) == 0);
        U_PORT_TEST_ASSERT(gpGattNotifyParams == &subParams);
        U_PORT_TEST_ASSERT(gGattNotificationLength == 4);
        signalFromTest();

        gpGattNotifyParams = NULL;
        gGattNotificationLength = 0;
        gGattIterReturnValue = U_PORT_GATT_ITER_STOP; // Stop subscription on next notification
        uPortLog("U_PORT_TEST: write attribute on GATT server again\n");
        memset(gGattReceivedData, 0x00, sizeof gGattReceivedData);
        errorCode = uPortGattWriteAttribute(gGattConnHandle,
                                            gNinaW15SpsService.attrHandle + 2,
                                            "efgh", 4);
        U_PORT_TEST_ASSERT(errorCode == (int32_t)U_ERROR_COMMON_SUCCESS);

        uPortLog("U_PORT_TEST: get notified from GATT server and stop subscription\n");
        U_PORT_TEST_ASSERT(waitForCallback(WAIT_FOR_CALLBACK_TIMEOUT));
        U_PORT_TEST_ASSERT(memcmp(gGattReceivedData, "efgh", 4) == 0);
        U_PORT_TEST_ASSERT(gpGattNotifyParams == &subParams);
        U_PORT_TEST_ASSERT(gGattNotificationLength == 4);
        signalFromTest();

        uPortLog("U_PORT_TEST: write attribute on GATT server yet one more time\n");
        gpGattNotifyParams = NULL;
        gGattNotificationLength = 0;
        errorCode = uPortGattWriteAttribute(gGattConnHandle,
                                            gNinaW15SpsService.attrHandle + 2,
                                            "ijkl", 4);
        U_PORT_TEST_ASSERT(errorCode == (int32_t)U_ERROR_COMMON_SUCCESS);

        // There could be a last notification with 0 bytes data length
        // Don't assert on waitForCallback since we don't care weather we
        // get this last notification or not
        if (waitForCallback(WAIT_FOR_CALLBACK_TIMEOUT)) {
            // Just make sure that if there was a notification it did not have any data
            U_PORT_TEST_ASSERT(gGattNotificationLength == 0);
        }

        uPortLog("U_PORT_TEST: write attribute on GATT server one last time\n");
        errorCode = uPortGattWriteAttribute(gGattConnHandle,
                                            gNinaW15SpsService.attrHandle + 2,
                                            "mnop", 4);
        U_PORT_TEST_ASSERT(errorCode == (int32_t)U_ERROR_COMMON_SUCCESS);
        // There should be no more notifications
        U_PORT_TEST_ASSERT(!waitForCallback(WAIT_FOR_CALLBACK_TIMEOUT));

        uPortLog("U_PORT_TEST: disconnect\n");
        U_PORT_TEST_ASSERT(uPortGattDisconnectGap(connHandle) == 0);
        U_PORT_TEST_ASSERT(waitForCallback(WAIT_FOR_CALLBACK_TIMEOUT));
        // Verify values
        U_PORT_TEST_ASSERT(gGattCallbackParamIn == gGattCallbackParamOut);
        U_PORT_TEST_ASSERT(gGattConnStatus == U_PORT_GATT_GAP_DISCONNECTED);
        signalFromTest();
        uPortTaskBlock(WAIT_FOR_CALLBACK_FINISH_DELAY);
        uPortGattDown();

        testOK = true;
    }
    U_PORT_TEST_ASSERT(testOK);

    deleteSemaphores();

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_PORT_TEST: %d byte(s) of heap were lost to"
             " the C library during this test and we have"
             " leaked %d byte(s).\n",
             gSystemHeapLost - heapClibLossOffset,
             heapUsed - (gSystemHeapLost - heapClibLossOffset));
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT((heapUsed < 0) ||
                       (heapUsed <= ((int32_t) gSystemHeapLost) - heapClibLossOffset));
}

U_PORT_TEST_FUNCTION("[portGatt]", "portGattServerConf")
{
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
    createSemaphores();

    uPortLog("U_PORT_TEST: GATT server registration and functionality\n");
    U_PORT_TEST_ASSERT(uPortGattInit() == 0);
    U_PORT_TEST_ASSERT(uPortGattAdd() == 0);
    uPortLog("U_PORT_TEST: uPortGattAddPrimaryService - NULL service\n");
    errorCode = uPortGattAddPrimaryService(NULL);
    U_PORT_TEST_ASSERT(errorCode == (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);
    uPortLog("U_PORT_TEST: uPortGattAddPrimaryService\n");
    U_PORT_TEST_ASSERT(uPortGattAddPrimaryService(&gTestSpsService) == 0);
    U_PORT_TEST_ASSERT(uPortGattUp(true) == 0);
    uPortGattSetGapConnStatusCallback(gapConnStatusCallback, gGattCallbackParamIn);

    // Retry this a couple of times if connection setup fails
    bool testOK = false;
    for (int i = 0; i < NBR_OF_CONNECTION_RETRIES && !testOK; i++) {
        int32_t connHandle = uPortGattConnectGap(gRemoteSpsCentral, gRemoteSpsCentralType);
        U_PORT_TEST_ASSERT(connHandle != U_PORT_GATT_GAP_INVALID_CONNHANDLE);

        U_PORT_TEST_ASSERT(waitForCallback(CONNECTION_SETUP_TIMEOUT));
        // Verify values
        U_PORT_TEST_ASSERT(gGattCallbackParamIn == gGattCallbackParamOut);
        signalFromTest();
        if (gGattConnStatus != U_PORT_GATT_GAP_CONNECTED) {
            // Block for small amount of time to let callback finish so we can connect again
            uPortTaskBlock(WAIT_FOR_CALLBACK_FINISH_DELAY);
            continue;
        }
        U_PORT_TEST_ASSERT(gGattConnHandle == connHandle);

        uPortLog("U_PORT_TEST: wait for Credit CCC write\n");
        U_PORT_TEST_ASSERT(waitForCallback(CONNECTION_SETUP_TIMEOUT));
        U_PORT_TEST_ASSERT(gGattCcc == 1);
        gGattCcc = 0;
        signalFromTest();

        uPortLog("U_PORT_TEST: wait for FIFO CCC write\n");
        U_PORT_TEST_ASSERT(waitForCallback(CONNECTION_SETUP_TIMEOUT));
        U_PORT_TEST_ASSERT(gGattCcc == 1);
        gGattSpsCredits = 0;
        signalFromTest();

        uPortLog("U_PORT_TEST: wait for Credit write\n");
        U_PORT_TEST_ASSERT(waitForCallback(CONNECTION_SETUP_TIMEOUT));
        U_PORT_TEST_ASSERT(gGattSpsCredits > 1);
        signalFromTest();

        uint8_t credits = 10;
        uPortLog("U_PORT_TEST: uPortGattNotify - invalid connection handle\n");
        errorCode = uPortGattNotify(-1, &gSpsCreditsChar, &credits, 1);
        U_PORT_TEST_ASSERT(errorCode == (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);
        uPortLog("U_PORT_TEST: uPortGattNotify - NULL characteristics\n");
        errorCode = uPortGattNotify(gGattConnHandle, NULL, &credits, 1);
        U_PORT_TEST_ASSERT(errorCode == (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);
        uPortLog("U_PORT_TEST: uPortGattNotify - NULL data\n");
        errorCode = uPortGattNotify(gGattConnHandle, &gSpsCreditsChar, NULL, 1);
        U_PORT_TEST_ASSERT(errorCode == (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);
        uPortLog("U_PORT_TEST: uPortGattNotify - data length = 0\n");
        errorCode = uPortGattNotify(gGattConnHandle, &gSpsCreditsChar, &credits, 0);
        U_PORT_TEST_ASSERT(errorCode == (int32_t)U_ERROR_COMMON_INVALID_PARAMETER);

        uPortLog("U_PORT_TEST: notify credits to remote client\n");
        errorCode = uPortGattNotify(gGattConnHandle, &gSpsCreditsChar, &credits, 1);
        U_PORT_TEST_ASSERT(errorCode == (int32_t)U_ERROR_COMMON_SUCCESS);
        // We have no way of verifying directly that the credits reached the remote side
        // Indirectly it is verified if we get data back in the next step since remote
        // side should not send unless it has credits

        // If we send data before first credits has been processed on remote side it will be
        // dropped, so we have to wait a little
        uPortTaskBlock(200);

        memset(gGattReceivedData, 0x00, sizeof gGattReceivedData);
        uPortLog("U_PORT_TEST: notify data to remote client\n");
        errorCode = uPortGattNotify(gGattConnHandle, &gSpsFifoChar, "abcd", 4);
        uPortLog("U_PORT_TEST: wait for data to echo back\n");
        U_PORT_TEST_ASSERT(waitForCallback(CONNECTION_SETUP_TIMEOUT));
        U_PORT_TEST_ASSERT(memcmp(gGattReceivedData, "abcd", sizeof gGattReceivedData) == 0);

        uPortLog("U_PORT_TEST: disconnect\n");
        U_PORT_TEST_ASSERT(uPortGattDisconnectGap(connHandle) == 0);
        U_PORT_TEST_ASSERT(waitForCallback(WAIT_FOR_CALLBACK_TIMEOUT));
        // Verify values
        U_PORT_TEST_ASSERT(gGattCallbackParamIn == gGattCallbackParamOut);
        U_PORT_TEST_ASSERT(gGattConnStatus == U_PORT_GATT_GAP_DISCONNECTED);
        signalFromTest();
        uPortTaskBlock(WAIT_FOR_CALLBACK_FINISH_DELAY);
        uPortGattDown();

        testOK = true;
    }
    U_PORT_TEST_ASSERT(testOK);

    deleteSemaphores();

    // Check for memory leaks
    heapUsed -= uPortGetHeapFree();
    uPortLog("U_PORT_TEST: %d byte(s) of heap were lost to"
             " the C library during this test and we have"
             " leaked %d byte(s).\n",
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
    uPortLog("U_PORT_TEST: main task stack had a minimum of %d"
             " byte(s) free at the end of these tests.\n", x);
    U_PORT_TEST_ASSERT(x >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);

    uPortDeinit();

    x = uPortGetHeapMinFree();
    if (x >= 0) {
        uPortLog("U_PORT_TEST: heap had a minimum of %d"
                 " byte(s) free at the end of these tests.\n", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

#endif
// End of file
