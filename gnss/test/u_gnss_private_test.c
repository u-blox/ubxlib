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
 * @brief Tests for the private GNSS API; these functions are
 * generally tested implicitly since they are called through everything
 * else however a few need special attention here.
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the U_PORT_TEST_FUNCTION()
 * macro.
 */

# ifdef U_CFG_OVERRIDE
#  include "u_cfg_override.h" // For a customer's configuration override
# endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // strlen()/strncpy()
#include "stdlib.h"    // rand()
#include "ctype.h"     // isprint()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_ubx_protocol.h"

#include "u_port_clib_platform_specific.h" // in some cases rand()
#include "u_port_heap.h"
#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h" // Needed by u_gnss_private.h

#include "u_ringbuffer.h"

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss.h"
#include "u_gnss_private.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_GNSS_PRIVATE_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#ifndef U_GNSS_PRIVATE_TEST_RUBBISH_ROOM_BYTES
/** The amount of extra space to include in the test message buffer
 * to allow insertion of random data either side of the actual
 * message.
  */
# define U_GNSS_PRIVATE_TEST_RUBBISH_ROOM_BYTES 10
#endif

#ifndef U_GNSS_PRIVATE_TEST_NUM_LOOPS
/** How many times to go around the randomised decode test loops.
 */
# define U_GNSS_PRIVATE_TEST_NUM_LOOPS 1000
#endif

#ifndef U_GNSS_PRIVATE_TEST_NMEA_SENTENCE_MAX_LENGTH_BYTES
/** The maximum length of an NMEA message/sentence, including the
 * $ on the front and the CR/LF on the end.
 */
# define U_GNSS_PRIVATE_TEST_NMEA_SENTENCE_MAX_LENGTH_BYTES 82
#endif

/** The length of the NMEA data, when formed into an NMEA string,
 * of the first entry in gNmeaTestMessage[].
 */
#define U_GNSS_PRIVATE_TEST_NMEA_MESSAGE_0_LENGTH 72

#ifndef U_GNSS_PRIVATE_TEST_RINGBUFFER_SIZE
/** The size of ring buffer to use in the private GNSS tests.
 */
# define U_GNSS_PRIVATE_TEST_RINGBUFFER_SIZE 2048
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Struct to hold an NMEA test item, all null-terminated strings.
 */
typedef struct {
    const char *pTalkerSentenceStr;
    const char *pBodyStr;
    const char *pChecksumHexStr;
} uGnssPrivateTestNmea_t;

/** Struct to hold a pointer to some NMEA test data, a matching
 * talker/sentence and the expected outcome from ID matching.
 */
typedef struct {
    const uGnssPrivateTestNmea_t *pNmea;
    char talkerSentenceStr[U_GNSS_NMEA_MESSAGE_MATCH_LENGTH_CHARACTERS + 10];
    const int32_t result;
} uGnssPrivateTestNmeaMatch_t;

/** Struct to hold a pointer to some RTCM test data and a matching ID.
 */
typedef struct {
    const char *pRtcm;
    size_t rtcmSize;
    uint16_t id;
} uGnssPrivateTestRtcmMatch_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** A place to hook the linear buffer that forms the basis of the ring buffer.
 */
static char *gpLinearBuffer = NULL;

/** A place to hook a message buffer.
 */
static char *gpBuffer = NULL;

/** The ring buffer to use when testing.
 */
static uRingBuffer_t gRingBuffer = {0};

/** A place to hook a message body, used for UBX-format message testing.
 */
static char *gpBody = NULL;

# ifndef __ZEPHYR__

/** Some sample NMEA message strings, taken from
 * https://en.wikipedia.org/wiki/NMEA_0183.
 */
static const uGnssPrivateTestNmea_t gNmeaTestMessage[] = {
    // This first entry is also referenced by gTalkerSentenceMatch;
    // its length, when formed into an NMEA message, must be
    // U_GNSS_PRIVATE_TEST_NMEA_MESSAGE_0_LENGTH
    {"GPGGA", "092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,", "76"},
    {"GPGSA", "A,3,10,07,05,02,29,04,08,13,,,,,1.72,1.03,1.38", "0A"},
    {"GPGSV", "3,1,11,10,63,137,17,07,61,098,15,05,59,290,20,08,54,157,30", "70"},
    {"GPGSV", "3,2,11,02,39,223,19,13,28,070,17,26,23,252,,04,14,186,14", "79"},
    {"GPGSV", "3,3,11,29,09,301,24,16,09,020,,36,,,", "76"},
    {"GPRMC", "092750.000,A,5321.6802,N,00630.3372,W,0.02,31.66,280511,,,A", "43"},
    {"GPGGA", "092751.000,5321.6802,N,00630.3371,W,1,8,1.03,61.7,M,55.3,M,,", "75"},
    {"GPGSA", "A,3,10,07,05,02,29,04,08,13,,,,,1.72,1.03,1.38", "0A"},
    {"GPGSV", "3,1,11,10,63,137,17,07,61,098,15,05,59,290,20,08,54,157,30", "70"},
    {"GPGSV", "3,2,11,02,39,223,16,13,28,070,17,26,23,252,,04,14,186,15", "77"},
    {"GPGSV", "3,3,11,29,09,301,24,16,09,020,,36,,,", "76"},
    {"GPRMC", "092751.000,A,5321.6802,N,00630.3371,W,0.06,31.66,280511,,,A", "45"}
};

/** Some talker/sentence ID match data; the first entry of gNmeaTestMessage,
 * which will be "$GPGGA,0927..." when formed into a full NMEA string,
 * is referenced here.
 */
static uGnssPrivateTestNmeaMatch_t gTalkerSentenceMatch[] = {
    {&(gNmeaTestMessage[0]), "GPGGA", U_GNSS_PRIVATE_TEST_NMEA_MESSAGE_0_LENGTH},
    {&(gNmeaTestMessage[0]), "?PGGA", U_GNSS_PRIVATE_TEST_NMEA_MESSAGE_0_LENGTH},
    {&(gNmeaTestMessage[0]), "G?GGA", U_GNSS_PRIVATE_TEST_NMEA_MESSAGE_0_LENGTH},
    {&(gNmeaTestMessage[0]), "GP?GA", U_GNSS_PRIVATE_TEST_NMEA_MESSAGE_0_LENGTH},
    {&(gNmeaTestMessage[0]), "GPG?A", U_GNSS_PRIVATE_TEST_NMEA_MESSAGE_0_LENGTH},
    {&(gNmeaTestMessage[0]), "GPGG?", U_GNSS_PRIVATE_TEST_NMEA_MESSAGE_0_LENGTH},
    {&(gNmeaTestMessage[0]), "?PGG?", U_GNSS_PRIVATE_TEST_NMEA_MESSAGE_0_LENGTH},
    {&(gNmeaTestMessage[0]), "?P?G?", U_GNSS_PRIVATE_TEST_NMEA_MESSAGE_0_LENGTH},
    {&(gNmeaTestMessage[0]), "?????", U_GNSS_PRIVATE_TEST_NMEA_MESSAGE_0_LENGTH},
    {&(gNmeaTestMessage[0]), "GPGGA?", (int32_t) U_ERROR_COMMON_TIMEOUT},
    {&(gNmeaTestMessage[0]), "?GPGGA", (int32_t) U_ERROR_COMMON_TIMEOUT},
    {&(gNmeaTestMessage[0]), "X????", (int32_t) U_ERROR_COMMON_TIMEOUT},
    {&(gNmeaTestMessage[0]), "????X", (int32_t) U_ERROR_COMMON_TIMEOUT},
    {&(gNmeaTestMessage[0]), "??X??", (int32_t) U_ERROR_COMMON_TIMEOUT},
    {&(gNmeaTestMessage[0]), "GPGGA?", (int32_t) U_ERROR_COMMON_TIMEOUT}
};

/** Some sample RTCM messages, taken from
 * https://cdn.sparkfun.com/assets/5/3/8/5/7/Example_RTCM_Binary_Output.txt.
 */
static const uGnssPrivateTestRtcmMatch_t gRtcmTestMessage[] = {
    {
        "\xD3\x00\x99\x43\x50\x00\x28\x63\xF7\x46\x00\x00\x00\x25\x0C\x94"
        "\x80\x00\x00\x00\x20\x00\x00\x00\x7F\xD2\x14\x51\xD0\xD1\x53\x52"
        "\xD4\x54\x00\x00\x00\x00\x00\x41\x9D\x22\x89\x8A\x3C\x9F\xCF\xDD"
        "\x4C\xA3\x05\x43\xD7\x8F\x94\x00\x6D\xFF\xE8\x19\xF0\x49\xFD\x20"
        "\x0A\xB4\x7B\xF5\x80\x9D\x4B\xD2\x93\x4C\x9E\x6F\xF1\xBD\xC5\xE8"
        "\x04\xF0\xC4\xCA\xE9\x90\x62\xDA\x81\xF4\xF3\xBF\x94\xEF\xFF\x67"
        "\x8D\x43\x45\xDC\x7F\xE5\x52\x81\x66\xD3\x03\x99\xFD\x03\x22\xE9"
        "\x81\x53\x44\x24\xA9\xC2\x70\x97\x27\x18\x3E\x26\xA2\x1E\x40\x06"
        "\x21\x48\x66\x17\x85\x81\x50\x5C\x12\x04\xE0\x8D\x9E\xDB\x7F\xE9"
        "\xD1\x4F\x57\xD9\x4F\x4F\x24\x27\xEA\xC0\x63\x00\x52\xBC\xB7",
        159, 1077
    },
    {
        "\xD3\x00\x6D\x43\xF0\x00\x41\xC3\x2C\x04\x00\x00\x07\x00\x0E\x00"
        "\x00\x00\x00\x00\x20\x00\x00\x00\x7E\x9C\x82\x86\x98\x80\x89\x07"
        "\x93\x68\x32\xAA\x5F\xDF\x2F\x52\xE6\x3E\xA9\x7D\xCC\x0A\xE7\x9C"
        "\xBF\x71\x04\x21\xFA\xDF\xD9\x77\x14\x17\x50\x1B\x75\xFB\xA4\x4F"
        "\xA7\x57\xD3\xFE\x69\x8D\xE2\xEA\xE2\x06\xC5\xA7\xE5\xD8\xBD\xE7"
        "\xA3\xDA\x19\x56\x19\x3F\x4D\x31\xEA\xEC\xDA\x46\x20\x52\x11\x85"
        "\x41\x00\x58\x17\x86\x8A\xCB\xD2\x21\x89\x74\x05\xF6\x07\x07\x1E"
        "\xC4\x38\xC4",
        115, 1087
    },
    {
        "\xD3\x00\x13\x3E\xD0\x00\x03\x3C\xFF\x55\x48\x17\xB5\x02\xDE\xCA"
        "\xBC\x09\x80\x35\x10\x31\x09\xFA\x3C",
        25, 1005
    },
    {
        "\xD3\x00\x99\x43\x50\x00\x28\x64\x06\xE6\x00\x00\x00\x25\x0C\x94"
        "\x80\x00\x00\x00\x20\x00\x00\x00\x7F\xD2\x14\x51\xD0\xD1\x53\x52"
        "\xD4\x54\x00\x00\x00\x00\x00\x40\x9D\xA2\x99\x8A\x3C\x9F\x8F\xCD"
        "\x58\xA1\x05\x43\xD7\x7F\x94\x00\x6D\xFF\xE8\x19\xF0\x49\xFD\x20"
        "\x0A\xB4\x2F\x43\x07\x88\xD8\xC7\x2B\x80\xB0\xC3\xFD\xF7\x38\x76"
        "\xBC\xEB\xE9\x3B\x70\x5B\xB4\xF3\x00\xC0\xF1\x81\x51\x3C\x43\x36"
        "\xE6\x40\x4A\x5C\x00\x16\x15\xFE\x0B\x38\xC3\x85\x29\xBF\x41\xCC"
        "\x7E\x97\xAB\x24\xA9\xC2\x70\x97\x27\x18\x3E\x27\xA2\x1E\x40\x06"
        "\x21\x40\x66\x17\x85\x81\x50\x5E\x12\x05\x00\x82\x33\xFD\xD7\x9E"
        "\x60\xE1\x76\xE3\x4D\x5F\x9E\x74\xE9\x3A\x5A\x7C\x90\x0D\xE3",
        159, 1077
    },
    {
        "\xD3\x00\x6D\x43\xF0\x00\x41\xC3\x3B\xA4\x00\x00\x07\x00\x0E\x00"
        "\x00\x00\x00\x00\x20\x00\x00\x00\x7E\x9C\x82\x86\x98\x80\x89\x07"
        "\x93\x68\x33\xAA\x7F\xD7\x33\x53\x66\x1E\xA9\x7D\xCC\x0A\xE7\x9C"
        "\xBF\x71\x04\x1E\x53\x35\xB8\xEB\xA5\xE7\x19\x66\xFD\x6F\x9E\xF7"
        "\xFA\x7F\x12\x14\x84\xF5\xE0\xC8\x39\xFA\xB2\xDC\x13\xCB\xF5\xE6"
        "\x43\x77\xE5\xEE\x59\x3F\x4D\x31\xEA\xEE\xDA\x46\x20\x52\x11\x85"
        "\x40\xF8\x58\x17\x86\x79\xDB\x52\xE0\x58\xF2\x5A\xEF\x19\x4A\x9C"
        "\xDC\x77\x74",
        115, 1087
    },
    {
        "\xD3\x00\x13\x3E\xD0\x00\x03\x3C\xFF\x55\x48\x17\xB5\x02\xDE\xCA"
        "\xBC\x09\x80\x35\x10\x31\x09\xFA\x3C",
        25, 1005
    },
    {
        "\xD3\x00\x99\x43\x50\x00\x28\x64\x16\x86\x00\x00\x00\x25\x0C\x94"
        "\x80\x00\x00\x00\x20\x00\x00\x00\x7F\xD2\x14\x51\xD0\xD1\x53\x52"
        "\xD4\x54\x00\x00\x00\x00\x00\x3F\x9E\x22\xB9\x8A\x3C\x9F\x0F\xBD"
        "\x60\x9F\x05\x43\xD7\x7F\x94\x00\x6D\xFF\xE8\x19\xF0\x49\xFD\x20"
        "\x0A\xB7\xE2\xF8\xCE\x79\xF3\xBB\x45\x74\xBD\xA0\x0A\x81\x4B\x05"
        "\xE0\xE6\xAE\x0B\xE8\xE7\x06\x94\xFF\x8C\xFF\x43\x0D\x92\xBF\x06"
        "\xA0\x3D\x4F\x04\x40\x47\x16\x42\xAF\xDE\x43\x70\xD7\x03\x60\x79"
        "\x7B\xDC\x05\x24\xA9\xC2\x70\x97\x27\x28\x3E\x27\xA2\x1E\x40\x06"
        "\x21\x40\x68\x17\x85\x81\x50\x5E\x11\x85\x00\x7E\xB7\xF7\x1F\x72"
        "\xC0\xB3\xF6\x67\x0C\x66\x9B\x1D\xEA\x18\x59\x54\x21\x3F\xE8",
        159, 1077
    },
    {
        "\xD3\x00\x6D\x43\xF0\x00\x41\xC3\x4B\x44\x00\x00\x07\x00\x0E\x00"
        "\x00\x00\x00\x00\x20\x00\x00\x00\x7E\x9C\x82\x86\x98\x80\x89\x07"
        "\x93\x68\x35\x2A\x9F\xCF\x39\x53\xE5\xDE\xA9\x7D\xCA\x0A\xDF\x9C"
        "\xBF\x71\x04\x1F\xAB\x7D\x58\x5E\x37\xB6\xE9\xE2\x4E\x27\x99\x53"
        "\x7D\xA6\x0B\xEA\x9D\xDF\xDE\xA8\x7F\xEE\xA3\x3C\x01\xBF\xB9\xE4"
        "\xE9\x9B\xF2\x8A\xA1\x3F\x4D\x31\xEA\xF0\xDA\xC6\x20\x52\x11\x85"
        "\x40\xF8\x58\x17\x86\x8C\xDF\xDF\x49\x58\xF1\xF9\xEA\x02\x43\xE4"
        "\x31\x8A\x5B",
        115, 1087
    },
    {
        "\xD3\x00\x13\x3E\xD0\x00\x03\x3C\xFF\x55\x48\x17\xB5\x02\xDE\xCA"
        "\xBC\x09\x80\x35\x10\x31\x09\xFA\x3C",
        25, 1005
    }
};

#endif // #ifndef __ZEPHYR__

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

# ifndef __ZEPHYR__

// Fill a buffer with safe randomness: avoiding dollar (start of an
// NMEA message) or 0xb5 (start of a UBX-format message) or a
// 0xd3 (start of an RTCM message).
static void fillBufferRand(char *pBuffer, size_t size)
{
    for (size_t x = 0; x < size; x++, pBuffer++) {
        *pBuffer = (char) rand();
        if ((*pBuffer == '$') || (*pBuffer == 0xd3) || (*pBuffer == 0xb5)) {
            *pBuffer = '_';
        }
    }
}

// Assemble an NMEA message from the components into pBuffer
// (which must include room for at least U_GNSS_NMEA_SENTENCE_MAX_LENGTH_BYTES)
// and return the size of the message; NO null termnator is included.
static size_t makeNmeaMessage(char *pBuffer, const char *pTalkerSentenceStr,
                              const char *pBodyStr, const char *pChecksumHexStr)
{
    size_t size;
    size_t y;
    size_t x;

    // +5 for the opening dollar, the comma after the sentence/talker,
    // the * before the checksum and the CRLF on the end
    size = strlen(pTalkerSentenceStr) + strlen(pBodyStr) + strlen(pChecksumHexStr) + 5;
    y = size;

    // Assemble the message, dollar first
    *pBuffer = '$';
    pBuffer++;
    y--;
    // Add the talker/sentence string
    x = strlen(pTalkerSentenceStr);
    strncpy(pBuffer, pTalkerSentenceStr, y);
    pBuffer += x;
    y -= x;
    // Add the comma afterwards
    *pBuffer = ',';
    pBuffer++;
    y--;
    // Add the message body string
    x = strlen(pBodyStr);
    strncpy(pBuffer, pBodyStr, y);
    pBuffer += x;
    y -= x;
    // Add the star that separates the body from the hex checksum string
    *pBuffer = '*';
    pBuffer++;
    y--;
    // Add the hex checksum string
    x = strlen(pChecksumHexStr);
    strncpy(pBuffer, pChecksumHexStr, y);
    pBuffer += x;
    // Add the CRLF on the end
    *pBuffer = '\r';
    pBuffer++;
    *pBuffer = '\n';
    pBuffer++;

    return size;
}

// Call uRingBufferParseHandle() with the given parameters and
// return true if good, else false; NMEA flavour.
static bool checkDecodeNmea(uRingBuffer_t *pRingBuffer, int32_t readHandle,
                            const char *pBuffer, size_t bufferSize,
                            char *pTalkerSentenceStr, int32_t expectedReturnValue)
{
    uGnssPrivateMessageId_t msgId = {0};
    bool passNotFail = true;
    int32_t errorCodeOrSize;

    msgId.type = U_GNSS_PROTOCOL_NMEA;
    if (pTalkerSentenceStr != NULL) {
        strncpy(msgId.id.nmea, pTalkerSentenceStr, sizeof(msgId.id.nmea));
    }

    // Add pBuffer to the ring buffer and attempt to decode the message
    U_PORT_TEST_ASSERT(uRingBufferAdd(pRingBuffer, pBuffer, bufferSize));
    errorCodeOrSize = uGnssPrivateStreamDecodeRingBuffer(pRingBuffer, readHandle, &msgId);
    if (errorCodeOrSize != expectedReturnValue) {
        passNotFail = false;
        uPortLog(U_TEST_PREFIX "decoding buffer \"");
        for (size_t x = 0; x < bufferSize; x++) {
            if (!isprint((int32_t) *pBuffer)) {
                uPortLog("[%02x]", (unsigned char) *pBuffer);
            } else {
                uPortLog("%c", *pBuffer);
            }
            pBuffer++;
        }
        uPortLog("\" (%d bytes)\n", bufferSize);
        uPortLog(U_TEST_PREFIX "with talker/sentence ");
        if (pTalkerSentenceStr == NULL) {
            uPortLog("NULL");
        } else {
            uPortLog("\"%s\"", pTalkerSentenceStr);
        }
        uPortLog(", failed to meet expectations:\n");
        U_TEST_PRINT_LINE("expected return value %d, actual return value %d.",
                          expectedReturnValue, errorCodeOrSize);
        U_PORT_TEST_ASSERT(msgId.type == U_GNSS_PROTOCOL_NMEA);
    }

    // Remove the message from the ring buffer
    uRingBufferReadHandle(pRingBuffer, readHandle, NULL, bufferSize);

    return passNotFail;
}

// Call uRingBufferParseHandle() with the given parameters and
// return true if good, else false; RTCM flavour.
static bool checkDecodeRtcm(uRingBuffer_t *pRingBuffer, int32_t readHandle,
                            const char *pBuffer, size_t bufferSize,
                            uint16_t id, int32_t expectedReturnValue)
{
    uGnssPrivateMessageId_t msgId = {0};
    bool passNotFail = true;
    int32_t errorCodeOrSize;

    msgId.type = U_GNSS_PROTOCOL_RTCM;
    msgId.id.rtcm = id;

    // Add pBuffer to the ring buffer and attempt to decode the message
    U_PORT_TEST_ASSERT(uRingBufferAdd(pRingBuffer, pBuffer, bufferSize));
    errorCodeOrSize = uGnssPrivateStreamDecodeRingBuffer(pRingBuffer, readHandle, &msgId);
    if (errorCodeOrSize != expectedReturnValue) {
        passNotFail = false;
        uPortLog(U_TEST_PREFIX "decoding buffer \"");
        for (size_t x = 0; x < bufferSize; x++) {
            uPortLog("[%02x]", (unsigned char) *pBuffer);
            pBuffer++;
        }
        uPortLog("\" (%d bytes)\n", bufferSize);
        uPortLog(U_TEST_PREFIX "with ID 0x%04x", id);
        uPortLog(", failed to meet expectations:\n");
        U_TEST_PRINT_LINE("expected return value %d, actual return value %d.",
                          expectedReturnValue, errorCodeOrSize);
        U_PORT_TEST_ASSERT(msgId.type == U_GNSS_PROTOCOL_RTCM);
    }

    // Remove the message from the ring buffer
    uRingBufferReadHandle(pRingBuffer, readHandle, NULL, bufferSize);

    return passNotFail;
}

// Call uRingBufferParseHandle() with the given parameters and
// return true if good, else false; UBX flavour.
static bool checkDecodeUbx(uRingBuffer_t *pRingBuffer, int32_t readHandle,
                           const char *pBuffer, size_t bufferSize,
                           uint8_t messageClass, uint8_t messageId,
                           int32_t expectedReturnValue)
{
    uGnssPrivateMessageId_t msgId = {0};
    bool passNotFail = true;
    int32_t errorCodeOrSize;

    msgId.type = U_GNSS_PROTOCOL_UBX;
    msgId.id.ubx = (((uint16_t) messageClass) << 8) + messageId;

    // Add pBuffer to the ring buffer and attempt to decode the message
    U_PORT_TEST_ASSERT(uRingBufferAdd(pRingBuffer, pBuffer, bufferSize));
    errorCodeOrSize = uGnssPrivateStreamDecodeRingBuffer(pRingBuffer, readHandle, &msgId);
    if (errorCodeOrSize != expectedReturnValue) {
        passNotFail = false;
        uPortLog(U_TEST_PREFIX "decoding buffer \"");
        for (size_t x = 0; x < bufferSize; x++) {
            uPortLog("[%02x]", (unsigned char) *pBuffer);
            pBuffer++;
        }
        uPortLog("\" (%d bytes)\n", bufferSize);
        uPortLog(U_TEST_PREFIX "with class 0x%02x, ID 0x%02x", messageClass, messageId);
        uPortLog(", failed to meet expectations:\n");
        U_TEST_PRINT_LINE("expected return value %d, actual return value %d.",
                          expectedReturnValue, errorCodeOrSize);
        U_PORT_TEST_ASSERT(msgId.type == U_GNSS_PROTOCOL_UBX);
    }

    // Remove the message from the ring buffer
    uRingBufferReadHandle(pRingBuffer, readHandle, NULL, bufferSize);

    return passNotFail;
}

#endif // #ifndef __ZEPHYR__

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

# ifndef __ZEPHYR__

/** Test the NMEA message decode function.  Note that we do not run
 * these tests on Zephyr because it has proved pretty much impossible
 * to get Zephyr-on-NRF52 to provide a working rand() function; the
 * maze of KConfig possiblities is just too great for anyone, including
 * Nordic support, to navigate to a successful conclusion in our
 * case; either KConfig errors result or the rand() function causes a
 * memory exception when called. So we gave up.
 *
 * This is not a huge problem as none of the ubxlib operations here are
 * likely to be platform specific in nature, testing on the other
 * platforms should suffice.
 */
U_PORT_TEST_FUNCTION("[gnss]", "gnssPrivateNmea")
{
    // +1 for the null terminator
    char talkerSentenceBuffer[U_GNSS_NMEA_MESSAGE_MATCH_LENGTH_CHARACTERS + 1];
    int32_t readHandle;
    char *pMessage;
    size_t messageSize;
    size_t z;
    int32_t heapUsed;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    U_PORT_TEST_ASSERT(uPortInit() == 0);

    // Allocate memory to use for the ring buffer
    gpLinearBuffer = (char *) pUPortMalloc(U_GNSS_PRIVATE_TEST_RINGBUFFER_SIZE);
    U_PORT_TEST_ASSERT(gpLinearBuffer != NULL);

    // Create a ring buffer from the linear buffer with a single read handle allowed
    U_PORT_TEST_ASSERT(uRingBufferCreateWithReadHandle(&gRingBuffer, gpLinearBuffer,
                                                       U_GNSS_PRIVATE_TEST_RINGBUFFER_SIZE,
                                                       1) == 0);

    // Set this so that the default non-handled read doesn't hold on
    // to data in the ring buffer
    uRingBufferSetReadRequiresHandle(&gRingBuffer, true);

    // Grab a read handle for it
    readHandle = uRingBufferTakeReadHandle(&gRingBuffer);
    U_PORT_TEST_ASSERT(readHandle >= 0);

    // Allocate a buffer to decode from
    gpBuffer = (char *) pUPortMalloc(U_GNSS_PRIVATE_TEST_NMEA_SENTENCE_MAX_LENGTH_BYTES +
                                     U_GNSS_PRIVATE_TEST_RUBBISH_ROOM_BYTES);
    U_PORT_TEST_ASSERT(gpBuffer != NULL);

    // Parse all the test data
    for (size_t x = 0; x < sizeof(gNmeaTestMessage) / sizeof(gNmeaTestMessage[0]); x++) {

        U_TEST_PRINT_LINE("test decoding NMEA message $%s,%s*%s",
                          gNmeaTestMessage[x].pTalkerSentenceStr,
                          gNmeaTestMessage[x].pBodyStr,
                          gNmeaTestMessage[x].pChecksumHexStr);

        // Do this multiple times for good randomness
        for (size_t y = 0; y < U_GNSS_PRIVATE_TEST_NUM_LOOPS; y++) {
            // Fill the buffer with safe randomness
            fillBufferRand(gpBuffer, U_GNSS_PRIVATE_TEST_NMEA_SENTENCE_MAX_LENGTH_BYTES +
                           U_GNSS_PRIVATE_TEST_RUBBISH_ROOM_BYTES);

            // Assemble the message, starting a random distance into the buffer
            z = rand() % U_GNSS_PRIVATE_TEST_RUBBISH_ROOM_BYTES;
            pMessage = gpBuffer + z;
            messageSize = makeNmeaMessage(pMessage, gNmeaTestMessage[x].pTalkerSentenceStr,
                                          gNmeaTestMessage[x].pBodyStr,
                                          gNmeaTestMessage[x].pChecksumHexStr);

            // Decode it with a wild-card message ID first
            U_PORT_TEST_ASSERT(checkDecodeNmea(&gRingBuffer, readHandle, gpBuffer,
                                               U_GNSS_PRIVATE_TEST_NMEA_SENTENCE_MAX_LENGTH_BYTES +
                                               U_GNSS_PRIVATE_TEST_RUBBISH_ROOM_BYTES,
                                               NULL, messageSize));

            // Then with the exact message ID
            strncpy(talkerSentenceBuffer, gNmeaTestMessage[x].pTalkerSentenceStr, sizeof(talkerSentenceBuffer));
            // Ensure terminator
            talkerSentenceBuffer[sizeof(talkerSentenceBuffer) - 1] = 0;
            U_PORT_TEST_ASSERT(checkDecodeNmea(&gRingBuffer, readHandle, gpBuffer,
                                               U_GNSS_PRIVATE_TEST_NMEA_SENTENCE_MAX_LENGTH_BYTES +
                                               U_GNSS_PRIVATE_TEST_RUBBISH_ROOM_BYTES,
                                               talkerSentenceBuffer, messageSize));

            // Then with a partial message ID of random length 1 or more
            z = 1 + (rand() % (strlen(gNmeaTestMessage[x].pTalkerSentenceStr) - 1));
            talkerSentenceBuffer[z] = 0;
            U_PORT_TEST_ASSERT(checkDecodeNmea(&gRingBuffer, readHandle, gpBuffer,
                                               U_GNSS_PRIVATE_TEST_NMEA_SENTENCE_MAX_LENGTH_BYTES +
                                               U_GNSS_PRIVATE_TEST_RUBBISH_ROOM_BYTES,
                                               talkerSentenceBuffer, messageSize));

            // Then with a wrong message ID
            strncpy(talkerSentenceBuffer, gNmeaTestMessage[x].pTalkerSentenceStr, sizeof(talkerSentenceBuffer));
            z = rand() % strlen(talkerSentenceBuffer);
            talkerSentenceBuffer[z] = '_';
            // Ensure terminator
            talkerSentenceBuffer[sizeof(talkerSentenceBuffer) - 1] = 0;
            U_PORT_TEST_ASSERT(checkDecodeNmea(&gRingBuffer, readHandle, gpBuffer,
                                               U_GNSS_PRIVATE_TEST_NMEA_SENTENCE_MAX_LENGTH_BYTES +
                                               U_GNSS_PRIVATE_TEST_RUBBISH_ROOM_BYTES,
                                               talkerSentenceBuffer, U_ERROR_COMMON_TIMEOUT));

            // Then with a broken message
            z = rand() % (messageSize);
            *(pMessage + z) = '_';
            U_PORT_TEST_ASSERT(checkDecodeNmea(&gRingBuffer, readHandle, gpBuffer,
                                               U_GNSS_PRIVATE_TEST_NMEA_SENTENCE_MAX_LENGTH_BYTES +
                                               U_GNSS_PRIVATE_TEST_RUBBISH_ROOM_BYTES,
                                               NULL, U_ERROR_COMMON_TIMEOUT));
        }
        // Some platforms run a task watchdog which might be starved with such
        // a large processing loop: give it a bone
        uPortTaskBlock(U_CFG_OS_YIELD_MS);
    }

    // Check that wild-card matches work
    for (size_t x = 0; x < sizeof(gTalkerSentenceMatch) / sizeof(gTalkerSentenceMatch[0]); x++) {
        U_TEST_PRINT_LINE("test wildcard talker/sentence match %s",
                          gTalkerSentenceMatch[x].talkerSentenceStr);
        makeNmeaMessage(gpBuffer, gTalkerSentenceMatch[x].pNmea->pTalkerSentenceStr,
                        gTalkerSentenceMatch[x].pNmea->pBodyStr,
                        gTalkerSentenceMatch[x].pNmea->pChecksumHexStr);
        U_PORT_TEST_ASSERT(checkDecodeNmea(&gRingBuffer, readHandle, gpBuffer,
                                           U_GNSS_PRIVATE_TEST_NMEA_SENTENCE_MAX_LENGTH_BYTES +
                                           U_GNSS_PRIVATE_TEST_RUBBISH_ROOM_BYTES,
                                           (char *) (gTalkerSentenceMatch[x].talkerSentenceStr),
                                           gTalkerSentenceMatch[x].result));
    }

    // Free memory.
    uPortFree(gpBuffer);
    gpBuffer = NULL;
    uRingBufferDelete(&gRingBuffer);
    uPortFree(gpLinearBuffer);
    gpLinearBuffer = NULL;

    uPortDeinit();

# ifndef __XTENSA__
    // Check for memory leaks
    // TODO: this if'ed out for ESP32 (xtensa compiler) at
    // the moment as there is an issue with ESP32 hanging
    // on to memory in the UART drivers that can't easily be
    // accounted for.
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
# else
    (void) heapUsed;
# endif
}

/** Test the RTCM message decode function; not tested on Zephyr for
 * the same reasons as the test gnssPrivateNmea.
 *
 * This is a pretty minimal test, needs improving.
 */
U_PORT_TEST_FUNCTION("[gnss]", "gnssPrivateRtcm")
{
    const uGnssPrivateTestRtcmMatch_t *pRtcmTest;
    int32_t readHandle;
    char *pMessage;
    size_t bufferSize;
    size_t z;
    int32_t heapUsed;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    U_PORT_TEST_ASSERT(uPortInit() == 0);

    // Allocate memory to use for the ring buffer
    gpLinearBuffer = (char *) pUPortMalloc(U_GNSS_PRIVATE_TEST_RINGBUFFER_SIZE);
    U_PORT_TEST_ASSERT(gpLinearBuffer != NULL);

    // Create a ring buffer from the linear buffer with a single read handle allowed
    U_PORT_TEST_ASSERT(uRingBufferCreateWithReadHandle(&gRingBuffer, gpLinearBuffer,
                                                       U_GNSS_PRIVATE_TEST_RINGBUFFER_SIZE,
                                                       1) == 0);

    // Set this so that the default non-handled read doesn't hold on
    // to data in the ring buffer
    uRingBufferSetReadRequiresHandle(&gRingBuffer, true);

    // Grab a read handle for it
    readHandle = uRingBufferTakeReadHandle(&gRingBuffer);
    U_PORT_TEST_ASSERT(readHandle >= 0);

    // Parse all the test data
    for (size_t x = 0; x < sizeof(gRtcmTestMessage) / sizeof(gRtcmTestMessage[0]); x++) {
        pRtcmTest = &(gRtcmTestMessage[x]);
        bufferSize = pRtcmTest->rtcmSize + U_GNSS_PRIVATE_TEST_RUBBISH_ROOM_BYTES;
        // Allocate a buffer to decode from
        gpBuffer = (char *) pUPortMalloc(bufferSize);
        U_PORT_TEST_ASSERT(gpBuffer != NULL);

        U_TEST_PRINT_LINE("test decoding RTCM message %d (ID %d, %d byte(s)).",
                          x + 1, pRtcmTest->id, pRtcmTest->rtcmSize);

        // Do this multiple times for good randomness
        for (size_t y = 0; y < U_GNSS_PRIVATE_TEST_NUM_LOOPS; y++) {
            // Fill the buffer with safe randomness
            fillBufferRand(gpBuffer, bufferSize);

            // Copy in the message, starting a random distance into the buffer
            pMessage = gpBuffer + (rand() % U_GNSS_PRIVATE_TEST_RUBBISH_ROOM_BYTES);
            memcpy(pMessage, pRtcmTest->pRtcm, pRtcmTest->rtcmSize);

            // Decode it with a wild-card message ID first
            U_PORT_TEST_ASSERT(checkDecodeRtcm(&gRingBuffer, readHandle, gpBuffer, bufferSize,
                                               U_GNSS_RTCM_MESSAGE_ID_ALL, pRtcmTest->rtcmSize));

            // Then with the exact message ID
            U_PORT_TEST_ASSERT(checkDecodeRtcm(&gRingBuffer, readHandle, gpBuffer, bufferSize,
                                               pRtcmTest->id, pRtcmTest->rtcmSize));

            // Then with a wrong message ID
            z = (int32_t) (uint16_t) rand();
            if (z == U_GNSS_RTCM_MESSAGE_ID_ALL) {
                z = ~U_GNSS_RTCM_MESSAGE_ID_ALL;
            }
            if (z == pRtcmTest->id) {
                z++;
            }
            U_PORT_TEST_ASSERT(checkDecodeRtcm(&gRingBuffer, readHandle, gpBuffer, bufferSize,
                                               (uint16_t) z, U_ERROR_COMMON_TIMEOUT));

            // Then with a broken message
            z = rand() % pRtcmTest->rtcmSize;
            *(pMessage + z) = ~*(pMessage + z);
            U_PORT_TEST_ASSERT(checkDecodeRtcm(&gRingBuffer, readHandle, gpBuffer, bufferSize,
                                               U_GNSS_RTCM_MESSAGE_ID_ALL, U_ERROR_COMMON_TIMEOUT));
        }

        // Some platforms run a task watchdog which might be starved with such
        // a large processing loop: give it a bone
        uPortTaskBlock(U_CFG_OS_YIELD_MS);

        // Free memory
        uPortFree(gpBuffer);
        gpBuffer = NULL;
    }

    // Free memory.
    uRingBufferDelete(&gRingBuffer);
    uPortFree(gpLinearBuffer);
    gpLinearBuffer = NULL;

    uPortDeinit();

# ifndef __XTENSA__
    // Check for memory leaks
    // TODO: this if'ed out for ESP32 (xtensa compiler) at
    // the moment as there is an issue with ESP32 hanging
    // on to memory in the UART drivers that can't easily be
    // accounted for.
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
# else
    (void) heapUsed;
# endif
}

/** Test the UBX message decode function; not tested on Zephyr for
 * the same reasons as the test gnssPrivateNmea.
 */
U_PORT_TEST_FUNCTION("[gnss]", "gnssPrivateUbx")
{
    int32_t readHandle;
    char *pMessage;
    size_t bodySize;
    uint8_t messageClass;
    uint8_t messageId;
    size_t bufferSize;
    size_t y;
    int32_t heapUsed;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    U_PORT_TEST_ASSERT(uPortInit() == 0);

    // Allocate memory to use for the ring buffer
    gpLinearBuffer = (char *) pUPortMalloc(U_GNSS_PRIVATE_TEST_RINGBUFFER_SIZE);
    U_PORT_TEST_ASSERT(gpLinearBuffer != NULL);

    // Create a ring buffer from the linear buffer with a single read handle allowed
    U_PORT_TEST_ASSERT(uRingBufferCreateWithReadHandle(&gRingBuffer, gpLinearBuffer,
                                                       U_GNSS_PRIVATE_TEST_RINGBUFFER_SIZE,
                                                       1) == 0);

    // Set this so that the default non-handled read doesn't hold on
    // to data in the ring buffer
    uRingBufferSetReadRequiresHandle(&gRingBuffer, true);

    // Grab a read handle for it
    readHandle = uRingBufferTakeReadHandle(&gRingBuffer);
    U_PORT_TEST_ASSERT(readHandle >= 0);

    // Do this multiple times for good randomness
    for (size_t x = 0; x < U_GNSS_PRIVATE_TEST_NUM_LOOPS; x++) {
        // Create a UBX message with random class, ID and length
        bodySize = rand() % (uRingBufferAvailableSize(&gRingBuffer) - (U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES
                                                                       +
                                                                       U_GNSS_PRIVATE_TEST_RUBBISH_ROOM_BYTES));
        messageClass = (uint8_t) rand();
        if (messageClass == U_GNSS_UBX_MESSAGE_CLASS_ALL) {
            messageClass++;
        }
        messageId = (uint8_t) rand();
        if (messageId == U_GNSS_UBX_MESSAGE_ID_ALL) {
            messageId++;
        }
        // Create a message body, filled with safe randomness
        gpBody = (char *) pUPortMalloc(bodySize);
        U_PORT_TEST_ASSERT(gpBody != NULL);
        fillBufferRand(gpBody, bodySize);

        // Create a buffer filled with safe randomness
        bufferSize = bodySize + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES +
                     U_GNSS_PRIVATE_TEST_RUBBISH_ROOM_BYTES;
        gpBuffer = (char *) pUPortMalloc(bufferSize);
        U_PORT_TEST_ASSERT(gpBuffer != NULL);
        fillBufferRand(gpBuffer, bufferSize);

        // Create the message, starting a random distance into the buffer
        pMessage = gpBuffer + (rand() % U_GNSS_PRIVATE_TEST_RUBBISH_ROOM_BYTES);
        U_PORT_TEST_ASSERT(uUbxProtocolEncode(messageClass, messageId,
                                              (const char *) gpBody, bodySize,
                                              pMessage) == bodySize + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES);
        // Decode it with a wild-card ID first
        U_PORT_TEST_ASSERT(checkDecodeUbx(&gRingBuffer, readHandle, gpBuffer, bufferSize,
                                          U_GNSS_UBX_MESSAGE_CLASS_ALL, U_GNSS_UBX_MESSAGE_ID_ALL,
                                          bodySize + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES));

        // Then with the exact message ID
        U_PORT_TEST_ASSERT(checkDecodeUbx(&gRingBuffer, readHandle, gpBuffer, bufferSize,
                                          messageClass, messageId,
                                          bodySize + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES));

        // Then with a wrong message class
        y = (uint8_t) (messageClass + 1);
        if (y == U_GNSS_UBX_MESSAGE_CLASS_ALL) {
            y = ~U_GNSS_UBX_MESSAGE_CLASS_ALL;
        }
        U_PORT_TEST_ASSERT(checkDecodeUbx(&gRingBuffer, readHandle, gpBuffer, bufferSize,
                                          (uint8_t) y, messageId, U_ERROR_COMMON_TIMEOUT));

        // Then with a wrong message ID
        y = (uint8_t) (messageId + 1);
        if (y == U_GNSS_UBX_MESSAGE_ID_ALL) {
            y = ~U_GNSS_UBX_MESSAGE_ID_ALL;
        }
        U_PORT_TEST_ASSERT(checkDecodeUbx(&gRingBuffer, readHandle, gpBuffer, bufferSize,
                                          messageClass, (uint8_t) y, U_ERROR_COMMON_TIMEOUT));

        // Then with a broken message
        y = rand() % (bodySize + U_UBX_PROTOCOL_OVERHEAD_LENGTH_BYTES);
        *(pMessage + y) = ~*(pMessage + y);
        U_PORT_TEST_ASSERT(checkDecodeUbx(&gRingBuffer, readHandle, gpBuffer, bufferSize,
                                          U_GNSS_UBX_MESSAGE_CLASS_ALL, U_GNSS_UBX_MESSAGE_ID_ALL,
                                          U_ERROR_COMMON_TIMEOUT));

        if ((x % 100) == 0) {
            // Some platforms run a task watchdog which might be starved with such
            // a large processing loop: give it a bone
            uPortTaskBlock(U_CFG_OS_YIELD_MS);
        }

        // Free memory
        uPortFree(gpBody);
        gpBody = NULL;
        uPortFree(gpBuffer);
        gpBuffer = NULL;
    }

    // Free memory.
    uRingBufferDelete(&gRingBuffer);
    uPortFree(gpLinearBuffer);
    gpLinearBuffer = NULL;

    uPortDeinit();

# ifndef __XTENSA__
    // Check for memory leaks
    // TODO: this if'ed out for ESP32 (xtensa compiler) at
    // the moment as there is an issue with ESP32 hanging
    // on to memory in the UART drivers that can't easily be
    // accounted for.
    heapUsed -= uPortGetHeapFree();
    U_TEST_PRINT_LINE("we have leaked %d byte(s).", heapUsed);
    // heapUsed < 0 for the Zephyr case where the heap can look
    // like it increases (negative leak)
    U_PORT_TEST_ASSERT(heapUsed <= 0);
# else
    (void) heapUsed;
# endif
}

#endif // #ifndef __ZEPHYR__

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[gnss]", "gnssPrivateCleanUp")
{
    int32_t x;

    uPortFree(gpBody);
    uPortFree(gpBuffer);
    uRingBufferDelete(&gRingBuffer);
    uPortFree(gpLinearBuffer);

    uGnssDeinit();

    x = uPortTaskStackMinFree(NULL);
    if (x != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_TEST_PRINT_LINE("main task stack had a minimum of %d byte(s)"
                          " free at the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);
    }

    uPortDeinit();

    x = uPortGetHeapMinFree();
    if (x >= 0) {
        U_TEST_PRINT_LINE("heap had a minimum of %d byte(s) free"
                          " at the end of these tests.", x);
        U_PORT_TEST_ASSERT(x >= U_CFG_TEST_HEAP_MIN_FREE_BYTES);
    }
}

// End of file
