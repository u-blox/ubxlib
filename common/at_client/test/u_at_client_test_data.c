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
 * @brief Test data for the AT client tests.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // strlen(), memcmp()
#include "stdio.h"     // snprintf()
#include "ctype.h"     // isprint()

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port_clib_platform_specific.h" /* Integer stdio, must be included
                                              before the other port files if
                                              any print or scan function is used. */
#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"

#include "u_at_client.h"
#include "u_at_client_test.h"
#include "u_at_client_test_data.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test, with
 * an iteration on the end.
 */
#define U_TEST_PREFIX_X "U_AT_CLIENT_TEST_%d: "

/** Print a whole line, with iteration and terminator, prefixed for
 * this test file.
 */
#define U_TEST_PRINT_LINE_X(format, ...) uPortLog(U_TEST_PREFIX_X format "\n", ##__VA_ARGS__)

/** A standard AT response prefix to use in testing.
 */
#define U_AT_CLIENT_TEST_PREFIX "+PREFIX:"

/** The number of characters in U_AT_CLIENT_TEST_PREFIX.
 */
#define U_AT_CLIENT_TEST_PREFIX_LENGTH 8

/** Response string for skip params and skip bytes checking.
 * IMPORTANT: don't change this without also changing ALL of
 * the tests which use it.  Basically, don't change it.
 */
#define U_AT_CLIENT_TEST_ECHO_SKIP "\r\n" U_AT_CLIENT_TEST_PREFIX " string1,\"string2\","  \
                                   "18446744073709551615,2147483647,\x00\x7f\xff\r\nOK\r\n"

/** The number of characters in U_AT_CLIENT_TEST_ECHO_SKIP.
 */
#define U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH (U_AT_CLIENT_TEST_PREFIX_LENGTH + 62)

/** A string for testing a particular case of the "wait for single character" test
 * where a URC arrives at the same time as we are waiting for the character.
 */
#define U_AT_CLIENT_TEST_ECHO_WAIT "@" U_AT_CLIENT_TEST_PREFIX " \"string2\"\r\nOK\r\n"

/** The number of characters in U_AT_CLIENT_TEST_ECHO_WAIT.
 */
#define U_AT_CLIENT_TEST_ECHO_WAIT_LENGTH (U_AT_CLIENT_TEST_PREFIX_LENGTH + 17)

/** A test string.
 */
#define U_AT_CLIENT_TEST_STRING_THREE "string3"

/** Number of characters in U_AT_CLIENT_TEST_STRING_THREE.
 */
#define U_AT_CLIENT_TEST_STRING_THREE_LENGTH 7

/** A test byte array.
 */
#define U_AT_CLIENT_TEST_BYTES_TWO "\x01\x8f\x1f"

/** Number of bytes in U_AT_CLIENT_TEST_BYTES_TWO.
 */
#define U_AT_CLIENT_TEST_BYTES_TWO_LENGTH 3

/** When testing timeouts we start a timer when waiting for the
 * response whereas the timer actually starts when the AT client
 * is locked so allow a tolerance because of that.
 */
#define U_AT_CLIENT_TEST_TIMEOUT_TOLERANCE_MS 5

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

#if (U_CFG_TEST_UART_A >= 0) && (U_CFG_TEST_UART_B >= 0)

/** A URC consisting of a single int32_t, to be referenced in
 * gAtClientTestSet1 or gAtClientTestSet2.
 */
//lint -e{785} Suppress too few initialisers
static const uAtClientTestResponseLine_t gAtClientUrc0 = {"+URC0:", 1,
    {{10, "2147483647"}},
    {{U_AT_CLIENT_TEST_PARAMETER_INT32, {.int32 = INT32_MAX}, 0}}
};

/** A URC consisting of a single uint64_t, to be referenced in
 * gAtClientTestSet1 or gAtClientTestSet2.
 */
//lint -e{785} Suppress too few initialisers
static const uAtClientTestResponseLine_t gAtClientUrc1 = {"+URC1:", 1,
    {{20, "18446744073709551615"}},
    {{U_AT_CLIENT_TEST_PARAMETER_UINT64, {.uint64 = UINT64_MAX}, 0}}
};

/** A URC consisting of a single quoted string, to be referenced in
 * gAtClientTestSet1 or gAtClientTestSet2.
 */
//lint -e{785} Suppress too few initialisers
static const uAtClientTestResponseLine_t gAtClientUrc2 = {"+URC2:", 1,
    {{46, "\"The quick brown fox jumps over the lazy dog.\""}},
    {{U_AT_CLIENT_TEST_PARAMETER_STRING, {.pString = "The quick brown fox jumps over the lazy dog."}, 0}}
};

/** A URC consisting of a single unquoted string, to be referenced in
 * gAtClientTestSet1 or gAtClientTestSet2.
 */
//lint -e{785} Suppress too few initialisers
static const uAtClientTestResponseLine_t gAtClientUrc3 = {"+URC3:", 1,
    {{44, "The quick brown fox jumps over the lazy dog."}},
    {{U_AT_CLIENT_TEST_PARAMETER_STRING, {.pString = "The quick brown fox jumps over the lazy dog."}, 0}}
};

/** A URC consisting of a single byte array, to be referenced in
 * gAtClientTestSet1 or gAtClientTestSet2.
 */
//lint -e{785} Suppress too few initialisers
static const uAtClientTestResponseLine_t gAtClientUrc4 = {"+URC4:", 1,
    {{3, "\x00\x7f\xff"}},
    {{U_AT_CLIENT_TEST_PARAMETER_BYTES, {.pBytes = "\x00\x7f\xff"}, 3}}
};

/** A URC with a bit of everything, to be referenced in
 * gAtClientTestSet1 or gAtClientTestSet2.
 */
//lint -e{785} Suppress too few initialisers
static const uAtClientTestResponseLine_t gAtClientUrc5 = {"+URC5:", 5,
    {{3, "\xff\x7f\x00"}, {1, "0"}, {20, "18446744073709551615"}, {7, "\"Bing.\""}, {5, "Bong."}},
    {   {U_AT_CLIENT_TEST_PARAMETER_BYTES, {.pBytes = "\xff\x7f\x00"}, 3},
        {U_AT_CLIENT_TEST_PARAMETER_INT32, {.int32 = 0}, 0},
        {U_AT_CLIENT_TEST_PARAMETER_UINT64, {.uint64 = UINT64_MAX}, 0},
        {U_AT_CLIENT_TEST_PARAMETER_STRING, {.pString = "Bing."}, 0},
        {U_AT_CLIENT_TEST_PARAMETER_STRING, {.pString = "Bong."}, 0}
    }
};

/** Parameters for skip params test, first iteration,
 * to be referenced in gAtClientTestSet2.
 */
static const uAtClientTestEchoSkipParams_t gAtClientTestEchoSkipParams0 = {U_AT_CLIENT_TEST_PREFIX, 5, 0,
    {
        U_AT_CLIENT_TEST_PARAMETER_STRING,
        {.pString = "string1"}, 0
    }
};

/** Parameters for skip params test, second iteration,
 * to be referenced in gAtClientTestSet2.
 */
static const uAtClientTestEchoSkipParams_t gAtClientTestEchoSkipParams1 = {U_AT_CLIENT_TEST_PREFIX, 5, 1,
    {
        U_AT_CLIENT_TEST_PARAMETER_STRING,
        {.pString = "string2"}, 0
    }
};

/** Parameters for skip params test, third iteration,
 * to be referenced in gAtClientTestSet2.
 */
static const uAtClientTestEchoSkipParams_t gAtClientTestEchoSkipParams2 = {U_AT_CLIENT_TEST_PREFIX, 5, 2,
    {
        U_AT_CLIENT_TEST_PARAMETER_UINT64,
        {.uint64 = UINT64_MAX}, 0
    }
};

/** Parameters for skip params test, fourth iteration,
 * to be referenced in gAtClientTestSet2.
 */
static const uAtClientTestEchoSkipParams_t gAtClientTestEchoSkipParams3 = {U_AT_CLIENT_TEST_PREFIX, 5, 3,
    {
        U_AT_CLIENT_TEST_PARAMETER_INT32,
        {.int32 = INT32_MAX}, 0
    }
};

/** Parameters for skip params test, fifth iteration,
 * to be referenced in gAtClientTestSet2.
 */
static const uAtClientTestEchoSkipParams_t gAtClientTestEchoSkipParams4 = {U_AT_CLIENT_TEST_PREFIX, 5, 4,
    {
        U_AT_CLIENT_TEST_PARAMETER_BYTES,
        {.pBytes = "\x00\x7f\xff"}, 3
    }
};

/** Parameters for skip bytes test, first iteration,
 * to be referenced in gAtClientTestSet2.
 * Skips the first two bytes of the first parameter.
 */
//lint -e{785} Suppress too few initialisers
//lint -e{708} Suppress union initialisation
static const uAtClientTestEchoSkipBytes_t gAtClientTestEchoSkipBytes0 = {U_AT_CLIENT_TEST_PREFIX, 5, 0, 2,
    {
        {
            U_AT_CLIENT_TEST_PARAMETER_STRING,
            {.pString = "ring1"}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_STRING,
            {.pString = "string2"}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_UINT64,
            {.uint64 = UINT64_MAX}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_INT32,
            {.int32 = INT32_MAX}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_BYTES,
            {.pBytes = "\x00\x7f\xff"}, 3
        }
    }
};

/** Parameters for skip bytes test, second iteration,
 * to be referenced in gAtClientTestSet2.
 * Reads part of the first parameter then skips nine bytes,
 * which should take out all of the second parameter
 * leaving an empty string, then reads the remaining
 * parameters.
 */
//lint -e{785} Suppress too few initialisers
//lint -e{708} Suppress union initialisation
static const uAtClientTestEchoSkipBytes_t gAtClientTestEchoSkipBytes1 = {U_AT_CLIENT_TEST_PREFIX, 5, 1, 9,
    {
        {
            U_AT_CLIENT_TEST_PARAMETER_STRING,
            {.pString = "str"}, 4 // Includes room for terminator
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_STRING,
            {.pString = ""}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_UINT64,
            {.uint64 = UINT64_MAX}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_INT32,
            {.int32 = INT32_MAX}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_BYTES,
            {.pBytes = "\x00\x7f\xff"}, 3
        }
    }
};

/** Parameters for skip bytes test, third iteration,
 * to be referenced in gAtClientTestSet2.
 * Reads the first two parameters then skips part of the
 * uint64_t resulting in a different value.
 */
//lint -e{785} Suppress too few initialisers
//lint -e{708} Suppress union initialisation
static const uAtClientTestEchoSkipBytes_t gAtClientTestEchoSkipBytes2 = {U_AT_CLIENT_TEST_PREFIX, 5, 2, 18,
    {
        {
            U_AT_CLIENT_TEST_PARAMETER_STRING,
            {.pString = "string1"}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_STRING,
            {.pString = "string2"}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_UINT64,
            {.uint64 = 15}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_INT32,
            {.int32 = INT32_MAX}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_BYTES,
            {.pBytes = "\x00\x7f\xff"}, 3
        }
    }
};

/** As above but absorb the delimiter also, resulting
 * in one fewer parameters and a different value.
 */
//lint -e{785} Suppress too few initialisers
//lint -e{708} Suppress union initialisation
static const uAtClientTestEchoSkipBytes_t gAtClientTestEchoSkipBytes3 = {U_AT_CLIENT_TEST_PREFIX, 4, 2, 30,
    {
        {
            U_AT_CLIENT_TEST_PARAMETER_STRING,
            {.pString = "string1"}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_STRING,
            {.pString = "string2"}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_INT32,
            {.int32 = 7}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_BYTES,
            {.pBytes = "\x00\x7f\xff"}, 3
        }
    }
};

/** As the third iteration but this time fiddling with the
 * int32_t value.
 */
//lint -e{785} Suppress too few initialisers
//lint -e{708} Suppress union initialisation
static const uAtClientTestEchoSkipBytes_t gAtClientTestEchoSkipBytes4 = {U_AT_CLIENT_TEST_PREFIX, 5, 3, 1,
    {
        {
            U_AT_CLIENT_TEST_PARAMETER_STRING,
            {.pString = "string1"}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_STRING,
            {.pString = "string2"}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_UINT64,
            {.uint64 = UINT64_MAX}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_INT32,
            {.int32 = 147483647}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_BYTES,
            {.pBytes = "\x00\x7f\xff"}, 3
        }
    }
};

/** Finally, remove characters from the byte array.
 */
//lint -e{785} Suppress too few initialisers
//lint -e{708} Suppress union initialisation
static const uAtClientTestEchoSkipBytes_t gAtClientTestEchoSkipBytes5 = {U_AT_CLIENT_TEST_PREFIX, 5, 4, 2,
    {
        {
            U_AT_CLIENT_TEST_PARAMETER_STRING,
            {.pString = "string1"}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_STRING,
            {.pString = "string2"}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_UINT64,
            {.uint64 = UINT64_MAX}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_INT32,
            {.int32 = INT32_MAX}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_BYTES,
            {.pBytes = "\xff"}, 1
        }
    }
};

/** Parameters for early stop test, first iteration,
 * to be referenced in gAtClientTestSet2.
 * Reads no parameters at all.
 */
//lint -e{785} Suppress too few initialisers
static const uAtClientTestEchoEarlyStop_t gAtClientTestEchoEarlyStop0 = {U_AT_CLIENT_TEST_PREFIX, 0};

/** Parameters for early stop test, second iteration,
 * to be referenced in gAtClientTestSet2.
 * Reads just the first parameter.
 */
//lint -e{785} Suppress too few initialisers
//lint -e{708} Suppress union initialisation
static const uAtClientTestEchoEarlyStop_t gAtClientTestEchoEarlyStop1 = {U_AT_CLIENT_TEST_PREFIX, 1,
    {
        {
            U_AT_CLIENT_TEST_PARAMETER_STRING,
            {.pString = "string1"}, 0
        }
    }
};

/** Parameters for early stop test, third iteration,
 * to be referenced in gAtClientTestSet2.
 * Reads just the first two parameters.
 */
//lint -e{785} Suppress too few initialisers
//lint -e{708} Suppress union initialisation
static const uAtClientTestEchoEarlyStop_t gAtClientTestEchoEarlyStop2 = {U_AT_CLIENT_TEST_PREFIX, 2,
    {
        {
            U_AT_CLIENT_TEST_PARAMETER_STRING,
            {.pString = "string1"}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_STRING,
            {.pString = "string2"}, 0
        }
    }
};

/** Parameters for early stop test, fourth iteration,
 * to be referenced in gAtClientTestSet2.
 * Reads just the first three parameters.
 */
//lint -e{785} Suppress too few initialisers
//lint -e{708} Suppress union initialisation
static const uAtClientTestEchoEarlyStop_t gAtClientTestEchoEarlyStop3 = {U_AT_CLIENT_TEST_PREFIX, 3,
    {
        {
            U_AT_CLIENT_TEST_PARAMETER_STRING,
            {.pString = "string1"}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_STRING,
            {.pString = "string2"}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_UINT64,
            {.uint64 = UINT64_MAX}, 0
        }
    }
};

/** Parameters for early stop test, fifth iteration,
 * to be referenced in gAtClientTestSet2.
 * Reads just the first four parameters.
 */
//lint -e{785} Suppress too few initialisers
//lint -e{708} Suppress union initialisation
static const uAtClientTestEchoEarlyStop_t gAtClientTestEchoEarlyStop4 = {U_AT_CLIENT_TEST_PREFIX, 4,
    {
        {
            U_AT_CLIENT_TEST_PARAMETER_STRING,
            {.pString = "string1"}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_STRING,
            {.pString = "string2"}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_UINT64,
            {.uint64 = UINT64_MAX}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_INT32,
            {.int32 = INT32_MAX}, 0
        }
    }
};

/** Parameters for "wait for char" test, to be
 * referenced in gAtClientTestSet2.
 * Wait for the '@' character at the start, the point
 * being to check that the URC which should arrive at the
 * same time is handled correctly; this is intended to work
 * with the string U_AT_CLIENT_TEST_ECHO_WAIT.
 */
//lint -e{785} Suppress too few initialisers
//lint -e{708} Suppress union initialisation
static const uAtClientTestEchoWaitForChar_t gAtClientTestEchoWaitForChar0 = {U_AT_CLIENT_TEST_PREFIX, '@', 1,
    {
        {
            U_AT_CLIENT_TEST_PARAMETER_STRING,
            {.pString = "string2"}, 0
        }
    }
};

/** Parameters for error test where no timeout is expected,
 * to be referenced in gAtClientTestSet2.
 */
static const uAtClientTestEchoError_t gAtClientTestEchoNoTimeout = {1000, 0, 1500};

/** Parameters for error test where a timeout is expected,
 * to be referenced in gAtClientTestSet2.  Make sure that the timeout
 * number here is different to (smaller than) U_AT_CLIENT_TEST_TIMEOUT_MS.
 */
static const uAtClientTestEchoError_t gAtClientTestEchoTimeout = {1000,
                                                                  1000 - U_AT_CLIENT_TEST_TIMEOUT_TOLERANCE_MS,
                                                                  1500
                                                                 };

/** Parameters for misc test, matches U_AT_CLIENT_TEST_ECHO_SKIP
 * and gAtClientUrc5, to be referenced in gAtClientTestSet2.
 */
//lint -e{785} Suppress too few initialisers
//lint -e{708} Suppress union initialisation
static const uAtClientTestEchoMisc_t gAtClientTestEchoMisc = { U_AT_CLIENT_TEST_PREFIX, 5,
    {
        {
            U_AT_CLIENT_TEST_PARAMETER_STRING,
            {.pString = "string1"}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_STRING,
            {.pString = "string2"}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_UINT64,
            {.uint64 = UINT64_MAX}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_INT32,
            {.int32 = INT32_MAX}, 0
        },
        {
            U_AT_CLIENT_TEST_PARAMETER_BYTES,
            {.pBytes = "\x00\x7f\xff"}, 3
        }
    },
    &gAtClientUrc5
};

#endif

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

#if (U_CFG_TEST_UART_A >= 0) && (U_CFG_TEST_UART_B >= 0)

// Function to check that whole parameters can be skipped, referenced
// by gAtClientTestSet2.
// pParameter is a pointer to uAtClientTestEchoSkipParams_t.
// Returns zero on success, else error.
static int32_t handleSkipParams(uAtClientHandle_t atClientHandle,
                                size_t index, const void *pParameter)
{
    int32_t lastError;
    const uAtClientTestEchoSkipParams_t *pSkipParams;
    char buffer[5]; // Enough characters for a 3 digit index as a string

    snprintf(buffer, sizeof(buffer), "_%d", index + 1);
    pSkipParams = (const uAtClientTestEchoSkipParams_t *) pParameter;

    U_TEST_PRINT_LINE_X("checking that uAtClientSkipParameters()"
                        " works on parameter %d of %d parameter(s).",
                        index + 1, pSkipParams->paramNotSkipIndex + 1,
                        pSkipParams->numParameters);

    // Begin processing the response
    uAtClientResponseStart(atClientHandle, pSkipParams->pPrefix);

    // Skip any initial parameters
    if (pSkipParams->paramNotSkipIndex > 0) {
        U_TEST_PRINT_LINE_X("skipping %d parameter(s)...", index + 1,
                            pSkipParams->paramNotSkipIndex);
        uAtClientSkipParameters(atClientHandle,
                                pSkipParams->paramNotSkipIndex);
    }

    // Check that the non-skipped parameter is as expected
    lastError = uAtClientTestCheckParam(atClientHandle, &(pSkipParams->parameter), buffer);

    // Skip any remaining parameters
    if (pSkipParams->paramNotSkipIndex + 1 < pSkipParams->numParameters) {
        U_TEST_PRINT_LINE_X("skipping %d parameter(s)...\n", index + 1,
                            pSkipParams->numParameters - (pSkipParams->paramNotSkipIndex + 1));
        uAtClientSkipParameters(atClientHandle,
                                pSkipParams->numParameters - (pSkipParams->paramNotSkipIndex + 1));
    }

    // Finish off
    uAtClientResponseStop(atClientHandle);

    return lastError;
}

// Function to check that bytes can be skipped, referenced
// by gAtClientTestSet2.
// pParameter is a pointer to uAtClientTestEchoSkipBytes_t.
// Returns zero on success, else error.
static int32_t handleSkipBytes(uAtClientHandle_t atClientHandle,
                               size_t index, const void *pParameter)
{
    int32_t lastError = 0;
    const uAtClientTestEchoSkipBytes_t *pSkipBytes;
    char buffer[5]; // Enough characters for a 3 digit index as a string

    snprintf(buffer, sizeof(buffer), "_%d", index + 1);
    pSkipBytes = (const uAtClientTestEchoSkipBytes_t *) pParameter;

    U_TEST_PRINT_LINE_X("checking that uAtClientSkipBytes() works on"
                        " parameter %d of %d parameter(s).",
                        index + 1, pSkipBytes->paramIndex + 1,
                        pSkipBytes->numParameters);

    // Begin processing the response
    uAtClientResponseStart(atClientHandle, pSkipBytes->pPrefix);

    // Read any initial parameters
    for (size_t x = 0; (x < pSkipBytes->paramIndex) && (lastError == 0); x++) {
        U_TEST_PRINT_LINE_X("reading parameter %d...", index + 1, x + 1);
        lastError = uAtClientTestCheckParam(atClientHandle,
                                            &(pSkipBytes->parameters[x]), buffer);
    }

    if (lastError == 0) {
        U_TEST_PRINT_LINE_X("skipping %d byte(s) in parameter %d...", index + 1,
                            pSkipBytes->skipLength, pSkipBytes->paramIndex + 1);
        uAtClientSkipBytes(atClientHandle, pSkipBytes->skipLength);
    }

    if (lastError == 0) {
        // Read the rest of the parameters
        for (size_t x = pSkipBytes->paramIndex;
             (x < pSkipBytes->numParameters) && (lastError == 0); x++) {
            U_TEST_PRINT_LINE_X("reading parameter %d...", index + 1, x + 1);
            lastError = uAtClientTestCheckParam(atClientHandle,
                                                &(pSkipBytes->parameters[x]),
                                                buffer);
        }
    }

    // Finish off
    uAtClientResponseStop(atClientHandle);

    return lastError;
}

// Function to check that we can stop reading a response early,
// referenced by gAtClientTestSet2.
// pParameter is a pointer to uAtClientTestEchoEarlyStop_t.
// Returns zero on success, else error.
static int32_t handleEarlyStop(uAtClientHandle_t atClientHandle,
                               size_t index, const void *pParameter)
{
    int32_t lastError = 0;
    const uAtClientTestEchoEarlyStop_t *pEarlyStop;
    char buffer[5]; // Enough characters for a 3 digit index as a string

    snprintf(buffer, sizeof(buffer), "_%d", index + 1);
    pEarlyStop = (const uAtClientTestEchoEarlyStop_t *) pParameter;

    U_TEST_PRINT_LINE_X("checking that uAtClientResponseStop() can be called"
                        " after reading %d parameter(s).", index + 1,
                        pEarlyStop->numParameters);

    // Begin processing the response
    uAtClientResponseStart(atClientHandle, pEarlyStop->pPrefix);

    // Read the given number of parameters
    for (size_t x = 0; (x < pEarlyStop->numParameters) && (lastError == 0); x++) {
        U_TEST_PRINT_LINE_X("reading parameter %d...", index + 1, x + 1);
        lastError = uAtClientTestCheckParam(atClientHandle, &(pEarlyStop->parameters[x]), buffer);
    }

    // Finish off
    U_TEST_PRINT_LINE_X("calling uAtClientResponseStop()...", index + 1);
    uAtClientResponseStop(atClientHandle);

    return lastError;
}

// Function to check that we can wait for a character, referenced
// by gAtClientTestSet2.
// pParameter is a pointer to uAtClientTestEchoWaitForChar_t.
// Returns zero on success, else error.
static int32_t handleWaitForChar(uAtClientHandle_t atClientHandle,
                                 size_t index, const void *pParameter)
{
    int32_t lastError;
    const uAtClientTestEchoWaitForChar_t *pWaitForChar;
    char buffer[5]; // Enough characters for a 3 digit index as a string

    snprintf(buffer, sizeof(buffer), "_%d", index + 1);
    pWaitForChar = (const uAtClientTestEchoWaitForChar_t *) pParameter;

    uPortLog(U_TEST_PREFIX_X "checking that we can wait for character"
             " 0x%02x ", index + 1, pWaitForChar->character);
    if (isprint((int32_t) pWaitForChar->character)) {
        uPortLog("('%c') ", pWaitForChar->character);
    }
    uPortLog("between a command and a response, then read the response"
             " and the remaining %d parameter(s).\n", pWaitForChar->numParameters);

    // Wait for the character
    uPortLog(U_TEST_PREFIX_X "waiting for character 0x%02x", index + 1,
             pWaitForChar->character);
    if (isprint((int32_t) pWaitForChar->character)) {
        uPortLog(" ('%c')", pWaitForChar->character);
    }
    uPortLog("...\n");

    lastError = uAtClientWaitCharacter(atClientHandle, pWaitForChar->character);
    if (lastError == 0) {
        U_TEST_PRINT_LINE_X("received character 0x%02x.", index + 1,
                            pWaitForChar->character);
    } else {
        U_TEST_PRINT_LINE_X("character didn't turn up.", index + 1);
    }

    // Begin processing the response
    uAtClientResponseStart(atClientHandle, pWaitForChar->pPrefix);

    // Read the given number of parameters
    for (size_t x = 0; (x < pWaitForChar->numParameters) && (lastError == 0); x++) {
        U_TEST_PRINT_LINE_X("reading parameter %d...", index + 1, x + 1);
        lastError = uAtClientTestCheckParam(atClientHandle,
                                            &(pWaitForChar->parameters[x]),
                                            buffer);
    }

    // Finish off
    uAtClientResponseStop(atClientHandle);

    return lastError;
}

// Function to check that string/byte reads into NULL buffers
// (i.e. throwing stuff away) are successful, referenced by
// gAtClientTestSet2.  pParameter is unused.
// Returns zero on success, else error.
static int32_t handleNullBuffer(uAtClientHandle_t atClientHandle,
                                size_t index, const void *pParameter)
{
    int32_t lastError = 0;
    size_t readLength;
    int32_t y;
    char buffer[15]; // Enough characters for the short strings employed here
    // i.e. U_AT_CLIENT_TEST_STRING_THREE and
    // U_AT_CLIENT_TEST_BYTES_TWO

    (void) pParameter;
#if !U_CFG_ENABLE_LOGGING
    (void) index;
#endif

    U_TEST_PRINT_LINE_X("checking that string/byte reads into a NULL buffer work.",
                        index + 1);

    // Begin processing the response
    uAtClientResponseStart(atClientHandle, U_AT_CLIENT_TEST_PREFIX);

    // Read some of the string and throw it away
    readLength = 64;
    y = uAtClientReadString(atClientHandle, NULL, readLength, false);
    if (y != 7 /* The length of "string1" */) {
        U_TEST_PRINT_LINE_X("read of \" string1\" returned %d when 7 was"
                            " expected.", index + 1, y);
        lastError = 1;
    }

    // Read some of the second string and throw it away
    readLength = 3;
    y = uAtClientReadString(atClientHandle, NULL, readLength, false);
    if (y != readLength - 1 /* -1 for terminator */) {
        U_TEST_PRINT_LINE_X("string-read returned %d when %d was expected.",
                            index + 1, y, readLength - 1);
        lastError = 2;
    }

    // Read the third string and it should be present and correct
    y = uAtClientReadString(atClientHandle, buffer, sizeof(buffer), false);
    if ((y != U_AT_CLIENT_TEST_STRING_THREE_LENGTH) ||
        (strcmp(buffer, U_AT_CLIENT_TEST_STRING_THREE) != 0)) {
        uPortLog(U_TEST_PREFIX_X "string read returned \"", index + 1);
        uAtClientTestPrint(buffer, y);
        uPortLog("\" (%d characters) when \"%s\" (%d character(s))"
                 " was expected.\n", y, U_AT_CLIENT_TEST_STRING_THREE,
                 U_AT_CLIENT_TEST_STRING_THREE_LENGTH);
        lastError = 3;
    }

    // Read the first byte array and throw it away
    readLength = 3;
    y = uAtClientReadBytes(atClientHandle, NULL, readLength, false);
    if (y != readLength) {
        U_TEST_PRINT_LINE_X("byte-read returned %d when %d was expected.",
                            index + 1, y, readLength);
        lastError = 4;
    }

    // Read the second byte array and it should be present and correct
    y = uAtClientReadBytes(atClientHandle, buffer, sizeof(buffer), false);
    if ((y != U_AT_CLIENT_TEST_BYTES_TWO_LENGTH) ||
        (memcmp(buffer, U_AT_CLIENT_TEST_BYTES_TWO, y) != 0)) {
        uPortLog(U_TEST_PREFIX_X "byte read returned \"", index + 1);
        uAtClientTestPrint(buffer, y);
        uPortLog("\" (%d byte(s)) when \"", y);
        uAtClientTestPrint(U_AT_CLIENT_TEST_BYTES_TWO,
                           U_AT_CLIENT_TEST_BYTES_TWO_LENGTH);
        uPortLog("\" (%d byte(s)) was expected.\n", U_AT_CLIENT_TEST_BYTES_TWO_LENGTH);
        lastError = 5;
    }

    // Finish off
    uAtClientResponseStop(atClientHandle);

    return lastError;
}

// Function to check that attempts to read parameters when
// the AT server has returned an error fail correctly,
// referenced by gAtClientTestSet2.
// pParameter is a pointer to a uAtClientTestEchoError_t.
// Returns zero on success, else error.
static int32_t handleReadOnError(uAtClientHandle_t atClientHandle,
                                 size_t index, const void *pParameter)
{
    int32_t lastError;
    uint64_t uint64;
    int64_t startTime;
    int32_t duration;
    const uAtClientTestEchoError_t *pError;

#if !U_CFG_ENABLE_LOGGING
    (void) index;
#endif

    startTime = uPortGetTickTimeMs();
    pError = (const uAtClientTestEchoError_t *) pParameter;

    U_TEST_PRINT_LINE_X("checking that parameter reads return error when"
                        " they should.", index + 1);

    // Set the AT timeout
    uAtClientTimeoutSet(atClientHandle, pError->atTimeoutMs);

    // Begin processing the response
    uAtClientResponseStart(atClientHandle, NULL);

    lastError = uAtClientReadInt(atClientHandle);
    if (lastError >= 0) {
        U_TEST_PRINT_LINE_X("integer read returned value %d when it should"
                            " return error.", index + 1, lastError);
        lastError = 1;
    } else {
        lastError = 0;
    }
    if (lastError == 0) {
        lastError = uAtClientReadUint64(atClientHandle, &uint64);
        if (lastError >= 0) {
            U_TEST_PRINT_LINE_X("uint64_t read returned %d when it should"
                                " return error.", index + 1, lastError);
            lastError = 2;
        } else {
            lastError = 0;
        }
    }
    if (lastError == 0) {
        lastError = uAtClientReadString(atClientHandle, NULL, 5, false);
        if (lastError >= 0) {
            U_TEST_PRINT_LINE_X("string read returned value %d when it"
                                " should return error.", index + 1, lastError);
            lastError = 3;
        } else {
            lastError = 0;
        }
    }
    if (lastError == 0) {
        lastError = uAtClientReadBytes(atClientHandle, NULL, 5, false);
        if (lastError >= 0) {
            U_TEST_PRINT_LINE_X("string read returned value %d when it should"
                                " return error.", index + 1, lastError);
            lastError = 4;
        } else {
            lastError = 0;
        }
    }
    if (lastError == 0) {
        uAtClientResponseStop(atClientHandle);
        lastError = uAtClientUnlock(atClientHandle);
        if (lastError == 0) {
            U_TEST_PRINT_LINE_X("uAtClientUnlock() returned success when it!"
                                " should return error.", index + 1);
            lastError = 5;
        } else {
            lastError = 0;
        }
    }

    // The errors should be returned within the guard times
    duration = (int32_t) (uPortGetTickTimeMs() - startTime);
    if (lastError == 0) {
        if (duration < pError->timeMinMs) {
            U_TEST_PRINT_LINE_X("reads took %d ms when a minimum of %d ms was"
                                " expected.", index + 1, duration,
                                pError->timeMinMs);
            lastError = 6;
        }
    }

    if (lastError == 0) {
        if (duration > pError->timeMaxMs) {
            U_TEST_PRINT_LINE_X("reads took %d ms when a maximum of %d ms"
                                " was expected.", index + 1, duration,
                                pError->timeMaxMs);
            lastError = 7;
        }
    }

    // Finish off
    uAtClientResponseStop(atClientHandle);

    return lastError;
}

// Callback used by handleMiscUseLast().
static void atCallback(uAtClientHandle_t atClientHandle,
                       void *pParameter)
{
    (void) atClientHandle;

    *((bool *) pParameter) = true;
}

// URC handler used by handleMiscUseLast().
// pParameters should be a pointer to uAtClientTestCheckUrc_t
// containing a const * pointer to the definition
// (uAtClientTestResponseLine_t) of the URC sent while this
// URC handler is active.
//lint -e{818} suppress "could be declared as pointing to const",
// callback has to follow function signature
static void dumbUrcHandler(uAtClientHandle_t atClientHandle,
                           void *pParameters)
{
    const uAtClientTestResponseLine_t *pUrc;
    uAtClientTestCheckUrc_t *pCheckUrc;
    int32_t lastError = 0;

    pCheckUrc = (uAtClientTestCheckUrc_t *) pParameters;
    pUrc = (const uAtClientTestResponseLine_t *) pCheckUrc->pUrc;

    // Cause a mess by attempting to read more things
    // than are there, all as integers irrespective
    // of type
    for (size_t p = 0; (p < pUrc->numParameters + 1) && (lastError >= 0); p++) {
        lastError = uAtClientReadInt(atClientHandle);
    }

    if (lastError < 0) {
        pCheckUrc->lastError = lastError;
    }
}

// Function to test misc things, referenced by gAtClientTestSet2,
// should be last in the list since it fiddles with the URC
// handler without the test function body knowing about it.
// pParameter should be a pointer to uAtClientTestEchoMisc_t.
// Returns zero on success, else error.
static int32_t handleMiscUseLast(uAtClientHandle_t atClientHandle,
                                 size_t index, const void *pParameter)
{
    int32_t lastError;
    bool callbackCalled = false;
    bool urcHasCausedError = false;
    const uAtClientTestEchoMisc_t *pEcho;
    uAtClientTestCheckUrc_t checkUrc;
    char buffer[5]; // Enough characters for a 3 digit index as a string

    snprintf(buffer, sizeof(buffer), "_%d", index + 1);
    pEcho = (const uAtClientTestEchoMisc_t *) pParameter;

    memset(&checkUrc, 0, sizeof(checkUrc));
    checkUrc.pUrc = pEcho->pUrc;

    U_TEST_PRINT_LINE_X("installing dumb URC handler...", index + 1);
    // Swap out the URC handler for our own dumb URC
    // handler so that we can cause deliberate read failures
    uAtClientRemoveUrcHandler(atClientHandle, pEcho->pUrc->pPrefix);
    lastError = uAtClientSetUrcHandler(atClientHandle,
                                       pEcho->pUrc->pPrefix,
                                       dumbUrcHandler,
                                       (void *) &checkUrc);
    if (lastError == 0) {
        // Begin processing the response
        uAtClientResponseStart(atClientHandle, pEcho->pPrefix);

        // Read all of the parameters, should succeed
        // despite the dumb URC handler
        for (size_t x = 0; (x < pEcho->numParameters) && (lastError == 0); x++) {
            U_TEST_PRINT_LINE_X("reading parameter %d...", index + 1, x + 1);
            // At some point during the parameter reads the
            // URC handler should have been called and caused
            // an error (which should not affect the parameter
            // reads here).  Check that it did.
            if (checkUrc.lastError != 0) {
                urcHasCausedError = true;
            }
            lastError = uAtClientTestCheckParam(atClientHandle,
                                                &(pEcho->parameters[x]),
                                                buffer);
        }

        if (!urcHasCausedError) {
            U_TEST_PRINT_LINE_X("failed to cause deliberate errors in URC handler.",
                                index + 1);
            lastError = 1;
        }

        if (lastError == 0) {
            U_TEST_PRINT_LINE_X("flushing the input...", index + 1);
            // Flush the input and be sure that we have no errors
            uAtClientFlush(atClientHandle);
            lastError = uAtClientErrorGet(atClientHandle);
            if (lastError != 0) {
                U_TEST_PRINT_LINE_X("AT client reported error (%d) when there"
                                    " should have been none.", index + 1,
                                    lastError);
            }
        }

        // Finish off and we _should_ now have an
        // error because the flush will have removed the "OK"
        uAtClientResponseStop(atClientHandle);
        if (lastError == 0) {
            if (uAtClientErrorGet(atClientHandle) != 0) {
                // Finally, clear the error
                uAtClientClearError(atClientHandle);
            } else {
                U_TEST_PRINT_LINE_X("uAtClientResponseStop() didn't set error"
                                    " when it should have.", index + 1);
                lastError = 2;
            }
        }
    }

    if (lastError == 0) {
        U_TEST_PRINT_LINE_X("checking that uAtClientCallback() works.", index + 1);

        // Make an AT callback
        lastError = uAtClientCallback(atClientHandle, atCallback,
                                      (void *) &callbackCalled);
        // Yield so that it can run, then check that it has run
        uPortTaskBlock(U_CFG_OS_YIELD_MS);
        if (!callbackCalled) {
            U_TEST_PRINT_LINE_X("callback didn't execute.", index + 1);
            lastError = 3;
        }
    }

    return lastError;
}

#endif

/* ----------------------------------------------------------------
 * EXTERNED VARIABLES: gAtClientTestSet1 AND gAtClientTestSet2
 * -------------------------------------------------------------- */

#if (U_CFG_TEST_UART_A >= 0) && (U_CFG_TEST_UART_B >= 0)

/** Loopback test data for the AT client, requires two UARTs.
 * NOTE: if you change the number of references to URCs here then
 * don't forget to change U_AT_CLIENT_TEST_NUM_URCS_SET_1 to match.
 */
//lint -e{785} Suppress too few initialisers
//lint -e{708} Suppress union initialisation
const uAtClientTestCommandResponse_t gAtClientTestSet1[] = {
    // 001: just AT with OK in response
    {{"AT", 0}, {U_AT_CLIENT_TEST_RESPONSE_OK, 0}, NULL},
    // 002: as above but with a URC interleaved
    {{"AT", 0}, {U_AT_CLIENT_TEST_RESPONSE_OK, 0}, &gAtClientUrc0},
    // 003: another simple one, no parameters again
    {{"AT+BLAH1", 0}, {U_AT_CLIENT_TEST_RESPONSE_OK, 0}, NULL},
    // 004: simple outgoing command with error response
    {{"AT+BLAH2", 0}, {U_AT_CLIENT_TEST_RESPONSE_ERROR, 0}, NULL},
    // 005: as above but with a URC interleaved
    {{"AT+BLAH3", 0}, {U_AT_CLIENT_TEST_RESPONSE_ERROR, 0}, &gAtClientUrc1},
    // 006: simple outgoing command with CME error response
    {{"AT+BLAH4", 0}, {U_AT_CLIENT_TEST_RESPONSE_CME_ERROR, 0}, NULL},
    // 007: simple outgoing command with CMS error response
    {{"AT+BLAH5", 0}, {U_AT_CLIENT_TEST_RESPONSE_CMS_ERROR, 0}, NULL},
    // 008: as above but with a URC interleaved
    {{"AT+BLAH6", 0}, {U_AT_CLIENT_TEST_RESPONSE_CMS_ERROR, 0}, &gAtClientUrc2},
    // 009: simple outgoing command with aborted response
    {{"AT+BLAH7", 0}, {U_AT_CLIENT_TEST_RESPONSE_ABORTED, 0}, NULL},
    // 010: as above but with a URC interleaved
    {{"AT+BLAH8", 0}, {U_AT_CLIENT_TEST_RESPONSE_ABORTED, 0}, &gAtClientUrc3},
    // 011: simple outgoing command, response is a single line with no prefix and a
    // single int32_t parameter with value 0
    {   {"AT+INT1", 0}, {
            U_AT_CLIENT_TEST_RESPONSE_OK, 1, {
                {
                    NULL, 1, {{1, "0"}},
                    {{U_AT_CLIENT_TEST_PARAMETER_INT32, {0}, 0}}
                }
            }
        }, NULL
    },
    // 012: as above but with value 0x7FFFFFFF and this time with a prefix
    {   {"AT+INT2", 0}, {
            U_AT_CLIENT_TEST_RESPONSE_OK, 1, {
                {
                    "+INT:", 1, {{10, "2147483647"}},
                    {{U_AT_CLIENT_TEST_PARAMETER_INT32, {.int32 = INT32_MAX}, 0}}
                }
            }
        }, NULL
    },
    // 013: as two lines above but parameter is now uint64_t with value 0
    {   {"AT+UINT641", 0}, {
            U_AT_CLIENT_TEST_RESPONSE_OK, 1, {
                {
                    NULL, 1, {{1, "0"}},
                    {{U_AT_CLIENT_TEST_PARAMETER_UINT64, {0}, 0}}
                }
            }
        }, NULL
    },
    // 014: as two lines above but parameter is now uint64_t with
    // value 0xFFFFFFFFFFFFFFFF and there's a prefix
    {   {"AT+UINT642", 0}, {
            U_AT_CLIENT_TEST_RESPONSE_OK, 1, {
                {
                    "+UINT64:", 1, {{20, "18446744073709551615"}},
                    {{U_AT_CLIENT_TEST_PARAMETER_UINT64, {.uint64 = UINT64_MAX}, 0}}
                }
            }
        }, NULL
    },
    // 015: simple outgoing command, response is a single line with no prefix
    // and the parameters are a single unquoted string.
    {   {"AT+STRING1", 0}, {
            U_AT_CLIENT_TEST_RESPONSE_OK, 1, {
                {
                    NULL, 1, {{44, "The quick brown fox jumps over the lazy dog."}},
                    {   {
                            U_AT_CLIENT_TEST_PARAMETER_STRING, {
                                .pString = "The quick brown fox jumps over the lazy dog."
                            }, 0
                        }
                    }
                }
            }
        }, NULL
    },
    // 016: as above but with the stop-tag "\r\n" inserted in the string,
    // which should stop things right there, also add a prefix.
    {   {"AT+STRING2", 0}, {
            U_AT_CLIENT_TEST_RESPONSE_OK, 1, {
                {
                    "+STRING:", 1, {{42, "The quick brown fox jumps over\r\n lazy dog."}},
                    {   {
                            U_AT_CLIENT_TEST_PARAMETER_STRING, {
                                .pString = "The quick brown fox jumps over"
                            }, 0
                        }
                    }
                }
            }
        }, NULL
    },
    // 017: as above but with a URC interleaved
    {   {"AT+STRING2", 0}, {
            U_AT_CLIENT_TEST_RESPONSE_OK, 1, {
                {
                    "+STRING:", 1, {{42, "The quick brown fox jumps over\r\n lazy dog."}},
                    {   {
                            U_AT_CLIENT_TEST_PARAMETER_STRING, {
                                .pString = "The quick brown fox jumps over"
                            }, 0
                        }
                    }
                }
            }
        }, &gAtClientUrc3
    },
    // 018: as 014 but with "ignore stop-tag" (and a buffer length)
    // set so the "\r\n" should have no effect and remove the prefix again.
    {   {"AT+STRING3", 0}, {
            U_AT_CLIENT_TEST_RESPONSE_OK, 1, {
                {
                    NULL, 1, {{42, "The quick brown fox jumps over\r\n lazy dog."}},
                    {   {
                            U_AT_CLIENT_TEST_PARAMETER_RESPONSE_STRING_IGNORE_STOP_TAG, {
                                .pString = "The quick brown fox jumps over\r\n lazy dog."
                            }, 42 + 1
                        }
                    }
                }
            }
        }, NULL
    },
    // 019: as 016 but with the string in quotes, which should mean
    // that the stop tag and the delimiters inserted are ignored.
    // Also put the prefix back.
    {   {"AT+STRING4", 0}, {
            U_AT_CLIENT_TEST_RESPONSE_OK, 1, {
                {
                    "+STRING:", 1, {{46, "\"The quick, brown, fox jumps over\r\n lazy dog.\""}},
                    {   {
                            U_AT_CLIENT_TEST_PARAMETER_STRING, {
                                .pString = "The quick, brown, fox jumps over\r\n lazy dog."
                            }, 0
                        }
                    }
                }
            }
        }, NULL
    },
    // 020: simple outgoing command, response is a single line with no prefix
    // and the parameters are a stream of all bytes.
    //lint -e{786} Suppress string concatenation within initializer
    {   {"AT+BYTES1", 0}, {
            U_AT_CLIENT_TEST_RESPONSE_OK, 1, {
                {
                    NULL, 1, {{
                            256, "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e"
                            "\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d"
                            "\x1e\x1f\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b\x2c"
                            "\x2d\x2e\x2f"
                            "0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
                            "abcdefghijklmnopqrstuvwxyz{|}~\x07f"
                            "\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e"
                            "\x8f\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d"
                            "\x9e\x9f\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac"
                            "\xad\xae\xaf\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb"
                            "\xbc\xbd\xbe\xbf\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca"
                            "\xcb\xcc\xcd\xce\xcf\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9"
                            "\xda\xdb\xdc\xdd\xde\xdf\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8"
                            "\xe9\xea\xeb\xec\xed\xee\xef\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7"
                            "\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff"
                        }
                    },
                    {   {
                            U_AT_CLIENT_TEST_PARAMETER_BYTES, {
                                .pBytes = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c"
                                "\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19"
                                "\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21\x22\x23\x24\x25\x26"
                                "\x27\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f"
                                "0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
                                "abcdefghijklmnopqrstuvwxyz{|}~\x07f"
                                "\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c"
                                "\x8d\x8e\x8f\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99"
                                "\x9a\x9b\x9c\x9d\x9e\x9f\xa0\xa1\xa2\xa3\xa4\xa5\xa6"
                                "\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf\xb0\xb1\xb2\xb3"
                                "\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf\xc0"
                                "\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd"
                                "\xce\xcf\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda"
                                "\xdb\xdc\xdd\xde\xdf\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7"
                                "\xe8\xe9\xea\xeb\xec\xed\xee\xef\xf0\xf1\xf2\xf3\xf4"
                                "\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff"
                            }, 256
                        }
                    }
                }
            }
        }, NULL
    },
    // 021: as above but with a URC interleaved and a prefix added (otherwise there
    // is no way to tell the URC from the expected response)
    {   {"AT+BYTES1", 0}, {
            U_AT_CLIENT_TEST_RESPONSE_OK, 1, {
                {
                    "+BYTES", 1, {{
                            256, "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e"
                            "\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d"
                            "\x1e\x1f\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b\x2c"
                            "\x2d\x2e\x2f"
                            "0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
                            "abcdefghijklmnopqrstuvwxyz{|}~\x07f"
                            "\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e"
                            "\x8f\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d"
                            "\x9e\x9f\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac"
                            "\xad\xae\xaf\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb"
                            "\xbc\xbd\xbe\xbf\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca"
                            "\xcb\xcc\xcd\xce\xcf\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9"
                            "\xda\xdb\xdc\xdd\xde\xdf\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8"
                            "\xe9\xea\xeb\xec\xed\xee\xef\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7"
                            "\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff"
                        }
                    },
                    {   {
                            U_AT_CLIENT_TEST_PARAMETER_BYTES, {
                                .pBytes = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c"
                                "\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19"
                                "\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21\x22\x23\x24\x25\x26"
                                "\x27\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f"
                                "0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
                                "abcdefghijklmnopqrstuvwxyz{|}~\x07f"
                                "\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c"
                                "\x8d\x8e\x8f\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99"
                                "\x9a\x9b\x9c\x9d\x9e\x9f\xa0\xa1\xa2\xa3\xa4\xa5\xa6"
                                "\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf\xb0\xb1\xb2\xb3"
                                "\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf\xc0"
                                "\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd"
                                "\xce\xcf\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda"
                                "\xdb\xdc\xdd\xde\xdf\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7"
                                "\xe8\xe9\xea\xeb\xec\xed\xee\xef\xf0\xf1\xf2\xf3\xf4"
                                "\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff"
                            }, 256
                        }
                    }
                }
            }
        }, &gAtClientUrc4
    },
    // 022: as 020 but with the stop-tag "\r\n" inserted in the string,
    // which should stop things right there, and add a prefix.
    //lint -e{786} Suppress string concatenation within initializer
    {   {"AT+BYTES2", 0}, {
            U_AT_CLIENT_TEST_RESPONSE_OK, 1, {
                {
                    "+BYTES:", 1, {{
                            258, "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e"
                            "\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d"
                            "\x1e\x1f\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b\x2c"
                            "\x2d\x2e\x2f"
                            "0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
                            "abcdefghijklmnopqrstuvwxyz{|}~\x07f"
                            "\r\n"
                            "\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e"
                            "\x8f\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d"
                            "\x9e\x9f\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac"
                            "\xad\xae\xaf\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb"
                            "\xbc\xbd\xbe\xbf\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca"
                            "\xcb\xcc\xcd\xce\xcf\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9"
                            "\xda\xdb\xdc\xdd\xde\xdf\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8"
                            "\xe9\xea\xeb\xec\xed\xee\xef\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7"
                            "\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff"
                        }
                    },
                    {   {
                            U_AT_CLIENT_TEST_PARAMETER_BYTES, {
                                .pBytes = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c"
                                "\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19"
                                "\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21\x22\x23\x24\x25\x26"
                                "\x27\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f"
                                "0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
                                "abcdefghijklmnopqrstuvwxyz{|}~\x07f"
                            }, 128
                        }
                    }
                }
            }
        }, NULL
    },
    // 023: as above but with "ignore stop-tag" set so the "\r\n" should have no effect
    // and remove the prefix again.
    //lint -e{786} Suppress string concatenation within initializer
    {   {"AT+BYTES3", 0}, {
            U_AT_CLIENT_TEST_RESPONSE_OK, 1, {
                {
                    NULL, 1, {{
                            258, "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e"
                            "\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d"
                            "\x1e\x1f\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b\x2c"
                            "\x2d\x2e\x2f"
                            "0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
                            "abcdefghijklmnopqrstuvwxyz{|}~\x07f"
                            "\r\n"
                            "\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e"
                            "\x8f\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d"
                            "\x9e\x9f\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac"
                            "\xad\xae\xaf\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb"
                            "\xbc\xbd\xbe\xbf\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca"
                            "\xcb\xcc\xcd\xce\xcf\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9"
                            "\xda\xdb\xdc\xdd\xde\xdf\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8"
                            "\xe9\xea\xeb\xec\xed\xee\xef\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7"
                            "\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff"
                        }
                    },
                    {   {
                            U_AT_CLIENT_TEST_PARAMETER_RESPONSE_BYTES_IGNORE_STOP_TAG, {
                                .pBytes = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c"
                                "\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19"
                                "\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21\x22\x23\x24\x25\x26"
                                "\x27\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f"
                                "0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
                                "abcdefghijklmnopqrstuvwxyz{|}~\x07f"
                                "\r\n"
                                "\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c"
                                "\x8d\x8e\x8f\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99"
                                "\x9a\x9b\x9c\x9d\x9e\x9f\xa0\xa1\xa2\xa3\xa4\xa5\xa6"
                                "\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf\xb0\xb1\xb2\xb3"
                                "\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf\xc0"
                                "\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd"
                                "\xce\xcf\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda"
                                "\xdb\xdc\xdd\xde\xdf\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7"
                                "\xe8\xe9\xea\xeb\xec\xed\xee\xef\xf0\xf1\xf2\xf3\xf4"
                                "\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff"
                            }, 258
                        }
                    }
                }
            }
        }, NULL
    },
    // 024: simple outgoing command, response is a single line with a prefix and
    // multiple int32_t/uint64_ parameters
    {   {"AT+MULTIPARAMRESP1", 0}, {
            U_AT_CLIENT_TEST_RESPONSE_OK, 1, {
                {
                    "+INTS:", 3, {{1, "1"}, {20, "18446744073709551615"}, {2, "64"}},
                    {   {U_AT_CLIENT_TEST_PARAMETER_INT32, {1}, 0},
                        {U_AT_CLIENT_TEST_PARAMETER_UINT64, {.uint64 = UINT64_MAX}, 0},
                        {U_AT_CLIENT_TEST_PARAMETER_INT32, {64}, 0}
                    }
                }
            }
        }, NULL
    },
    // 025: as above but with a URC interleaved
    {   {"AT+MULTIPARAMRESP1", 0}, {
            U_AT_CLIENT_TEST_RESPONSE_OK, 1, {
                {
                    "+INTS:", 3, {{1, "1"}, {20, "18446744073709551615"}, {2, "64"}},
                    {   {U_AT_CLIENT_TEST_PARAMETER_INT32, {1}, 0},
                        {U_AT_CLIENT_TEST_PARAMETER_UINT64, {.uint64 = UINT64_MAX}, 0},
                        {U_AT_CLIENT_TEST_PARAMETER_INT32, {64}, 0}
                    }
                }
            }
        }, &gAtClientUrc5
    },
    // 026: simple outgoing command, response is a single line with a prefix and
    // mixed integer/string/byte parameters
    {   {"AT+MULTIPARAMRESP2", 0}, {
            U_AT_CLIENT_TEST_RESPONSE_OK, 1, {
                {
                    "+MIXED:", 8, {
                        {25, "\"Quoted string parameter\""}, {5, "65531"},
                        {33, "\"Another quoted string parameter\""}, {1, "1"},
                        {28, "An unquoted string parameter"}, {2, "42"},
                        {2, "\x00\xff"}, {20, "18446744073709551615"}
                    },
                    {   {
                            U_AT_CLIENT_TEST_PARAMETER_STRING, {
                                .pString = "Quoted string parameter"
                            }, 0
                        },
                        {U_AT_CLIENT_TEST_PARAMETER_INT32, {65531}, 0},
                        {
                            U_AT_CLIENT_TEST_PARAMETER_STRING, {
                                .pString = "Another quoted string parameter"
                            }, 0
                        },
                        {U_AT_CLIENT_TEST_PARAMETER_INT32, {1}, 0},
                        {
                            U_AT_CLIENT_TEST_PARAMETER_STRING, {
                                .pString = "An unquoted string parameter"
                            }, 28 + 1
                        },
                        {U_AT_CLIENT_TEST_PARAMETER_INT32, {42}, 0},
                        {
                            U_AT_CLIENT_TEST_PARAMETER_BYTES, {
                                .pBytes = "\x00\xff"
                            }, 2
                        },
                        {U_AT_CLIENT_TEST_PARAMETER_UINT64, {.uint64 = UINT64_MAX}, 0}
                    }
                }
            }
        }, NULL
    },
    // 027: simple outgoing command, response is two lines with a prefix on
    // each and integer parameters
    {   {"AT+MULTILINE1", 0}, {
            U_AT_CLIENT_TEST_RESPONSE_OK, 2, {
                {
                    "+INTS:", 3, {{1, "1"}, {20, "18446744073709551615"}, {2, "64"}},
                    {   {U_AT_CLIENT_TEST_PARAMETER_INT32, {1}, 0},
                        {U_AT_CLIENT_TEST_PARAMETER_UINT64, {.uint64 = UINT64_MAX}, 0},
                        {U_AT_CLIENT_TEST_PARAMETER_INT32, {64}, 0}
                    }
                },
                {
                    "+INTS:", 4, {{20, "18446744073709551615"}, {5, "65536"}, {20, "18446744073709551615"}, {1, "0"}},
                    {   {U_AT_CLIENT_TEST_PARAMETER_UINT64, {.uint64 = UINT64_MAX}, 0},
                        {U_AT_CLIENT_TEST_PARAMETER_INT32, {65536}, 0},
                        {U_AT_CLIENT_TEST_PARAMETER_UINT64, {.uint64 = UINT64_MAX}, 0},
                        {U_AT_CLIENT_TEST_PARAMETER_UINT64, {.uint64 = 0}, 0}
                    }
                }
            }
        }, NULL
    },
    // 028: simple outgoing command, response is three lines with a prefix on
    // the first line only and mixed integer/string/byte parameters
    {   {"AT+MULTILINE2", 0}, {
            U_AT_CLIENT_TEST_RESPONSE_OK, 3, {
                {
                    "+MIXED:", 3, {{8, "\"Quoted\""}, {16, "\"Another quoted\""}, {1, "U"}},
                    {
                        {
                            U_AT_CLIENT_TEST_PARAMETER_STRING, {
                                .pString = "Quoted"
                            }, 0
                        },
                        {
                            U_AT_CLIENT_TEST_PARAMETER_STRING, {
                                .pString = "Another quoted"
                            }, 0
                        },
                        {
                            U_AT_CLIENT_TEST_PARAMETER_STRING, {
                                .pString = "U"
                            }, 0
                        }
                    },
                },
                {
                    NULL, 2, {{6, "\"More\""}, {5, "\xFF\x01,\x02\x7F"}},
                    {
                        {
                            U_AT_CLIENT_TEST_PARAMETER_STRING, {
                                .pString = "More"
                            }, 0
                        },
                        {
                            U_AT_CLIENT_TEST_PARAMETER_BYTES, {
                                .pBytes = "\xFF\x01,\x02\x7F"
                            }, 5
                        }
                    }
                },
                {
                    NULL, 1, {{10, "2147483647"}},
                    {
                        {
                            U_AT_CLIENT_TEST_PARAMETER_INT32, {.int32 = INT32_MAX}, 0
                        }
                    }
                }
            }
        }, NULL
    },
    // 029: as 027 but with spaces added around integers and before terminators
    {   {"AT+MULTILINESPACES", 0}, {
            U_AT_CLIENT_TEST_RESPONSE_OK, 2, {
                {
                    "+SPACES:", 3, {{3, " 1 "}, {25, "  18446744073709551615   "},
                        {7, "64     "}
                    },
                    {   {U_AT_CLIENT_TEST_PARAMETER_INT32, {1}, 0},
                        {U_AT_CLIENT_TEST_PARAMETER_UINT64, {.uint64 = UINT64_MAX}, 0},
                        {U_AT_CLIENT_TEST_PARAMETER_INT32, {64}, 0}
                    }
                },
                {
                    "+MORESPACES:", 4, {{23, "   18446744073709551615"}, {6, " 65536"},
                        {22, " 18446744073709551615 "}, {1, "0"}
                    },
                    {   {U_AT_CLIENT_TEST_PARAMETER_UINT64, {.uint64 = UINT64_MAX}, 0},
                        {U_AT_CLIENT_TEST_PARAMETER_INT32, {65536}, 0},
                        {U_AT_CLIENT_TEST_PARAMETER_UINT64, {.uint64 = UINT64_MAX}, 0},
                        {U_AT_CLIENT_TEST_PARAMETER_UINT64, {.uint64 = 0}, 0}
                    }
                }
            }
        }, NULL
    },
    // 030: outgoing command with a single integer parameter and simple "OK" response
    {   {
            "AT+CMD1=", 1, {{U_AT_CLIENT_TEST_PARAMETER_INT32, {.int32 = INT32_MAX}, 0}},
            {{10, "2147483647"}}
        },
        {U_AT_CLIENT_TEST_RESPONSE_OK, 0}, NULL
    },
    // 031: outgoing command with a single uint64_t parameter and simple "OK" response
    {   {
            "AT+CMD2=", 1, {{U_AT_CLIENT_TEST_PARAMETER_UINT64, {.uint64 = UINT64_MAX}, 0}},
            {{20, "18446744073709551615"}}
        },
        {U_AT_CLIENT_TEST_RESPONSE_OK, 0}, NULL
    },
    // 032: outgoing command with a single unquoted string parameter and simple "OK" response
    {   {
            "AT+CMD3=", 1, {{
                    U_AT_CLIENT_TEST_PARAMETER_STRING,
                    {.pString = "The quick brown fox jumps over the lazy dog."}, 0
                }
            },
            {{44, "The quick brown fox jumps over the lazy dog."}}
        },
        {U_AT_CLIENT_TEST_RESPONSE_OK, 0}, NULL
    },
    // 033: outgoing command with a single quoted string parameter and simple "OK" response
    {   {
            "AT+CMD4=", 1, {{
                    U_AT_CLIENT_TEST_PARAMETER_COMMAND_QUOTED_STRING,
                    {.pString = "The quick brown fox jumps over the lazy dog."}, 0
                }
            },
            {{46, "\"The quick brown fox jumps over the lazy dog.\""}}
        },
        {U_AT_CLIENT_TEST_RESPONSE_OK, 0}, NULL
    },
    // 034: outgoing command with a parameter of all possible bytes and simple "OK" response
    {   {
            "AT+CMD5=", 1, {{
                    U_AT_CLIENT_TEST_PARAMETER_COMMAND_BYTES_STANDALONE, {
                        .pBytes = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e"
                        "\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d"
                        "\x1e\x1f\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b\x2c"
                        "\x2d\x2e\x2f"
                        "0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
                        "abcdefghijklmnopqrstuvwxyz{|}~\x07f"
                        "\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e"
                        "\x8f\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d"
                        "\x9e\x9f\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac"
                        "\xad\xae\xaf\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb"
                        "\xbc\xbd\xbe\xbf\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca"
                        "\xcb\xcc\xcd\xce\xcf\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9"
                        "\xda\xdb\xdc\xdd\xde\xdf\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8"
                        "\xe9\xea\xeb\xec\xed\xee\xef\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7"
                        "\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff"
                    }, 256
                }
            }, {{
                    256, "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e"
                    "\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d"
                    "\x1e\x1f\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b\x2c"
                    "\x2d\x2e\x2f"
                    "0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
                    "abcdefghijklmnopqrstuvwxyz{|}~\x07f"
                    "\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e"
                    "\x8f\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d"
                    "\x9e\x9f\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac"
                    "\xad\xae\xaf\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb"
                    "\xbc\xbd\xbe\xbf\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca"
                    "\xcb\xcc\xcd\xce\xcf\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9"
                    "\xda\xdb\xdc\xdd\xde\xdf\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8"
                    "\xe9\xea\xeb\xec\xed\xee\xef\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7"
                    "\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff"
                }
            }
        },
        {U_AT_CLIENT_TEST_RESPONSE_OK, 0}, NULL
    },
    // 035: outgoing command with a mixture of integer parameters and simple "OK" response
    {   {
            "AT+MULTIPARAMCMD1=", 5, {{U_AT_CLIENT_TEST_PARAMETER_INT32, {.int32 = 0}, 0},
                {U_AT_CLIENT_TEST_PARAMETER_UINT64, {.uint64 = UINT64_MAX}, 0},
                {U_AT_CLIENT_TEST_PARAMETER_INT32, {.int32 = INT32_MAX}, 0},
                {U_AT_CLIENT_TEST_PARAMETER_UINT64, {.uint64 = 1}, 0},
                {U_AT_CLIENT_TEST_PARAMETER_UINT64, {.uint64 = 0}, 0}
            },
            {{1, "0"}, {20, "18446744073709551615"}, {10, "2147483647"}, {1, "1"}, {1, "0"}}
        },
        {U_AT_CLIENT_TEST_RESPONSE_OK, 0}, NULL
    },
    // 036: outgoing command with a mixture of integer/string/byte parameters
    // and simple "OK" response
    {   {
            "AT+MULTIPARAMCMD2=", 4, {{U_AT_CLIENT_TEST_PARAMETER_INT32, {.int32 = INT32_MAX}, 0},
                {
                    U_AT_CLIENT_TEST_PARAMETER_COMMAND_QUOTED_STRING,
                    {.pString = "The quick brown fox jumps over the lazy dog."}, 0
                },
                {U_AT_CLIENT_TEST_PARAMETER_COMMAND_BYTES_STANDALONE, {.pBytes = "\x00\xff\x7f"}, 3},
                {U_AT_CLIENT_TEST_PARAMETER_BYTES, {.pBytes = "\x01\xfe\xff"}, 3}
            },
            {   {10, "2147483647"}, {46, "\"The quick brown fox jumps over the lazy dog.\""},
                {3, "\x00\xff\x7f"}, {3, "\x01\xfe\xff"}
            }
        },
        {U_AT_CLIENT_TEST_RESPONSE_OK, 0}, NULL
    },
    // 037: big complicated thing in both directions
    {   {
            "AT+COMPLEX1=", 6, {
                {U_AT_CLIENT_TEST_PARAMETER_BYTES, {.pBytes = "\x00"}, 1},
                {U_AT_CLIENT_TEST_PARAMETER_INT32, {.int32 = INT32_MAX}, 0},
                {
                    U_AT_CLIENT_TEST_PARAMETER_STRING,
                    {.pString = "The quick brown fox jumps over the lazy dog."}, 0
                },
                {U_AT_CLIENT_TEST_PARAMETER_BYTES, {.pBytes = "\x00\xff\x7f"}, 3},
                {U_AT_CLIENT_TEST_PARAMETER_UINT64, {.uint64 = 70}, 0},
                {U_AT_CLIENT_TEST_PARAMETER_COMMAND_BYTES_STANDALONE, {.pBytes = "\x7f"}, 1}
            },
            {   {1, "\x00"}, {10, "2147483647"}, {44, "The quick brown fox jumps over the lazy dog."},
                {3, "\x00\xff\x7f"}, {2, "70"}, {1, "\x7f"}
            }
        },
        {
            U_AT_CLIENT_TEST_RESPONSE_OK, 3,
            {
                {
                    "+COMPLEX:", 3, {{8, "Unquoted"}, {8, "\"Quoted\""}, {7, "1234567"}},
                    {
                        {
                            U_AT_CLIENT_TEST_PARAMETER_STRING, {
                                .pString = "Unquoted"
                            }, 0
                        },
                        {
                            U_AT_CLIENT_TEST_PARAMETER_STRING, {
                                .pString = "Quoted"
                            }, 0
                        },
                        {
                            U_AT_CLIENT_TEST_PARAMETER_UINT64, {
                                .uint64 = 1234567
                            }, 0
                        }
                    },
                },
                {
                    NULL, 2, {{9, "\"Stringy\""}, {5, "\xFF\x01,\x02\x7F"}},
                    {
                        {
                            U_AT_CLIENT_TEST_PARAMETER_STRING, {
                                .pString = "Stringy"
                            }, 0
                        },
                        {
                            U_AT_CLIENT_TEST_PARAMETER_BYTES, {
                                .pBytes = "\xFF\x01,\x02\x7F"
                            }, 5
                        }
                    }
                },
                {
                    "+COMPLEX:", 1, {{1, "\x00"}},
                    {
                        {
                            U_AT_CLIENT_TEST_PARAMETER_BYTES, {
                                .pBytes = "\x00"
                            }, 1
                        }
                    }
                }
            }
        }, NULL
    },
    // 038: as above but with a URC interleaved
    {   {
            "AT+COMPLEX1=", 6, {
                {U_AT_CLIENT_TEST_PARAMETER_BYTES, {.pBytes = "\x00"}, 1},
                {U_AT_CLIENT_TEST_PARAMETER_INT32, {.int32 = INT32_MAX}, 0},
                {
                    U_AT_CLIENT_TEST_PARAMETER_STRING,
                    {.pString = "The quick brown fox jumps over the lazy dog."}, 0
                },
                {U_AT_CLIENT_TEST_PARAMETER_BYTES, {.pBytes = "\x00\xff\x7f"}, 3},
                {U_AT_CLIENT_TEST_PARAMETER_UINT64, {.uint64 = 70}, 0},
                {U_AT_CLIENT_TEST_PARAMETER_BYTES, {.pBytes = "\x7f"}, 1}
            },
            {   {1, "\x00"}, {10, "2147483647"}, {44, "The quick brown fox jumps over the lazy dog."},
                {3, "\x00\xff\x7f"}, {2, "70"}, {1, "\x7f"}
            }
        },
        {
            U_AT_CLIENT_TEST_RESPONSE_OK, 3,
            {
                {
                    "+COMPLEX:", 3, {{8, "Unquoted"}, {8, "\"Quoted\""}, {7, "1234567"}},
                    {
                        {
                            U_AT_CLIENT_TEST_PARAMETER_STRING, {
                                .pString = "Unquoted"
                            }, 0
                        },
                        {
                            U_AT_CLIENT_TEST_PARAMETER_STRING, {
                                .pString = "Quoted"
                            }, 0
                        },
                        {
                            U_AT_CLIENT_TEST_PARAMETER_UINT64, {
                                .uint64 = 1234567
                            }, 0
                        }
                    },
                },
                {
                    NULL, 2, {{9, "\"Stringy\""}, {5, "\xFF\x01,\x02\x7F"}},
                    {
                        {
                            U_AT_CLIENT_TEST_PARAMETER_STRING, {
                                .pString = "Stringy"
                            }, 0
                        },
                        {
                            U_AT_CLIENT_TEST_PARAMETER_BYTES, {
                                .pBytes = "\xFF\x01,\x02\x7F"
                            }, 5
                        }
                    }
                },
                {
                    "+COMPLEX:", 1, {{1, "\x00"}},
                    {
                        {
                            U_AT_CLIENT_TEST_PARAMETER_BYTES, {
                                .pBytes = "\x00"
                            }, 1
                        }
                    }
                }
            }
        }, &gAtClientUrc5
    }
};

/** Number of items in the gAtClientTestSet1 array, has to be
 * done in this file and externed or GCC complains about asking
 * for the size of a partially defined type.
 */
const size_t gAtClientTestSetSize1 = sizeof(gAtClientTestSet1) / sizeof(gAtClientTestSet1[0]);

/** Echo test data for the AT client, bringing together the
 * gAtClientTestEcho* items defined above; requires two UARTs.
 * NOTE: if you change the number of references to URCs here then
 * don't forget to change U_AT_CLIENT_TEST_NUM_URCS_SET_2 to match.
 */
//lint -e{786} Suppress string concatenation within initializer
const uAtClientTestEcho_t gAtClientTestSet2[] = {
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, NULL,
        handleSkipParams, (const void *) &gAtClientTestEchoSkipParams0,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, &gAtClientUrc5,
        handleSkipParams, (const void *) &gAtClientTestEchoSkipParams0,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, NULL,
        handleSkipParams, (const void *) &gAtClientTestEchoSkipParams1,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, &gAtClientUrc5,
        handleSkipParams, (const void *) &gAtClientTestEchoSkipParams1,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, NULL,
        handleSkipParams, (const void *) &gAtClientTestEchoSkipParams2,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, &gAtClientUrc5,
        handleSkipParams, (const void *) &gAtClientTestEchoSkipParams2,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, NULL,
        handleSkipParams, (const void *) &gAtClientTestEchoSkipParams3,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, &gAtClientUrc5,
        handleSkipParams, (const void *) &gAtClientTestEchoSkipParams3,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, NULL,
        handleSkipParams, (const void *) &gAtClientTestEchoSkipParams4,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, &gAtClientUrc5,
        handleSkipParams, (const void *) &gAtClientTestEchoSkipParams4,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, NULL,
        handleSkipBytes, (const void *) &gAtClientTestEchoSkipBytes0,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, &gAtClientUrc5,
        handleSkipBytes, (const void *) &gAtClientTestEchoSkipBytes0,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, NULL,
        handleSkipBytes, (const void *) &gAtClientTestEchoSkipBytes1,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, &gAtClientUrc5,
        handleSkipBytes, (const void *) &gAtClientTestEchoSkipBytes1,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, NULL,
        handleSkipBytes, (const void *) &gAtClientTestEchoSkipBytes2,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, &gAtClientUrc5,
        handleSkipBytes, (const void *) &gAtClientTestEchoSkipBytes2,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, NULL,
        handleSkipBytes, (const void *) &gAtClientTestEchoSkipBytes3,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, &gAtClientUrc5,
        handleSkipBytes, (const void *) &gAtClientTestEchoSkipBytes3,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, NULL,
        handleSkipBytes, (const void *) &gAtClientTestEchoSkipBytes4,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, &gAtClientUrc5,
        handleSkipBytes, (const void *) &gAtClientTestEchoSkipBytes4,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, NULL,
        handleSkipBytes, (const void *) &gAtClientTestEchoSkipBytes5,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, &gAtClientUrc5,
        handleSkipBytes, (const void *) &gAtClientTestEchoSkipBytes5,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, NULL,
        handleEarlyStop, (const void *) &gAtClientTestEchoEarlyStop0,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, &gAtClientUrc5,
        handleEarlyStop, (const void *) &gAtClientTestEchoEarlyStop0,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, NULL,
        handleEarlyStop, (const void *) &gAtClientTestEchoEarlyStop1,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, &gAtClientUrc5,
        handleEarlyStop, (const void *) &gAtClientTestEchoEarlyStop1,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, NULL,
        handleEarlyStop, (const void *) &gAtClientTestEchoEarlyStop2,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, &gAtClientUrc5,
        handleEarlyStop, (const void *) &gAtClientTestEchoEarlyStop2,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, NULL,
        handleEarlyStop, (const void *) &gAtClientTestEchoEarlyStop3,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, &gAtClientUrc5,
        handleEarlyStop, (const void *) &gAtClientTestEchoEarlyStop3,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, NULL,
        handleEarlyStop, (const void *) &gAtClientTestEchoEarlyStop4,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, &gAtClientUrc5,
        handleEarlyStop, (const void *) &gAtClientTestEchoEarlyStop4,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_WAIT, U_AT_CLIENT_TEST_ECHO_WAIT_LENGTH, &gAtClientUrc5,
        handleWaitForChar, (const void *) &gAtClientTestEchoWaitForChar0,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        U_AT_CLIENT_TEST_ECHO_WAIT, U_AT_CLIENT_TEST_ECHO_WAIT_LENGTH, &gAtClientUrc5,
        handleWaitForChar, (const void *) &gAtClientTestEchoWaitForChar0,
        (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        // If you change the string below you must change handleNullBuffer() to match
        "\r\n" U_AT_CLIENT_TEST_PREFIX " string1,\"string2\",\"" U_AT_CLIENT_TEST_STRING_THREE
        "\",\x00\x7f\xff," U_AT_CLIENT_TEST_BYTES_TWO "\r\nOK\r\n",
        U_AT_CLIENT_TEST_PREFIX_LENGTH + U_AT_CLIENT_TEST_STRING_THREE_LENGTH +
        U_AT_CLIENT_TEST_BYTES_TWO_LENGTH + 34,
        NULL, handleNullBuffer, NULL, (int32_t) U_ERROR_COMMON_SUCCESS
    },
    {
        "\r\n" U_AT_CLIENT_TEST_ERROR "\r\n", U_AT_CLIENT_TEST_ERROR_LENGTH + 4, NULL,
        handleReadOnError, (const void *) &gAtClientTestEchoNoTimeout,
        (int32_t) U_ERROR_COMMON_DEVICE_ERROR
    },
    {
        "\r\n" U_AT_CLIENT_TEST_CME_ERROR "0\r\n", U_AT_CLIENT_TEST_CMX_ERROR_LENGTH + 5, NULL,
        handleReadOnError, (const void *) &gAtClientTestEchoNoTimeout,
        (int32_t) U_ERROR_COMMON_DEVICE_ERROR
    },
    {
        "\r\n" U_AT_CLIENT_TEST_CMS_ERROR "0\r\n", U_AT_CLIENT_TEST_CMX_ERROR_LENGTH + 5, NULL,
        handleReadOnError, (const void *) &gAtClientTestEchoNoTimeout,
        (int32_t) U_ERROR_COMMON_DEVICE_ERROR
    },
    {
        "\r\n" U_AT_CLIENT_TEST_ABORTED "\r\n", U_AT_CLIENT_TEST_ABORTED_LENGTH + 4, NULL,
        handleReadOnError, (const void *) &gAtClientTestEchoNoTimeout,
        (int32_t) U_ERROR_COMMON_DEVICE_ERROR
    },
    {
        "", 0, NULL, handleReadOnError, (const void *) &gAtClientTestEchoTimeout,
        (int32_t) U_ERROR_COMMON_DEVICE_ERROR
    },
    {
        U_AT_CLIENT_TEST_ECHO_SKIP, U_AT_CLIENT_TEST_ECHO_SKIP_LENGTH, &gAtClientUrc5,
        handleMiscUseLast, (const void *) &gAtClientTestEchoMisc,
        (int32_t) U_ERROR_COMMON_SUCCESS
    }
};

/** Number of items in the gAtClientTestSet2 array, has to be
 * done in this file and externed or GCC complains about asking
 * for the size of a partially defined type.
 */
const size_t gAtClientTestSetSize2 = sizeof(gAtClientTestSet2) / sizeof(gAtClientTestSet2[0]);

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

#endif

// End of file
