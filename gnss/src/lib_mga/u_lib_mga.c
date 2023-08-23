/******************************************************************************
 * Copyright 2013-2023 u-blox AG, Thalwil, Switzerland
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
 *
 ******************************************************************************
 *
 * Project: libMGA
 * Purpose: Library providing functions to help a host application to download
 *          MGA assistance data and pass it on to a u-blox GNSS receiver.
 *
 *****************************************************************************/

// MODIFIED throughout:
// assert() -> U_ASSERT(),
// malloc -> pUPortMalloc,
// free -> uPortFree,
// time() -> uPortGetTickTimeMs() / 1000

// MODIFIED: headers added
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

///////////////////////////////////////////////////////////////////////////////
// includes
// MODIFIED: header file renamed
#include "u_lib_mga.h"

// MODIFIED: native headers removed, we need to go through the port API
#if 0
#ifdef WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#else // WIN32
#  include <sys/types.h>
#  include <sys/socket.h>
#  include <sys/ioctl.h>
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <unistd.h>
#  include <errno.h>
#  include <fcntl.h>
#  include <pthread.h>

typedef int SOCKET;
#  define INVALID_SOCKET        (-1)
#  define SOCKET_ERROR          (-1)
#endif // WIN32
#include <ctype.h>  // MODIFIED: no longer required
#endif // Not required

// MODIFED: quotes instead of <> for consistency with ubxlib
#include "time.h"
#include "string.h"

// MODIFIED: headers added
#include "u_port_clib_platform_specific.h" /* Integer stdio, must be included
                                              before the other port files if
                                              any print or scan function is used. */
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_time.h"
#include "u_ubx_protocol.h" // uUbxProtocolUint32Encode()
#include "u_gnss_mga.h"

// MODIFIED: assert.h -> u_assert.h
#include "u_assert.h"

//uncomment if SSL should be used
//#define USE_SSL

#if defined USE_SSL
#include "mbedtls/include/mbedtls/config.h"
#include "mbedtls/include/mbedtls/platform.h"
#include "mbedtls/include/mbedtls/net.h"
#include "mbedtls/include/mbedtls/debug.h"
#include "mbedtls/include/mbedtls/ssl.h"
#include "mbedtls/include/mbedtls/entropy.h"
#include "mbedtls/include/mbedtls/ctr_drbg.h"
#include "mbedtls/include/mbedtls/error.h"
#include "mbedtls/include/mbedtls/certs.h"
#  ifdef WIN32
#    ifdef NDEBUG
#      pragma comment (lib, "mbedTLS.lib")
#    else
#      pragma comment (lib, "mbedTLS_Debug.lib")
#    endif //NDEBUG
#  endif // WIN32
#else
#  include <stdio.h>
#  include <stdlib.h>
#endif //USE_SSL

///////////////////////////////////////////////////////////////////////////////
// definitions & types
#define UBX_SIG_PREFIX_1        0xB5
#define UBX_SIG_PREFIX_2        0x62
#define UBX_MSG_FRAME_SIZE      8
#define UBX_MSG_PAYLOAD_OFFSET  6

#define UBX_CLASS_MGA           0x13
#define UBX_MGA_ANO             0x20
#define UBX_MGA_ACK             0x60
#define UBX_MGA_BDS             0x03
#define UBX_MGA_GPS             0x00
#define UBX_MGA_GAL             0x02
#define UBX_MGA_QZSS            0x05
#define UBX_MGA_GLO             0x06
#define UBX_MGA_INI             0x40
#define UBX_MGA_FLASH           0x21
#define UBX_MGA_DBD_MSG         0x80
#define UBX_CFG_NAVX5           0x23

// MODIFIED: add UBX_CFG_VALSET and the key for ack aiding
#define UBX_CFG_VALSET          0x8a
#define CFG_NAVSPG_ACKAIDING    0x10110025

#define UBX_CLASS_ACK           0x05
#define UBX_ACK_ACK             0x01
#define UBX_ACK_NAK             0x00

#define UBX_CLASS_CFG           0x06
#define UBX_CFG_MSG             0x01

#define UBX_CLASS_AID           0x0B
#define UBX_AID_INI             0x01
#define UBX_AID_HUI             0x02
#define UBX_AID_ALM             0x30
#define UBX_AID_EPH             0x31
#define UBX_AID_ALP             0x50
#define UBX_AID_ALPSRV          0x32

#define UBX_AID_ALP_ACK_SIZE    9

#define FLASH_DATA_MSG_PAYLOAD  512
#define MS_IN_A_NS              1000000

#define MIN(a,b)                (((a) < (b)) ? (a) : (b))

#define PRIMARY_SERVER_RESPONSE_TIMEOUT      5      // seconds
#define SECONDARY_SERVER_RESPONSE_TIMEOUT   30      // seconds

#define DEFAULT_AID_DAYS    14
#define MAX_AID_DAYS        14
#define DEFAULT_MGA_DAYS    28
#define MAX_MGA_DAYS        35

#define NUM_SAT_ID 6
#define NUM_GPS_ID 0
#define NUM_GAL_ID 1
#define NUM_BDS_ID 2
#define NUM_QZSS_ID 3
#define NUM_GLO_ID 4
#define NUM_ANO_ID 5

#define MGA_MAX_CONTENT_LEN 1000000

#ifndef SOCK_NONBLOCK
#  define SOCK_NONBLOCK O_NONBLOCK //!< If SOCK_NONBLOCK is not defined, O_NONBLOCK is used instead
#endif //SOCK_NONBLOCK

typedef struct
{
    UBX_U1 header1;
    UBX_U1 header2;
    UBX_U1 msgClass;
    UBX_U1 msgId;
    UBX_U2 payloadLength;
    UBX_U1 type;
    UBX_U1 typeVersion;
    UBX_U2 sequence;
    UBX_U2 payloadCount;
} FlashDataMsgHeader;

// Legacy aiding message (UBX-AID-ALP) header layout
typedef struct
{
    UBX_U1 header1;
    UBX_U1 header2;
    UBX_U1 msgClass;
    UBX_U1 msgId;
    UBX_U2 payloadLength;
} UbxMsgHeader;

typedef enum
{
    MGA_ACK_MSG_NAK = 0,
    MGA_ACK_MSG_ACK
} MGA_ACK_TYPES;

// Legacy aiding flash data transfer process states
typedef enum
{
    LEGACY_AIDING_IDLE,
    LEGACY_AIDING_STARTING,
    LEGACY_AIDING_MAIN_SEQ,
    LEGACY_AIDING_STOPPING
} LEGACY_AIDING_STATE;

///////////////////////////////////////////////////////////////////////////////
// module variables
static const MgaEventInterface* s_pEvtInterface = NULL;
static const MgaFlowConfiguration* s_pFlowConfig = NULL;
static const void* s_pCallbackContext = NULL;

static MGA_LIB_STATE s_sessionState = MGA_IDLE;

static UBX_U1* s_pMgaDataSession = NULL;

static MgaMsgInfo* s_pMgaMsgList = NULL;
static UBX_U4 s_mgaBlockCount = 0;
static UBX_U4 s_ackCount = 0;
static MgaMsgInfo* s_pLastMsgSent = NULL;
static UBX_U4 s_messagesSent = 0;

static MgaMsgInfo* s_pMgaFlashBlockList = NULL;
static UBX_U4 s_mgaFlashBlockCount = 0;
static MgaMsgInfo* s_pLastFlashBlockSent = NULL;
static UBX_U4 s_flashMessagesSent = 0;
static UBX_U2 s_flashSequence = 0;

// Specific support for legacy aiding
static bool s_bLegacyAiding = false;
static LEGACY_AIDING_STATE s_aidState = LEGACY_AIDING_IDLE;
static time_t s_aidingTimeout = 0;

static UBX_U2 s_alpfileId = 0;
static UBX_U1 *s_pAidingData = NULL;
static UBX_U4 s_aidingDataSize = 0;
static bool s_aidingSrvActive = false;

#ifdef UBXONLY
bool s_DBDCheckActive = false;
#endif

// MODIFIED: removed, not required
#if 0
static long s_serverResponseTimeout = 0;
#endif // Not required

static const int AID_DAYS[] = { 1, 2, 3, 5, 7, 10, 14 };

// MODIFIED: use ubxlib port API instead of WIN32 or pthread_mutex_t
static uPortMutexHandle_t s_mgaLock = NULL;

///////////////////////////////////////////////////////////////////////////////
// local function declarations
// MODIFIED: these functions removed, not required
#if 0
static SOCKET connectServer(const char* strServer, UBX_U2 wPort);
static int getHttpHeader(SOCKET sock, char* buf, int cnt);
static const char* skipSpaces(const char* pHttpPos);
static const char* nextToken(const char* pText);
static int getData(SOCKET sock, char* p, size_t iSize);
#endif // Not required

#if defined USE_SSL
static MGA_API_RESULT getOnlineDataFromServerSSL(const char* pServer, const MgaOnlineServerConfig* pServerConfig, UBX_U1** ppData, UBX_I4* piSize);
static MGA_API_RESULT getOfflineDataFromServerSSL(const char* pServer, const MgaOfflineServerConfig* pServerConfig, UBX_U1** ppData, UBX_I4* piSize);
static MGA_API_RESULT getDataFromServiceSSL(const char* pRequest, const char* server, UBX_U2 port, bool bVerifyServerCert, UBX_U1** ppData, UBX_I4* piSize);
static UBX_U1 checkForHTTPS(const char* pServer);
#endif //USE_SSL
// MODIFIED: these functions removed, not required
#if 0
static MGA_API_RESULT getOnlineDataFromServer(const char* pServer, MgaOnlineServerConfig* pServerConfig, UBX_U1** ppData, UBX_I4* piSize);
static MGA_API_RESULT getOfflineDataFromServer(const char* pServer, MgaOfflineServerConfig* pServerConfig, UBX_U1** ppData, UBX_I4* piSize);
static MGA_API_RESULT getDataFromService(SOCKET iSock, const char* pRequest, UBX_U1** ppData, UBX_I4* piSize);
#endif // Not required

static void lock(void);
static void unlock(void);

static MGA_API_RESULT handleAidAckMsg(int ackType);
static MGA_API_RESULT handleMgaAckMsg(const UBX_U1* pPayload);
static MGA_API_RESULT handleFlashAckMsg(const UBX_U1* pPayload);
static MGA_API_RESULT handleAidFlashAckMsg(void);
static MGA_API_RESULT handleAidFlashNakMsg(void);
static MGA_API_RESULT handleAidingResponseMsg(const UBX_U1* pMessageData);

static void legacyAidingCheckMessage(const UBX_U1* pData, UBX_U4 iSize);
static void legacyAidingRequestData(const LegacyAidingRequestHeader *pAidingRequestHeader);
static void legacyAidingUpdateData(const LegacyAidingUpdateDataHeader *pLegacyAidingUpdateHeader);

static void handleLegacyAidingTimeout(void);
static void sendMgaFlashBlock(bool next);
static void sendFlashMainSeqBlock(void);
static UBX_I4 sendNextMgaMessage(void);
static void sendAllMessages(void);
static void resendMessage(MgaMsgInfo* pResendMsg);
static void addMgaIniTime(const UBX_U1* pMgaData, UBX_I4* iSize, UBX_U1** pMgaDataOut, const MgaTimeAdjust* pTime);
static void addMgaIniPos(const UBX_U1* pMgaData, UBX_I4* iSize, UBX_U1** pMgaDataOut, const MgaPosAdjust* pPos);
static void sendCfgMgaAidAcks(bool enable, bool bV3);
static void sendInitialMsgBatch(void);
static void sendFlashStop(void);
static void sendAidingFlashStop(void);
static void sendEmptyFlashBlock(void);
static void initiateMessageTransfer(void);

static MGA_API_RESULT countMgaMsg(const UBX_U1* pMgaData, UBX_I4 iSize, UBX_U4* piCount);
static MgaMsgInfo* buildMsgList(const UBX_U1* pMgaData, unsigned int uNumEntries);
static void sessionStop(MGA_PROGRESS_EVENT_TYPE evtType, const void* pEventInfo, size_t evtInfoSize);
static MgaMsgInfo* findMsgBlock(UBX_U1 msgId, const UBX_U1* pMgaHeader);

static bool validChecksum(const UBX_U1* pPayload, size_t iSize);
static void addChecksum(UBX_U1* pPayload, size_t iSize);
static bool checkForIniMessage(const UBX_U1* pUbxMsg);
static void adjustMgaIniTime(MgaMsgInfo* pMsgInfo, const MgaTimeAdjust* pMgaTime);
static bool isAlmMatch(const UBX_U1* pMgaData);
static bool isAnoMatch(const UBX_U1* pMgaData, int cy, int cm, int cd);
static void adjustTimeToBestMatch(const UBX_U1* pMgaData, UBX_I4 pMgaDataSize, const struct tm* pTimeOriginal, struct tm* pTimeAdjusted);
// This function is MODIFIED from the libMga original to accept a NULL pText.
static void commaToPoint(char* pText);
// MODIFIED: this function removed, not required
#if 0
static int strcicmp(char const *a, char const *b);
#endif // Not required

static int checkValidAidDays(const int *array, size_t size, int value);
static int checkValidMgaDays(int value);
// MODIFIED: these functions removed, not required
#if 0
static void setDaysRequestParameter(UBX_CH* pBuffer, int nrOfDays);
static void reportError(MgaServiceErrors errorType, const char* errorMessage, UBX_U4 httpRc);
#endif // Not required

// MODIFIED: the static functions below are all added to the original libMga.

// This function is used by the MODIFIED mgaBuildOnlineRequestParams() and mgaBuildOfflineRequestParams().
// stringLen should be strlen(pString), pResult and pEncodedMessageLength cannot be NULL.
static void concatWithBuffer(UBX_CH* pBuffer, UBX_I4 iSize, const char* pString, int stringLen, MGA_API_RESULT* pResult, int* pEncodedMessageLength);

// This function is used by the MODIFIED mgaBuildOnlineRequestParams() and mgaBuildOfflineRequestParams().
// prefixLen should be strlen(pPrefix), pResult and pEncodedMessageLength cannot be NULL.
static void concatNumberWithBuffer(UBX_CH* pBuffer, UBX_I4 iSize, const char* pPrefix, int prefixLen, int number, int fractionalDigits, MGA_API_RESULT* pResult, int* pEncodedMessageLength);

// This function is used by concatNumberWithBuffer().
// It converts an integer that represents a number with up to 7 decimal places into a whole number
// and a fractional part.  The whole number is negative or positive to reflect the value of the
// input, the fractional part is always positive.  The result may be printed with snprintf() format
// specifiers %d.%07d, e.g. something like:
//
// char buffer[16];
// int whole;
// int fraction;
// numberToParts(latitudeX1e7, 7, &whole, &fraction)
// snprintf(buffer, sizeof(buffer), "%d.%07d", whole, fraction);
//
// Basically, fractionalDigits represents the position of the decimal point: 0 for
// an integer, 1000 for a time in milliseconds or a distance in millimetres, 10,000,000 for
// a lat/long, etc.
static void numberToParts(int number, int fractionalDigits, int* pWhole, int* pFraction);

///////////////////////////////////////////////////////////////////////////////
// libMga API implementation

MGA_API_RESULT mgaInit(void)
{
    MGA_API_RESULT result = MGA_API_OUT_OF_MEMORY;

    U_ASSERT(s_sessionState == MGA_IDLE);

    // MODIFIED: use ubxlib port API instead of WIN32 or pthread_mutex_t
    if ((s_mgaLock != NULL) || (uPortMutexCreate(&s_mgaLock) == 0)) {
        result = MGA_API_OK;
    }

    return result;
}

MGA_API_RESULT mgaDeinit(void)
{
    // MODIFIED: use ubxlib port API instead of WIN32 or pthread_mutex_t
    uPortMutexDelete(s_mgaLock);
    s_mgaLock = NULL;
    return MGA_API_OK;
}

const UBX_CH* mgaGetVersion(void)
{
    return LIBMGA_VERSION;
}

MGA_API_RESULT mgaConfigure(const MgaFlowConfiguration* pFlowConfig,
                            const MgaEventInterface* pEvtInterface,
                            const void* pCallbackContext)
{
    MGA_API_RESULT res = MGA_API_OK;

    lock();

    if (s_sessionState == MGA_IDLE)
    {
        s_pEvtInterface = pEvtInterface;
        s_pFlowConfig = pFlowConfig;
        s_pCallbackContext = pCallbackContext;
    }
    else
        res = MGA_API_ALREADY_RUNNING;

    unlock();

    return res;
}

MGA_API_RESULT mgaSessionStart(void)
{
    MGA_API_RESULT res = MGA_API_OK;

    lock();

    if (s_sessionState == MGA_IDLE)
    {
        U_ASSERT(s_pMgaMsgList == NULL);
        U_ASSERT(s_mgaBlockCount == 0);
        U_ASSERT(s_pLastMsgSent == NULL);
        U_ASSERT(s_messagesSent == 0);
        U_ASSERT(s_ackCount == 0);

        U_ASSERT(s_pMgaFlashBlockList == NULL);
        U_ASSERT(s_mgaFlashBlockCount == 0);
        U_ASSERT(s_pLastFlashBlockSent == NULL);
        U_ASSERT(s_flashMessagesSent == 0);
        U_ASSERT(s_flashSequence == 0);

        U_ASSERT(s_aidState == LEGACY_AIDING_IDLE);
        U_ASSERT(s_aidingTimeout == 0);

        s_sessionState = MGA_ACTIVE_PROCESSING_DATA;
    }
    else
        res = MGA_API_ALREADY_RUNNING;

    unlock();

    return res;
}

MGA_API_RESULT mgaSessionStop(void)
{
    MGA_API_RESULT res = MGA_API_OK;

    lock();

    if (s_sessionState != MGA_IDLE)
    {
        EVT_TERMINATION_REASON stopReason = TERMINATE_HOST_CANCEL;
        sessionStop(MGA_PROGRESS_EVT_TERMINATED, &stopReason, sizeof(stopReason));
    }
    else
        res = MGA_API_ALREADY_IDLE;

    unlock();

    return res;
}

MGA_API_RESULT mgaSessionSendOnlineData(const UBX_U1* pMgaData, UBX_I4 iSize, const MgaTimeAdjust* pMgaTimeAdjust)
{
    if (iSize <= 0)
    {
        return MGA_API_NO_DATA_TO_SEND;
    }

    MGA_API_RESULT res = MGA_API_OK;

    lock();

    if (s_sessionState != MGA_IDLE)
    {
        U_ASSERT(s_mgaBlockCount == 0);
        U_ASSERT(s_pMgaMsgList == NULL);

        res = countMgaMsg(pMgaData, iSize, &s_mgaBlockCount);
        if (res == MGA_API_OK)
        {
            if (s_mgaBlockCount > 0)
            {
                s_pMgaMsgList = buildMsgList(pMgaData, s_mgaBlockCount);

                if (s_pMgaMsgList != NULL)
                {
                    if (checkForIniMessage(s_pMgaMsgList[0].pMsg))
                    {
                        if (s_pEvtInterface->evtProgress)
                        {
                            s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_START, s_pCallbackContext, (const void *)&s_mgaBlockCount, (UBX_I4) sizeof(s_mgaBlockCount));
                        }

                        if (pMgaTimeAdjust != NULL)
                        {
                            adjustMgaIniTime(&s_pMgaMsgList[0], pMgaTimeAdjust);
                        }

                        // send initial set of messages to receiver
                        initiateMessageTransfer();

                        res = MGA_API_OK;
                    }
                    else
                    {
                        res = MGA_API_NO_MGA_INI_TIME;
                    }
                }
                else
                {
                    s_mgaBlockCount = 0;
                    res = MGA_API_OUT_OF_MEMORY;
                }
            }
            else
            {
                // nothing to send
                res = MGA_API_NO_DATA_TO_SEND;
            }
        }
    }
    else
    {
        res = MGA_API_ALREADY_IDLE;
    }

    unlock();

    return res;
}

MGA_API_RESULT mgaSessionSendOfflineData(const UBX_U1* pMgaData, UBX_I4 iSize, const MgaTimeAdjust* pTime, const MgaPosAdjust* pPos)
{
    MGA_API_RESULT res = MGA_API_OK;

    lock();

    if (s_sessionState != MGA_IDLE)
    {
        U_ASSERT(s_mgaBlockCount == 0);
        U_ASSERT(s_pMgaMsgList == NULL);

        UBX_U1* pMgaDataTemp = NULL;

        if (pPos != NULL)
        {
            addMgaIniPos(pMgaData, &iSize, &pMgaDataTemp, pPos);
            addMgaIniTime(pMgaDataTemp, &iSize, &s_pMgaDataSession, pTime);
            uPortFree(pMgaDataTemp);
        }
        else
            addMgaIniTime(pMgaData, &iSize, &s_pMgaDataSession, pTime);

        res = countMgaMsg(s_pMgaDataSession, iSize, &s_mgaBlockCount);

        if (s_mgaBlockCount > 0)
        {
            s_pMgaMsgList = buildMsgList(s_pMgaDataSession, s_mgaBlockCount);

            if (s_pMgaMsgList != NULL)
            {
                // send initial set of messages to receiver
                if (s_pEvtInterface->evtProgress)
                {
                    s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_START, s_pCallbackContext, (const void *)&s_mgaBlockCount, (UBX_I4) sizeof(s_mgaBlockCount));
                }

                // send initial set of messages to receiver
                initiateMessageTransfer();

                res = MGA_API_OK;
            }
            else
                res = MGA_API_OUT_OF_MEMORY;
        }
    }
    else
        res = MGA_API_ALREADY_IDLE;

    unlock();

    return res;
}

MGA_API_RESULT mgaProcessReceiverMessage(const UBX_U1* pMgaData, UBX_I4 iSize)
{
    MGA_API_RESULT res = MGA_API_IGNORED_MSG;

    lock();

    if (s_sessionState != MGA_IDLE)
    {
        // Look for ACK & NAK
        if ((pMgaData[0] == UBX_SIG_PREFIX_1) &&
            (pMgaData[1] == UBX_SIG_PREFIX_2) &&
            (iSize >= UBX_MSG_FRAME_SIZE))
        {
            // UBX message
            if (s_aidingSrvActive)
            {
                // Legacy aiding server is active
                legacyAidingCheckMessage(pMgaData, iSize);
            }
            else if (iSize == UBX_AID_ALP_ACK_SIZE)
            {
                if (s_bLegacyAiding)
                {
                    res = handleAidingResponseMsg(pMgaData);
                }
            }
            else
            {
                // Look for MGA ack/nak
                switch (pMgaData[2])
                {
                case UBX_CLASS_MGA:  // MGA
                    // MGA message
                    if ((pMgaData[3] == UBX_MGA_ACK) && (iSize == (UBX_MSG_FRAME_SIZE + 8)))
                    {
                        // MGA-ACK
                        if (s_pLastMsgSent != NULL)
                        {
                            res = handleMgaAckMsg(&pMgaData[6]);
                        }

#ifdef UBXONLY
                        if (s_DBDCheckActive)
                        {
                            if (pMgaData[6] == MGA_ACK_MSG_NAK)
                            {
                                EVT_TERMINATION_REASON stopReason = TERMINATE_RECEIVER_NAK;
                                sessionStop(MGA_PROGRESS_EVT_TERMINATED, &stopReason, sizeof(stopReason));
                            }
                            else
                                sessionStop(MGA_PROGRESS_EVT_FINISH, NULL, 0);
                        }
#endif
                    }
                    else if ((pMgaData[3] == UBX_MGA_FLASH) && (iSize == (UBX_MSG_FRAME_SIZE + 6)))
                    {
                        // MGA-FLASH-ACK
                        res = handleFlashAckMsg(&pMgaData[6]);
                    }
                    else
                    {
                        // other MGA message - Ignore
                    }
                    break;

                case UBX_CLASS_ACK:  // generic ACK/NAK message
                    if (iSize == (UBX_MSG_FRAME_SIZE + 2))
                    {
                        if ((s_pLastMsgSent != NULL) &&
                            (pMgaData[6] == UBX_CLASS_AID) &&
                            (pMgaData[7] == s_pLastMsgSent->mgaMsg.msgId))
                        {
                            res = handleAidAckMsg(pMgaData[3]);
                        }
                        else if ((s_pLastMsgSent != NULL) &&
                                 (pMgaData[6] == UBX_CLASS_CFG) &&
                                 (pMgaData[7] == UBX_CFG_NAVX5) &&
                                 (pMgaData[3] == 0))
                        {
                            sendCfgMgaAidAcks(true, true);
                            res = MGA_API_OK;
                        }

                    }
                    break;

                default:
                    break;
                }

            }
        }
    }

    unlock();

    return res;
}

// MODIFIED: mgaGetOnlineData() removed, not required
#if 0
MGA_API_RESULT mgaGetOnlineData(MgaOnlineServerConfig* pServerConfig, UBX_U1** ppData, UBX_I4* piSize)
{
    MGA_API_RESULT res = MGA_API_OK;

#if defined USE_SSL
    //check for HTTPS in primary server
    if (checkForHTTPS(pServerConfig->strPrimaryServer))
    {
        res = getOnlineDataFromServerSSL(pServerConfig->strPrimaryServer, pServerConfig, ppData, piSize);
        if (res != MGA_API_OK)
        {
            if (checkForHTTPS(pServerConfig->strSecondaryServer))
            {
                res = getOnlineDataFromServerSSL(pServerConfig->strSecondaryServer, pServerConfig, ppData, piSize);
            }
        }

        return res;
    }
    else if ((strcmp(pServerConfig->strPrimaryServer, "") == 0) && (checkForHTTPS(pServerConfig->strSecondaryServer))) //check for HTTPS in secondary server if primary is empty
    {
        res = getOnlineDataFromServerSSL(pServerConfig->strSecondaryServer, pServerConfig, ppData, piSize);

        return res;
    }
#endif  //USE_SSL

    s_serverResponseTimeout = PRIMARY_SERVER_RESPONSE_TIMEOUT;

    res = getOnlineDataFromServer(pServerConfig->strPrimaryServer, pServerConfig, ppData, piSize);

    if (res != MGA_API_OK)
    {
        s_serverResponseTimeout = SECONDARY_SERVER_RESPONSE_TIMEOUT;
        res = getOnlineDataFromServer(pServerConfig->strSecondaryServer, pServerConfig, ppData, piSize);
    }

    return res;
}
#endif // Not required

// This function is MODIFIED from the libMga original in order to support
// use of a NULL pBuffer and perform length checking.
MGA_API_RESULT mgaBuildOnlineRequestParams(MgaOnlineServerConfig* pServerConfig,
                                           UBX_CH* pBuffer,
                                           UBX_I4 iSize)
{
    MGA_API_RESULT result = MGA_API_OK;
    int encodedMessageLength = 0;

    concatWithBuffer(pBuffer, iSize, "token=", 6, &result, &encodedMessageLength);
    concatWithBuffer(pBuffer, iSize, pServerConfig->strServerToken, strlen(pServerConfig->strServerToken), &result, &encodedMessageLength);

    // check which GNSS requested
    if (pServerConfig->gnssTypeFlags)
    {
        concatWithBuffer(pBuffer, iSize, ";gnss=", 6, &result, &encodedMessageLength);
        if (pServerConfig->gnssTypeFlags & MGA_GNSS_GPS)
            concatWithBuffer(pBuffer, iSize, "gps,", 4, &result, &encodedMessageLength);
        if (pServerConfig->gnssTypeFlags & MGA_GNSS_GLO)
            concatWithBuffer(pBuffer, iSize, "glo,", 4, &result, &encodedMessageLength);
        if (pServerConfig->gnssTypeFlags & MGA_GNSS_QZSS)
            concatWithBuffer(pBuffer, iSize, "qzss,", 5, &result, &encodedMessageLength);
        if (pServerConfig->gnssTypeFlags & MGA_GNSS_BEIDOU)
            concatWithBuffer(pBuffer, iSize, "bds,", 4, &result, &encodedMessageLength);
        if (pServerConfig->gnssTypeFlags & MGA_GNSS_GALILEO)
            concatWithBuffer(pBuffer, iSize, "gal,", 4, &result, &encodedMessageLength);

        // remove last comma
        if (encodedMessageLength > 0)
            encodedMessageLength--;
        if ((pBuffer != NULL) && (iSize > encodedMessageLength))
            pBuffer[encodedMessageLength] = '\0';
    }

    // check which data type requested
    if (pServerConfig->dataTypeFlags)
    {
        concatWithBuffer(pBuffer, iSize, ";datatype=", 10, &result, &encodedMessageLength);
        if (pServerConfig->dataTypeFlags & MGA_DATA_EPH)
            concatWithBuffer(pBuffer, iSize, "eph,", 4, &result, &encodedMessageLength);
        if (pServerConfig->dataTypeFlags & MGA_DATA_ALM)
            concatWithBuffer(pBuffer, iSize, "alm,", 4, &result, &encodedMessageLength);
        if (pServerConfig->dataTypeFlags & MGA_DATA_AUX)
            concatWithBuffer(pBuffer, iSize, "aux,", 4, &result, &encodedMessageLength);
        if (pServerConfig->dataTypeFlags & MGA_DATA_POS)
            concatWithBuffer(pBuffer, iSize, "pos,", 4, &result, &encodedMessageLength);

        // remove last comma
        if (encodedMessageLength > 0)
            encodedMessageLength--;
        if ((pBuffer != NULL) && (iSize > encodedMessageLength))
            pBuffer[encodedMessageLength] = '\0';
    }

    // check if position should be used
    if (pServerConfig->useFlags & MGA_FLAGS_USE_POSITION)
    {
        char* pStart = NULL;
        if (pBuffer != NULL)
            pStart = pBuffer + encodedMessageLength;
        concatNumberWithBuffer(pBuffer, iSize, ";lat=", 5, pServerConfig->intX1e7Latitude, 7, &result, &encodedMessageLength);
        concatNumberWithBuffer(pBuffer, iSize, ";lon=", 5, pServerConfig->intX1e7Longitude, 7, &result, &encodedMessageLength);
        concatNumberWithBuffer(pBuffer, iSize, ";alt=", 5, pServerConfig->intX1e3Altitude, 3, &result, &encodedMessageLength);
        concatNumberWithBuffer(pBuffer, iSize, ";pacc=", 6, pServerConfig->intX1e3Accuracy, 3, &result, &encodedMessageLength);

        // make sure if commas used, then convert to decimal place
        if (result == MGA_API_OK)
            commaToPoint(pStart);
    }

    // check if ephemeris should be filtered on position
    if (pServerConfig->bFilterOnPos)
    {
        concatWithBuffer(pBuffer, iSize, ";filteronpos", 12, &result, &encodedMessageLength);
    }

    // check if latency should be used (for time aiding)
    if (pServerConfig->useFlags & MGA_FLAGS_USE_LATENCY)
    {
        char* pStart = NULL;
        if (pBuffer != NULL)
            pStart = pBuffer + encodedMessageLength;
        concatNumberWithBuffer(pBuffer, iSize, ";latency=", 9, pServerConfig->intX1e3Latency, 3, &result, &encodedMessageLength);
        if (result == MGA_API_OK)
            commaToPoint(pStart);
    }

    // check if time accuracy should be used (for time aiding)
    if (pServerConfig->useFlags & MGA_FLAGS_USE_TIMEACC)
    {
        char* pStart = NULL;
        if (pBuffer != NULL)
            pStart = pBuffer + encodedMessageLength;
        concatNumberWithBuffer(pBuffer, iSize, ";tacc=", 6, pServerConfig->intX1e3TimeAccuracy, 3, &result, &encodedMessageLength);
        if (result == MGA_API_OK)
            commaToPoint(pStart);
    }

    if (pServerConfig->useFlags & MGA_FLAGS_USE_LEGACY_AIDING)
    {
        concatWithBuffer(pBuffer, iSize, ";format=aid", 11, &result, &encodedMessageLength);
    }

#ifdef UBXONLY
    if (pServerConfig->pInternal)
    {
        concatWithBuffer(pBuffer, iSize, (const char*)pServerConfig->pInternal, strlen(pServerConfig->pInternal), &result, &encodedMessageLength);
    }
#endif // UBXONLY

    pServerConfig->encodedMessageLength = encodedMessageLength;
    return result;
}

// MODIFIED: mgaGetOfflineData() removed, not required
#if 0
MGA_API_RESULT mgaGetOfflineData(MgaOfflineServerConfig* pServerConfig, UBX_U1** ppData, UBX_I4* piSize)
{
    MGA_API_RESULT res;

#if defined USE_SSL
    //check for HTTPS in primary server
    if (checkForHTTPS(pServerConfig->strPrimaryServer))
    {
        res = getOfflineDataFromServerSSL(pServerConfig->strPrimaryServer, pServerConfig, ppData, piSize);
        if (res != MGA_API_OK)
        {
            if (checkForHTTPS(pServerConfig->strSecondaryServer))
            {
                res = getOfflineDataFromServerSSL(pServerConfig->strSecondaryServer, pServerConfig, ppData, piSize);
            }
        }

        return res;
    }
    else if ((strcmp(pServerConfig->strPrimaryServer, "") == 0) && (checkForHTTPS(pServerConfig->strSecondaryServer))) //check for HTTPS in secondary server if primary is empty
    {
        res = getOfflineDataFromServerSSL(pServerConfig->strSecondaryServer, pServerConfig, ppData, piSize);

        return res;
    }
#endif  //USE_SSL

    s_serverResponseTimeout = PRIMARY_SERVER_RESPONSE_TIMEOUT;

    res = getOfflineDataFromServer(pServerConfig->strPrimaryServer, pServerConfig, ppData, piSize);

    if (res != MGA_API_OK)
    {
        s_serverResponseTimeout = SECONDARY_SERVER_RESPONSE_TIMEOUT;
        res = getOfflineDataFromServer(pServerConfig->strSecondaryServer, pServerConfig, ppData, piSize);
    }

    return res;
}
#endif // Not required

// This function is MODIFIED from the libMga original in order to support
// use of a NULL pBuffer and perform length checking.
MGA_API_RESULT mgaBuildOfflineRequestParams(MgaOfflineServerConfig* pServerConfig,
                                            UBX_CH* pBuffer,
                                            UBX_I4 iSize)
{
    MGA_API_RESULT result = MGA_API_OK;
    int encodedMessageLength = 0;

    concatWithBuffer(pBuffer, iSize, "token=", 6, &result, &encodedMessageLength);
    concatWithBuffer(pBuffer, iSize, pServerConfig->strServerToken, strlen(pServerConfig->strServerToken), &result, &encodedMessageLength);

    // check which GNSS requested
    if (pServerConfig->gnssTypeFlags)
    {
        concatWithBuffer(pBuffer, iSize, ";gnss=", 6, &result, &encodedMessageLength);
        if (pServerConfig->gnssTypeFlags & MGA_GNSS_GPS)
            concatWithBuffer(pBuffer, iSize, "gps,", 4, &result, &encodedMessageLength);
        if (pServerConfig->gnssTypeFlags & MGA_GNSS_GLO)
            concatWithBuffer(pBuffer, iSize, "glo,", 4, &result, &encodedMessageLength);
        if (pServerConfig->gnssTypeFlags & MGA_GNSS_QZSS)
            concatWithBuffer(pBuffer, iSize, "qzss,", 5, &result, &encodedMessageLength);
        if (pServerConfig->gnssTypeFlags & MGA_GNSS_BEIDOU)
            concatWithBuffer(pBuffer, iSize, "bds,", 4, &result, &encodedMessageLength);
        if (pServerConfig->gnssTypeFlags & MGA_GNSS_GALILEO)
            concatWithBuffer(pBuffer, iSize, "gal,", 4, &result, &encodedMessageLength);

        // remove last comma
        if (encodedMessageLength > 0)
            encodedMessageLength--;
        if ((pBuffer != NULL) && (iSize > encodedMessageLength))
            pBuffer[encodedMessageLength] = '\0';
    }

    // check which data type requested
    if (pServerConfig->almFlags)
    {
        concatWithBuffer(pBuffer, iSize, ";alm=", 5, &result, &encodedMessageLength);
        if (pServerConfig->almFlags & MGA_GNSS_GPS)
            concatWithBuffer(pBuffer, iSize, "gps,", 4, &result, &encodedMessageLength);
        if (pServerConfig->almFlags & MGA_GNSS_GLO)
            concatWithBuffer(pBuffer, iSize, "glo,", 4, &result, &encodedMessageLength);
        if (pServerConfig->almFlags & MGA_GNSS_QZSS)
            concatWithBuffer(pBuffer, iSize, "qzss,", 5, &result, &encodedMessageLength);
        if (pServerConfig->almFlags & MGA_GNSS_BEIDOU)
            concatWithBuffer(pBuffer, iSize, "bds,", 4, &result, &encodedMessageLength);
        if (pServerConfig->almFlags & MGA_GNSS_GALILEO)
            concatWithBuffer(pBuffer, iSize, "gal,", 4, &result, &encodedMessageLength);

        // remove last comma
        if (encodedMessageLength > 0)
            encodedMessageLength--;
        if ((pBuffer != NULL) && (iSize > encodedMessageLength))
            pBuffer[encodedMessageLength] = '\0';
    }

    if (pServerConfig->useFlags & MGA_FLAGS_USE_LEGACY_AIDING)
    {
        concatWithBuffer(pBuffer, iSize, ";format=aid", 11, &result, &encodedMessageLength);
        // check if number of days should be used
        if (pServerConfig->numofdays > 0)
        {
            concatNumberWithBuffer(pBuffer, iSize, ";days=", 6, checkValidAidDays(AID_DAYS, sizeof(AID_DAYS) / sizeof(AID_DAYS[0]), pServerConfig->numofdays), 0, &result, &encodedMessageLength);
        }
    }
    else
    {
        if (pServerConfig->numofdays > 0)
        {
            concatNumberWithBuffer(pBuffer, iSize, ";days=", 6, checkValidMgaDays(pServerConfig->numofdays), 0, &result, &encodedMessageLength);
        }
    }

    // check if period (in weeks) should be used
    if (pServerConfig->period > 0)
    {
        concatNumberWithBuffer(pBuffer, iSize, ";period=", 8, pServerConfig->period, 0, &result, &encodedMessageLength);
    }

    // check if resolution (in days) should be used
    if (pServerConfig->resolution > 0)
    {
        concatNumberWithBuffer(pBuffer, iSize, ";resolution=", 12, pServerConfig->resolution, 0, &result, &encodedMessageLength);
    }

#ifdef UBXONLY
    if (pServerConfig->pInternal)
    {
        concatWithBuffer(pBuffer, iSize, (const char*)pServerConfig->pInternal, strlen(pServerConfig->pInternal), &result, &encodedMessageLength);
    }
#endif // UBXONLY

    pServerConfig->encodedMessageLength = encodedMessageLength;
    return result;
}

#ifdef UBXONLY
// don't document yet - TODO
//MGA_API_RESULT mgaRetrieveAndTransferData(const MgaServerConfig* pServerConfig)
//{
//  pServerConfig;
//  return MGA_API_OK;
//}
#endif // UBXONLY

MGA_API_RESULT mgaSessionSendOfflineToFlash(const UBX_U1* pMgaData, UBX_I4 iSize)
{
    MGA_API_RESULT res = MGA_API_OK;

    lock();

    if (s_sessionState != MGA_IDLE)
    {
        //filter out potential Almanac data
        bool bAnoData = false;
        while (!bAnoData || iSize <= 0)
        {
            UBX_U4 payloadSize = pMgaData[4] + (pMgaData[5] << 8);
            UBX_U4 msgSize = payloadSize + UBX_MSG_FRAME_SIZE;

            if (isAlmMatch(pMgaData))
            {
                pMgaData += msgSize;
                iSize -= msgSize;
            }
            else
                bAnoData = true;
        }

        s_mgaFlashBlockCount = (UBX_U4)iSize / FLASH_DATA_MSG_PAYLOAD;
        UBX_U2 lastBlockSize = (UBX_U2)iSize % FLASH_DATA_MSG_PAYLOAD;

        if (lastBlockSize > 0)
            s_mgaFlashBlockCount++;

        if (s_mgaFlashBlockCount > 0)
        {
            s_pMgaFlashBlockList = (MgaMsgInfo*)pUPortMalloc(s_mgaFlashBlockCount * sizeof(MgaMsgInfo));

            if (s_pMgaFlashBlockList != NULL)
            {
                for (UBX_U4 i = 0; i < s_mgaFlashBlockCount; i++)
                {
                    s_pMgaFlashBlockList[i].pMsg = pMgaData;
                    s_pMgaFlashBlockList[i].state = MGA_MSG_WAITING_TO_SEND;
                    s_pMgaFlashBlockList[i].sequenceNumber = (UBX_U2)i;
                    s_pMgaFlashBlockList[i].retryCount = 0;
                    s_pMgaFlashBlockList[i].timeOut = 0;
                    s_pMgaFlashBlockList[i].mgaFailedReason = MGA_FAILED_REASON_CODE_NOT_SET;

                    if ((i == (s_mgaFlashBlockCount - 1)) && (lastBlockSize > 0))
                    {
                        // last block
                        s_pMgaFlashBlockList[i].msgSize = lastBlockSize;
                    }
                    else
                    {
                        s_pMgaFlashBlockList[i].msgSize = FLASH_DATA_MSG_PAYLOAD;
                    }

                    pMgaData += s_pMgaFlashBlockList[i].msgSize;
                }

                if (s_pEvtInterface->evtProgress)
                {
                    s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_START, s_pCallbackContext, (const void *)&s_mgaFlashBlockCount, sizeof(s_mgaFlashBlockCount));
                }

                // send initial set of messages to receiver
                sendMgaFlashBlock(true);
                res = MGA_API_OK;
            }
            else
            {
                s_mgaFlashBlockCount = 0;
                res = MGA_API_OUT_OF_MEMORY;
            }
        }
        else
        {
            // nothing to send
            res = MGA_API_NO_DATA_TO_SEND;
        }
    }
    else
    {
        res = MGA_API_ALREADY_IDLE;
    }

    unlock();

    return res;
}

MGA_API_RESULT mgaSessionSendLegacyOfflineToFlash(const UBX_U1* pAidingData, UBX_U4 iSize)
{
    MGA_API_RESULT res = MGA_API_OK;

    lock();

    if (s_sessionState != MGA_IDLE)
    {
        s_mgaFlashBlockCount = (UBX_U4)iSize / FLASH_DATA_MSG_PAYLOAD;
        UBX_U2 lastBlockSize = (UBX_U2)iSize % FLASH_DATA_MSG_PAYLOAD;

        if (lastBlockSize > 0)
            s_mgaFlashBlockCount++;

        if (s_mgaFlashBlockCount > 0)
        {
            s_pMgaFlashBlockList = (MgaMsgInfo*)pUPortMalloc(s_mgaFlashBlockCount * sizeof(MgaMsgInfo));

            if (s_pMgaFlashBlockList != NULL)
            {
                for (UBX_U4 i = 0; i < s_mgaFlashBlockCount; i++)
                {
                    s_pMgaFlashBlockList[i].pMsg = pAidingData;
                    s_pMgaFlashBlockList[i].state = MGA_MSG_WAITING_TO_SEND;
                    s_pMgaFlashBlockList[i].sequenceNumber = (UBX_U2)i;
                    s_pMgaFlashBlockList[i].retryCount = 0;
                    s_pMgaFlashBlockList[i].timeOut = 0;
                    s_pMgaFlashBlockList[i].mgaFailedReason = MGA_FAILED_REASON_CODE_NOT_SET;

                    if ((i == (s_mgaFlashBlockCount - 1)) && (lastBlockSize > 0))
                    {
                        // last block
                        s_pMgaFlashBlockList[i].msgSize = lastBlockSize;
                    }
                    else
                    {
                        s_pMgaFlashBlockList[i].msgSize = FLASH_DATA_MSG_PAYLOAD;
                    }

                    pAidingData += s_pMgaFlashBlockList[i].msgSize;
                }
                if (s_pEvtInterface->evtProgress)
                {
                    s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_LEGACY_AIDING_STARTUP, s_pCallbackContext, (const void *)&s_mgaFlashBlockCount, sizeof(s_mgaFlashBlockCount));
                }

                // send initial set of messages to receiver
                s_bLegacyAiding = true;
                s_aidState = LEGACY_AIDING_STARTING;
                sendAidingFlashStop();                  // Quirky 'starting' process for aiding is to send a 'stop'
                res = MGA_API_OK;
            }
            else
            {
                s_mgaFlashBlockCount = 0;
                res = MGA_API_OUT_OF_MEMORY;
            }
        }
        else
        {
            // nothing to send
            res = MGA_API_NO_DATA_TO_SEND;
        }
    }
    else
    {
        res = MGA_API_ALREADY_IDLE;
    }

    unlock();

    return res;
}


MGA_API_RESULT mgaCheckForTimeOuts(void)
{
    lock();

    if ((s_pMgaMsgList == NULL) && (s_pMgaFlashBlockList == NULL))
    {
        // no work to do
        unlock();

        return MGA_API_OK;
    }

    if (s_bLegacyAiding == true)
    {
        handleLegacyAidingTimeout();
    }
    else if (s_pMgaMsgList != NULL)
    {
        U_ASSERT(s_mgaBlockCount > 0);

        MgaMsgInfo* pMsgInfo = s_pMgaMsgList;

        UBX_U4 i;
        size_t rob = 0;
        for (i = 0; i < s_mgaBlockCount; i++)
        {
            if (pMsgInfo->state == MGA_MSG_WAITING_FOR_ACK)
            {
                rob++;
                int32_t now = uPortGetTickTimeMs();

                if (now > pMsgInfo->timeOut)
                {
                    if (pMsgInfo->retryCount < s_pFlowConfig->msgRetryCount)
                    {
                        pMsgInfo->state = MGA_MSG_WAITING_FOR_RESEND;
                        pMsgInfo->retryCount++;
                        resendMessage(pMsgInfo);
                    }
                    else
                    {
                        // too many retries - so message transfer has failed
                        pMsgInfo->state = MGA_MSG_FAILED;
                        pMsgInfo->mgaFailedReason = MGA_FAILED_REASON_TOO_MANY_RETRIES;
                        U_ASSERT(s_pEvtInterface);
                        U_ASSERT(s_pEvtInterface->evtWriteDevice);

                        if (s_pEvtInterface->evtProgress)
                        {
                            s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_MSG_TRANSFER_FAILED, s_pCallbackContext, pMsgInfo, sizeof(MgaMsgInfo));
                        }

                        sendNextMgaMessage();

                        // check for last expected message
                        if (s_messagesSent == s_mgaBlockCount)
                        {
                            // stop the session
                            sessionStop(MGA_PROGRESS_EVT_FINISH, NULL, 0);
                        }
                    }
                }
            }
            pMsgInfo++;
        }

    }
    else
    {
        // doing a flash transfer
        U_ASSERT(s_mgaFlashBlockCount > 0);
        U_ASSERT(s_pLastFlashBlockSent != NULL);

        if ((s_pLastFlashBlockSent->state == MGA_MSG_WAITING_FOR_ACK) ||
            (s_pLastFlashBlockSent->state == MGA_MSG_WAITING_FOR_ACK_SECOND_CHANCE))
        {
            int32_t now = uPortGetTickTimeMs();
            if (now > s_pLastFlashBlockSent->timeOut)
            {
                // Timed out
                if (s_pLastFlashBlockSent->state == MGA_MSG_WAITING_FOR_ACK_SECOND_CHANCE)
                {
                    // resend last block
                    sendMgaFlashBlock(false);
                }
                else
                {
                    // Send nudge byte to receiver
                    s_pLastFlashBlockSent->state = MGA_MSG_WAITING_FOR_ACK_SECOND_CHANCE;
                    UBX_U1 flashDataMsg = 0;
                    s_pEvtInterface->evtWriteDevice(s_pCallbackContext, &flashDataMsg, sizeof(flashDataMsg));
                }
            }
        }
    }

    unlock();

    return MGA_API_OK;
}

MGA_API_RESULT mgaEraseOfflineFlash(void)
{
    MGA_API_RESULT res = MGA_API_OK;

    lock();

    if (s_sessionState != MGA_IDLE)
    {
        sendEmptyFlashBlock();
        sendFlashStop();
    }
    else
        res = MGA_API_ALREADY_IDLE;

    unlock();

    return res;
}

MGA_API_RESULT mgaGetAlmOfflineData(UBX_U1* pOfflineData, UBX_I4 offlineDataSize, UBX_U1** ppAlmData, UBX_I4* pAlmDataSize)
{
    U_ASSERT(ppAlmData);
    U_ASSERT(pAlmDataSize);
    U_ASSERT(pOfflineData);
    U_ASSERT(offlineDataSize);

    *ppAlmData = NULL;
    *pAlmDataSize = 0;

    UBX_U4 todaysSize = 0;
    UBX_U4 totalSize = 0;
    UBX_U1* pMgaData = pOfflineData;

    while (totalSize < (UBX_U4)offlineDataSize)
    {
        if ((pMgaData[0] == UBX_SIG_PREFIX_1) && (pMgaData[1] == UBX_SIG_PREFIX_2))
        {
            // UBX message
            UBX_U4 payloadSize = pMgaData[4] + (pMgaData[5] << 8);
            UBX_U4 msgSize = payloadSize + UBX_MSG_FRAME_SIZE;

            if (isAlmMatch(pMgaData))
            {
                todaysSize += msgSize;
            }
            pMgaData += msgSize;
            totalSize += msgSize;
        }
        else
        {
            U_ASSERT(0);
            //lint -save -e527
            break;
            //lint -restore
        }
    }

    if (todaysSize == 0)
    {
        return MGA_API_NO_DATA_TO_SEND;
    }

    UBX_U1* pTodaysData = (UBX_U1*)pUPortMalloc(todaysSize);
    if (pTodaysData)
    {
        *ppAlmData = pTodaysData;
        *pAlmDataSize = (UBX_I4)todaysSize;

        totalSize = 0;
        pMgaData = pOfflineData;

        while (totalSize < (UBX_U4)offlineDataSize)
        {
            if ((pMgaData[0] == UBX_SIG_PREFIX_1) && (pMgaData[1] == UBX_SIG_PREFIX_2))
            {
                // UBX message
                UBX_U4 payloadSize = pMgaData[4] + (pMgaData[5] << 8);
                UBX_U4 msgSize = payloadSize + UBX_MSG_FRAME_SIZE;

                if (isAlmMatch(pMgaData))
                {
                    memcpy(pTodaysData, pMgaData, msgSize);
                    pTodaysData += msgSize;
                }

                pMgaData += msgSize;
                totalSize += msgSize;
            }
            else
            {
                U_ASSERT(0);
                //lint -save -e527
                break;
                //lint -restore
            }
        }
    }
    return MGA_API_OK;
}

MGA_API_RESULT mgaGetTodaysOfflineData(const struct tm* pTime, UBX_U1* pOfflineData, UBX_I4 offlineDataSize, UBX_U1** ppTodaysData, UBX_I4* pTodaysDataSize)
{
    U_ASSERT(ppTodaysData);
    U_ASSERT(pTodaysDataSize);
    U_ASSERT(pOfflineData);
    U_ASSERT(offlineDataSize);

    struct tm pTimeAdjusted = {0};

    adjustTimeToBestMatch(pOfflineData, offlineDataSize, pTime, &pTimeAdjusted);

    int curYear = pTimeAdjusted.tm_year + 1900;
    int curMonth = pTimeAdjusted.tm_mon + 1;
    int curDay = pTimeAdjusted.tm_mday;

    *ppTodaysData = NULL;
    *pTodaysDataSize = 0;

    UBX_U4 todaysSize = 0;
    UBX_U4 totalSize = 0;
    UBX_U1* pMgaData = pOfflineData;

    while (totalSize < (UBX_U4)offlineDataSize)
    {
        if ((pMgaData[0] == UBX_SIG_PREFIX_1) && (pMgaData[1] == UBX_SIG_PREFIX_2))
        {
            // UBX message
            UBX_U4 payloadSize = pMgaData[4] + (pMgaData[5] << 8);
            UBX_U4 msgSize = payloadSize + UBX_MSG_FRAME_SIZE;

            if (isAnoMatch(pMgaData, curYear, curMonth, curDay) || isAlmMatch(pMgaData))
            {
                todaysSize += msgSize;
            }
            pMgaData += msgSize;
            totalSize += msgSize;
        }
        else
        {
            U_ASSERT(0);
            //lint -save -e527
            break;
            //lint -restore
        }
    }

    if (todaysSize == 0)
    {
        return MGA_API_NO_DATA_TO_SEND;
    }

    UBX_U1* pTodaysData = (UBX_U1*)pUPortMalloc(todaysSize);
    if (pTodaysData)
    {
        *ppTodaysData = pTodaysData;
        *pTodaysDataSize = (UBX_I4)todaysSize;

        totalSize = 0;
        pMgaData = pOfflineData;

        while (totalSize < (UBX_U4)offlineDataSize)
        {
            if ((pMgaData[0] == UBX_SIG_PREFIX_1) && (pMgaData[1] == UBX_SIG_PREFIX_2))
            {
                // UBX message
                UBX_U4 payloadSize = pMgaData[4] + (pMgaData[5] << 8);
                UBX_U4 msgSize = payloadSize + UBX_MSG_FRAME_SIZE;

                if (isAnoMatch(pMgaData, curYear, curMonth, curDay) || isAlmMatch(pMgaData))
                {
                    memcpy(pTodaysData, pMgaData, msgSize);
                    pTodaysData += msgSize;
                }

                pMgaData += msgSize;
                totalSize += msgSize;
            }
            else
            {
                U_ASSERT(0);
                //lint -save -e527
                break;
                //lint -restore
            }
        }
    }
    return MGA_API_OK;
}

// MODIFIED: mgaStartLegacyAiding() removed, not required, but mainly because it removes the need for srand()/rand()/RAND_MAX
#if 0
MGA_API_RESULT mgaStartLegacyAiding(UBX_U1* pAidingData, UBX_I4 iSize)
{
    U_ASSERT(pAidingData != NULL);
    U_ASSERT(iSize > 0);

    MGA_API_RESULT res = MGA_API_OK;

    lock();

    // Allocate a new aiding file Id
    time_t t = uPortGetTickTimeMs() / 1000;
    srand((int)t);
    s_alpfileId = (UBX_U2)((0xffff * rand() / RAND_MAX) + 1);

    U_ASSERT(s_pAidingData == NULL);
    s_pAidingData = pAidingData;
    s_aidingDataSize = iSize;

    // Activate the AID-ALP message
    UBX_U1 enableAidALP[] = { UBX_SIG_PREFIX_1, UBX_SIG_PREFIX_2,
        UBX_CLASS_CFG, UBX_CFG_MSG,
        0x08, 0x00,
        UBX_CLASS_AID, UBX_AID_ALPSRV,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x00,
        0x00, 0x00 };

    addChecksum(&enableAidALP[2], sizeof(enableAidALP) - 4);
    U_ASSERT(validChecksum(&enableAidALP[2], sizeof(enableAidALP) - 4));

    s_pEvtInterface->evtWriteDevice(s_pCallbackContext, enableAidALP, sizeof(enableAidALP));

    // Build the ALP header to inform the receiver there is new ALP data
    UBX_U1 aidingStartMsg[sizeof(UbxMsgHeader) + sizeof(LegacyAidingRequestHeader) + sizeof(LegacyAidingDataHeader) + 2] = { 0 };

    UbxMsgHeader *pMsgHeader = (UbxMsgHeader *)aidingStartMsg;
    pMsgHeader->header1 = UBX_SIG_PREFIX_1;
    pMsgHeader->header2 = UBX_SIG_PREFIX_2;
    pMsgHeader->msgClass = UBX_CLASS_AID;
    pMsgHeader->msgId = UBX_AID_ALPSRV;
    pMsgHeader->payloadLength = sizeof(aidingStartMsg) - UBX_MSG_FRAME_SIZE;

    LegacyAidingRequestHeader *pRqstId = (LegacyAidingRequestHeader*)(aidingStartMsg + sizeof(UbxMsgHeader));
    pRqstId->idSize = sizeof(LegacyAidingRequestHeader);
    pRqstId->type = 1;
    pRqstId->ofs = 0;
    pRqstId->size = sizeof(LegacyAidingDataHeader) / 2;       // Size in words
    pRqstId->fileId = s_alpfileId;
    pRqstId->dataSize = sizeof(LegacyAidingDataHeader);

    //lint -save -e419
    memcpy(&pRqstId[1], s_pAidingData, sizeof(LegacyAidingDataHeader));
    //lint -restore

    addChecksum(&aidingStartMsg[2], sizeof(aidingStartMsg) - 4);
    U_ASSERT(validChecksum(&aidingStartMsg[2], sizeof(aidingStartMsg) - 4));

    // Send new ALP data message
    s_pEvtInterface->evtWriteDevice(s_pCallbackContext, aidingStartMsg, sizeof(aidingStartMsg));

    s_aidingSrvActive = true;

    s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_LEGACY_AIDING_SERVER_STARTED,
                                 s_pCallbackContext,
                                 pRqstId,
                                 sizeof(LegacyAidingRequestHeader) + sizeof(LegacyAidingDataHeader));
    unlock();

    return res;
}
#endif // Not required

MGA_API_RESULT mgaStopLegacyAiding(void)
{
    MGA_API_RESULT res = MGA_API_OK;

    lock();

    s_aidingSrvActive = false;

    // Switch off the AID-ALP message
    UBX_U1 disableAidALP[] = { UBX_SIG_PREFIX_1, UBX_SIG_PREFIX_2,
        UBX_CLASS_CFG, UBX_CFG_MSG,
        0x03, 0x00,
        UBX_CLASS_AID, UBX_AID_ALPSRV,
        0x00,
        0x00, 0x00 };

    addChecksum(&disableAidALP[2], sizeof(disableAidALP) - 4);
    U_ASSERT(validChecksum(&disableAidALP[2], sizeof(disableAidALP) - 4));

    s_pEvtInterface->evtWriteDevice(s_pCallbackContext, disableAidALP, sizeof(disableAidALP));

    sessionStop(MGA_PROGRESS_EVT_LEGACY_AIDING_SERVER_STOPPED, NULL, 0);

    unlock();

    return res;
}

///////////////////////////////////////////////////////////////////////////////
// private functions

// MODIFIED: getDataFromService(), connectServer() and getHttpHeader() removed, not required
#if 0
static MGA_API_RESULT getDataFromService(SOCKET iSock, const char* pRequest, UBX_U1** ppData, UBX_I4* piSize)
{
    // send the HTTP get request
    U_ASSERT(s_pEvtInterface);

    if (s_pEvtInterface->evtProgress)
    {
        s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_REQUEST_HEADER, s_pCallbackContext, NULL, 0);
    }

    MGA_API_RESULT res = MGA_API_OK;
    size_t requestSize = strlen(pRequest);
    send(iSock, pRequest, requestSize, 0);

    // get reply
    char sData[0x2000];
    memset(sData, 0, sizeof(sData));
    getHttpHeader(iSock, sData, sizeof(sData));

    // search for HTTP header
    const char* pHttpTxt = "HTTP/";
    char* pHttpPos = strstr(sData, pHttpTxt);

    if (!pHttpPos)
    {
        // response header format error
        reportError(MGA_SERVICE_ERROR_NOT_HTTP_HEADER, "", 0);
        res = MGA_API_CANNOT_GET_DATA;
    }

    // search for HTTP response code
    const char* pResponseCode = NULL;

    if (res == MGA_API_OK)
    {
        pResponseCode = nextToken(pHttpPos);

        if (!pResponseCode)
        {
            // response header format error
            reportError(MGA_SERVICE_ERROR_NO_RESPONSE_CODE, "", 0);
            res = MGA_API_CANNOT_GET_DATA;
        }
    }

    if (res == MGA_API_OK)
    {
        //lint -save -e668
        int rc = atoi(pResponseCode);
        //lint -restore
        if (rc != 200)
        {
            // extract response status text
            const char* pResponseStatus = nextToken(pResponseCode);
            if(pResponseStatus == NULL) {
              pResponseStatus = "";
            }

            reportError(MGA_SERVICE_ERROR_BAD_STATUS, pResponseStatus, rc);
            res = MGA_API_CANNOT_GET_DATA;
        }
    }

    // search for HTTP content-length
    const char* pLength = NULL;
    const char* pContentLenTxt = "CONTENT-LENGTH: ";
    const size_t contentLenSize = strlen(pContentLenTxt);

    if (res == MGA_API_OK)
    {
        pLength = strstr(sData, pContentLenTxt);

        if (!pLength)
        {
            // no length
            reportError(MGA_SERVICE_ERROR_NO_LENGTH, "", 0);
            res = MGA_API_CANNOT_GET_DATA;
        }
    }

    size_t contentLength = 0;
    if (res == MGA_API_OK)
    {
        U_ASSERT(pLength);
        //lint -save -e613      pLength will not be NULL
        pLength += contentLenSize;
        //lint -restore

        //lint -save -e571      Cast is not suspicious
        contentLength = (size_t)atoi(pLength);
        //lint -restore

        if (!contentLength)
        {
            // content length is 0
            reportError(MGA_SERVICE_ERROR_ZERO_LENGTH, "Data length is 0", 0);
            res = MGA_API_CANNOT_GET_DATA;
        }

        if (contentLength > MGA_MAX_CONTENT_LEN)
        {
            //content length too big
            reportError(MGA_SERVICE_ERROR_LENGTH_TOO_BIG, "Data length is too big", 0);

            res = MGA_API_CANNOT_GET_DATA;
        }
    }

    const char* pContentTypeTxt = "CONTENT-TYPE: ";
    const size_t contentTypeSize = strlen(pContentTypeTxt);
    const char* pContentType = strstr(sData, pContentTypeTxt);

    if (!pContentType)
    {
        reportError(MGA_SERVICE_ERROR_NO_CONTENT_TYPE, "", 0);
        res = MGA_API_CANNOT_GET_DATA;
    }

    if (res == MGA_API_OK)
    {
        // check if its a UBX server
        //lint -save -e613      pContentType will not be NULL
        if (strncmp(pContentType + contentTypeSize, "APPLICATION/UBX", 15) != 0)
        {
            //lint -restore
            reportError(MGA_SERVICE_ERROR_NOT_UBX_CONTENT, "Content type not UBX", 0);
            res = MGA_API_CANNOT_GET_DATA;
        }
    }

    char* pBuffer = NULL;
    if (res == MGA_API_OK)
    {
        // allocate buffer to receiver data from service
        // this buffer will be passed to the client, who will ultimately free it
        pBuffer = (char*)pUPortMalloc(contentLength);
        if (pBuffer == NULL)
        {
            res = MGA_API_OUT_OF_MEMORY;
        }
    }

    if (res == MGA_API_OK)
    {
        if (s_pEvtInterface->evtProgress)
        {
            s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_RETRIEVE_DATA, s_pCallbackContext, NULL, 0);
        }
        //lint -save -e571     Cast is not suspicious
        size_t received = (size_t)getData(iSock, pBuffer, contentLength);
        //lint -restore
        if (received != contentLength)
        {
            // did not retrieved all data
            reportError(MGA_SERVICE_ERROR_PARTIAL_CONTENT, "", 0);

            // as there is an error, and all the data could not be retrieved, free the buffer and return an error code, so the client does not release the buffer again.
            uPortFree(pBuffer);
            res = MGA_API_CANNOT_GET_DATA;
        }
        else
        {
            *ppData = (UBX_U1*)pBuffer;
            *piSize = (UBX_I4)contentLength;
        }
    }
    //lint -save -e593
    return res;
    //lint -restore
}

static SOCKET connectServer(const char* strServer, UBX_U2 wPort)
{
    // compose server name with port
    size_t iSize = (strlen(strServer) + 6 + 1) * sizeof(char); // len of server string + ':' + largest port number (65535) + null
    char* serverString = (char*)pUPortMalloc(iSize);
    if (serverString == NULL)
    {
        return INVALID_SOCKET;
    }

    memset(serverString, 0, iSize);
    sprintf(serverString, "%s:%hu", strServer, wPort);

    if (s_pEvtInterface->evtProgress)
    {
        s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_SERVER_CONNECTING, s_pCallbackContext, (const void*)serverString, (UBX_I4)strlen(serverString) + 1);
    }

    if (strlen(strServer) == 0)
    {
        if (s_pEvtInterface->evtProgress)
        {
            s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_UNKNOWN_SERVER, s_pCallbackContext, (const void*)serverString, (UBX_I4)strlen(serverString) + 1);
        }
        uPortFree(serverString);
        return INVALID_SOCKET;
    }

    struct sockaddr_in server;
    // create the socket address of the server, it consists of type, IP address and port number
    memset(&server, 0, sizeof(server));
    unsigned long addr = inet_addr(strServer);

    if (addr != INADDR_NONE)  // numeric IP address
    {
        //lint -save -e419
        memcpy(&server.sin_addr, &addr, sizeof(addr));
        //lint -restore
    }
    else
    {
        struct hostent* host_info = gethostbyname(strServer);

        if (host_info != NULL)
        {
            //lint -save -e571
            memcpy(&server.sin_addr, host_info->h_addr, (size_t)host_info->h_length);
            //lint -restore
        }
        else
        {
            if (s_pEvtInterface->evtProgress)
            {
                s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_UNKNOWN_SERVER, s_pCallbackContext, (const void*)serverString, (UBX_I4)strlen(serverString) + 1);
            }
            uPortFree(serverString);
            return INVALID_SOCKET;
        }
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(wPort);

    // create the socket and connect
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock != INVALID_SOCKET)
    {
        bool connFailed = false;
#ifndef WIN32
        // Make the socket non-blocking for the connect call
        // to avoid connect hanging up that can be observed on some platforms
        connFailed = (fcntl(sock, F_SETFL, SOCK_NONBLOCK) != 0);
#endif // WIN32

        // set up the connection to the server
        if (!connFailed)
        {
            connFailed = (connect(sock, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR);
#ifndef WIN32
            if (connFailed && errno == EINPROGRESS)
            {
                // Wait for connection (for 10 seconds)
                fd_set fdset;
                FD_ZERO(&fdset);
                FD_SET(sock, &fdset);
                struct timeval selTimeout;
                selTimeout.tv_sec = 10;
                selTimeout.tv_usec = 0;
                int soerr;
                socklen_t solen = sizeof(soerr);
                // Is the socket writeable (ready) now?
                if (select(sock + 1, NULL, &fdset, NULL, &selTimeout) == 1
                    && !getsockopt(sock, SOL_SOCKET, SO_ERROR, &soerr, &solen)
                    && soerr == 0)
                {
                    // Go into non-blocking mode again
                    const int flags = fcntl(sock, F_GETFL, 0);
                    connFailed = (fcntl(sock, F_SETFL, flags ^ SOCK_NONBLOCK) != 0);
                }
                else
                {
                    // An error occurred. Report, clean up and exit
                    connFailed = true;
                }
            }
#endif // WIN32
        }

        if (connFailed)
        {
            if (s_pEvtInterface->evtProgress)
            {
                s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_SERVER_CANNOT_CONNECT, s_pCallbackContext, (const void*)serverString, (UBX_I4)strlen(serverString) + 1);
            }
        }
        else //success
        {
            if (s_pEvtInterface->evtProgress)
            {
                s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_SERVER_CONNECT, s_pCallbackContext, (const void*)serverString, (UBX_I4)strlen(serverString) + 1);
            }
            uPortFree(serverString);
            return sock;
        }

#ifdef WIN32
        closesocket(sock);
#else // WIN32
        close(sock);
#endif // WIN32
    }
    uPortFree(serverString);
    return INVALID_SOCKET;
}

static int getHttpHeader(SOCKET sock, char* buf, int cnt)
{
    int c = 0;
    char* p = buf;
    do
    {
        fd_set fdset;
        //lint -save -e866
        FD_ZERO(&fdset);
        //lint -restore
#ifdef WIN32
#  pragma warning(push)
#  pragma warning( disable : 4127 )
#endif // WIN32
        FD_SET(sock, &fdset);
#ifdef WIN32
#  pragma warning(pop)
#endif // WIN32

        struct timeval tv;
        tv.tv_sec = s_serverResponseTimeout;
        tv.tv_usec = 0;

        if (select(sock + 1, &fdset, NULL, NULL, &tv) > 0)
        {
            int b = recv(sock, p, 1, 0);
            if (b <= 0)
            {
                // failed or timeout
                break;
            }
            else if ((b > 0) && (*p != '\r'))
            {
                //get response as upper case
                *p = (char)toupper(*p);
                p++;
                c++;
            }
        }
        else
        {
            // no response
            break;
        }
    } while ((c < cnt) && ((c < 2) || (p[-2] != '\n') || (p[-1] != '\n')));
    *p = '\0';

    return c;
}
#endif // Not required

//lint -sem(lock,thread_lock)
static void lock(void)
{
    // MODIFIED: use ubxlib port API instead of WIN32 or pthread_mutex_t
    uPortMutexLock(s_mgaLock);
}

//lint -sem(unlock,thread_unlock)
static void unlock(void)
{
    // MODIFIED: use ubxlib port API instead of WIN32 or pthread_mutex_t
    uPortMutexUnlock(s_mgaLock);
}

static MGA_API_RESULT handleAidAckMsg(int ackType)
{
    // do not lock here - lock must already be in place
    U_ASSERT(s_pFlowConfig->mgaFlowControl != MGA_FLOW_SMART);
    U_ASSERT(s_pLastMsgSent != NULL);

    if (s_pFlowConfig->mgaFlowControl == MGA_FLOW_NONE)
    {
        // no flow control, so ignore ACK/NAKs
        return MGA_API_IGNORED_MSG;
    }

    MGA_API_RESULT res = MGA_API_IGNORED_MSG;
    bool continueAckProcessing = false;

    switch (ackType)
    {
    case UBX_ACK_NAK:  // NAK for last AID message
        s_ackCount++;
        s_pLastMsgSent->state = MGA_MSG_FAILED;
        s_pLastMsgSent->mgaFailedReason = MGA_FAILED_REASON_CODE_NOT_SET;

        if (s_pEvtInterface->evtProgress)
        {
            s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_MSG_TRANSFER_FAILED, s_pCallbackContext, s_pLastMsgSent, sizeof(MgaMsgInfo));
        }

        continueAckProcessing = true;
        break;

    case UBX_ACK_ACK:  // ACK for last AID message
        s_ackCount++;
        s_pLastMsgSent->state = MGA_MSG_RECEIVED;

        if (s_pEvtInterface->evtProgress)
        {
            s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_MSG_TRANSFER_COMPLETE, s_pCallbackContext, s_pLastMsgSent, sizeof(MgaMsgInfo));
        }

        continueAckProcessing = true;
        break;

    default:
        U_ASSERT(false);
        break;
    }

    if (continueAckProcessing)
    {
        if (s_ackCount == s_mgaBlockCount)
        {
            // last ACK received
            sessionStop(MGA_PROGRESS_EVT_FINISH, NULL, 0);
        }
        else
        {
            // not last ACK, can send another message if there is still some to send
            if (s_messagesSent < s_mgaBlockCount)
            {
                sendNextMgaMessage();
            }
        }
        res = MGA_API_OK;
    }

    return res;
}

static MGA_API_RESULT handleMgaAckMsg(const UBX_U1* pPayload)
{
    // do not lock here - lock must already be in place
    if (s_pFlowConfig->mgaFlowControl == MGA_FLOW_NONE)
    {
        // no flow control, so ignore ACK/NAKs
        return MGA_API_IGNORED_MSG;
    }

    if (s_pLastMsgSent == NULL)
    {
        // no message in flow
        return MGA_API_IGNORED_MSG;
    }

    MGA_API_RESULT res = MGA_API_IGNORED_MSG;
    MGA_ACK_TYPES type = (MGA_ACK_TYPES)pPayload[0];
    UBX_U1 msgId = pPayload[3];
    const UBX_U1* pMgaHeader = &pPayload[4];

    bool continueAckProcessing = false;

    switch (type)
    {
    case MGA_ACK_MSG_NAK:
    {
        // NAK - report NAK & carry on
        MgaMsgInfo* pAckMsg = findMsgBlock(msgId, pMgaHeader);

        if (pAckMsg)
        {
            s_ackCount++;
            pAckMsg->state = MGA_MSG_FAILED;
            pAckMsg->mgaFailedReason = (MGA_FAILED_REASON)pPayload[2];

            if (s_pEvtInterface->evtProgress)
            {
                s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_MSG_TRANSFER_FAILED, s_pCallbackContext, pAckMsg, sizeof(MgaMsgInfo));
            }

            continueAckProcessing = true;
        }
    }
    break;

    case MGA_ACK_MSG_ACK:
    {
        // ACK
        MgaMsgInfo* pAckMsg = findMsgBlock(msgId, pMgaHeader);

        if (pAckMsg)
        {
            // ACK is for an outstanding transmitted message
            s_ackCount++;
            pAckMsg->state = MGA_MSG_RECEIVED;

            if (s_pEvtInterface->evtProgress)
            {
                s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_MSG_TRANSFER_COMPLETE, s_pCallbackContext, pAckMsg, sizeof(MgaMsgInfo));
            }

            continueAckProcessing = true;
        }
    }
    break;

    default:
        // ignored
        break;
    }

    if (continueAckProcessing)
    {
        if (s_ackCount == s_mgaBlockCount)
        {
            // last ACK received
            sessionStop(MGA_PROGRESS_EVT_FINISH, NULL, 0);
        }
        else
        {
            // not last ACK, can send another message if there is still some to send
            if (s_messagesSent < s_mgaBlockCount)
            {
                sendNextMgaMessage();
            }
        }
        res = MGA_API_OK;
    }

    return res;
}

static MGA_API_RESULT handleFlashAckMsg(const UBX_U1* pPayload)
{
    // do not lock here - lock must already be in place
    UBX_U1 type = pPayload[0];
    UBX_U1 typeVersion = pPayload[1];
    UBX_U1 ackType = pPayload[2];
    UBX_U2 sequence = pPayload[4] + (pPayload[5] << 8);

    (void)typeVersion; // unreferenced

    if (type != 3)
    {
        // not a UBX-MGA-FLASH-ACK message
        return MGA_API_IGNORED_MSG;
    }

    // it is a UBX-MGA-FLASH-ACK message
    MGA_API_RESULT res = MGA_API_OK;

    U_ASSERT(typeVersion == 0);

    switch (ackType)
    {
    case 0: // ACK
        if (sequence == 0xFFFF)
        {
            // ACK for stop message
            sessionStop(MGA_PROGRESS_EVT_FINISH, NULL, 0);
        }
        else
        {
            if ((s_flashMessagesSent < s_mgaFlashBlockCount) &&
                (s_pLastFlashBlockSent->sequenceNumber == sequence))
            {
                sendMgaFlashBlock(true);
            }
            else
            {
                // MODIFIED: ignore repeated/out-of-date acks
            }
        }
        break;

    case 1:     // NAK - retry
        sendMgaFlashBlock(false);
        break;

    case 2:     // NAK - give up
    {
        // report giving up
        EVT_TERMINATION_REASON stopReason = TERMINATE_RECEIVER_NAK;
        sessionStop(MGA_PROGRESS_EVT_TERMINATED, &stopReason, sizeof(stopReason));
    }
    break;

    default:
        U_ASSERT(0);
        break;
    }

    return res;
}

static MGA_API_RESULT handleAidFlashAckMsg(void)
{
    // do not lock here - locks must already be in place
    MGA_API_RESULT res = MGA_API_OK;

    switch (s_aidState)
    {
    case LEGACY_AIDING_STARTING:
        s_aidState = LEGACY_AIDING_MAIN_SEQ;

        if (s_pEvtInterface->evtProgress)
        {
            s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_LEGACY_AIDING_STARTUP_COMPLETED,
                                         s_pCallbackContext,
                                         NULL,
                                         MGA_PROGRESS_EVT_LEGACY_AIDING_FINALIZE_START);
        }

        // Send next aiding flash block
        sendFlashMainSeqBlock();
        break;

    case LEGACY_AIDING_MAIN_SEQ:
        U_ASSERT(s_mgaFlashBlockCount > s_flashMessagesSent);
        // Send next aiding flash block
        sendFlashMainSeqBlock();
        break;

    case LEGACY_AIDING_STOPPING:
        s_aidState = LEGACY_AIDING_IDLE;
        if (s_pEvtInterface->evtProgress)
        {
            s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_LEGACY_AIDING_FINALIZE_COMPLETED,
                                         s_pCallbackContext,
                                         NULL,
                                         0);
        }
        sessionStop(MGA_PROGRESS_EVT_FINISH, NULL, 0);
        break;

    default:
        U_ASSERT(false);
        break;
    }

    return res;
}

static MGA_API_RESULT handleAidingResponseMsg(const UBX_U1* pMessageData)
{
    MGA_API_RESULT res = MGA_API_IGNORED_MSG;

    // Look for aiding ack/nak
    const UBX_U1 Ack[] = { UBX_SIG_PREFIX_1, UBX_SIG_PREFIX_2, 0x0B, 0x50, 0x01, 0x00, 0x01, 0x5D, 0x7B };
    const UBX_U1 Nak[] = { UBX_SIG_PREFIX_1, UBX_SIG_PREFIX_2, 0x0B, 0x50, 0x01, 0x00, 0x00, 0x5C, 0x7A };

    if (memcmp(pMessageData, Ack, UBX_AID_ALP_ACK_SIZE) == 0)
    {
        res = handleAidFlashAckMsg();
    }
    else if (memcmp(pMessageData, Nak, UBX_AID_ALP_ACK_SIZE) == 0)
    {
        if (s_aidState == LEGACY_AIDING_STARTING)
        {
            // The quirky nature of legacy aiding means that a NAK here needs to be treated as an ACK
            res = handleAidFlashAckMsg();
        }
        else
        {
            res = handleAidFlashNakMsg();
        }
    }

    return res;
}

static MGA_API_RESULT handleAidFlashNakMsg(void)
{
    // do not lock here - locks must already be in place
    EVT_TERMINATION_REASON stopReason = TERMINATE_RECEIVER_NAK;
    sessionStop(MGA_PROGRESS_EVT_TERMINATED, &stopReason, sizeof(stopReason));

    return MGA_API_OK;
}

static void handleLegacyAidingTimeout(void)
{
    // do not lock here - locks must already be in place

    U_ASSERT(s_bLegacyAiding);
    int32_t now = uPortGetTickTimeMs();

    switch (s_aidState)
    {
    case LEGACY_AIDING_STARTING:
        if (now > s_aidingTimeout)
        {
            U_ASSERT(s_pEvtInterface);
            if (s_pEvtInterface->evtProgress)
            {
                UBX_I4 reason = MGA_FAILED_REASON_LEGACY_NO_ACK;
                s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_LEGACY_AIDING_STARTUP_FAILED, s_pCallbackContext, &reason, sizeof(reason));
            }
            sendAidingFlashStop();
            sessionStop(MGA_PROGRESS_EVT_FINISH, NULL, 0);
        }
        break;

    case LEGACY_AIDING_MAIN_SEQ:
        U_ASSERT(s_mgaFlashBlockCount > 0);
        U_ASSERT(s_pLastFlashBlockSent != NULL);
        U_ASSERT((s_pLastFlashBlockSent->state == MGA_MSG_WAITING_FOR_ACK) ||
               (s_pLastFlashBlockSent->state == MGA_MSG_WAITING_FOR_ACK_SECOND_CHANCE));

        if (now > s_pLastFlashBlockSent->timeOut)
        {
            if (s_pLastFlashBlockSent->state == MGA_MSG_WAITING_FOR_ACK)
            {
                // Send nudge byte to receiver
                s_pLastFlashBlockSent->state = MGA_MSG_WAITING_FOR_ACK_SECOND_CHANCE;
                UBX_U1 flashDataMsg = 0;
                s_pEvtInterface->evtWriteDevice(s_pCallbackContext, &flashDataMsg, sizeof(flashDataMsg));
            }
            else
            {
                s_pLastFlashBlockSent->state = MGA_MSG_FAILED;
                s_pLastFlashBlockSent->mgaFailedReason = MGA_FAILED_REASON_LEGACY_NO_ACK;

                U_ASSERT(s_pEvtInterface);
                if (s_pEvtInterface->evtProgress)
                {
                    s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_LEGACY_AIDING_FLASH_BLOCK_FAILED, s_pCallbackContext, s_pLastFlashBlockSent, sizeof(MgaMsgInfo));
                }
                sendAidingFlashStop();
                sessionStop(MGA_PROGRESS_EVT_FINISH, NULL, 0);
            }
        }
        break;

    case LEGACY_AIDING_STOPPING:
        if (now > s_aidingTimeout)
        {
            // Give up waiting for 'stop' ack
            if (s_pEvtInterface->evtProgress)
            {
                s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_LEGACY_AIDING_FINALIZE_FAILED, s_pCallbackContext, NULL, 0);
            }
            sessionStop(MGA_PROGRESS_EVT_FINISH, NULL, 0);
        }
        break;

    case LEGACY_AIDING_IDLE:
        // Nothing to check
        break;

    default:
        U_ASSERT(false);
        break;
    }
}

static void sendMgaFlashBlock(bool next)
{
    // do not lock here - locks must already be in place
    bool terminated = false;

    if (s_pLastFlashBlockSent == NULL)
    {
        // 1st message to send
        U_ASSERT(next == true);
        U_ASSERT(s_pMgaFlashBlockList);
        U_ASSERT(s_flashMessagesSent == 0);
        s_pLastFlashBlockSent = &s_pMgaFlashBlockList[0];
    }
    else
    {
        if (next)
        {
            if (s_flashMessagesSent < s_mgaFlashBlockCount)
            {
                // move on to next block is possible
                // mark last block sent as successful - report success
                s_pLastFlashBlockSent->state = MGA_MSG_RECEIVED;

                if (s_pEvtInterface->evtProgress)
                {
                    s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_MSG_TRANSFER_COMPLETE, s_pCallbackContext, s_pLastFlashBlockSent, sizeof(MgaMsgInfo));
                }

                // move next message
                s_pLastFlashBlockSent++;
                s_flashMessagesSent++;
            }
            else
            {
                // MODIFIED: take no action: the stop message will be sent below
            }
        }
        else
        {
            // retry
            s_pLastFlashBlockSent->retryCount++;
            // MODIFED: don't increment sequence number if retrying
            U_ASSERT(s_flashSequence > 0);
            s_flashSequence--;
            if (s_pLastFlashBlockSent->retryCount > s_pFlowConfig->msgRetryCount)
            {
                // too many retries - give up
                s_pLastFlashBlockSent->state = MGA_MSG_FAILED;

                // report failed block
                if (s_pEvtInterface->evtProgress)
                {
                    s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_MSG_TRANSFER_FAILED, s_pCallbackContext, s_pLastFlashBlockSent, sizeof(MgaMsgInfo));
                }

                terminated = true;
                s_mgaFlashBlockCount = s_flashMessagesSent; // force stop
            }
        }
    }

    if (terminated)
    {
        // report giving up
        EVT_TERMINATION_REASON stopReason = TERMINATE_RECEIVER_NOT_RESPONDING;
        sessionStop(MGA_PROGRESS_EVT_TERMINATED, &stopReason, sizeof(stopReason));
    }
    else if (s_flashMessagesSent >= s_mgaFlashBlockCount)
    {
        // all data messages sent.
        // now send message to tell receiver there is no more data
        sendFlashStop();
    }
    else
    {
        // generate event to send next message
        U_ASSERT(s_pEvtInterface);
        U_ASSERT(s_pEvtInterface->evtWriteDevice);

        U_ASSERT(sizeof(FlashDataMsgHeader) == 12);
        UBX_U1 flashDataMsg[sizeof(FlashDataMsgHeader) + FLASH_DATA_MSG_PAYLOAD + 2];

        FlashDataMsgHeader* pflashDataMsgHeader = (FlashDataMsgHeader*)flashDataMsg;

        pflashDataMsgHeader->header1 = UBX_SIG_PREFIX_1;
        pflashDataMsgHeader->header2 = UBX_SIG_PREFIX_2;
        pflashDataMsgHeader->msgClass = UBX_CLASS_MGA;
        pflashDataMsgHeader->msgId = UBX_MGA_FLASH;
        pflashDataMsgHeader->payloadLength = 6 + s_pLastFlashBlockSent->msgSize;

        // UBX-MGA-FLASH-DATA message
        pflashDataMsgHeader->type = 1;
        pflashDataMsgHeader->typeVersion = 0;
        pflashDataMsgHeader->sequence = s_flashSequence;
        pflashDataMsgHeader->payloadCount = s_pLastFlashBlockSent->msgSize;

        size_t flashMsgTotalSize = sizeof(FlashDataMsgHeader);
        UBX_U1* pFlashDataPayload = flashDataMsg + sizeof(FlashDataMsgHeader);
        memcpy(pFlashDataPayload, s_pLastFlashBlockSent->pMsg, s_pLastFlashBlockSent->msgSize);
        flashMsgTotalSize += s_pLastFlashBlockSent->msgSize;
        U_ASSERT(flashMsgTotalSize == s_pLastFlashBlockSent->msgSize + sizeof(FlashDataMsgHeader));

        addChecksum(&pflashDataMsgHeader->msgClass, flashMsgTotalSize - 2);
        flashMsgTotalSize += 2;
        U_ASSERT(validChecksum(&flashDataMsg[2], flashMsgTotalSize - 4));

        s_flashSequence++;
        s_pEvtInterface->evtWriteDevice(s_pCallbackContext, flashDataMsg, (UBX_I4)flashMsgTotalSize);

        s_pLastFlashBlockSent->state = MGA_MSG_WAITING_FOR_ACK;
        s_pLastFlashBlockSent->timeOut = s_pFlowConfig->msgTimeOut + uPortGetTickTimeMs();

        if (s_pEvtInterface->evtProgress)
        {
            s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_MSG_SENT, s_pCallbackContext, s_pLastFlashBlockSent, sizeof(MgaMsgInfo));
        }
    }
}

static void sendFlashMainSeqBlock(void)
{
    // do not lock here - locks must already be in place
    bool terminated = false;

    if (s_pLastFlashBlockSent == NULL)
    {
        // 1st message to send
        U_ASSERT(s_pMgaFlashBlockList);
        U_ASSERT(s_flashMessagesSent == 0);
        s_pLastFlashBlockSent = &s_pMgaFlashBlockList[0];
    }
    else
    {
        if (s_flashMessagesSent < s_mgaFlashBlockCount)
        {
            // move on to next block is possible
            // mark last block sent as successful - report success
            s_pLastFlashBlockSent->state = MGA_MSG_RECEIVED;

            if (s_pEvtInterface->evtProgress)
            {
                s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_LEGACY_AIDING_FLASH_BLOCK_COMPLETE, s_pCallbackContext, s_pLastFlashBlockSent, sizeof(MgaMsgInfo));
            }

            // move next message
            s_pLastFlashBlockSent++;
            s_flashMessagesSent++;
        }
        else
        {
            U_ASSERT(0);
            // shouldn't happen
            //lint -save -e527
            terminated = true;
            //lint -restore
        }
    }

    if (terminated)
    {
        // report giving up
        EVT_TERMINATION_REASON stopReason = TERMINATE_RECEIVER_NOT_RESPONDING;
        sessionStop(MGA_PROGRESS_EVT_TERMINATED, &stopReason, sizeof(stopReason));
    }
    else if (s_flashMessagesSent >= s_mgaFlashBlockCount)
    {
        // All data messages sent.
        // Main sequence finished. Next needs to be stop block
        s_aidState = LEGACY_AIDING_STOPPING;
        if (s_pEvtInterface->evtProgress)
        {
            s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_LEGACY_AIDING_FINALIZE_START,
                                         s_pCallbackContext,
                                         NULL,
                                         0);
        }
        sendAidingFlashStop();        // now send message to tell receiver there is no more data
    }
    else
    {
        // Generate event to send next message
        U_ASSERT(s_pEvtInterface);
        U_ASSERT(s_pEvtInterface->evtWriteDevice);

        U_ASSERT(sizeof(UbxMsgHeader) == 6);
        UBX_U1 aidingFlashDataMsg[sizeof(UbxMsgHeader) + FLASH_DATA_MSG_PAYLOAD + 2];

        UbxMsgHeader* pflashDataMsgHeader = (UbxMsgHeader*)aidingFlashDataMsg;

        pflashDataMsgHeader->header1 = UBX_SIG_PREFIX_1;
        pflashDataMsgHeader->header2 = UBX_SIG_PREFIX_2;
        pflashDataMsgHeader->msgClass = UBX_CLASS_AID;
        pflashDataMsgHeader->msgId = UBX_AID_ALP;
        pflashDataMsgHeader->payloadLength = s_pLastFlashBlockSent->msgSize;

        size_t flashMsgTotalSize = sizeof(UbxMsgHeader);
        UBX_U1* pFlashDataPayload = aidingFlashDataMsg + sizeof(UbxMsgHeader);
        memcpy(pFlashDataPayload, s_pLastFlashBlockSent->pMsg, s_pLastFlashBlockSent->msgSize);

        flashMsgTotalSize += s_pLastFlashBlockSent->msgSize;
        U_ASSERT(flashMsgTotalSize == s_pLastFlashBlockSent->msgSize + sizeof(UbxMsgHeader));
        flashMsgTotalSize += 2; // Add checksum length

        addChecksum(&pflashDataMsgHeader->msgClass, flashMsgTotalSize - 4);
        U_ASSERT(validChecksum(&aidingFlashDataMsg[2], flashMsgTotalSize - 4));

        s_pEvtInterface->evtWriteDevice(s_pCallbackContext, aidingFlashDataMsg, (UBX_I4)flashMsgTotalSize);

        s_pLastFlashBlockSent->state = MGA_MSG_WAITING_FOR_ACK;
        s_pLastFlashBlockSent->timeOut = s_pFlowConfig->msgTimeOut + uPortGetTickTimeMs();

        if (s_pEvtInterface->evtProgress)
        {
            s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_LEGACY_AIDING_FLASH_BLOCK_SENT, s_pCallbackContext, s_pLastFlashBlockSent, sizeof(MgaMsgInfo));
        }
    }
}

static void sendEmptyFlashBlock(void)
{
    UBX_U1 emptyFlashMsg[14] = { UBX_SIG_PREFIX_1, UBX_SIG_PREFIX_2,
        UBX_CLASS_MGA, UBX_MGA_FLASH,
        0x06, 0x00,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00 };

    addChecksum(&emptyFlashMsg[2], sizeof(emptyFlashMsg) - 4);
    U_ASSERT(validChecksum(&emptyFlashMsg[2], sizeof(emptyFlashMsg) - 4));
    s_pEvtInterface->evtWriteDevice(s_pCallbackContext, emptyFlashMsg, sizeof(emptyFlashMsg));
}

static void sendFlashStop(void)
{
    UBX_U1 flashStopMsg[10] = { UBX_SIG_PREFIX_1, UBX_SIG_PREFIX_2,
        UBX_CLASS_MGA, UBX_MGA_FLASH,
        0x02, 0x00,
        0x02, 0x00,
        0x00, 0x00 };

    addChecksum(&flashStopMsg[2], sizeof(flashStopMsg) - 4);
    U_ASSERT(validChecksum(&flashStopMsg[2], sizeof(flashStopMsg) - 4));
    s_pEvtInterface->evtWriteDevice(s_pCallbackContext, flashStopMsg, sizeof(flashStopMsg));
}

static void sendAidingFlashStop(void)
{
    UBX_U1 aidingFlashStopMsg[] = { UBX_SIG_PREFIX_1, UBX_SIG_PREFIX_2,
        UBX_CLASS_AID, UBX_AID_ALP,
        0x01, 0x00,
        0xFF,
        0x00, 0x00 };

    addChecksum(&aidingFlashStopMsg[2], sizeof(aidingFlashStopMsg) - 4);
    U_ASSERT(validChecksum(&aidingFlashStopMsg[2], sizeof(aidingFlashStopMsg) - 4));
    s_pEvtInterface->evtWriteDevice(s_pCallbackContext, aidingFlashStopMsg, sizeof(aidingFlashStopMsg));

    s_aidingTimeout = (s_pFlowConfig->msgTimeOut / 1000) + (uPortGetTickTimeMs() / 1000);
}

static void addMgaIniTime(const UBX_U1* pMgaData, UBX_I4* iSize, UBX_U1** pMgaDataOut, const MgaTimeAdjust* pTime)
{
    UBX_I4 nSizeTemp = *iSize;
    enum { nMsgSize = 24 + UBX_MSG_FRAME_SIZE };
    UBX_U1 mgaIniTimeMsg[nMsgSize] = {
        UBX_SIG_PREFIX_1, UBX_SIG_PREFIX_2, // header
        UBX_CLASS_MGA, UBX_MGA_INI,         // UBX-MGA-INI message
        0x18, 0x00,                         // length (24 bytes)
        0x10,                               // type
        0x00,                               // version
        0x00,                               // ref
        0x80,                               // leapSecs - really -128
        0x00, 0x00,                         // year
        0x00,                               // month
        0x00,                               // day
        0x00,                               // hour
        0x00,                               // minute
        0x00,                               // second
        0x00,                               // reserved2
        0x00, 0x00, 0x00, 0x00,             // ns
        0x02, 0x00,                         // tAccS
        0x00, 0x00,                         // reserved3
        0x00, 0x00, 0x00, 0x00,             // tAccNs
        0x00, 0x00                          // checksum
    };

    mgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 4] = (UBX_U1)(pTime->mgaYear & 0xFF);
    mgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 5] = (UBX_U1)(pTime->mgaYear >> 8);
    mgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 6] = pTime->mgaMonth;
    mgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 7] = pTime->mgaDay;
    mgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 8] = pTime->mgaHour;
    mgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 9] = pTime->mgaMinute;
    mgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 10] = pTime->mgaSecond;
    mgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 16] = (UBX_U1)(pTime->mgaAccuracyS & 0xFF);
    mgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 17] = (UBX_U1)(pTime->mgaAccuracyS >> 8);

    UBX_U4 timeInNs = ((UBX_U4)pTime->mgaAccuracyMs) * MS_IN_A_NS;
    mgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 20] = (UBX_U1)timeInNs;
    mgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 21] = (UBX_U1)(timeInNs >> 8);
    mgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 22] = (UBX_U1)(timeInNs >> 16);
    mgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 23] = (UBX_U1)(timeInNs >> 24);

    U_ASSERT(sizeof(mgaIniTimeMsg) == (nMsgSize));

    addChecksum(&mgaIniTimeMsg[2], sizeof(mgaIniTimeMsg) - 4);
    U_ASSERT(validChecksum(&mgaIniTimeMsg[2], sizeof(mgaIniTimeMsg) - 4));

    *iSize = *iSize + (UBX_I4)nMsgSize;

    *pMgaDataOut = (UBX_U1*)pUPortMalloc(*iSize);
    memcpy(*pMgaDataOut, mgaIniTimeMsg, nMsgSize);
    memcpy(*pMgaDataOut + nMsgSize, pMgaData, nSizeTemp);
}

static void addMgaIniPos(const UBX_U1* pMgaData, UBX_I4* iSize, UBX_U1** pMgaDataOut, const MgaPosAdjust* pPos)
{
    UBX_I4 nSizeTemp = *iSize;
    enum { nMsgSize = 20 + UBX_MSG_FRAME_SIZE };
    UBX_U1 mgaIniPosMsg[nMsgSize] = {
        UBX_SIG_PREFIX_1, UBX_SIG_PREFIX_2, // header
        UBX_CLASS_MGA, UBX_MGA_INI,         // UBX-MGA-INI message
        0x14, 0x00,                         // length (20 bytes)
        0x01,                               // type
        0x00,                               // version
        0x00, 0x00,                         // reserved
        0x00, 0x00, 0x00, 0x00,             // lat
        0x02, 0x00, 0x00, 0x00,             // lon
        0x00, 0x00, 0x00, 0x00,             // alt
        0x00, 0x00, 0x00, 0x00,             // acc
        0x00, 0x00                          // checksum
    };

    mgaIniPosMsg[UBX_MSG_PAYLOAD_OFFSET + 4] = (UBX_U1) pPos->mgaLatX1e7;
    mgaIniPosMsg[UBX_MSG_PAYLOAD_OFFSET + 5] = (UBX_U1)(pPos->mgaLatX1e7 >> 8);
    mgaIniPosMsg[UBX_MSG_PAYLOAD_OFFSET + 6] = (UBX_U1)(pPos->mgaLatX1e7 >> 16);
    mgaIniPosMsg[UBX_MSG_PAYLOAD_OFFSET + 7] = (UBX_U1)(pPos->mgaLatX1e7 >> 24);
    mgaIniPosMsg[UBX_MSG_PAYLOAD_OFFSET + 8] = (UBX_U1) pPos->mgaLonX1e7;
    mgaIniPosMsg[UBX_MSG_PAYLOAD_OFFSET + 9] = (UBX_U1)(pPos->mgaLonX1e7 >> 8);
    mgaIniPosMsg[UBX_MSG_PAYLOAD_OFFSET + 10] = (UBX_U1)(pPos->mgaLonX1e7 >> 16);
    mgaIniPosMsg[UBX_MSG_PAYLOAD_OFFSET + 11] = (UBX_U1)(pPos->mgaLonX1e7 >> 24);
    mgaIniPosMsg[UBX_MSG_PAYLOAD_OFFSET + 12] = (UBX_U1)pPos->mgaAlt;
    mgaIniPosMsg[UBX_MSG_PAYLOAD_OFFSET + 13] = (UBX_U1)(pPos->mgaAlt >> 8);
    mgaIniPosMsg[UBX_MSG_PAYLOAD_OFFSET + 14] = (UBX_U1)(pPos->mgaAlt >> 16);
    mgaIniPosMsg[UBX_MSG_PAYLOAD_OFFSET + 15] = (UBX_U1)(pPos->mgaAlt >> 24);
    mgaIniPosMsg[UBX_MSG_PAYLOAD_OFFSET + 16] = (UBX_U1)pPos->mgaAcc;
    mgaIniPosMsg[UBX_MSG_PAYLOAD_OFFSET + 17] = (UBX_U1)(pPos->mgaAcc >> 8);
    mgaIniPosMsg[UBX_MSG_PAYLOAD_OFFSET + 18] = (UBX_U1)(pPos->mgaAcc >> 16);
    mgaIniPosMsg[UBX_MSG_PAYLOAD_OFFSET + 19] = (UBX_U1)(pPos->mgaAcc >> 24);

    U_ASSERT(sizeof(mgaIniPosMsg) == (UBX_MSG_FRAME_SIZE + 20));

    addChecksum(&mgaIniPosMsg[2], sizeof(mgaIniPosMsg) - 4);
    U_ASSERT(validChecksum(&mgaIniPosMsg[2], sizeof(mgaIniPosMsg) - 4));

    *iSize = *iSize + (UBX_I4)nMsgSize;

    *pMgaDataOut = (UBX_U1*)pUPortMalloc(*iSize);
    memcpy(*pMgaDataOut, mgaIniPosMsg, nMsgSize);
    memcpy(*pMgaDataOut + nMsgSize, pMgaData, nSizeTemp);
}

// MODIFIED to support the UBX-CFG-VAL interface (M10 and beyond) as well as UBX-CFG-NAVX5
static void sendCfgMgaAidAcks(bool enable, bool bV3)
{
    int nLen;
    UBX_U1 msg[44 + UBX_MSG_FRAME_SIZE] = { 0 };

    if (s_pFlowConfig->mgaCfgVal) {
        // Encode using uUbxProtocolUint32Encode() to ensure correct endianness
        UBX_U4 key = uUbxProtocolUint32Encode(CFG_NAVSPG_ACKAIDING);
        nLen = 9;

        msg[0] = UBX_SIG_PREFIX_1;
        msg[1] = UBX_SIG_PREFIX_2;
        msg[2] = UBX_CLASS_CFG;
        msg[3] = UBX_CFG_VALSET;
        msg[4] = (UBX_U1)nLen;
        msg[5] = 0;
        msg[6] = 0;
        msg[7] = 0x01;  // Store in RAM
        memcpy(msg + 10, &key, sizeof(key));
        if (enable)
            msg[14] = 0x01;
    } else {
        nLen = 40;

        if (bV3 == true)
            nLen = 44;

        msg[0] = UBX_SIG_PREFIX_1;
        msg[1] = UBX_SIG_PREFIX_2;
        msg[2] = UBX_CLASS_CFG;
        msg[3] = UBX_CFG_NAVX5;
        msg[4] = (UBX_U1)nLen;
        msg[5] = 0;

        UBX_U1* pPayload = &msg[6];
        if (!bV3)
            pPayload[0] = 0;
        else
            pPayload[0] = 3;

        pPayload[1] = 0;

        pPayload[2] = 0x00;
        pPayload[3] = 0x04;                 // apply assistance acknowledgment settings
        pPayload[17] = enable ? 1 : 0;      // issue acknowledgments for assistance message input

        U_ASSERT(sizeof(msg) == (UBX_MSG_FRAME_SIZE + 44));
    }

    addChecksum(&msg[2], nLen + UBX_MSG_FRAME_SIZE - 4);
    U_ASSERT(validChecksum(&msg[2], nLen + UBX_MSG_FRAME_SIZE - 4));
    s_pEvtInterface->evtWriteDevice(s_pCallbackContext, msg, nLen + UBX_MSG_FRAME_SIZE);
}

static void sendAllMessages(void)
{
    s_pLastMsgSent = &s_pMgaMsgList[0];
    for (UBX_U4 i = 0; i < s_mgaBlockCount; i++)
    {
        U_ASSERT(s_pEvtInterface);
        U_ASSERT(s_pEvtInterface->evtWriteDevice);
        s_pEvtInterface->evtWriteDevice(s_pCallbackContext, s_pLastMsgSent->pMsg, s_pLastMsgSent->msgSize);
        s_pLastMsgSent->state = MGA_MSG_WAITING_FOR_ACK;

        if (s_pEvtInterface->evtProgress)
        {
            s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_MSG_SENT, s_pCallbackContext, s_pLastMsgSent, sizeof(MgaMsgInfo));
        }

        s_pLastMsgSent->state = MGA_MSG_RECEIVED;

        if (s_pEvtInterface->evtProgress)
        {
            s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_MSG_TRANSFER_COMPLETE, s_pCallbackContext, s_pLastMsgSent, sizeof(MgaMsgInfo));
        }

        s_pLastMsgSent++;
        s_messagesSent++;

        // MODIFIED add a compile-time-fixed delay between messages
        uPortTaskBlock(U_GNSS_MGA_INTER_MESSAGE_DELAY_MS);
    }

    // all done
    sessionStop(MGA_PROGRESS_EVT_FINISH, NULL, 0);
}

static UBX_I4 sendNextMgaMessage(void)
{
    // do not lock here - lock must already be in place
    U_ASSERT(s_pFlowConfig->mgaFlowControl != MGA_FLOW_NONE);

    UBX_I4 msgSize = 0;
    if (s_pLastMsgSent == NULL)
    {
        // 1st message to send
        U_ASSERT(s_pMgaMsgList);
        U_ASSERT(s_messagesSent == 0);
        s_pLastMsgSent = &s_pMgaMsgList[0];
    }
    else
    {
        // move next message
        s_pLastMsgSent++;
        s_messagesSent++;
    }

    if (s_pLastMsgSent == NULL)
        return 0;

    if (s_messagesSent < s_mgaBlockCount)
    {
        // generate event to send next message
        U_ASSERT(s_pEvtInterface);
        U_ASSERT(s_pEvtInterface->evtWriteDevice);
        msgSize = s_pLastMsgSent->msgSize;
        s_pEvtInterface->evtWriteDevice(s_pCallbackContext, s_pLastMsgSent->pMsg, msgSize);
        s_pLastMsgSent->state = MGA_MSG_WAITING_FOR_ACK;
        s_pLastMsgSent->timeOut = s_pFlowConfig->msgTimeOut + uPortGetTickTimeMs();

        if (s_pEvtInterface->evtProgress)
        {
            s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_MSG_SENT, s_pCallbackContext, s_pLastMsgSent, sizeof(MgaMsgInfo));
        }
    }

    return msgSize;
}

static void resendMessage(MgaMsgInfo* pResendMsg)
{
    // do not lock here - lock must already be in place
    U_ASSERT(pResendMsg->retryCount != 0);

    // generate event to resend message
    U_ASSERT(s_pEvtInterface);
    U_ASSERT(s_pEvtInterface->evtWriteDevice);
    s_pEvtInterface->evtWriteDevice(s_pCallbackContext, pResendMsg->pMsg, pResendMsg->msgSize);
    pResendMsg->state = MGA_MSG_WAITING_FOR_ACK;
    pResendMsg->timeOut = s_pFlowConfig->msgTimeOut + uPortGetTickTimeMs();

    if (s_pEvtInterface->evtProgress)
    {
        s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_MSG_SENT, s_pCallbackContext, pResendMsg, sizeof(MgaMsgInfo));
    }
}

static MGA_API_RESULT countMgaMsg(const UBX_U1* pMgaData, UBX_I4 iSize, UBX_U4* piCount)
{
    U_ASSERT(piCount);

    MGA_API_RESULT res = MGA_API_BAD_DATA;
    UBX_I4 msgCount = 0;
    UBX_I4 totalSize = 0;
    *piCount = 0;

    while (totalSize < iSize)
    {
        if ((pMgaData[0] == UBX_SIG_PREFIX_1) && (pMgaData[1] == UBX_SIG_PREFIX_2))
        {
            // UBX message
            UBX_U4 payloadSize = pMgaData[4] + (pMgaData[5] << 8);
            UBX_U4 msgSize = payloadSize + UBX_MSG_FRAME_SIZE;
            bool bMsgOfInterest = false;

            switch (pMgaData[2])
            {
            case UBX_CLASS_MGA:  // MGA messages
                switch (pMgaData[3])
                {
                case UBX_MGA_GPS:
                case UBX_MGA_GAL:
                case UBX_MGA_BDS:
                case UBX_MGA_QZSS:
                case UBX_MGA_GLO:
                case UBX_MGA_ANO:
                case UBX_MGA_INI:
                    // MGA message of interest
                    bMsgOfInterest = true;
                    break;

                default:
                    // ignore
                    break;
                }
                break;

            case UBX_CLASS_AID:  // AID messages
                switch (pMgaData[3])
                {
                case UBX_AID_INI:
                case UBX_AID_HUI:
                case UBX_AID_ALM:
                case UBX_AID_EPH:
                    // AID message of interest
                    bMsgOfInterest = true;
                    break;

                default:
                    // ignore
                    break;
                }
                break;

            default:
                // ignore
                break;
            }

            if (bMsgOfInterest)
            {
                if (validChecksum(&pMgaData[2], (size_t)(msgSize - 4)))
                {
                    msgCount++;
                }
                else
                {
                    // bad checksum - move on
                }
            }
            pMgaData += msgSize;
            totalSize += (UBX_I4)msgSize;
        }
        else
        {
            // corrupt data - abort
            break;
        }
    }

    if (totalSize == iSize)
    {
        *piCount = (UBX_U4)msgCount;
        res = MGA_API_OK;
    }

    return res;
}

static bool validChecksum(const UBX_U1* pPayload, size_t iSize)
{
    UBX_U1 ChksumA = 0;
    UBX_U1 ChksumB = 0;

    for (size_t i = 0; i < iSize; i++)
    {
        ChksumA = (UBX_U1)(ChksumA + *pPayload);
        pPayload++;
        ChksumB = (UBX_U1)(ChksumB + ChksumA);
    }

    return ((ChksumA == pPayload[0]) && (ChksumB == pPayload[1]));
}

static void addChecksum(UBX_U1* pPayload, size_t iSize)
{
    UBX_U1 ChksumA = 0;
    UBX_U1 ChksumB = 0;

    for (size_t i = 0; i < iSize; i++)
    {
        ChksumA = (UBX_U1)(ChksumA + *pPayload);
        pPayload++;
        ChksumB = (UBX_U1)(ChksumB + ChksumA);
    }

    *pPayload = ChksumA;
    pPayload++;
    *pPayload = ChksumB;
}

static MgaMsgInfo* buildMsgList(const UBX_U1* pMgaData, unsigned int uNumEntries)
{
    // do not lock here - lock must already be in place

    MgaMsgInfo* pMgaMsgList = (MgaMsgInfo*)pUPortMalloc(sizeof(MgaMsgInfo) * uNumEntries);
    if (pMgaMsgList == NULL)
        return NULL;

    MgaMsgInfo* pCurrentBlock = pMgaMsgList;

    unsigned int i = 0;
    while (i < uNumEntries)
    {
        UBX_U2 payloadSize = pMgaData[4] + (pMgaData[5] << 8);
        UBX_U2 msgSize = payloadSize + UBX_MSG_FRAME_SIZE;
        bool bProcessMsg = false;

        if ((pMgaData[0] == UBX_SIG_PREFIX_1) && (pMgaData[1] == UBX_SIG_PREFIX_2))
        {
            //UBX message
            switch (pMgaData[2])
            {
            case UBX_CLASS_MGA:  // MGA message
                switch (pMgaData[3])
                {
                case UBX_MGA_GPS:
                case UBX_MGA_GAL:
                case UBX_MGA_BDS:
                case UBX_MGA_QZSS:
                case UBX_MGA_GLO:
                case UBX_MGA_ANO:
                case UBX_MGA_INI:
                    bProcessMsg = true;
                    break;

                default:
                    // ignore
                    break;
                }
                break;

            case UBX_CLASS_AID:  // AID message
                switch (pMgaData[3])
                {
                case UBX_AID_INI:
                case UBX_AID_HUI:
                case UBX_AID_ALM:
                case UBX_AID_EPH:
                    bProcessMsg = true;
                    break;

                default:
                    // ignore
                    break;
                }
                break;

            default:
                // ignore
                break;
            }

            if (bProcessMsg)
            {
                U_ASSERT(pCurrentBlock < &pMgaMsgList[uNumEntries]);
                const UBX_U1* pPayload = &pMgaData[6];

                pCurrentBlock->mgaMsg.msgId = pMgaData[3];
                memcpy(pCurrentBlock->mgaMsg.mgaPayloadStart, pPayload, sizeof(pCurrentBlock->mgaMsg.mgaPayloadStart));
                pCurrentBlock->pMsg = pMgaData;
                pCurrentBlock->msgSize = msgSize;
                pCurrentBlock->state = MGA_MSG_WAITING_TO_SEND;
                pCurrentBlock->timeOut = 0; // set when transfer takes place
                pCurrentBlock->retryCount = 0;
                pCurrentBlock->sequenceNumber = (UBX_U2)i;
                pCurrentBlock->mgaFailedReason = MGA_FAILED_REASON_CODE_NOT_SET;
                pCurrentBlock++;

                i++;
            }
        }
        pMgaData += msgSize;
    }

    U_ASSERT(pCurrentBlock == &pMgaMsgList[uNumEntries]);
    return pMgaMsgList;
}

static bool checkForIniMessage(const UBX_U1* pUbxMsg)
{
    if ((pUbxMsg[2] == UBX_CLASS_MGA) && (pUbxMsg[3] == UBX_MGA_INI) && (pUbxMsg[6] == 0x10))
    {
        // UBX-MGA-INI-TIME
        return true;
    }
    else if ((pUbxMsg[2] == UBX_CLASS_AID) && (pUbxMsg[3] == UBX_AID_INI))
    {
        // UBX-AID-INI
        return true;
    }

    return false;
}

static void sessionStop(MGA_PROGRESS_EVENT_TYPE evtType, const void* pEventInfo, size_t evtInfoSize)
{
    // do not lock here - lock must already be in place
    U_ASSERT(s_sessionState != MGA_IDLE);

    if (s_pEvtInterface->evtProgress)
    {
        s_pEvtInterface->evtProgress(evtType, s_pCallbackContext, pEventInfo, (UBX_I4)evtInfoSize);
    }

    // Tidy up and MGA transfer settings
    uPortFree(s_pMgaDataSession);
    s_pMgaDataSession = NULL;
    uPortFree(s_pMgaMsgList);
    s_pMgaMsgList = NULL;
    s_mgaBlockCount = 0;
    s_sessionState = MGA_IDLE;
    s_pLastMsgSent = NULL;
    s_messagesSent = 0;
    s_ackCount = 0;

    // Tidy up any flash transfer settings
    uPortFree(s_pMgaFlashBlockList);
    s_pMgaFlashBlockList = NULL;
    s_mgaFlashBlockCount = 0;
    s_pLastFlashBlockSent = NULL;
    s_flashMessagesSent = 0;
    s_flashSequence = 0;

    // Tidy up any specific legacy aiding flash transfer settings
    s_bLegacyAiding = false;
    s_aidState = LEGACY_AIDING_IDLE;
    s_aidingTimeout = 0;

    // Tidy up any legacy aiding server settings
    s_pAidingData = NULL;
    s_aidingDataSize = 0;
    s_alpfileId = 0;
}

// MODIFIED: skipSpaces(), nextToken() and getData() removed, not required
#if 0
static const char* skipSpaces(const char* pText)
{
    // MODIFIED: isspace() takes an integer
    while ((*pText != 0) && isspace((int) *pText))
    {
        pText++;
    }

    return *pText == 0 ? NULL : pText;
}

static const char* nextToken(const char* pText)
{
    // MODIFIED: isspace() takes an integer
    //lint -save -e613
    while ((*pText != 0) && (!isspace((int) *pText)))
    {
        pText++;
    }
    //lint -restore
    return skipSpaces(pText);
}

static int getData(SOCKET sock, char* p, size_t iSize)
{
    size_t c = 0;
    do
    {
        fd_set fdset;
        //lint -save -e866
        FD_ZERO(&fdset);
        //lint -restore

#ifdef WIN32
#  pragma warning(push)
#  pragma warning( disable : 4127 )
#endif // WIN32
        FD_SET(sock, &fdset);
#ifdef WIN32
#  pragma warning(pop)
#endif // WIN32

        struct timeval tv;
        tv.tv_sec = s_serverResponseTimeout;
        tv.tv_usec = 0;

        if (select(sock + 1, &fdset, NULL, NULL, &tv) > 0)
        {
            int b = recv(sock, p, 1, 0);
            if (b <= 0)
            {
                // failed or timeout
                break;
            }
            else if (b > 0)
            {
                //lint -save -e613
                p++;
                //lint -restore
                c++;
            }
        }
        else
        {
            // no response
            break;
        }
    } while (c < iSize);

    return (int)c;
}
#endif // Not required

static void adjustMgaIniTime(MgaMsgInfo* pMsgInfo, const MgaTimeAdjust* pMgaTime)
{
    struct tm adjustedTime;
    U_ASSERT(pMsgInfo);
    U_ASSERT(pMsgInfo->pMsg[0] == UBX_SIG_PREFIX_1);
    U_ASSERT(pMsgInfo->pMsg[1] == UBX_SIG_PREFIX_2);
    U_ASSERT((pMsgInfo->pMsg[2] == UBX_CLASS_MGA) || (pMsgInfo->pMsg[2] == UBX_CLASS_AID));

    UBX_U1* pMgaIniTimeMsg = (UBX_U1*)pMsgInfo->pMsg;

    if (pMsgInfo->pMsg[2] == UBX_CLASS_MGA)
    {
        // MGA data
        U_ASSERT(pMsgInfo->pMsg[3] == UBX_MGA_INI);
        U_ASSERT(pMsgInfo->pMsg[6] == 0x10);

        switch (pMgaTime->mgaAdjustType)
        {
        case MGA_TIME_ADJUST_ABSOLUTE:
            pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 4] = (UBX_U1)(pMgaTime->mgaYear & 0xFF);
            pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 5] = (UBX_U1)(pMgaTime->mgaYear >> 8);
            pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 6] = (UBX_U1)pMgaTime->mgaMonth;
            pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 7] = (UBX_U1)pMgaTime->mgaDay;
            pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 8] = (UBX_U1)pMgaTime->mgaHour;
            pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 9] = (UBX_U1)pMgaTime->mgaMinute;
            pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 10] = (UBX_U1)pMgaTime->mgaSecond;
            pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 16] = (UBX_U1)(pMgaTime->mgaAccuracyS & 0xFF);
            pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 17] = (UBX_U1)(pMgaTime->mgaAccuracyS >> 8);
            *((UBX_U4*)(&pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 20])) = ((UBX_U4)pMgaTime->mgaAccuracyMs) * MS_IN_A_NS;
            break;

        case MGA_TIME_ADJUST_RELATIVE:
        {
            struct tm timeAsStruct;
            memset(&timeAsStruct, 0, sizeof(timeAsStruct));

            timeAsStruct.tm_year = (pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 4] + (pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 5] << 8)) - 1900;
            timeAsStruct.tm_mon = pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 6] - 1;
            timeAsStruct.tm_mday = pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 7];
            timeAsStruct.tm_hour = pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 8];
            timeAsStruct.tm_min = pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 9];
            timeAsStruct.tm_sec = pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 10];
            // MODIFIED this will cause mktime() to consider DST or not by itself, which is
            // the approach that uPortGetTimezoneOffsetSeconds() (see below) uses
            timeAsStruct.tm_isdst = -1;

            time_t t1 = mktime(&timeAsStruct);
            U_ASSERT(t1 != -1);

            U_ASSERT(pMgaTime->mgaYear == 0);
            U_ASSERT(pMgaTime->mgaMonth == 0);
            U_ASSERT(pMgaTime->mgaDay == 0);

            time_t adjustment = (pMgaTime->mgaHour * 3600) + (pMgaTime->mgaMinute * 60) + pMgaTime->mgaSecond;
            t1 += adjustment;
            // MODIFIED: timezone only works on Linux, not Windows; use uPortGetTimezoneOffsetSeconds() instead
            // MODIFIED: ...and add it, rather than subtracting it: mktime() expects its _input_ to be UTC, and
            // will subtract the time zone offset, so we need to add it back to get to UTC
            t1 += (time_t) uPortGetTimezoneOffsetSeconds();
            struct tm * pAdjustedTime = gmtime_r(&t1, &adjustedTime);
            if (pAdjustedTime == NULL) {
                //something went wrong with gmtime - can't adjust time
                break;
            }

            pAdjustedTime->tm_year += 1900;
            pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 4] = (UBX_U1)(pAdjustedTime->tm_year & 0xFF);
            pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 5] = (UBX_U1)(pAdjustedTime->tm_year >> 8);
            pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 6] = (UBX_U1)(pAdjustedTime->tm_mon + 1);
            pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 7] = (UBX_U1)pAdjustedTime->tm_mday;
            pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 8] = (UBX_U1)pAdjustedTime->tm_hour;
            pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 9] = (UBX_U1)pAdjustedTime->tm_min;
            pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 10] = (UBX_U1)pAdjustedTime->tm_sec;
        }
        break;

        default:
            U_ASSERT(0);
            break;
        }

        // recalculate message checksum
        addChecksum(&pMgaIniTimeMsg[2], (UBX_MSG_FRAME_SIZE + 24) - 4);
        U_ASSERT(validChecksum(&pMgaIniTimeMsg[2], (UBX_MSG_FRAME_SIZE + 24) - 4));
    }
    else
    {
        // Legacy online data
        U_ASSERT(pMsgInfo->pMsg[3] == UBX_AID_INI);

        switch (pMgaTime->mgaAdjustType)
        {
        case MGA_TIME_ADJUST_ABSOLUTE:
            pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 19] = (UBX_U1)(pMgaTime->mgaYear - 2000);
            pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 18] = (UBX_U1)pMgaTime->mgaMonth;
            pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 23] = (UBX_U1)pMgaTime->mgaDay;
            pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 22] = (UBX_U1)pMgaTime->mgaHour;
            pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 21] = (UBX_U1)pMgaTime->mgaMinute;
            pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 20] = (UBX_U1)pMgaTime->mgaSecond;

            *((UBX_U4*)(&pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 28])) = (UBX_U4)(pMgaTime->mgaAccuracyS * 1000) + pMgaTime->mgaAccuracyMs;
            *((UBX_U4*)(&pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 32])) = 0;    // ((UBX_U4)) * MS_IN_A_NS;

            *((UBX_U4 *)(&pMgaIniTimeMsg[UBX_MSG_PAYLOAD_OFFSET + 44])) |= 0x400;
            break;

        case MGA_TIME_ADJUST_RELATIVE:
            U_ASSERT(false);  // Not supported
            break;

        default:
            U_ASSERT(0);
            break;
        }

        // recalculate message checksum
        addChecksum(&pMgaIniTimeMsg[2], (UBX_MSG_FRAME_SIZE + 48) - 4);
        U_ASSERT(validChecksum(&pMgaIniTimeMsg[2], (UBX_MSG_FRAME_SIZE + 48) - 4));
    }
}

static MgaMsgInfo* findMsgBlock(UBX_U1 msgId, const UBX_U1* pMgaHeader)
{
    U_ASSERT(s_pMgaMsgList);

    MgaMsgInfo* pMsgInfo = &s_pMgaMsgList[0];
    for (UBX_U4 i = 0; i < s_mgaBlockCount; i++)
    {
        if ((pMsgInfo->state == MGA_MSG_WAITING_FOR_ACK) &&
            (pMsgInfo->mgaMsg.msgId == msgId) &&
            (memcmp(pMgaHeader, pMsgInfo->mgaMsg.mgaPayloadStart, sizeof(pMsgInfo->mgaMsg.mgaPayloadStart)) == 0))
        {
            // found match
            return pMsgInfo;
        }
        pMsgInfo++;
    }

    return NULL;
}

static void sendInitialMsgBatch(void)
{
    // MODIFIED: use U_GNSS_MGA_RX_BUFFER_SIZE_BYTES rather than the fixed 1000
    // dispatch maximum amount of messages
    UBX_I4 rxBufferSize = U_GNSS_MGA_RX_BUFFER_SIZE_BYTES;

    while (rxBufferSize > 0)
    {
        UBX_I4 msgSize = sendNextMgaMessage();
        if (msgSize == 0)
        {
            break;
        }
        rxBufferSize -= msgSize;
    }
}

static void initiateMessageTransfer(void)
{
    switch (s_pFlowConfig->mgaFlowControl)
    {
    case MGA_FLOW_SIMPLE:
        sendCfgMgaAidAcks(true, false);
        sendNextMgaMessage();
        break;

    case MGA_FLOW_NONE:
        sendAllMessages();
        break;

    case MGA_FLOW_SMART:
        sendCfgMgaAidAcks(true, false);
        sendInitialMsgBatch();
        break;

    default:
        U_ASSERT(0);
        break;
    }
}

static bool isAlmMatch(const UBX_U1* pMgaData)
{
    if ((pMgaData[2] == UBX_CLASS_MGA) && ((pMgaData[3] == UBX_MGA_BDS) || (pMgaData[3] == UBX_MGA_GPS) || (pMgaData[3] == UBX_MGA_GAL) || (pMgaData[3] == UBX_MGA_GLO) || (pMgaData[3] == UBX_MGA_QZSS)))
    {
        return true;
    }

    return false;
}

static bool isAnoMatch(const UBX_U1* pMgaData, int cy, int cm, int cd)
{
    if ((pMgaData[2] == UBX_CLASS_MGA) && (pMgaData[3] == UBX_MGA_ANO))
    {
        // UBX-MGA-ANO
        const UBX_U1* pPayload = &pMgaData[6];
        if (((pPayload[4] + 2000) == cy) && (pPayload[5] == cm) && (pPayload[6] == cd))
        {
            return true;
        }
    }

    return false;
}

static void adjustTimeToBestMatch(const UBX_U1* pMgaData, UBX_I4 pMgaDataSize, const struct tm* pTimeOriginal, struct tm* pTimeAdjusted)
{
    UBX_U4 totalSize = 0;
    bool noneFound = true;
    time_t diffSecondsMin = 0;  // MODIFIED: double -> time_t

    //find the closest offline data compared to current time
    while (totalSize < (UBX_U4)pMgaDataSize)
    {
        if ((pMgaData[0] == UBX_SIG_PREFIX_1) && (pMgaData[1] == UBX_SIG_PREFIX_2))
        {
            // UBX message
            UBX_U4 payloadSize = pMgaData[4] + (pMgaData[5] << 8);
            UBX_U4 msgSize = payloadSize + UBX_MSG_FRAME_SIZE;

            if ((pMgaData[2] == UBX_CLASS_MGA) && (pMgaData[3] == UBX_MGA_ANO))
            {
                struct tm timeOfflineData;
                memset(&timeOfflineData, 0, sizeof(struct tm));  // MODIFIED tm -> struct tm (doesn't compile under MSVC otherwise)
                timeOfflineData.tm_year = pMgaData[10] + 100;
                timeOfflineData.tm_mon = pMgaData[11] - 1;
                timeOfflineData.tm_mday = pMgaData[12];
                timeOfflineData.tm_hour = pMgaData[13];

                // MODIFIED double -> time_t, difftime() becomes a subtraction and tm -> struct tm (doesn't compile under MSVC otherwise)
                time_t diffSeconds = mktime(&timeOfflineData) - mktime((struct tm*)pTimeOriginal);
                if (noneFound || diffSeconds < diffSecondsMin)
                {
                    diffSecondsMin = diffSeconds;
                    noneFound = false;
                }
            }
            pMgaData += msgSize;
            totalSize += msgSize;
        }
        else
        {
            U_ASSERT(0);
            //lint -save -e527
            break;
            //lint -restore
        }
    }

    // MODIFIED tm -> struct tm (doesn't compile under MSVC otherwise)
    //set time to the best match in offline data
    time_t correctTime = mktime((struct tm*)pTimeOriginal);
    correctTime += diffSecondsMin;

    // MODIFIED localtime() -> gmtime_r() since some platforms don't have a localtime() and we have a gmtime_r(),
    // and, with this, also need to adjust for the timezone offset which mktime() will have subtracted
    // (localtime() would have done this itself, gmtime() doesn't).
    correctTime += uPortGetTimezoneOffsetSeconds();
    gmtime_r(&correctTime, pTimeAdjusted);
}

// MODIFIED: strcicmp(), getOnlineDataFromServer() and getOfflineDataFromServer() removed, not required
#if 0
static int strcicmp(char const *a, char const *b)
{
    for (;; a++, b++) {
        int d = tolower(*a) - tolower(*b);
        if (d != 0 || !*a)
            return d;
    }
}

static MGA_API_RESULT getOnlineDataFromServer(const char* pServer, MgaOnlineServerConfig* pServerConfig, UBX_U1** ppData, UBX_I4* piSize)
{
    *ppData = NULL;
    *piSize = 0;

    char* ptr;
    char strServer[80] = { 0 };
    char host[80] = { 0 };
    UBX_U2 wPort = 80;  // default port number

    // copy the server name
    strncpy(strServer, pServer, sizeof(strServer) - 1);

    // try to get server string
    if ((ptr = strtok(strServer, ":")) != NULL)
    {
        if (strcicmp(ptr, "http") == 0)
        {
            ptr = strtok(NULL, ":");
            strncpy(host, ptr + 2, sizeof(host) - 1); //remove the leading backslashes
        }
        else
            strncpy(host, ptr, sizeof(host) - 1);
    }

    // try to get port number
    if ((ptr = strtok(NULL, ":")) != NULL)
        wPort = (UBX_U2)atoi(ptr);

    // connect to server
    SOCKET iSock = connectServer(host, wPort);
    if (iSock == INVALID_SOCKET)
    {
        return MGA_API_CANNOT_CONNECT;
    }

    char requestParams[500];
    U_ASSERT(mgaBuildOnlineRequestParams(pServerConfig, requestParams, sizeof(requestParams)) == MGA_API_OK);

    char strHttp[1000];
    sprintf(strHttp, "GET /GetOnlineData.ashx?%s HTTP/1.1\r\n"
            "User-Agent: %s\r\n"
            "Host: %s\r\n"
            "Accept: */*\r\n"
            "Connection: Keep-Alive\r\n\r\n",
            requestParams,
            MGA_USER_AGENT,
            host);

    U_ASSERT(strlen(strHttp) < sizeof(strHttp));
    MGA_API_RESULT res = getDataFromService(iSock, strHttp, ppData, piSize);

#ifdef WIN32
    closesocket(iSock);
#else // WIN32
    close(iSock);
#endif // WIN32

    return res;
}

static MGA_API_RESULT getOfflineDataFromServer(const char* pServer, MgaOfflineServerConfig* pServerConfig, UBX_U1** ppData, UBX_I4* piSize)
{
    *ppData = NULL;
    *piSize = 0;

    char* ptr;
    char strServer[80] = { 0 };
    char host[80] = { 0 };
    UBX_U2 wPort = 80;  // default port number

    // copy the server name
    strncpy(strServer, pServer, sizeof(strServer) - 1);

    // try to get server string
    if ((ptr = strtok(strServer, ":")) != NULL)
    {
        if (strcicmp(ptr, "http") == 0)
        {
            ptr = strtok(NULL, ":");
            strncpy(host, ptr + 2, sizeof(host) - 1); //remove the leading backslashes
        }
        else
            strncpy(host, ptr, sizeof(host) - 1);
    }

    // try to get port number
    if ((ptr = strtok(NULL, ":")) != NULL)
        wPort = (UBX_U2)atoi(ptr);

    // connect to server
    SOCKET iSock = connectServer(host, wPort);
    if (iSock == INVALID_SOCKET)
    {
        return MGA_API_CANNOT_CONNECT;
    }

    char requestParams[500];
    mgaBuildOfflineRequestParams(pServerConfig, requestParams, sizeof(requestParams));

    char strHttp[1000];
    sprintf(strHttp, "GET /GetOfflineData.ashx?%s HTTP/1.1\r\n"
            "User-Agent: %s\r\n"
            "Host: %s\r\n"
            "Accept: */*\r\n"
            "Connection: Keep-Alive\r\n\r\n",
            requestParams,
            MGA_USER_AGENT,
            host);

    U_ASSERT(strlen(strHttp) < sizeof(strHttp));
    MGA_API_RESULT res = getDataFromService(iSock, strHttp, ppData, piSize);

#ifdef WIN32
    closesocket(iSock);
#else // WIN32
    close(iSock);
#endif // WIN32

    return res;
}
#endif // Not required

#if defined USE_SSL
static MGA_API_RESULT getOnlineDataFromServerSSL(const char* pServer, const MgaOnlineServerConfig* pServerConfig, UBX_U1** ppData, UBX_I4* piSize)
{
    *ppData = NULL;
    *piSize = 0;
    bool bVerifyServerCert = pServerConfig->bValidateServerCert;

    char* ptr;
    char strServer[80] = { 0 };
    char host[80] = { 0 };
    UBX_U2 wPort = 443;  // default port number

    // copy the server name
    strncpy(strServer, pServer, sizeof(strServer) - 1);

    // try to get server string
    if ((ptr = strtok(strServer, ":")) != NULL)
    {
        ptr = strtok(NULL, ":");
        strncpy(host, ptr + 2, sizeof(host) - 1); //remove the leading backslashes
    }

    // try to get port number
    if ((ptr = strtok(NULL, ":")) != NULL)
        wPort = (UBX_U2)atoi(ptr);

    char requestParams[500];
    U_ASSERT(mgaBuildOnlineRequestParams(pServerConfig, requestParams, sizeof(requestParams)) == MGA_API_OK);

    char strHttp[1000];
    sprintf(strHttp, "GET /GetOnlineData.ashx?%s HTTP/1.1\r\n"
            "User-Agent: %s\r\n"
            "Host: %s\r\n"
            "Accept: */*\r\n"
            "Connection: Keep-Alive\r\n\r\n",
            requestParams,
            MGA_USER_AGENT,
            host);

    U_ASSERT(strlen(strHttp) < sizeof(strHttp));
    MGA_API_RESULT res = getDataFromServiceSSL(strHttp, host, wPort, bVerifyServerCert, ppData, piSize);

    return res;
}

static MGA_API_RESULT getOfflineDataFromServerSSL(const char* pServer, const MgaOfflineServerConfig* pServerConfig, UBX_U1** ppData, UBX_I4* piSize)
{
    *ppData = NULL;
    *piSize = 0;
    bool bVerifyServerCert = pServerConfig->bValidateServerCert;

    char* ptr;
    char strServer[80] = { 0 };
    char host[80] = { 0 };
    UBX_U2 wPort = 443;  // default port number

    // copy the server name
    strncpy(strServer, pServer, sizeof(strServer) - 1);

    // try to get server string
    if ((ptr = strtok(strServer, ":")) != NULL)
    {
        ptr = strtok(NULL, ":");
        strncpy(host, ptr + 2, sizeof(host) - 1); //remove the leading backslashes
    }

    // try to get port number
    if ((ptr = strtok(NULL, ":")) != NULL)
        wPort = (UBX_U2)atoi(ptr);

    char requestParams[500];
    mgaBuildOfflineRequestParams(pServerConfig, requestParams, sizeof(requestParams));

    char strHttp[1000];
    sprintf(strHttp, "GET /GetOfflineData.ashx?%s HTTP/1.1\r\n"
            "User-Agent: %s\r\n"
            "Host: %s\r\n"
            "Accept: */*\r\n"
            "Connection: Keep-Alive\r\n\r\n",
            requestParams,
            MGA_USER_AGENT,
            host);

    U_ASSERT(strlen(strHttp) < sizeof(strHttp));
    MGA_API_RESULT res = getDataFromServiceSSL(strHttp, host, wPort, bVerifyServerCert, ppData, piSize);

    return res;
}

static MGA_API_RESULT getDataFromServiceSSL(const char* pRequest, const char* server, UBX_U2 port, bool bVerifyServerCert, UBX_U1** ppData, UBX_I4* piSize)
{
    const char* contentLengthStr = "CONTENT-LENGTH: ";
    char contentBuf[10];
    const char* badRequest = "BAD REQUEST";
    char firstResponse[1024];
    UBX_I4 ret, len, lastLen = 0;
    uint32_t flags;
    UBX_I4 firstPackage = 1;
    UBX_I4 stop = 0;
    MGA_API_RESULT res = MGA_API_OK;;
    mbedtls_net_context server_fd;
    unsigned char buf[1024];
    const char *pers = "ssl_client";

    char* pBuffer = NULL;
    size_t contentLength = 0;

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cacert;

    char* pContentPos;
    char* pContentPosEnd;
    UBX_I4 offset;

    //Initialize the RNG and the session data
    mbedtls_net_init(&server_fd);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_x509_crt_init(&cacert);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    mbedtls_entropy_init(&entropy);
    if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
        (const unsigned char *)pers,
        strlen(pers))) != 0)
    {
        reportError(MGA_SERVICE_ERROR_INIT_SSL, "SSL initialization failed", 0);
        res = MGA_API_INIT_SSL_FAIL;
        goto exit;
    }

    //Initialize certificates
    ret = mbedtls_x509_crt_parse(&cacert, (const unsigned char *)mbedtls_globalsign_pem,
                                 mbedtls_globalsign_pem_len);
    if (ret < 0)
    {
        reportError(MGA_SERVICE_ERROR_INIT_CERT_SSL, "Error when initializing certificates", 0);
        res = MGA_API_INIT_SSL_CERT_FAIL;
        goto exit;
    }

    //Start the connection
    char szPort[8];
    sprintf(szPort, "%d", port);

    if ((ret = mbedtls_net_connect(&server_fd, server,
        szPort, MBEDTLS_NET_PROTO_TCP)) != 0)
    {
        reportError(MGA_SERVICE_ERROR_CONNECT_SSL, "SSL connection failed", 0);
        res = MGA_API_CONNECT_SSL_FAIL;
        goto exit;
    }
    else
    {
        char serverString[255];
        memset(&serverString, 0, sizeof(serverString));
        strncpy(serverString, server, sizeof(serverString) - 1);
        strcat(serverString, ":");
        strcat(serverString, szPort);
        U_ASSERT(strlen(serverString) < sizeof(serverString) - 1);
        if (s_pEvtInterface->evtProgress)
        {
            s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_SERVER_CONNECTING, s_pCallbackContext, (const void*)serverString, (UBX_I4)strlen(serverString) + 1);
        }
    }

    //Setup
    if ((ret = mbedtls_ssl_config_defaults(&conf,
        MBEDTLS_SSL_IS_CLIENT,
        MBEDTLS_SSL_TRANSPORT_STREAM,
        MBEDTLS_SSL_PRESET_DEFAULT)) != 0)
    {
        reportError(MGA_SERVICE_ERROR_CONFIGURE_SSL, "SSL configuration failed", 0);
        res = MGA_API_CONFIG_SSL_FAIL;
        goto exit;
    }

    //set up verification for server certificate
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);

    if ((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0)
    {
        reportError(MGA_SERVICE_ERROR_SETUP_SSL, "SSL setup failed", 0);
        res = MGA_API_SETUP_SSL_FAIL;
        goto exit;
    }

    if ((ret = mbedtls_ssl_set_hostname(&ssl, server)) != 0)
    {
        reportError(MGA_SERVICE_ERROR_HOSTNAME_SSL, "SSL setting hostname failed", 0);
        res = MGA_API_SET_HOST_SSL_FAIL;
        goto exit;
    }

    mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

    //Handshake
    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0)
    {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
        {
            reportError(MGA_SERVICE_ERROR_HANDSHAKE_SSL, "SSL handshake failed", 0);
            res = MGA_API_HANDSHAKE_SSL_FAIL;
            goto exit;
        }
    }

    //Verify the server certificate if requested
    if (bVerifyServerCert == true)
    {
        if ((flags = mbedtls_ssl_get_verify_result(&ssl)) != 0)
        {
            reportError(MGA_SERVICE_ERROR_VERIFY_SSL, "SSL server certificate verification error", 0);
            res = MGA_API_WRITE_SSL_FAIL;
            goto exit;
        }
    }

    //Write the GET request
    len = sprintf((char *)buf, "%s", pRequest);

    while ((ret = mbedtls_ssl_write(&ssl, buf, len)) <= 0)
    {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
        {
            reportError(MGA_SERVICE_ERROR_WRITE_SSL, "SSL write request error", 0);
            res = MGA_API_WRITE_SSL_FAIL;
            goto exit;
        }
    }

    //Read the HTTP response
    do
    {
        len = sizeof(buf) - 1;
        memset(buf, 0, sizeof(buf));
        ret = mbedtls_ssl_read(&ssl, buf, len);

        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
        {
            stop = 0;
            continue;
        }

        if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
        {
            stop = 1;
            break;
        }

        if (ret < 0)
        {
            reportError(MGA_SERVICE_ERROR_READ_SSL, "SSL read response error", 0);
            res = MGA_API_READ_SSL_FAIL;
            stop = 1;
            break;
        }

        if (ret == 0)
        {
            stop = 1;
            break;
        }

        len = ret;

        offset = 0;

        if (firstPackage == 1) //first package
        {
            firstPackage = 0;

            //get upper case response with HTTP header
            for (int i = 0; i < len; i++)
            {
                firstResponse[i] = (char)toupper(buf[i]);
            }

            //check for bad response
            pContentPos = strstr(firstResponse, badRequest);
            if (pContentPos)
            {
                reportError(MGA_SERVICE_ERROR_BAD_STATUS, "Bad Response received", 0);
                res = MGA_API_CANNOT_GET_DATA;
                goto exit;
            }

            //get content length
            pContentPos = strstr(firstResponse, contentLengthStr);
            if (pContentPos)
            {
                pContentPos = pContentPos + strlen(contentLengthStr);
                pContentPosEnd = strstr(pContentPos, "\r\n");
                if (pContentPos && pContentPosEnd)
                {
                    memcpy(contentBuf, pContentPos, (UBX_I4)(pContentPosEnd - pContentPos));
                    contentBuf[(UBX_I4)(pContentPosEnd - pContentPos)] = '\0';
                    contentLength = atoi(contentBuf);

                    //get the actual data size
                    pContentPosEnd = strstr(pContentPos, "\r\n\r\n");
                    if (pContentPosEnd != NULL)
                    {
                        pContentPosEnd += 4;
                        offset = (UBX_I4)(pContentPosEnd - firstResponse);
                        len = len - (UBX_I4)(pContentPosEnd - firstResponse);
                    }
                    else
                    {
                        reportError(MGA_SERVICE_ERROR_BAD_STATUS, "Bad Response received", 0);
                        res = MGA_API_CANNOT_GET_DATA;
                        goto exit;
                    }
                }
                else
                {
                    reportError(MGA_SERVICE_ERROR_BAD_STATUS, "Bad Response received", 0);
                    res = MGA_API_CANNOT_GET_DATA;
                    goto exit;
                }
            }

            contentLength -= len;
            if (contentLength <= 0)
                stop = 1;

            pBuffer = (char*)pUPortMalloc(len);
            if (pBuffer == NULL)
            {
                res = MGA_API_OUT_OF_MEMORY;
                goto exit;
            }
            memcpy(pBuffer, buf + offset, len);
            lastLen = len;
        }
        else //this is just actual data
        {
            contentLength -= len;
            pBuffer = (char*)realloc(pBuffer, lastLen + len);
            if (pBuffer == NULL)
            {
                res = MGA_API_OUT_OF_MEMORY;
                goto exit;
            }
            memcpy(pBuffer + lastLen, buf, len);

            lastLen += len;
            if (contentLength <= 0)
                stop = 1;
        }

    } while (stop == 0);

    mbedtls_ssl_close_notify(&ssl);

    *ppData = (UBX_U1*)pBuffer;
    *piSize = (UBX_I4)lastLen;

exit:
    mbedtls_net_free(&server_fd);

    mbedtls_x509_crt_free(&cacert);
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    return res;
}

static UBX_U1 checkForHTTPS(const char* pServer)
{
    const char* pHttpsTxt = "https";
    const char* pHttpsTxtUpper = "HTTPS";
    const char* pContentType1 = strstr(pServer, pHttpsTxt);
    if (!pContentType1)
        pContentType1 = strstr(pServer, pHttpsTxtUpper);
    if (!pContentType1)
    {
        return 0;
    }

    return 1;
}
#endif //USE_SSL

// This function is MODIFIED from the libMga original to accept a NULL pText.
static void commaToPoint(char* pText)
{
    if (pText != NULL)
    {
        while (*pText)
        {
            if (*pText == ',')
                *pText = '.';
            pText++;
        }
    }
}

// Internal legacy aiding server support
static void legacyAidingCheckMessage(const UBX_U1* pData, UBX_U4 iSize)
{
    U_ASSERT(s_aidingSrvActive == true);
    U_ASSERT(s_pAidingData != NULL);
    U_ASSERT(s_aidingDataSize > 0);

    const UBX_U1 alpDataRqst[] = { UBX_SIG_PREFIX_1, UBX_SIG_PREFIX_2, UBX_CLASS_AID, UBX_AID_ALPSRV };

    // check if this is an ALP data request
    if ((iSize > 14) && (memcmp(pData, alpDataRqst, sizeof(alpDataRqst)) == 0))
    {
        LegacyAidingRequestHeader *pAidingRequestHeader = (LegacyAidingRequestHeader*)(pData + UBX_MSG_PAYLOAD_OFFSET);
        if ((iSize - UBX_MSG_FRAME_SIZE) >= pAidingRequestHeader->idSize)
        {
            if (pAidingRequestHeader->type != 0xFF)
            {
                // Handle aiding data request
                if (s_pEvtInterface->evtProgress)
                {
                    s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_LEGACY_AIDING_SERVER_REQUEST_RECEIVED,
                                                 s_pCallbackContext,
                                                 pAidingRequestHeader,
                                                 iSize - UBX_MSG_FRAME_SIZE);
                }
                legacyAidingRequestData(pAidingRequestHeader);
            }
            else
            {
                // Update aiding data
                LegacyAidingUpdateDataHeader *pAidingUpdateHeader = (LegacyAidingUpdateDataHeader *)pAidingRequestHeader;
                U_ASSERT((iSize - UBX_MSG_FRAME_SIZE) == sizeof(LegacyAidingUpdateDataHeader) + (pAidingUpdateHeader->size * 2));

                if (s_pEvtInterface->evtProgress)
                {
                    s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_LEGACY_AIDING_SERVER_UPDATE_RECEIVED,
                                                 s_pCallbackContext,
                                                 pAidingUpdateHeader,
                                                 iSize - UBX_MSG_FRAME_SIZE);
                }
                legacyAidingUpdateData(pAidingUpdateHeader);
            }
        }
    }
}

static void legacyAidingRequestData(const LegacyAidingRequestHeader *pAidingRequestHeader)
{
    U_ASSERT(s_aidingSrvActive == true);
    U_ASSERT(s_pAidingData != NULL);
    U_ASSERT(s_aidingDataSize > 0);
    U_ASSERT(pAidingRequestHeader != NULL);

    UBX_U4 msgReplySize = 0;
    UBX_U1 *pRqstDataReplyMsg = NULL;
    LegacyAidingRequestHeader *pId = NULL;

    UBX_U4 ofs = pAidingRequestHeader->ofs * 2;
    UBX_U4 dataSize = pAidingRequestHeader->size * 2;

    if ((ofs < s_aidingDataSize) && (dataSize > 0))
    {
        if (ofs + dataSize >= s_aidingDataSize)
        {
            if (ofs >= s_aidingDataSize)
            {
                ofs = s_aidingDataSize - 1;
            }
            // - Just send all we've got starting at the offset
            dataSize = s_aidingDataSize - ofs;
        }

        // Build the aiding request response message
        msgReplySize = UBX_MSG_FRAME_SIZE + pAidingRequestHeader->idSize + dataSize;
        pRqstDataReplyMsg = (UBX_U1 *)pUPortMalloc(msgReplySize);
        if (pRqstDataReplyMsg)
        {
            pId = (LegacyAidingRequestHeader*)(pRqstDataReplyMsg + UBX_MSG_PAYLOAD_OFFSET);

            // Copy the aiding request header
            U_ASSERT(pAidingRequestHeader->idSize == sizeof(LegacyAidingRequestHeader));
            memcpy(pId, pAidingRequestHeader, pAidingRequestHeader->idSize);

            // Update the aiding header
            pId->fileId = s_alpfileId;
            pId->dataSize = (UBX_U2)dataSize;

            // Copy the requested aiding data into the response payload
            memcpy(pRqstDataReplyMsg + UBX_MSG_PAYLOAD_OFFSET + pAidingRequestHeader->idSize, &s_pAidingData[ofs], dataSize);

            // Fill in the UBX message header
            UbxMsgHeader *pUbxMsg = (UbxMsgHeader *)pRqstDataReplyMsg;
            pUbxMsg->header1 = UBX_SIG_PREFIX_1;
            pUbxMsg->header2 = UBX_SIG_PREFIX_2;
            pUbxMsg->msgClass = UBX_CLASS_AID;
            pUbxMsg->msgId = UBX_AID_ALPSRV;
            pUbxMsg->payloadLength = (UBX_U2)(pAidingRequestHeader->idSize + dataSize);

            addChecksum(&pRqstDataReplyMsg[2], msgReplySize - 4);
            U_ASSERT(validChecksum(&pRqstDataReplyMsg[2], msgReplySize - 4));

            // Send the message
            s_pEvtInterface->evtWriteDevice(s_pCallbackContext, pRqstDataReplyMsg, msgReplySize);
        }
        else
        {
            // No memory
            if (s_pEvtInterface->evtProgress)
            {
                s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_LEGACY_AIDING_REQUEST_FAILED_NO_MEMORY,
                                             s_pCallbackContext,
                                             pAidingRequestHeader,
                                             sizeof(LegacyAidingRequestHeader));
            }
        }
    }

    if (s_pEvtInterface->evtProgress)
    {
        s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_LEGACY_AIDING_SERVER_REQUEST_COMPLETED,
                                     s_pCallbackContext,
                                     pId,
                                     pId ? msgReplySize - UBX_MSG_FRAME_SIZE : 0);
    }
    uPortFree(pRqstDataReplyMsg);
}

static void legacyAidingUpdateData(const LegacyAidingUpdateDataHeader *pLegacyAidingUpdateHeader)
{
    U_ASSERT(s_aidingSrvActive == true);
    U_ASSERT(s_pAidingData != NULL);
    U_ASSERT(s_aidingDataSize > 0);
    U_ASSERT(pLegacyAidingUpdateHeader != NULL);

    UBX_U4 ofs = pLegacyAidingUpdateHeader->ofs * 2;
    UBX_U4 dataSize = pLegacyAidingUpdateHeader->size * 2;

    if (pLegacyAidingUpdateHeader->fileId == s_alpfileId)    // Update applies to present data
    {
        if (dataSize > 0)
        {
            // There is some data to update
            if (ofs + dataSize >= s_aidingDataSize)
            {
                if (ofs >= s_aidingDataSize)
                {
                    ofs = s_aidingDataSize - 1;
                }
                // - Just send all we've got starting at the offset
                dataSize = s_aidingDataSize - ofs;
            }

            // Overwrite the original aiding data
            memcpy(&s_pAidingData[ofs], &pLegacyAidingUpdateHeader[1], dataSize);
        }
    }
    else
    {
        // Wrong fileId
        if (s_pEvtInterface->evtProgress)
        {
            s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_LEGACY_AIDING_REQUEST_FAILED_ID_MISMATCH,
                                         s_pCallbackContext,
                                         pLegacyAidingUpdateHeader,
                                         sizeof(LegacyAidingUpdateDataHeader));
        }
    }

    // All done
    if (s_pEvtInterface->evtProgress)
    {
        s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_LEGACY_AIDING_SERVER_UPDATE_COMPLETED,
                                     s_pCallbackContext,
                                     NULL,
                                     0);
    }
}

static int checkValidAidDays(const int *array, size_t size, int value)
{
    if (value <= 0)
        return 0;

    while (size --)
    {
        if (*array++ == value)
        {
            return value;
        }
    }

    return DEFAULT_AID_DAYS;
}

static int checkValidMgaDays(int value)
{
    if (value <= 0)
        return 0;

    if (value <= MAX_MGA_DAYS)
    {
        return value;
    }

    return DEFAULT_MGA_DAYS;
}

// MODIFIED: setDaysRequestParameter() is no longer used (it is replaced by concatNumberWithBuffer()).
#if 0
static void setDaysRequestParameter(UBX_CH* pBuffer, int nrOfDays)
{
    char numberBuffer[20];
    strcat(pBuffer, ";days=");
    sprintf(numberBuffer, "%d", nrOfDays);
    strcat(pBuffer, numberBuffer);
}
#endif // Not required

// MODIFIED: reportError() removed, not required
#if 0
static void reportError(MgaServiceErrors errorType, const char* errorMessage, UBX_U4 httpRc)
{
    EvtInfoServiceError serviceErrorInfo;
    memset(&serviceErrorInfo, 0, sizeof(serviceErrorInfo));
    serviceErrorInfo.errorType = errorType;
    serviceErrorInfo.httpRc = httpRc;
    strncpy(serviceErrorInfo.errorMessage, errorMessage, sizeof(serviceErrorInfo.errorMessage) - 1);
    U_ASSERT(strlen(serviceErrorInfo.errorMessage) < sizeof(serviceErrorInfo.errorMessage) - 1);

    if (s_pEvtInterface->evtProgress)
    {
        s_pEvtInterface->evtProgress(MGA_PROGRESS_EVT_SERVICE_ERROR, s_pCallbackContext, &serviceErrorInfo, sizeof(serviceErrorInfo));
    }
}
#endif // Not required

// MODIFIED: the functions below are added to the original libMga.

// This function is used by the MODIFIED mgaBuildOnlineRequestParams() and mgaBuildOfflineRequestParams().
static void concatWithBuffer(UBX_CH* pBuffer, UBX_I4 iSize, const char* pString, int stringLen, MGA_API_RESULT* pResult, int* pEncodedMessageLength)
{
    if (pString != NULL)
    {
        if (*pResult == MGA_API_OK)
        {
            if ((iSize == 0) || (iSize - *pEncodedMessageLength >= stringLen))
            {
                if (pBuffer != NULL)
                    strcat(pBuffer, pString);
            }
            else
            {
                *pResult = MGA_API_OUT_OF_MEMORY;
            }
        }
        *pEncodedMessageLength += stringLen;
    }
}

// This function is used by the MODIFIED mgaBuildOnlineRequestParams() and mgaBuildOfflineRequestParams().
static void concatNumberWithBuffer(UBX_CH* pBuffer, UBX_I4 iSize, const char* pPrefix, int prefixLen, int number, int fractionalDigits, MGA_API_RESULT* pResult, int* pEncodedMessageLength)
{
    // buffer big enough for  xxxx.yyyyyyy or INT_MAX with a sign
    char numberBuffer[16];
    int stringLen;
    int whole;
    int fraction = 0;

    concatWithBuffer(pBuffer, iSize, pPrefix, prefixLen, pResult, pEncodedMessageLength);
    numberToParts(number, fractionalDigits, &whole, &fraction);
    if (fraction > 0)
        stringLen = snprintf(numberBuffer, sizeof(numberBuffer), "%d.%07d",  whole, fraction);
    else
        stringLen = snprintf(numberBuffer, sizeof(numberBuffer), "%d",  whole);
    if (stringLen > 0)
        concatWithBuffer(pBuffer, iSize, numberBuffer, stringLen, pResult, pEncodedMessageLength);
}

// This function is used by concatNumberWithBuffer().
static void numberToParts(int number, int fractionalDigits, int* pWhole, int* pFraction)
{
    bool negative = false;
    int tens = 1;

    // Deal with the sign
    if (number < 0) {
        number = -number;
        negative = true;
    }
    for (int x = 0; x < fractionalDigits; x++)
        tens *= 10;
    *pWhole = number / tens;
    *pFraction = number % tens;
    if (negative)
        *pWhole = -*pWhole;
}