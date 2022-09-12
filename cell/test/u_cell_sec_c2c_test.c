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
 * @brief Tests for the internal cellular chip to chip security API.
 * These should pass on all platforms.  No cellular module is
 * required to run this set of tests, all testing is back to back.
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the U_PORT_TEST_FUNCTION()
 * macro.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memcpy(), memcmp(), strncpy(), strncat()
#include "ctype.h"     // isdigit()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"  // For #define U_CFG_OS_CLIB_LEAKS
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_uart.h"
#include "u_port_crypto.h"

#include "u_at_client.h"

#include "u_security.h"

#include "u_cell_module_type.h"
#include "u_cell_file.h"
#include "u_cell.h"         // Order is
#include "u_cell_net.h"     // important here
#include "u_cell_private.h" // don't change it

#include "u_cell_sec_c2c.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The base string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX_BASE "U_CELL_C2C_TEST"

/** The string to put at the start of all prints from this test
 * that do not require an iteration on the end.
 */
#define U_TEST_PREFIX U_TEST_PREFIX_BASE ": "

/** Print a whole line, with terminator, prefixed for this test
 * file, no iteration version.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

/** The string to put at the start of all prints from this test
 * where an interation is required on the end.
 */
#define U_TEST_PREFIX_X U_TEST_PREFIX_BASE "_%d: "

/** Print a whole line, with terminator and iteration on the end,
 * prefixed for this test file.
 */
#define U_TEST_PRINT_LINE_X(format, ...) uPortLog(U_TEST_PREFIX_X format "\n", ##__VA_ARGS__)

/** The 16 byte TE secret to use during testing.
 */
#define U_CELL_SEC_C2C_TEST_TE_SECRET "\x00\x01\x02\x03\x04\x05\x06\x07" \
                                      "\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff"

/** The 16 byte key to use during testing.
 */
#define U_CELL_SEC_C2C_TEST_KEY "\x10\x11\x12\x13\x14\x15\x16\x17" \
                                "\xe8\xe9\xea\xeb\xec\xed\xee\xef"

/** The 16 byte truncated HMAC (or tag) to use
 * during testing, needed for V2 only.
 */
#define U_CELL_SEC_C2C_TEST_HMAC_TAG "\x20\x21\x22\x23\x24\x25\x26\x27" \
                                     "\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf"

/** We only send back what we receive so the max length
 * is the max TX length.
 */
#define U_CELL_SEC_C2C_CHUNK_MAX_LENGTH_BYTES U_CELL_SEC_C2C_USER_MAX_TX_LENGTH_BYTES

/** Guard contents.
 */
#define U_CELL_SEC_C2C_GUARD "deadarea"

/** Length of guard contents.
 */
#define U_CELL_SEC_C2C_GUARD_LENGTH_BYTES 8

/** Check a buffer for underrun.
 */
#define U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(x)                                                   \
    U_PORT_TEST_ASSERT(memcmp(x, U_CELL_SEC_C2C_GUARD, U_CELL_SEC_C2C_GUARD_LENGTH_BYTES) == 0)

/** Check a buffer for overrun.
 */
#define U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(x)                                                   \
    U_PORT_TEST_ASSERT(memcmp(x + sizeof(x) - U_CELL_SEC_C2C_GUARD_LENGTH_BYTES,                \
                              U_CELL_SEC_C2C_GUARD, U_CELL_SEC_C2C_GUARD_LENGTH_BYTES) == 0)

#ifndef U_CELL_SEC_C2C_TEST_TASK_STACK_SIZE_BYTES
/** The stack size for the test task.  This is chosen to
 * work for all platforms, the governing factor being ESP32,
 * which seems to require around twice the stack of NRF52
 * or STM32F4 and more again in the version pre-built for
 * Arduino.
 */
# define U_CELL_SEC_C2C_TEST_TASK_STACK_SIZE_BYTES  2304
#endif

#ifndef U_CELL_SEC_C2C_TEST_TASK_PRIORITY
/** The priority for the C2C test task, re-using the
 * URC task priority for convenience.
 */
# define U_CELL_SEC_C2C_TEST_TASK_PRIORITY U_AT_CLIENT_URC_TASK_PRIORITY
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Definition of clear text and encrypted version for back to back
 * testing of the intercept functions.
 */
typedef struct {
    bool isV2;
    const char *pTeSecret;
    const char *pKey;
    const char *pHmacTag; /** Needed for V2 only. */
    const char *pClear;
    size_t chunkLengthMax;
    size_t numChunks;
    // Allow up to five chunks for test purposes
    size_t clearLength[5];
    // Allow up to five chunks for test purposes
    size_t encryptedLength[5];
} uCellSecC2cTest_t;

#if (U_CFG_TEST_UART_A >= 0) && (U_CFG_TEST_UART_B >= 0)

/** Definition of an outgoing AT command, what the response
 * should be plus an optional URC, for testing of the intercept
 * functions inside the AT client.
 * ORDER IS IMPORTANT: this is statically initialised.
 */
typedef struct {
    bool isV2;
    size_t chunkLengthMax;
    const char *pTeSecret;
    const char *pKey;
    const char *pHmacTag; /** Needed for V2 only. */
    const char *pCommandPrefix;
    bool isBinary; /** Command and response are either
                       a string or binary bytes. */
    const char *pCommandBody;
    size_t commandBodyLength;
    int32_t commandWaitTimeSeconds; /** How long the server should wait to receive the command. */
    const char *pUrcPrefix; /** Set to NULL if there is no URC. */
    const char *pUrcBody;  /** Can only be a string. */
    const char *pResponsePrefix;
    const char *pResponseBody;
    size_t responseBodyLength;
    int32_t responseWaitTimeSeconds; /** How long the client should wait to receive the response. */
} uCellSecC2cTestAt_t;

#endif

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Storage for the common part of the security context.
 */
static uCellSecC2cContext_t gContext;

/** Storage for the transmit/encode direction of the security context.
 */
static uCellSecC2cContextTx_t gContextTx;

/** Storage for the receive/decode direction of the security context.
 */
static uCellSecC2cContextRx_t gContextRx;

/** Test data.
 */
//lint -e{785} Suppress too few initialisers
//lint -e{786} Suppress string concatenation within initialiser
static uCellSecC2cTest_t gTestData[] = {
    {/* 1: Basic V1 */
        false, U_CELL_SEC_C2C_TEST_TE_SECRET, U_CELL_SEC_C2C_TEST_KEY, NULL,
        "Hello world!", U_CELL_SEC_C2C_CHUNK_MAX_LENGTH_BYTES, 1, {12},
        {1 + 2 + 12 + 4 /* pad to 16 */ + 32 /* SHA256 */ + 16  /* IV */ + 2 + 1}
    },
    {/* 2: Basic V2 */
        true, U_CELL_SEC_C2C_TEST_TE_SECRET, U_CELL_SEC_C2C_TEST_KEY, U_CELL_SEC_C2C_TEST_HMAC_TAG,
        "Hello world!", U_CELL_SEC_C2C_CHUNK_MAX_LENGTH_BYTES, 1, {12},
        {1 + 2 + 12 + 4 /* pad to 16 */ + 16  /* IV */ + 16 /* HMAC TAG */ + 2 + 1}
    },
    {/* 3: V1, clear text exactly 16 bytes (padding length) long */
        false, U_CELL_SEC_C2C_TEST_TE_SECRET, U_CELL_SEC_C2C_TEST_KEY, NULL,
        "0123456789abcdef", U_CELL_SEC_C2C_CHUNK_MAX_LENGTH_BYTES, 1, {16},
        {1 + 2 + 32 /* padding causes this */ + 32 /* SHA256 */ + 16  /* IV */ + 2 + 1}
    },
    {/* 4: V2, clear text exactly 16 bytes (padding length) long */
        true, U_CELL_SEC_C2C_TEST_TE_SECRET, U_CELL_SEC_C2C_TEST_KEY, U_CELL_SEC_C2C_TEST_HMAC_TAG,
        "0123456789abcdef", U_CELL_SEC_C2C_CHUNK_MAX_LENGTH_BYTES, 1, {16},
        {1 + 2 + 32 /* padding causes this */ + 16  /* IV */ + 16 /* HMAC TAG */ + 2 + 1}
    },
    {/* 5: V1, clear text of exactly chunk length when padded */
        false, U_CELL_SEC_C2C_TEST_TE_SECRET, U_CELL_SEC_C2C_TEST_KEY, NULL,
        "47 bytes, one less than the chunk length of 48.", 48, 1, {47},
        {1 + 2 + 48 /* max chunk length when padded */ + 32 /* SHA256 */ + 16  /* IV */ + 2 + 1}
    },
    {/* 6: V2, clear text of exactly chunk length when padded */
        true, U_CELL_SEC_C2C_TEST_TE_SECRET, U_CELL_SEC_C2C_TEST_KEY, U_CELL_SEC_C2C_TEST_HMAC_TAG,
        "47 bytes, one less than the chunk length of 48.", 48, 1, {47},
        {1 + 2 + 48 /* max chunk length when padded */ + 16  /* IV */ + 16 /* HMAC TAG */ + 2 + 1}
    },
    {/* 7: V1, clear text of greater than the chunk length */
        false, U_CELL_SEC_C2C_TEST_TE_SECRET, U_CELL_SEC_C2C_TEST_KEY, NULL,
        "With a chunk length of 48 this is just a bit longer at 58.", 48, 2, {47, 11},
        {
            1 + 2 + 48 /* max chunk length */ + 32 /* SHA256 */ + 16  /* IV */ + 2 + 1,
            1 + 2 + 16 /* remainder, padded to 16 */ + 32 /* SHA256 */ + 16  /* IV */ + 2 + 1
        }
    },
    {/* 8: V2, clear text of greater than the chunk length */
        true, U_CELL_SEC_C2C_TEST_TE_SECRET, U_CELL_SEC_C2C_TEST_KEY, U_CELL_SEC_C2C_TEST_HMAC_TAG,
        "With a chunk length of 48 this is just a bit longer at 58.", 48, 2, {47, 11},
        {
            1 + 2 + 48 /* max chunk length */ + 16  /* IV */ + 16 /* HMAC TAG */ + 2 + 1,
            1 + 2 + 16 /* remainder, padded to 16 */ + 16  /* IV */ + 16 /* HMAC TAG */ + 2 + 1
        }
    },
    {/* 9: V1, a biggee*/
        false, U_CELL_SEC_C2C_TEST_TE_SECRET, U_CELL_SEC_C2C_TEST_KEY, NULL,
        "_____0000:0123456789012345678901234567890123456789"
        "_____0001:0123456789012345678901234567890123456789"
        "_____0002:0123456789012345678901234567890123456789"
        "_____0003:0123456789012345678901234567890123456789", 48, 5, {47, 47, 47, 47, 12},
        {
            1 + 2 + 48 /* max chunk length */ + 32 /* SHA256 */ + 16  /* IV */ + 2 + 1,
            1 + 2 + 48 /* max chunk length */ + 32 /* SHA256 */ + 16  /* IV */ + 2 + 1,
            1 + 2 + 48 /* max chunk length */ + 32 /* SHA256 */ + 16  /* IV */ + 2 + 1,
            1 + 2 + 48 /* max chunk length */ + 32 /* SHA256 */ + 16  /* IV */ + 2 + 1,
            1 + 2 + 16 /* remainder, padded to 16 */ + 32 /* SHA256 */ + 16  /* IV */ + 2 + 1
        }
    },
    {/* 10: V2, a biggee*/
        true, U_CELL_SEC_C2C_TEST_TE_SECRET, U_CELL_SEC_C2C_TEST_KEY, U_CELL_SEC_C2C_TEST_HMAC_TAG,
        "_____0000:0123456789012345678901234567890123456789"
        "_____0001:0123456789012345678901234567890123456789"
        "_____0002:0123456789012345678901234567890123456789"
        "_____0003:0123456789012345678901234567890123456789", 48, 5, {47, 47, 47, 47, 12},
        {
            1 + 2 + 48 /* max chunk length */ + 16  /* IV */ + 16 /* HMAC TAG */ + 2 + 1,
            1 + 2 + 48 /* max chunk length */ + 16  /* IV */ + 16 /* HMAC TAG */ + 2 + 1,
            1 + 2 + 48 /* max chunk length */ + 16  /* IV */ + 16 /* HMAC TAG */ + 2 + 1,
            1 + 2 + 48 /* max chunk length */ + 16  /* IV */ + 16 /* HMAC TAG */ + 2 + 1,
            1 + 2 + 16 /* remainder, padded to 16 */ + 16  /* IV */ + 16 /* HMAC TAG */ + 2 + 1
        }
    }
};

/** A buffer for transmitted data.
 */
static char gBufferA[(U_CELL_SEC_C2C_CHUNK_MAX_LENGTH_BYTES * 5) +
                                                                 (U_CELL_SEC_C2C_GUARD_LENGTH_BYTES * 2)];

/** A buffer for received data.
 */
static char gBufferB[(U_CELL_SEC_C2C_CHUNK_MAX_LENGTH_BYTES * 5) +
                                                                 (U_CELL_SEC_C2C_GUARD_LENGTH_BYTES * 2)];

/** Handle for the AT client UART stream.
 */
static int32_t gUartAHandle = -1;

/** Handle for the AT server UART stream (the reverse direction).
 */
static int32_t gUartBHandle = -1;

#if (U_CFG_TEST_UART_A >= 0) && (U_CFG_TEST_UART_B >= 0)

/** A buffer for received URC data.
 */
static char gBufferC[(U_CELL_SEC_C2C_CHUNK_MAX_LENGTH_BYTES * 5) +
                                                                 (U_CELL_SEC_C2C_GUARD_LENGTH_BYTES * 2)];

/** For tracking heap lost to memory  lost by the C library.
 */
static size_t gSystemHeapLost = 0;

/** Count our way through the AT client-based tests.
 */
static size_t gAtTestCount = 0;

/** For the server to track how much it has received
 * and not yet decrypted.
 */
static size_t gAtServerLengthBuffered = 0;

/** For the server to track how much it has decrypted.
 */
static size_t gAtServerLengthDecrypted = 0;

/** For the server to track how long it has been waiting
 * for stuff to arrive.
 */
static int32_t gAtServerWaitTimeMs = 0;

/** Flag an error on the server side of the AT interface.
 */
static int32_t gAtServerErrorOrSize = 0;

/** Flag an error in a URC.
 */
static int32_t gUrcErrorOrSize = 0;

/** Count the number of URCs received.
 */
static size_t gUrcCount = 0;

/** A chip-to-chip security context for the
 * AT server side.
 */
static uCellSecC2cContext_t gAtServerContext;

/** A receive chip-to-chip security context for the
 * AT server-side to use to decrypt packets.
 */
static uCellSecC2cContextRx_t gAtServerContextRx;

/** A transmit chip-to-chip security context for the
 * AT server-side to use to encrypt packets.
 */
static uCellSecC2cContextTx_t gAtServerContextTx;

/** Test data for the AT client based testing.
 */
//lint -e{786} Suppress string concatenation within initialiser
static const uCellSecC2cTestAt_t gTestAt[] = {
    {/* 1: command with string parameter and OK response, no URC */
        false /* V1 */, U_CELL_SEC_C2C_CHUNK_MAX_LENGTH_BYTES,
        U_CELL_SEC_C2C_TEST_TE_SECRET, U_CELL_SEC_C2C_TEST_KEY, NULL,
        "AT+BLAH0=", false, "thing-thing", 11, 1, /* Command with string parameter */
        NULL, NULL, /* No URC */
        NULL, NULL, 0, 1 /* No prefix or response body */
    },
    {/* 2: command with string parameter and information response, no URC */
        false /* V1 */, U_CELL_SEC_C2C_CHUNK_MAX_LENGTH_BYTES,
        U_CELL_SEC_C2C_TEST_TE_SECRET, U_CELL_SEC_C2C_TEST_KEY, NULL,
        "AT+BLAH1=", false, "thing thang", 11, 1, /* Command with string parameter */
        NULL, NULL, /* No URC */
        "+BLAH1:", "thong", 5, 2 /* Information response prefix and body  */
    },
    {/* 3: command with string parameter, URC inserted then OK response */
        false /* V1 */, U_CELL_SEC_C2C_CHUNK_MAX_LENGTH_BYTES,
        U_CELL_SEC_C2C_TEST_TE_SECRET, U_CELL_SEC_C2C_TEST_KEY, NULL,
        "AT+BLAH2=", false, "whotsit", 7, 1, /* Command with string parameter */
        "+UBOO:", "bang", /* URC inserted */
        NULL, NULL, 0, 1 /* No prefix or response body */
    },
    {/* 4: command with string parameter, URC inserted then information response */
        false /* V1 */, U_CELL_SEC_C2C_CHUNK_MAX_LENGTH_BYTES,
        U_CELL_SEC_C2C_TEST_TE_SECRET, U_CELL_SEC_C2C_TEST_KEY, NULL,
        "AT+BLAH3=", false, "questionable", 12, 1, /* Command with string parameter */
        "+UPAF:", "boomer", /* URC inserted */
        "+BLAH3:", "not at all", 10, 2 /* Information response prefix and body  */
    },
    {/* 5: as (1) but with binary parameter and response */
        false /* V1 */, U_CELL_SEC_C2C_CHUNK_MAX_LENGTH_BYTES,
        U_CELL_SEC_C2C_TEST_TE_SECRET, U_CELL_SEC_C2C_TEST_KEY, NULL,
        "AT+BLING0=", true, "\x00\x01\x02\x04\xff\xfe\xfd\xfc", 8, 1, /* Command with binary parameter */
        NULL, NULL, /* No URC */
        NULL, NULL, 0, 1 /* No prefix or response body */
    },
    {/* 6: as (2) but with binary parameter and response */
        false /* V1 */, U_CELL_SEC_C2C_CHUNK_MAX_LENGTH_BYTES,
        U_CELL_SEC_C2C_TEST_TE_SECRET, U_CELL_SEC_C2C_TEST_KEY, NULL,
        "AT+BLING1=", true, "\xff\xfe\xfd\xfc\x03\x02\x01\x00", 8, 1, /* Command with binary parameter */
        NULL, NULL, /* No URC */
        "+BLAH1:", "\x00", 1, 2 /* Information response prefix and body  */
    },
    {/* 7: as (3) but with binary parameter and response */
        false /* V1 */, U_CELL_SEC_C2C_CHUNK_MAX_LENGTH_BYTES,
        U_CELL_SEC_C2C_TEST_TE_SECRET, U_CELL_SEC_C2C_TEST_KEY, NULL,
        "AT+BLING2=", true, "\xaa\x55", 2, 1, /* Command with binary parameter */
        "+UBLIM:", "blam", /* URC inserted */
        NULL, NULL, 0, 1 /* No prefix or response body */
    },
    {/* 8: as (4) but with binary parameter and response */
        false /* V1 */, U_CELL_SEC_C2C_CHUNK_MAX_LENGTH_BYTES,
        U_CELL_SEC_C2C_TEST_TE_SECRET, U_CELL_SEC_C2C_TEST_KEY, NULL,
        "AT+BLING3=", true, "\x55\xaa", 2, 1, /* Command with binary parameter */
        "+UPIF:", "blammer 1", /* URC inserted */
        "+BLING3:", "\x00\xff\x00\xff", 4, 2 /* Information response prefix and body  */
    },
    {/* 9: as (8) but with V2 scheme */
        true /* V2 */, U_CELL_SEC_C2C_CHUNK_MAX_LENGTH_BYTES,
        U_CELL_SEC_C2C_TEST_TE_SECRET, U_CELL_SEC_C2C_TEST_KEY, U_CELL_SEC_C2C_TEST_HMAC_TAG,
        "AT+BLING3=", true, "\x55\xaa", 2, 1, /* Command with binary parameter */
        "+UPIF:", "blammer 2", /* URC inserted */
        "+BLING3:", "\x00\xff\x00\xff", 4, 2 /* Information response prefix and body */
    },
    {   /* 10: as (8) but with command and response of the maximum amount */
        /* of user data that can be fitted into a chunk (which is one less */
        /* than U_CELL_SEC_C2C_CHUNK_MAX_LENGTH_BYTES because of the way */
        /* RFC 5652 padding works) */
        /* [This comment done as separate lines and with this exact indentation */
        /* as otherwise AStyle wants to move it another four spaces to the right */
        /* every time it processes it :-)] */
        false /* V1 */, U_CELL_SEC_C2C_CHUNK_MAX_LENGTH_BYTES,
        U_CELL_SEC_C2C_TEST_TE_SECRET, U_CELL_SEC_C2C_TEST_KEY, NULL,
        "AT+VERYLONG_V1=", false,  /* Command prefix 15 bytes */
        "_____0000:0123456789012345678901234567890123456789"
        "_____0001:0123456789012345678901234567890123456789"
        "_____0002:0123456789012345678901234567890123456789"
        "_____0003:0123456789012345678901234567890123456789"
        "_____0004:01234567890123456789012345678", 239, 5,
        /* (total becomes 255 with \r command delimiter) */
        "+UPUF:", "little URC 1", /* URC inserted */
        "+VERYLONG_V1:", /* Information response prefix 13 bytes */
        "_____0000:0123456789012345678901234567890123456789"
        "_____0001:0123456789012345678901234567890123456789"
        "_____0002:0123456789012345678901234567890123456789"
        "_____0003:0123456789012345678901234567890123456789"
        "_____0004:012345678901234567890123456789", 240, 5
        /* (total becomes 255 with \r\n response delimiter) */
    },
    {/* 11: as (10) but with V2 scheme */
        true /* V2 */, U_CELL_SEC_C2C_CHUNK_MAX_LENGTH_BYTES,
        U_CELL_SEC_C2C_TEST_TE_SECRET, U_CELL_SEC_C2C_TEST_KEY, U_CELL_SEC_C2C_TEST_HMAC_TAG,
        "AT+VERYLONG_V2=", false,  /* Command prefix 15 bytes */
        "_____0000:0123456789012345678901234567890123456789"
        "_____0001:0123456789012345678901234567890123456789"
        "_____0002:0123456789012345678901234567890123456789"
        "_____0003:0123456789012345678901234567890123456789"
        "_____0004:01234567890123456789012345678", 239, 5,
        /* (total becomes 255 with \r command delimiter) */
        "+UPUF:", "little URC 2", /* URC inserted */
        "+VERYLONG_V2:", /* Information response prefix 13 bytes */
        "_____0000:0123456789012345678901234567890123456789"
        "_____0001:0123456789012345678901234567890123456789"
        "_____0002:0123456789012345678901234567890123456789"
        "_____0003:0123456789012345678901234567890123456789"
        "_____0004:012345678901234567890123456789", 240, 5
        /* (total becomes 255 with \r\n response delimiter) */
    },
    {/* 12: a real biggee */
        false /* V1 */, U_CELL_SEC_C2C_CHUNK_MAX_LENGTH_BYTES,
        U_CELL_SEC_C2C_TEST_TE_SECRET, U_CELL_SEC_C2C_TEST_KEY, NULL,
        "AT+REALLYLONGONE=", false,  /* Command prefix */
        "_____0000:0123456789012345678901234567890123456789"
        "_____0001:0123456789012345678901234567890123456789"
        "_____0002:0123456789012345678901234567890123456789"
        "_____0003:0123456789012345678901234567890123456789"
        "_____0004:0123456789012345678901234567890123456789"
        "_____0005:0123456789012345678901234567890123456789"
        "_____0006:0123456789012345678901234567890123456789"
        "_____0007:0123456789012345678901234567890123456789"
        "_____0008:0123456789012345678901234567890123456789"
        "_____0009:0123456789012345678901234567890123456789",
        500, 15,
        "+UPUF:", "little URC 3", /* URC inserted */
        "+ALSOAREALLYLONGONE:", /* Information response prefix */
        "_____0000:0123456789012345678901234567890123456789"
        "_____0001:0123456789012345678901234567890123456789"
        "_____0002:0123456789012345678901234567890123456789"
        "_____0003:0123456789012345678901234567890123456789"
        "_____0004:0123456789012345678901234567890123456789"
        "_____0005:0123456789012345678901234567890123456789"
        "_____0006:0123456789012345678901234567890123456789"
        "_____0007:0123456789012345678901234567890123456789"
        "_____0008:0123456789012345678901234567890123456789"
        "_____0009:0123456789012345678901234567890123456789",
        500, 15
    },
    {/* 13: as (12) but with V2 scheme */
        true /* V2 */, U_CELL_SEC_C2C_CHUNK_MAX_LENGTH_BYTES,
        U_CELL_SEC_C2C_TEST_TE_SECRET, U_CELL_SEC_C2C_TEST_KEY, U_CELL_SEC_C2C_TEST_HMAC_TAG,
        "AT+ANOTHERREALLYLONGONE=", false,  /* Command prefix */
        "_____0000:0123456789012345678901234567890123456789"
        "_____0001:0123456789012345678901234567890123456789"
        "_____0002:0123456789012345678901234567890123456789"
        "_____0003:0123456789012345678901234567890123456789"
        "_____0004:0123456789012345678901234567890123456789"
        "_____0005:0123456789012345678901234567890123456789"
        "_____0006:0123456789012345678901234567890123456789"
        "_____0007:0123456789012345678901234567890123456789"
        "_____0008:0123456789012345678901234567890123456789"
        "_____0009:0123456789012345678901234567890123456789",
        500, 15,
        "+UPUF:", "little URC 4", /* URC inserted */
        "+ALSOANOTHERREALLYLONGONE:", /* Information response prefix */
        "_____0000:0123456789012345678901234567890123456789"
        "_____0001:0123456789012345678901234567890123456789"
        "_____0002:0123456789012345678901234567890123456789"
        "_____0003:0123456789012345678901234567890123456789"
        "_____0004:0123456789012345678901234567890123456789"
        "_____0005:0123456789012345678901234567890123456789"
        "_____0006:0123456789012345678901234567890123456789"
        "_____0007:0123456789012345678901234567890123456789"
        "_____0008:0123456789012345678901234567890123456789"
        "_____0009:0123456789012345678901234567890123456789",
        500, 15
    }
};

#endif

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise the guard areas on the buffers.
static void initGuards()
{
    // The buffers here doesn't need null termination so we suppress the
    // clang-tidy warning
    // NOLINTBEGIN(bugprone-not-null-terminated-result)
    memcpy(gBufferA, U_CELL_SEC_C2C_GUARD, U_CELL_SEC_C2C_GUARD_LENGTH_BYTES);
    memcpy(gBufferA + sizeof(gBufferA) - U_CELL_SEC_C2C_GUARD_LENGTH_BYTES,
           U_CELL_SEC_C2C_GUARD, U_CELL_SEC_C2C_GUARD_LENGTH_BYTES);
    memcpy(gBufferB, U_CELL_SEC_C2C_GUARD, U_CELL_SEC_C2C_GUARD_LENGTH_BYTES);
    memcpy(gBufferB + sizeof(gBufferB) - U_CELL_SEC_C2C_GUARD_LENGTH_BYTES,
           U_CELL_SEC_C2C_GUARD, U_CELL_SEC_C2C_GUARD_LENGTH_BYTES);
#if (U_CFG_TEST_UART_A >= 0) && (U_CFG_TEST_UART_B >= 0)
    memcpy(gBufferC, U_CELL_SEC_C2C_GUARD, U_CELL_SEC_C2C_GUARD_LENGTH_BYTES);
    memcpy(gBufferC + sizeof(gBufferC) - U_CELL_SEC_C2C_GUARD_LENGTH_BYTES,
           U_CELL_SEC_C2C_GUARD, U_CELL_SEC_C2C_GUARD_LENGTH_BYTES);
#endif
    // NOLINTEND(bugprone-not-null-terminated-result)
}

// Print out text.
static void print(const char *pStr, size_t length)
{
    char c;

    for (size_t x = 0; x < length; x++) {
        c = *pStr++;
        if (!isprint((int32_t) c)) {
            // Print the hex
            uPortLog("[%02x]", (unsigned char) c);
        } else {
            // Print the ASCII character
            uPortLog("%c", c);
        }
    }
}

// Print out binary.
//lint -esym(522, printHex) Suppress "lacks side effects", which
// will be true if logging is compiled out
static void printHex(const char *pStr, size_t length)
{
#if U_CFG_ENABLE_LOGGING
    char c;

    for (size_t x = 0; x < length; x++) {
        c = *pStr++;
        uPortLog("[%02x]", (unsigned char) c);
    }
#else
    (void) pStr;
    (void) length;
#endif
}

#if (U_CFG_TEST_UART_A >= 0) && (U_CFG_TEST_UART_B >= 0)
// On some platforms printing is line
// buffered so long strings will get lost unless
// they are chunked up: this function
// prints reasonable block sizes
//lint -esym(522, printBlock) Suppress "lacks side effects", which
// will be true if logging is compiled out
static void printBlock(const char *pStr, size_t length,
                       bool isBinary, size_t index)
{
#if U_CFG_ENABLE_LOGGING
    int32_t x = (int32_t) length;
    int32_t y;

    while (x > 0) {
        uPortLog(U_TEST_PREFIX_X, index);
        y = x;
        if (y > 32) {
            y = 32;
        }
        if (isBinary) {
            printHex(pStr, y);
        } else {
            print(pStr, y);
        }
        uPortLog("\n");
        // Don't overwhelm the poor debug output,
        // there there
        uPortTaskBlock(100);
        x -= y;
        pStr += y;
    }
#else
    (void) pStr;
    (void) length;
    (void) index;
    (void) isBinary;
#endif
}
#endif

// Check the result of an encryption.
static void checkEncrypted(size_t testIndex,
                           size_t chunkIndex,
                           const char *pEncrypted,
                           size_t encryptedLength,
                           const uCellSecC2cTest_t *pTestData)
{
    char *pData;
    char *pDecrypted;
    size_t length;
    size_t previousLength = 0;
    const char *pTmp = pEncrypted;
    size_t x;

    // Make sure that testIndex is used to keep
    // compilers happy if logging is compiled out
    (void) testIndex;

    U_TEST_PRINT_LINE_X("encrypted chunk %d, %d byte(s):",
                        testIndex + 1, chunkIndex + 1, encryptedLength);
    if (pTmp != NULL) {
        length = encryptedLength;
        while (length > 0) {
            uPortLog(U_TEST_PREFIX_X, testIndex + 1);
            x = length;
            if (x > 16) {
                x = 16;
            }
            printHex(pTmp, x);
            uPortLog("\n");
            // Don't overwhelm the poor debug output,
            // there there
            uPortTaskBlock(100);
            length -= x;
            pTmp += x;
        }
    } else {
        uPortLog("[NULL]");
    }
    U_PORT_TEST_ASSERT(encryptedLength == pTestData->encryptedLength[chunkIndex]);

    for (x = 0; x < chunkIndex; x++) {
        previousLength += pTestData->clearLength[x];
    }

    if (pEncrypted != NULL) {
        // Decrypt the data block to check if the contents were correct
        memcpy(gBufferB + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES + previousLength,
               pEncrypted, encryptedLength);
        pData = gBufferB + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES + previousLength;
        length = encryptedLength;
        pDecrypted = pUCellSecC2cInterceptRx(0, &pData, &length,
                                             &gContext);

        uPortLog(U_TEST_PREFIX_X "decrypted becomes %d byte(s) \"",
                 testIndex + 1, length);
        if (pDecrypted != NULL) {
            print(pDecrypted, length);
        } else {
            uPortLog("[NULL]");
        }
        uPortLog("\".\n");

        U_PORT_TEST_ASSERT(pData == gBufferB + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES +
                           previousLength + encryptedLength);
        U_PORT_TEST_ASSERT(length == pTestData->clearLength[chunkIndex]);
        if (pDecrypted != NULL) {
            U_PORT_TEST_ASSERT(memcmp(pDecrypted, pTestData->pClear + previousLength,
                                      pTestData->clearLength[chunkIndex]) == 0);
        }
    }
}

#if (U_CFG_TEST_UART_A >= 0) && (U_CFG_TEST_UART_B >= 0)

/** Send a thing over a UART.
 */
static int32_t atServerSendThing(int32_t uartHandle,
                                 const char *pThing,
                                 size_t length)
{
    int32_t sizeOrError = 0;

    U_TEST_PRINT_LINE_X("AT server sending %d byte(s):",
                        gAtTestCount + 1, length);
    printBlock(pThing, length, true, gAtTestCount + 1);

    while ((length > 0) && (sizeOrError >= 0)) {
        sizeOrError = uPortUartWrite(uartHandle,
                                     pThing, length);
        if (sizeOrError > 0) {
            pThing += sizeOrError;
            length -= sizeOrError;
        }
    }

    return sizeOrError;
}

/** Encrypt and send a buffer of stuff.
 */
static int32_t atServerEncryptAndSendThing(int32_t uartHandle,
                                           const char *pThing,
                                           size_t length,
                                           size_t chunkLengthMax)
{
    int32_t sizeOrError = 0;
    int32_t x;
    const char *pStart = pThing;
    const char *pOut;
    size_t outLength = length;

    // The AT server-side security context will
    // have already been set up, just need to
    // reset a few parameters
    gAtServerContext.pTx->txInLength = 0;
    gAtServerContext.pTx->txInLimit = chunkLengthMax;

    while ((pThing < pStart + length) && (sizeOrError >= 0)) {
        pOut = pUCellSecC2cInterceptTx(0, &pThing, &outLength,
                                       &gAtServerContext);
        if (outLength > 0) {
            // More than a chunk's worth must have accumulated,
            // send it
            x = atServerSendThing(uartHandle, pOut, outLength);
            if (x >= 0) {
                sizeOrError += x;
            } else {
                sizeOrError = x;
            }
        }
        outLength = length - (pThing - pStart);
    }

    if (sizeOrError >= 0) {
        // Flush the remainder out of the encryption function
        // by calling it again with NULL
        outLength = 0;
        pOut = pUCellSecC2cInterceptTx(0, NULL, &outLength,
                                       &gAtServerContext);
        if (outLength > 0) {
            x = atServerSendThing(uartHandle, pOut, outLength);
            if (x >= 0) {
                sizeOrError += x;
            } else {
                sizeOrError = x;
            }
        }
    }

    return sizeOrError;
}

// Callback which receives commands, decrypts them, checks them
// and then sends back potentially a URC and a response.
//lint -e{818} suppress "could be declared as pointing to const", callback
// has to follow function signature
static void atServerCallback(int32_t uartHandle, uint32_t eventBitmask,
                             void *pParameters)
{
    size_t x;
//lint -esym(438, y) Suppress last value not used, which will
// be the case if logging is off
    size_t y;
    size_t z;
    int32_t sizeOrError = 0;
    const uCellSecC2cTestAt_t *pTestAt = *((uCellSecC2cTestAt_t **) pParameters);
    size_t interceptLength = 0;
    char *pData;
    char *pTmp;
    char *pDecrypted;
    bool allReceived = false;
#if U_CFG_OS_CLIB_LEAKS
    int32_t heapUsed;
#endif

    U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferA);
    U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferA);
    U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferB);
    U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferB);
    U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferC);
    U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferC);

    if ((pTestAt != NULL) &&
        (eventBitmask & U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED)) {
        // Loop until there are no characters left to receive,
        // filling up gBufferA
        while ((sizeOrError >= 0) &&
               (gAtServerWaitTimeMs < pTestAt->commandWaitTimeSeconds * 1000) &&
               (uPortUartGetReceiveSize(uartHandle) > 0)) {
            sizeOrError = uPortUartRead(uartHandle,
                                        gBufferA + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES +
                                        gAtServerLengthDecrypted + gAtServerLengthBuffered,
                                        (sizeof(gBufferA) - (U_CELL_SEC_C2C_GUARD_LENGTH_BYTES * 2)) -
                                        (gAtServerLengthDecrypted + gAtServerLengthBuffered));

            U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferA);
            U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferA);

            if (sizeOrError > 0) {
                gAtServerLengthBuffered += sizeOrError;
                if (gAtServerLengthDecrypted + gAtServerLengthBuffered >
                    sizeof(gBufferA) - (U_CELL_SEC_C2C_GUARD_LENGTH_BYTES * 2)) {
                    U_TEST_PRINT_LINE_X("AT server receive overflow.",
                                        gAtTestCount + 1);
                    sizeOrError = -1;
                }
            }

            // Rest a while
            uPortTaskBlock(100);
            gAtServerWaitTimeMs += 100;
        }

        if ((sizeOrError > 0) && (gAtServerLengthBuffered > 0)) {
#if U_CFG_OS_CLIB_LEAKS
            // Calling printf() from a new task causes newlib
            // to allocate additional memory which, depending
            // on the OS/system, may not be recovered;
            // take account of that here.
            heapUsed = uPortGetHeapFree();
#endif
            U_TEST_PRINT_LINE_X("AT server has %d byte(s) to decrypt:",
                                gAtTestCount + 1, sizeOrError);
            printBlock(gBufferA + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES +
                       gAtServerLengthDecrypted, gAtServerLengthBuffered,
                       true, gAtTestCount + 1);

#if U_CFG_OS_CLIB_LEAKS
            // Take account of any heap lost through the first
            // printf()
            gSystemHeapLost += (size_t) (unsigned) (heapUsed - uPortGetHeapFree());
#endif

            // Try to decrypt the received chunk or chunks in place
            // by calling pUCellSecC2cInterceptRx with
            // the server context.
            pData = gBufferA + gAtServerLengthDecrypted + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES;
            pTmp = pData;
            x = gAtServerLengthBuffered;
            interceptLength = gAtServerLengthBuffered;
            sizeOrError = 0;
            while ((x > 0) && (sizeOrError >= 0)) {
                pDecrypted = pUCellSecC2cInterceptRx(0, &pData,
                                                     &interceptLength,
                                                     &gAtServerContext);

                U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferA);
                U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferA);
                U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferB);
                U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferB);

                if (pDecrypted != NULL) {
                    U_TEST_PRINT_LINE_X("AT server decrypted %d byte(s):",
                                        gAtTestCount + 1, interceptLength);
                    printBlock(pDecrypted, interceptLength, false, gAtTestCount + 1);
                    // Out intercept function returns a pointer to the
                    // start of the decrypted data in the buffer, i.e.
                    // to the value of pData when it was called, so just need
                    // to shuffle everything down so that the next pData
                    // we provide to the intercept function will be
                    // contiguous with the already decrypted data.
                    // The buffer is as below where "sizeOrError"
                    // is the decrypted data from a previous loop,
                    // "interceptLength" the decrypted data from this loop
                    // and "pData" is where we've got to in the buffer.
                    //
                    //                       |-------------------- X ------------------|
                    //    +------------------+-----------------+-----------------------+
                    //    |    sizeOrError   | interceptLength |                       |
                    //    +------------------+-----------------+-------+---------------+
                    //   pTmp           pDecrypted                   pData
                    //    =                                            |------ Y ------|
                    // gBufferA +                              |-- Z --|
                    // U_CELL_SEC_C2C_GUARD_LENGTH_BYTES +
                    // gAtServerLengthDecrypted
                    //
                    // y is the amount of data to move
                    y = pTmp + sizeOrError + x - pData;
                    // Grow size
                    sizeOrError += (int32_t) interceptLength;
                    // Do the move
                    memmove(pTmp + sizeOrError, pData, y);

                    U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferA);
                    U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferA);

                    // z is the distance it was moved
                    z = pData - (pTmp + sizeOrError);
                    // Shift pData down to match
                    pData -= z;
                    // Reduce the amount of data left to process
                    x -= z + interceptLength;
                    // The length passed to the intercept function
                    // becomes what we moved
                    interceptLength = y;
                }
            }
            gAtServerLengthBuffered = x;
            gAtServerLengthDecrypted += sizeOrError;

            U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferA);
            U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferA);
            U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferB);
            U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferB);

            x = strlen(pTestAt->pCommandPrefix);
            if ((sizeOrError >= 0) &&
                (gAtServerLengthDecrypted == x + pTestAt->commandBodyLength +
                 U_AT_CLIENT_COMMAND_DELIMITER_LENGTH_BYTES)) {
                // We've got the lot, check it
                if (memcmp(gBufferA + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES,
                           pTestAt->pCommandPrefix, x) == 0) {
                    if (memcmp(gBufferA + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES + x, pTestAt->pCommandBody,
                               pTestAt->commandBodyLength) == 0) {
                        // Should be the correct command delimiter on the end
                        if (memcmp(gBufferA + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES + x +
                                   pTestAt->commandBodyLength,
                                   U_AT_CLIENT_COMMAND_DELIMITER,
                                   U_AT_CLIENT_COMMAND_DELIMITER_LENGTH_BYTES) == 0) {
                            // All good
                            U_TEST_PRINT_LINE_X("command received is as expected.",
                                                gAtTestCount + 1);
                            allReceived = true;
                        } else {
                            uPortLog(U_TEST_PREFIX_X "expected command"
                                     " delimiter \"");
                            printHex(U_AT_CLIENT_COMMAND_DELIMITER,
                                     U_AT_CLIENT_COMMAND_DELIMITER_LENGTH_BYTES);
                            uPortLog("\" but received \"");
                            printHex(gBufferA + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES +
                                     x + pTestAt->commandBodyLength,
                                     U_AT_CLIENT_COMMAND_DELIMITER_LENGTH_BYTES);
                            uPortLog("\".\n");
                            sizeOrError = -400;
                        }
                    } else {
                        uPortLog(U_TEST_PREFIX_X "expected command body \"",
                                 gAtTestCount + 1);
                        if (pTestAt->isBinary) {
                            printHex(pTestAt->pCommandBody, pTestAt->commandBodyLength);
                        } else {
                            print(pTestAt->pCommandBody, pTestAt->commandBodyLength);
                        }
                        uPortLog("\"\n but received \"");
                        if (pTestAt->isBinary) {
                            printHex(gBufferA + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES + x,
                                     sizeOrError - x);
                        } else {
                            print(gBufferA + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES + x,
                                  sizeOrError - x);
                        }
                        uPortLog("\".\n");
                        sizeOrError = -300;
                    }
                } else {
                    uPortLog(U_TEST_PREFIX_X "expected command prefix \"",
                             gAtTestCount + 1);
                    print(pTestAt->pCommandPrefix, x);
                    uPortLog("\"\n but received \"");
                    print(gBufferA + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES, x);
                    uPortLog("\".\n");
                    sizeOrError = -200;
                }
            } else {
                if (sizeOrError >= 0) {
                    U_TEST_PRINT_LINE_X("decrypted %d byte(s) so far, expecting"
                                        " command length %d byte(s) (including"
                                        " terminator).",
                                        gAtTestCount + 1, gAtServerLengthDecrypted,
                                        x + pTestAt->commandBodyLength +
                                        U_AT_CLIENT_COMMAND_DELIMITER_LENGTH_BYTES);
                }
            }

            if (allReceived) {
                // If there is one, assemble and encrypt a URC
                sizeOrError = 0;
                if (pTestAt->pUrcPrefix != NULL) {
                    U_TEST_PRINT_LINE_X("AT server inserting URC \"%s %s\".",
                                        gAtTestCount + 1,
                                        pTestAt->pUrcPrefix, pTestAt->pUrcBody);
                    strncpy(gBufferA + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES,
                            pTestAt->pUrcPrefix,
                            sizeof(gBufferA) - (U_CELL_SEC_C2C_GUARD_LENGTH_BYTES * 2));
                    strncat(gBufferA + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES,
                            pTestAt->pUrcBody,
                            sizeof(gBufferA) - (U_CELL_SEC_C2C_GUARD_LENGTH_BYTES * 2));
                    strncat(gBufferA + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES, "\r\n",
                            sizeof(gBufferA) - (U_CELL_SEC_C2C_GUARD_LENGTH_BYTES * 2));
                    y = uPortGetTickTimeMs();
                    sizeOrError = atServerEncryptAndSendThing(uartHandle,
                                                              gBufferA + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES,
                                                              strlen(gBufferA + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES),
                                                              pTestAt->chunkLengthMax);
                    U_TEST_PRINT_LINE_X("...took %d ms.", gAtTestCount + 1,
                                        uPortGetTickTimeMs() - y);
                    U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferA);
                    U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferA);
                    U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferB);
                    U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferB);
                }

                if (sizeOrError >= 0) {
                    // Assemble and encrypt the response
                    U_TEST_PRINT_LINE_X("AT server sending response:", gAtTestCount + 1);
                    if ((pTestAt->pResponsePrefix != NULL) || (pTestAt->pResponseBody != NULL)) {
                        if (pTestAt->pResponsePrefix != NULL) {
                            U_TEST_PRINT_LINE_X("\"%s\" ...and then:",
                                                gAtTestCount + 1,
                                                pTestAt->pResponsePrefix);
                        }
                        if (pTestAt->pResponseBody != NULL) {
                            printBlock(pTestAt->pResponseBody,
                                       pTestAt->responseBodyLength,
                                       false, gAtTestCount + 1);
                        } else {
                            U_TEST_PRINT_LINE_X("[nothing]", gAtTestCount + 1);
                        }
                    } else {
                        U_TEST_PRINT_LINE_X("[nothing]", gAtTestCount + 1);
                    }
                    U_TEST_PRINT_LINE_X("...and then \"OK\".", gAtTestCount + 1);
                    *(gBufferA + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES) = 0;
                    if (pTestAt->pResponsePrefix != NULL) {
                        strncpy(gBufferA + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES,
                                pTestAt->pResponsePrefix,
                                sizeof(gBufferA) - (U_CELL_SEC_C2C_GUARD_LENGTH_BYTES * 2));
                    }
                    x = strlen(gBufferA + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES);
                    if (pTestAt->pResponseBody != NULL) {
                        memcpy(gBufferA + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES + x,
                               pTestAt->pResponseBody, pTestAt->responseBodyLength);
                        x += pTestAt->responseBodyLength;
                    }
                    memcpy(gBufferA + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES + x,
                           "\r\nOK\r\n", 6);
                    x += 6;
                    y = uPortGetTickTimeMs();
                    sizeOrError = atServerEncryptAndSendThing(uartHandle,
                                                              gBufferA + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES,
                                                              x, pTestAt->chunkLengthMax);
                    U_TEST_PRINT_LINE_X("...took %d ms.", gAtTestCount + 1,
                                        uPortGetTickTimeMs() - y);
                    U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferA);
                    U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferA);
                    U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferB);
                    U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferB);
                    U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferC);
                    U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferC);
                }
            } else {
                // Check for timeout
                if (gAtServerWaitTimeMs > pTestAt->commandWaitTimeSeconds * 1000) {
                    U_TEST_PRINT_LINE_X("AT server timed-out after %d second(s)"
                                        " with %d byte(s) decrypted.",
                                        gAtTestCount + 1, gAtServerWaitTimeMs / 1000,
                                        gAtServerLengthDecrypted);
                    if (gAtServerLengthBuffered > 0) {
                        U_TEST_PRINT_LINE_X("AT server buffer undecrypted buffer"
                                            " contained %d byte(s):", gAtTestCount + 1,
                                            gAtServerLengthBuffered);
                        printBlock(gBufferA + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES + gAtServerLengthDecrypted,
                                   gAtServerLengthBuffered, true, gAtTestCount + 1);
                    } else {
                        U_TEST_PRINT_LINE_X("AT server buffer had no undecrypted data.",
                                            gAtTestCount + 1);
                    }
                    sizeOrError = -100;
                }
            }
        }
    }

    if ((sizeOrError < 0) || allReceived) {
        // If there was an error or we've finished, reset
        // these so that we can start again
        gAtServerLengthBuffered = 0;
        gAtServerLengthDecrypted = 0;
        gAtServerWaitTimeMs = 0;
    }

    gAtServerErrorOrSize = sizeOrError;
}

// The URC handler for these tests.
//lint -e{818} suppress "could be declared as pointing to const", callback
// has to follow function signature
static void urcHandler(uAtClientHandle_t atClientHandle, void *pParameters)
{
    size_t x = 0;
    int32_t sizeOrError;
    const uCellSecC2cTestAt_t *pTestAt = (uCellSecC2cTestAt_t *) pParameters;
#if U_CFG_OS_CLIB_LEAKS
    int32_t heapUsed;
#endif

    U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferA);
    U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferA);
    U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferB);
    U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferB);
    U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferC);
    U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferC);

    // Read the single string parameter
    sizeOrError = uAtClientReadString(atClientHandle,
                                      gBufferC + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES,
                                      sizeof(gBufferC) - (U_CELL_SEC_C2C_GUARD_LENGTH_BYTES * 2),
                                      false);
    if (pTestAt != NULL) {
        if (pTestAt->pUrcBody != NULL) {
            x = strlen(pTestAt->pUrcBody);
        }
#if U_CFG_OS_CLIB_LEAKS
        // Calling printf() from a new task causes newlib
        // to allocate additional memory which, depending
        // on the OS/system, may not be recovered;
        // take account of that here.
        heapUsed = uPortGetHeapFree();
#endif
        uPortLog(U_TEST_PREFIX_X "AT client received URC \"%s ",
                 gAtTestCount + 1, pTestAt->pUrcPrefix);
        print(gBufferC + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES, x);
        uPortLog("\".\n");
#if U_CFG_OS_CLIB_LEAKS
        // Take account of any heap lost through the first
        // printf()
        gSystemHeapLost += (size_t) (unsigned) (heapUsed - uPortGetHeapFree());
#endif
        if (sizeOrError == x) {
            if (memcmp(gBufferC + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES, pTestAt->pUrcBody, x) != 0) {
                uPortLog(U_TEST_PREFIX_X "AT client expected URC body \"",
                         gAtTestCount + 1);
                print(pTestAt->pUrcBody, x);
                uPortLog("\".\n");
                sizeOrError = -800;
            }
        } else {
            U_TEST_PRINT_LINE_X("AT client expected URC body to be of length %d"
                                "  but was %d.", gAtTestCount + 1,
                                x, sizeOrError);
            sizeOrError = -700;
        }
    } else {
#if U_CFG_OS_CLIB_LEAKS
        // Calling printf() from a new task causes newlib
        // to allocate additional memory which, depending
        // on the OS/system, may not be recovered;
        // take account of that here.
        heapUsed = uPortGetHeapFree();
#endif
        uPortLog(U_TEST_PREFIX_X "AT client received URC fragment \"",
                 gAtTestCount + 1);
        print(gBufferC + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES, sizeOrError);
        uPortLog("\" when there wasn't meant to be one.\n");
#if U_CFG_OS_CLIB_LEAKS
        // Take account of any heap lost through the first
        // printf()
        gSystemHeapLost += (size_t) (unsigned) (heapUsed - uPortGetHeapFree());
#endif
        sizeOrError = -600;
    }

    U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferA);
    U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferA);
    U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferB);
    U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferB);
    U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferC);
    U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferC);

    gUrcCount++;
    gUrcErrorOrSize = sizeOrError;
}

#endif

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Test the transmit and receive intercept functions standalone.
 *
 * IMPORTANT: see notes in u_cfg_test_platform_specific.h for the
 * naming rules that must be followed when using the
 * U_PORT_TEST_FUNCTION() macro.
 */
U_PORT_TEST_FUNCTION("[cellSecC2c]", "cellSecC2cIntercept")
{
    uCellSecC2cTest_t *pTestData;
    const char *pData;
    const char *pDataStart;
    const char *pOut;
    size_t totalLength;
    size_t outLength;
    size_t numChunks;
    int32_t heapUsed;

    // Initialise the guard areas at either end of the buffers
    initGuards();
    U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferA);
    U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferA);
    U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferB);
    U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferB);

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();

    // On some platforms (e.g. ESP32) the crypto libraries,
    // which the underlying chip-to-chip encryption functions
    // call, allocate a semaphore when they are first called
    // which is never deleted.  To avoid that getting in their
    // way of our heap loss calculation, make a call to one
    // of the crypto functions here.
    uPortCryptoSha256(NULL, 0, gBufferA + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES);

    heapUsed = uPortGetHeapFree();
    U_PORT_TEST_ASSERT(uPortInit() == 0);

    U_TEST_PRINT_LINE("testing chip-to-chip encryption and decryption"
                      " intercept functions standalone.");

    gContext.pTx = &gContextTx;
    gContext.pRx = &gContextRx;

    for (size_t x = 0; x < sizeof(gTestData) / sizeof(gTestData[0]); x++) {
        pTestData = &gTestData[x];
        totalLength = 0;
        for (size_t y = 0; y < (sizeof(pTestData->clearLength) /
                                sizeof(pTestData->clearLength[0])); y++) {
            totalLength += pTestData->clearLength[y];
        }
        uPortLog(U_TEST_PREFIX_X "clear text %d byte(s) \"", x + 1, totalLength);
        print(pTestData->pClear, totalLength);
        uPortLog("\".\n");

        // Populate context
        gContext.isV2 = pTestData->isV2;
        memcpy(gContext.teSecret, pTestData->pTeSecret,
               sizeof(gContext.teSecret));
        memcpy(gContext.key, pTestData->pKey,
               sizeof(gContext.key));
        if (pTestData->pHmacTag != NULL) {
            memcpy(gContext.hmacKey, pTestData->pHmacTag,
                   sizeof(gContext.hmacKey));
        }
        gContext.pTx->txInLength = 0;
        gContext.pTx->txInLimit = pTestData->chunkLengthMax;

        memcpy(gBufferA + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES,
               pTestData->pClear, totalLength);
        pData = gBufferA + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES;
        numChunks = 0;
        pDataStart = pData;

        // Do the encryption by calling the transmit intercept
        do {
            U_PORT_TEST_ASSERT(numChunks < pTestData->numChunks);
            outLength = totalLength - (pData - pDataStart);
            pOut = pUCellSecC2cInterceptTx(0, &pData, &outLength,
                                           &gContext);
            if (outLength > 0) {
                // There will only be a result here if the input reached
                // the chunk length limit
                checkEncrypted(x, numChunks, pOut, outLength, pTestData);
                numChunks++;
            }
        } while (pData < gBufferA + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES + totalLength);

        U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferA);
        U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferA);
        U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferB);
        U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferB);

        // Flush the transmit intercept by calling it again with NULL
        outLength = 0;
        pOut = pUCellSecC2cInterceptTx(0, NULL, &outLength, &gContext);
        if (outLength > 0) {
            checkEncrypted(x, numChunks, pOut, outLength, pTestData);
            numChunks++;
        }

        U_PORT_TEST_ASSERT(numChunks == pTestData->numChunks);
        // When done, the RX buffer should contain the complete
        // clear message
        U_PORT_TEST_ASSERT(memcmp(gBufferB + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES,
                                  pTestData->pClear, totalLength) == 0);

        U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferA);
        U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferA);
        U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferB);
        U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferB);
    }

    uPortDeinit();

#ifndef __XTENSA__
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
#else
    (void) heapUsed;
#endif
}

#if (U_CFG_TEST_UART_A >= 0) && (U_CFG_TEST_UART_B >= 0)

/** Test use of the intercept functions inside the AT client
 * with a dummy AT server to loop stuff back to us.
 * NOTE: this test is a bit of a balancing act; need to
 * print lots of debug so that we can see what's going on
 * in case there's a problem but at the same time it has two
 * independent tasks running between two actual serial ports
 * without flow control (out of pins) and with deliberate
 * timing constraints in the AT client.  So, it works, but
 * I suggest you don't fiddle with any of the timings, it's
 * quite carefully tuned to work on all platforms.
 */
U_PORT_TEST_FUNCTION("[cellSecC2c]", "cellSecC2cAtClient")
{
    uAtClientHandle_t atClientHandle;
    int32_t sizeOrError;
    const uCellSecC2cTestAt_t *pTestAt = NULL;
    const char *pLastAtPrefix = NULL;
    size_t urcCount = 0;
    int32_t stackMinFreeBytes;
    int32_t heapUsed;
    int32_t heapClibLossOffset = (int32_t) gSystemHeapLost;
    int32_t y;

    // Initialise the guard areas at either end of the buffers
    initGuards();
    U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferA);
    U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferA);
    U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferB);
    U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferB);
    U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferC);
    U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferC);

    gContext.pTx = &gContextTx;
    gContext.pRx = &gContextRx;

    // Whatever called us likely initialised the
    // port so deinitialise it here to obtain the
    // correct initial heap size
    uPortDeinit();

    // On some platforms (e.g. ESP32) the crypto libraries,
    // which the underlying chip-to-chip encryption functions
    // call, allocate a semaphore when they are first called
    // which is never deleted.  To avoid that getting in their
    // way of our heap loss calculation, make a call to one
    // of the crypto functions here.
    uPortCryptoSha256(NULL, 0, gBufferA + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES);

    heapUsed = uPortGetHeapFree();

    U_TEST_PRINT_LINE("testing chip-to-chip encryption and decryption"
                      " intercept functions inside an AT client.");

    U_PORT_TEST_ASSERT(uPortInit() == 0);

    gUartAHandle = uPortUartOpen(U_CFG_TEST_UART_A,
                                 U_CFG_TEST_BAUD_RATE,
                                 NULL,
                                 U_CFG_TEST_UART_BUFFER_LENGTH_BYTES,
                                 U_CFG_TEST_PIN_UART_A_TXD,
                                 U_CFG_TEST_PIN_UART_A_RXD,
                                 U_CFG_TEST_PIN_UART_A_CTS,
                                 U_CFG_TEST_PIN_UART_A_RTS);
    U_PORT_TEST_ASSERT(gUartAHandle >= 0);

    U_TEST_PRINT_LINE("AT client will be on UART %d, TXD pin %d (0x%02x)"
                      " and RXD pin %d (0x%02x).",
                      U_CFG_TEST_UART_A, U_CFG_TEST_PIN_UART_A_TXD,
                      U_CFG_TEST_PIN_UART_A_TXD, U_CFG_TEST_PIN_UART_A_RXD,
                      U_CFG_TEST_PIN_UART_A_RXD);

    gUartBHandle = uPortUartOpen(U_CFG_TEST_UART_B,
                                 U_CFG_TEST_BAUD_RATE,
                                 NULL,
                                 U_CFG_TEST_UART_BUFFER_LENGTH_BYTES,
                                 U_CFG_TEST_PIN_UART_B_TXD,
                                 U_CFG_TEST_PIN_UART_B_RXD,
                                 U_CFG_TEST_PIN_UART_B_CTS,
                                 U_CFG_TEST_PIN_UART_B_RTS);
    U_PORT_TEST_ASSERT(gUartBHandle >= 0);

    U_TEST_PRINT_LINE("AT server will be on UART %d, TXD pin %d (0x%02x)"
                      " and RXD pin %d (0x%02x).",
                      U_CFG_TEST_UART_B, U_CFG_TEST_PIN_UART_B_TXD,
                      U_CFG_TEST_PIN_UART_B_TXD, U_CFG_TEST_PIN_UART_B_RXD,
                      U_CFG_TEST_PIN_UART_B_RXD);

    U_TEST_PRINT_LINE("make sure these pins are cross-connected.");

    // Set up an AT server event handler on UART B
    // This event handler receives our encrypted chunks, decrypts
    // them and sends back an encrypted response for us to decrypt.
    U_PORT_TEST_ASSERT(uPortUartEventCallbackSet(gUartBHandle,
                                                 U_PORT_UART_EVENT_BITMASK_DATA_RECEIVED,
                                                 atServerCallback, (void *) &pTestAt,
                                                 U_CELL_SEC_C2C_TEST_TASK_STACK_SIZE_BYTES,
                                                 U_CELL_SEC_C2C_TEST_TASK_PRIORITY) == 0);

    U_PORT_TEST_ASSERT(uAtClientInit() == 0);

    U_TEST_PRINT_LINE("adding an AT client on UART %d...", U_CFG_TEST_UART_A);
    atClientHandle = uAtClientAdd(gUartAHandle, U_AT_CLIENT_STREAM_TYPE_UART,
                                  NULL, U_CELL_AT_BUFFER_LENGTH_BYTES);
    U_PORT_TEST_ASSERT(atClientHandle != NULL);

    // Add transmit and receive intercepts
    uAtClientStreamInterceptTx(atClientHandle, pUCellSecC2cInterceptTx,
                               (void *) &gContext);
    uAtClientStreamInterceptRx(atClientHandle, pUCellSecC2cInterceptRx,
                               (void *) &gContext);

    U_TEST_PRINT_LINE("%d chunks(s) to execute.", sizeof(gTestAt) / sizeof(gTestAt[0]));
    for (size_t x = 0; x < sizeof(gTestAt) / sizeof(gTestAt[0]); x++) {
        pTestAt = &(gTestAt[x]);

        // Populate the AT client-side chip to chip
        // security context
        gContext.isV2 = pTestAt->isV2;
        memcpy(gContext.teSecret, pTestAt->pTeSecret,
               sizeof(gContext.teSecret));
        memcpy(gContext.key, pTestAt->pKey,
               sizeof(gContext.key));
        if (pTestAt->pHmacTag != NULL) {
            memcpy(gContext.hmacKey, pTestAt->pHmacTag,
                   sizeof(gContext.hmacKey));
        }
        gContext.pTx->txInLimit = pTestAt->chunkLengthMax;

        // Copy this into the AT server-side chip to chip
        // security context
        memcpy(&gAtServerContext, &gContext,
               sizeof(gAtServerContext));
        gAtServerContext.pRx = &gAtServerContextRx;
        gAtServerContext.pTx = &gAtServerContextTx;

        if (pTestAt->pUrcPrefix != NULL) {
            urcCount++;
        }
        // Add a URC handler if there is one, removing the old one
        if (pTestAt->pUrcPrefix != NULL) {
            if (pLastAtPrefix != NULL) {
                uAtClientRemoveUrcHandler(atClientHandle, pLastAtPrefix);
            }
            // GCC can complain here that
            // we're passing a const pointer
            // (pTestAt) as a parameter
            // that is not const.
            // Since this is an anonymous parameter
            // passed to a callback we have no
            // choice, the callback itself
            // has to know how to behave, we
            // can't dictate that.
            U_PORT_TEST_ASSERT(uAtClientSetUrcHandler(atClientHandle,
                                                      pTestAt->pUrcPrefix,
                                                      urcHandler,
                                                      //lint -e(1773) Suppress "attempt to cast away const":
                                                      // pTestAt definitely points to a const but the URC handler
                                                      // parameter is a generic part of a callback which can't know
                                                      // that.
                                                      (void *) pTestAt) == 0);
            pLastAtPrefix = pTestAt->pUrcPrefix;
        }

        // Send the AT string: we only test sending strings or
        // binary here, the other uAtClientWritexxx
        // operations are assumed to work in the same way
        U_TEST_PRINT_LINE_X("AT client sending: \"%s\" and then...",
                            x + 1, pTestAt->pCommandPrefix);
        printBlock(pTestAt->pCommandBody,
                   pTestAt->commandBodyLength,
                   pTestAt->isBinary, x + 1);

        U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferA);
        U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferA);
        U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferB);
        U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferB);
        U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferC);
        U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferC);

        uAtClientLock(atClientHandle);

        // We do a LOT of debug prints in the AT server task which responds
        // to this and we have to take our time with them so as not to
        // overload the debug stream on some platforms so give this plenty
        // time: enough time for the command to get there and be printed
        // out, and the response to be printed out and then received and
        // printed out
        y = 20000 + (pTestAt->commandWaitTimeSeconds * 1000 * 2) +
            (pTestAt->responseWaitTimeSeconds * 1000 * 3);
        U_TEST_PRINT_LINE_X("AT timeout set to %d ms.", x + 1, y);
        uAtClientTimeoutSet(atClientHandle, y);
        y = (int32_t) uPortGetTickTimeMs();
        uAtClientCommandStart(atClientHandle, pTestAt->pCommandPrefix);
        if (pTestAt->isBinary) {
            // Binary bytes
            U_PORT_TEST_ASSERT(uAtClientWriteBytes(atClientHandle, pTestAt->pCommandBody,
                                                   pTestAt->commandBodyLength, false) ==
                               pTestAt->commandBodyLength);
        } else {
            // String without quotes
            uAtClientWriteString(atClientHandle, pTestAt->pCommandBody, false);
        }
        uAtClientCommandStop(atClientHandle);

        uPortLog(U_TEST_PREFIX_X "AT client send took %d ms, waiting for response",
                 x + 1, ((int32_t) uPortGetTickTimeMs()) - y);
        if (pTestAt->pResponsePrefix != NULL) {
            uPortLog(" \"%s\"", pTestAt->pResponsePrefix);
        }
        uPortLog("...\n");
        y = (int32_t) uPortGetTickTimeMs();

        uAtClientResponseStart(atClientHandle, pTestAt->pResponsePrefix);
        if (pTestAt->isBinary) {
            // Standalone bytes
            sizeOrError = uAtClientReadBytes(atClientHandle,
                                             gBufferB + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES,
                                             sizeof(gBufferB) - (U_CELL_SEC_C2C_GUARD_LENGTH_BYTES * 2),
                                             true);
        } else {
            // Quoted string
            sizeOrError = uAtClientReadString(atClientHandle, gBufferB + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES,
                                              sizeof(gBufferB) - (U_CELL_SEC_C2C_GUARD_LENGTH_BYTES * 2),
                                              false);
        }
        uAtClientResponseStop(atClientHandle);

        U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferA);
        U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferA);
        U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferB);
        U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferB);
        U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferC);
        U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferC);

        // Wait a moment before printing so that any URCs get to
        // be printed without us trampling over them
        uPortTaskBlock(1000);
        U_TEST_PRINT_LINE_X("AT client read result (after %d ms wait) is %d.",
                            x + 1, ((int32_t) uPortGetTickTimeMs()) - y, sizeOrError);
        U_PORT_TEST_ASSERT(sizeOrError >= 0);
        U_TEST_PRINT_LINE_X("AT client received response:", x + 1);
        if (sizeOrError > 0) {
            if (pTestAt->pResponsePrefix != NULL) {
                U_TEST_PRINT_LINE_X("\"%s\" and then...", x + 1, pTestAt->pResponsePrefix);
            }
            printBlock(gBufferB + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES, sizeOrError,
                       pTestAt->isBinary, x + 1);
        } else {
            U_TEST_PRINT_LINE_X("[nothing]", x + 1);
        }

        U_PORT_TEST_ASSERT(uAtClientUnlock(atClientHandle) == 0);

        U_PORT_TEST_ASSERT(sizeOrError == pTestAt->responseBodyLength);
        if (sizeOrError > 0) {
            U_PORT_TEST_ASSERT(memcmp(gBufferB + U_CELL_SEC_C2C_GUARD_LENGTH_BYTES,
                                      pTestAt->pResponseBody,
                                      pTestAt->responseBodyLength) == 0);
        }

        U_PORT_TEST_ASSERT(gAtServerErrorOrSize >= 0);
        U_PORT_TEST_ASSERT(gUrcErrorOrSize >= 0);
        U_PORT_TEST_ASSERT(urcCount == gUrcCount);
        U_TEST_PRINT_LINE_X("...and then \"OK\"", x + 1);

        U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferA);
        U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferA);
        U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferB);
        U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferB);
        U_CELL_SEC_C2C_CHECK_GUARD_UNDERRUN(gBufferC);
        U_CELL_SEC_C2C_CHECK_GUARD_OVERRUN(gBufferC);

        gAtTestCount++;
        // Wait between iterations to avoid the debug
        // streams overunning
        uPortTaskBlock(1000);
    }
    U_PORT_TEST_ASSERT(gAtTestCount == sizeof(gTestAt) / sizeof(gTestAt[0]));

    stackMinFreeBytes = uAtClientUrcHandlerStackMinFree(atClientHandle);
    if (stackMinFreeBytes != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_TEST_PRINT_LINE("AT client URC task had min %d byte(s)"
                          " stack free out of %d.", stackMinFreeBytes,
                          U_CELL_SEC_C2C_TEST_TASK_STACK_SIZE_BYTES);
        U_PORT_TEST_ASSERT(stackMinFreeBytes > 0);
    }

    stackMinFreeBytes = uAtClientCallbackStackMinFree();
    if (stackMinFreeBytes != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_TEST_PRINT_LINE("AT client callback task had min %d byte(s)"
                          " stack free out of %d.", stackMinFreeBytes,
                          U_AT_CLIENT_CALLBACK_TASK_STACK_SIZE_BYTES);
        U_PORT_TEST_ASSERT(stackMinFreeBytes > 0);
    }

    // Check the stack extent for the task on the end of the
    // event queue
    stackMinFreeBytes = uPortUartEventStackMinFree(gUartBHandle);
    if (stackMinFreeBytes != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_TEST_PRINT_LINE("the AT server event queue task had %d byte(s)"
                          " free out of %d.", stackMinFreeBytes,
                          U_CELL_SEC_C2C_TEST_TASK_STACK_SIZE_BYTES);
        U_PORT_TEST_ASSERT(stackMinFreeBytes > 0);
    }

    U_TEST_PRINT_LINE("removing AT client...");
    uAtClientRemove(atClientHandle);
    uAtClientDeinit();

    uPortUartClose(gUartBHandle);
    gUartBHandle = -1;
    uPortUartClose(gUartAHandle);
    gUartAHandle = -1;
    uPortDeinit();

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

#endif

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[cellSecC2c]", "cellSecC2cCleanUp")
{
    int32_t minFreeStackBytes;

    uAtClientDeinit();
    if (gUartAHandle >= 0) {
        uPortUartClose(gUartAHandle);
    }
    if (gUartBHandle >= 0) {
        uPortUartClose(gUartBHandle);
    }

    minFreeStackBytes = uPortTaskStackMinFree(NULL);
    if (minFreeStackBytes != (int32_t) U_ERROR_COMMON_NOT_SUPPORTED) {
        U_TEST_PRINT_LINE("main task stack had a minimum of %d byte(s)"
                          " free at the end of these tests.", minFreeStackBytes);
        U_PORT_TEST_ASSERT(minFreeStackBytes >=
                           U_CFG_TEST_OS_MAIN_TASK_MIN_FREE_STACK_BYTES);
    }

    uPortDeinit();
}

// End of file
