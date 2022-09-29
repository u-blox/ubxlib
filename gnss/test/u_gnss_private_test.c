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
# if 0

// This is currently not used, will be replaced with new message parsing tests shortly...

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

#include "u_port_clib_platform_specific.h" // in some cases rand()
#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h" // Needed by u_gnss_private.h

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

/** The length of the NMEA data, when formed into an NMEA string,
 * of the first entry in gNmeaTestMessage[].
 */
#define U_GNSS_PRIVATE_TEST_NMEA_MESSAGE_0_LENGTH 72

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
} uGnssPrivateTestMatch_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

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
static uGnssPrivateTestMatch_t gTalkerSentenceMatch[] = {
    {&(gNmeaTestMessage[0]), "GPGGA", U_GNSS_PRIVATE_TEST_NMEA_MESSAGE_0_LENGTH},
    {&(gNmeaTestMessage[0]), "?PGGA", U_GNSS_PRIVATE_TEST_NMEA_MESSAGE_0_LENGTH},
    {&(gNmeaTestMessage[0]), "G?GGA", U_GNSS_PRIVATE_TEST_NMEA_MESSAGE_0_LENGTH},
    {&(gNmeaTestMessage[0]), "GP?GA", U_GNSS_PRIVATE_TEST_NMEA_MESSAGE_0_LENGTH},
    {&(gNmeaTestMessage[0]), "GPG?A", U_GNSS_PRIVATE_TEST_NMEA_MESSAGE_0_LENGTH},
    {&(gNmeaTestMessage[0]), "GPGG?", U_GNSS_PRIVATE_TEST_NMEA_MESSAGE_0_LENGTH},
    {&(gNmeaTestMessage[0]), "?PGG?", U_GNSS_PRIVATE_TEST_NMEA_MESSAGE_0_LENGTH},
    {&(gNmeaTestMessage[0]), "?P?G?", U_GNSS_PRIVATE_TEST_NMEA_MESSAGE_0_LENGTH},
    {&(gNmeaTestMessage[0]), "?????", U_GNSS_PRIVATE_TEST_NMEA_MESSAGE_0_LENGTH},
    {&(gNmeaTestMessage[0]), "GPGGA?", (int32_t) U_ERROR_COMMON_NOT_FOUND},
    {&(gNmeaTestMessage[0]), "?GPGGA", (int32_t) U_ERROR_COMMON_NOT_FOUND},
    {&(gNmeaTestMessage[0]), "X????", (int32_t) U_ERROR_COMMON_NOT_FOUND},
    {&(gNmeaTestMessage[0]), "????X", (int32_t) U_ERROR_COMMON_NOT_FOUND},
    {&(gNmeaTestMessage[0]), "??X??", (int32_t) U_ERROR_COMMON_NOT_FOUND},
    {&(gNmeaTestMessage[0]), "GPGGA?", (int32_t) U_ERROR_COMMON_NOT_FOUND}
};

#endif // #ifndef __ZEPHYR__

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

# ifndef __ZEPHYR__

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

// Call uGnssPrivateDecodeNmea() with the given parameters and
// return true if good, else false
static bool checkUGnssPrivateDecodeNmea(const char *pBuffer, size_t size,
                                        char *pTalkerSentenceStr,
                                        int32_t expectedReturnValue, size_t expectedDiscard)
{
    bool passNotFail = true;
    size_t discard = 0;
    size_t incrementalSize = size;
    size_t x;
    size_t y;
    int32_t errorCodeOrSize;
    uGnssPrivateMessageDecodeState_t savedState;
    uGnssPrivateMessageDecodeState_t *pSavedState = NULL;

    // Randomly chose whether to use saved state or not
    if (rand() % 2) {
        pSavedState = &savedState;
        U_GNSS_PRIVATE_MESSAGE_DECODE_STATE_DEFAULT(pSavedState);
    }
    if (pSavedState != NULL) {
        // If we're using a saved state, feed the buffer into the decoder
        // incrementally so as to make proper use of it
        incrementalSize = 1;
    }
    do {
        x = 0xffffffff;
        errorCodeOrSize = uGnssPrivateDecodeNmea(pBuffer + discard, incrementalSize - discard,
                                                 pTalkerSentenceStr, &x, pSavedState);
        discard += x;
        if ((pSavedState != NULL) && (incrementalSize <= size)) {
            y = rand() % (size - incrementalSize + 1);
            if (y == 0) {
                y++;
            }
            incrementalSize += y;
        }
    } while (((pSavedState == NULL) && (x > 0)) ||
             ((pSavedState != NULL) && (incrementalSize <= size)));

    if ((errorCodeOrSize != expectedReturnValue) || (expectedDiscard != discard)) {
        passNotFail = false;
        uPortLog(U_TEST_PREFIX "decoding buffer \"");
        for (size_t x = 0; x < size; x++) {
            if (!isprint((int32_t) *pBuffer)) {
                uPortLog("[%02x]", (unsigned char) *pBuffer);
            } else {
                uPortLog("%c", *pBuffer);
            }
            pBuffer++;
        }
        uPortLog("\" (%d bytes)\n", size);
        uPortLog(U_TEST_PREFIX "with talker/sentence ");
        if (pTalkerSentenceStr == NULL) {
            uPortLog("NULL");
        } else {
            uPortLog("\"%s\"", pTalkerSentenceStr);
        }
        uPortLog(", failed to meet expectations:\n");
        U_TEST_PRINT_LINE("expected return value %d, actual return value %d.",
                          expectedReturnValue, errorCodeOrSize);
        U_TEST_PRINT_LINE("expected discard %d, actual discard %d.",
                          expectedDiscard, discard);
    }

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
    // +10 to allow us to add random stuff
    char buffer[U_GNSS_NMEA_SENTENCE_MAX_LENGTH_BYTES + 10];
    // +1 for the null terminator
    char talkerSentenceBuffer[U_GNSS_NMEA_MESSAGE_MATCH_LENGTH_CHARACTERS + 1];
    char *pMessage;
    size_t messageSize;
    size_t z;
    size_t w;
    int32_t heapUsed;
    int32_t returnValue;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();
    heapUsed = uPortGetHeapFree();

    U_PORT_TEST_ASSERT(uPortInit() == 0);

    // Parse all the test data
    for (size_t x = 0; x < sizeof(gNmeaTestMessage) / sizeof(gNmeaTestMessage[0]); x++) {

        U_TEST_PRINT_LINE("test decoding NMEA message $%s,%s*%s",
                          gNmeaTestMessage[x].pTalkerSentenceStr,
                          gNmeaTestMessage[x].pBodyStr,
                          gNmeaTestMessage[x].pChecksumHexStr);

        // Do this multiple times for good randomness
        for (size_t y = 0; y < 1000; y++) {
            // First fill the buffer with randomness, but not randomness that
            // has a dollar in it as that would mark the start of a spurious message
            for (z = 0; z < sizeof(buffer) / sizeof(buffer[0]); z++) {
                buffer[z] = (char) rand();
                if (buffer[z] == '$') {
                    buffer[z] = '_';
                }
            }
            // Assemble the message, starting a random distance into the buffer
            z = rand() % ((sizeof(buffer) / sizeof(buffer[0])) - U_GNSS_NMEA_SENTENCE_MAX_LENGTH_BYTES);
            pMessage = buffer + z;
            messageSize = makeNmeaMessage(pMessage, gNmeaTestMessage[x].pTalkerSentenceStr,
                                          gNmeaTestMessage[x].pBodyStr,
                                          gNmeaTestMessage[x].pChecksumHexStr);

            // Decode it with a wild-card message ID first
            U_PORT_TEST_ASSERT(checkUGnssPrivateDecodeNmea(buffer, sizeof(buffer), NULL,
                                                           messageSize, pMessage - buffer));

            // Then with the exact message ID
            strncpy(talkerSentenceBuffer, gNmeaTestMessage[x].pTalkerSentenceStr, sizeof(talkerSentenceBuffer));
            // Ensure terminator
            talkerSentenceBuffer[sizeof(talkerSentenceBuffer) - 1] = 0;
            U_PORT_TEST_ASSERT(checkUGnssPrivateDecodeNmea(buffer, sizeof(buffer), talkerSentenceBuffer,
                                                           messageSize, pMessage - buffer));

            // Then with a partial message ID of random length 1 or more
            z = 1 + (rand() % (strlen(gNmeaTestMessage[x].pTalkerSentenceStr) - 1));
            talkerSentenceBuffer[z] = 0;
            U_PORT_TEST_ASSERT(checkUGnssPrivateDecodeNmea(buffer, sizeof(buffer), talkerSentenceBuffer,
                                                           messageSize, pMessage - buffer));

            // Then with a wrong message ID
            strncpy(talkerSentenceBuffer, gNmeaTestMessage[x].pTalkerSentenceStr, sizeof(talkerSentenceBuffer));
            z = rand() % strlen(talkerSentenceBuffer);
            talkerSentenceBuffer[z] = '_';
            // Ensure terminator
            talkerSentenceBuffer[sizeof(talkerSentenceBuffer) - 1] = 0;
            U_PORT_TEST_ASSERT(checkUGnssPrivateDecodeNmea(buffer, sizeof(buffer), talkerSentenceBuffer,
                                                           U_ERROR_COMMON_NOT_FOUND, sizeof(buffer)));

            // Then with a partial message
            z = 1 + rand() % (messageSize - 2);
            U_PORT_TEST_ASSERT(checkUGnssPrivateDecodeNmea(buffer, (pMessage - buffer) + z, NULL,
                                                           U_ERROR_COMMON_TIMEOUT, pMessage - buffer));

            // Then with a broken message
            z = rand() % (messageSize);
            returnValue = U_ERROR_COMMON_NOT_FOUND;
            w = sizeof(buffer);
            *(pMessage + z) = '_';
            U_PORT_TEST_ASSERT(checkUGnssPrivateDecodeNmea(buffer, sizeof(buffer), NULL, returnValue, w));
        }
    }

    // Check that wild-card matches work
    for (size_t x = 0; x < sizeof(gTalkerSentenceMatch) / sizeof(gTalkerSentenceMatch[0]); x++) {
        U_TEST_PRINT_LINE("test wildcard talker/sentence match %s",
                          gTalkerSentenceMatch[x].talkerSentenceStr);
        messageSize = makeNmeaMessage(buffer, gTalkerSentenceMatch[x].pNmea->pTalkerSentenceStr,
                                      gTalkerSentenceMatch[x].pNmea->pBodyStr,
                                      gTalkerSentenceMatch[x].pNmea->pChecksumHexStr);
        U_PORT_TEST_ASSERT(uGnssPrivateDecodeNmea(buffer, messageSize,
                                                  (char *) (gTalkerSentenceMatch[x].talkerSentenceStr),
                                                  &z, NULL) == gTalkerSentenceMatch[x].result);
    }

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
#endif
// End of file
