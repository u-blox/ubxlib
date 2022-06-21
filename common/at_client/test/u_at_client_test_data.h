/*
 * Copyright (c) 2017, Arm Limited and affiliates.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _U_AT_CLIENT_TEST_DATA_H_
#define _U_AT_CLIENT_TEST_DATA_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief Types for the AT client tests.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The maximum number of lines in an AT response.
 */
#define U_AT_CLIENT_TEST_MAX_NUM_LINES      10

/** The expected line ending for outgoing commands.
 */
#define U_AT_CLIENT_TEST_COMMAND_TERMINATOR "\r"

/** The line ending to use for incoming responses.
 */
#define U_AT_CLIENT_TEST_RESPONSE_TERMINATOR "\r\n"

/** The expected delimiter for commands and responses.
 */
#define U_AT_CLIENT_TEST_DELIMITER ","

/** The "OK" response.
 */
#define U_AT_CLIENT_TEST_OK "OK"

/** The "ERROR" response.
 */
#define U_AT_CLIENT_TEST_ERROR "ERROR"

/** The length of U_AT_CLIENT_TEST_ERROR.
 */
#define U_AT_CLIENT_TEST_ERROR_LENGTH 5

/** The "CME ERROR" response; will be followed by a number.
 */
#define U_AT_CLIENT_TEST_CME_ERROR "+CME ERROR: "

/** The "CMS ERROR" response; will be followed by a number.
 */
#define U_AT_CLIENT_TEST_CMS_ERROR "+CMS ERROR: "

/** The length of U_AT_CLIENT_TEST_CME_ERROR/
 * U_AT_CLIENT_TEST_CMS_ERROR.
 */
#define U_AT_CLIENT_TEST_CMX_ERROR_LENGTH 12

/** The "ABORTED" response.
 */
#define U_AT_CLIENT_TEST_ABORTED "ABORTED"

/** The length of U_AT_CLIENT_TEST_ABORTED.
 */
#define U_AT_CLIENT_TEST_ABORTED_LENGTH 7

/** The number of URCs that should be found in test set 1
 */
#define U_AT_CLIENT_TEST_NUM_URCS_SET_1 18

/** The number of URCs that should be found in test set 2
 */
#define U_AT_CLIENT_TEST_NUM_URCS_SET_2 34

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Definition of an outgoing AT command plus its parameters
 * and what they should be turned into by an AT client.
 */
typedef struct {
    const char *pString; /** The command. */
    size_t numParameters; /** The number of parameters in the command. */
    const uAtClientTestParameter_t parameters[U_AT_CLIENT_TEST_MAX_NUM_PARAMETERS];
    const uAtBytes_t parametersRaw[U_AT_CLIENT_TEST_MAX_NUM_PARAMETERS];
} uAtClientTestCommand_t;

/** The possible response types.
 */
typedef enum {
    U_AT_CLIENT_TEST_RESPONSE_NONE,
    U_AT_CLIENT_TEST_RESPONSE_OK,
    U_AT_CLIENT_TEST_RESPONSE_ERROR,
    U_AT_CLIENT_TEST_RESPONSE_CME_ERROR,
    U_AT_CLIENT_TEST_RESPONSE_CMS_ERROR,
    U_AT_CLIENT_TEST_RESPONSE_ABORTED
} uAtClientTestResponseType_t;

/** Definition of an incoming AT response.
 */
typedef struct {
    const uAtClientTestResponseType_t type;
    size_t numLines;
    const uAtClientTestResponseLine_t lines[U_AT_CLIENT_TEST_MAX_NUM_LINES];
} uAtClientTestResponse_t;

/** Definition of a test AT command/response with optional URC.
 */
typedef struct {
    const uAtClientTestCommand_t command;
    const uAtClientTestResponse_t response;
    const uAtClientTestResponseLine_t *pUrc;
} uAtClientTestCommandResponse_t;

/** Definition of an AT echo test.
 */
typedef struct {
    const char *pBytes; /** The response bytes to be echoed. */
    size_t length; /** The number of characters at pBytes. */
    const uAtClientTestResponseLine_t *pUrc; /** A URC, if one is to be interleaved. */
    int32_t (*pFunction) (uAtClientHandle_t atClientHandle, size_t,
                          const void *); /** Handler function to work on pBytes. */
    const void *pParameters; /** Parameters to pass in second argument to pFunction. */
    int32_t unlockErrorCode; /** The expected return value from uAtClientUnlock(). */
} uAtClientTestEcho_t;

/** Definition of pParameters for a "skip params" echo test.
 */
typedef struct {
    const char *pPrefix; /** The prefix at the start of the response. */
    size_t numParameters; /** The number of parameters in the response. */
    size_t paramNotSkipIndex; /** The index of the parameter in the response to NOT skip. */
    const uAtClientTestParameter_t parameter; /** The value of that parameter. */
} uAtClientTestEchoSkipParams_t;

/** Definition of pParameters for a "skip bytes" echo test.
 */
typedef struct {
    const char *pPrefix; /** The prefix at the start of the response. */
    size_t numParameters; /** The number of parameters in the response. */
    size_t paramIndex; /** The index of the parameter at which to start skipping. */
    size_t skipLength; /** The number of bytes to skip. */
    /** The values of all the parameters that should result. */
    const uAtClientTestParameter_t parameters[U_AT_CLIENT_TEST_MAX_NUM_PARAMETERS];
} uAtClientTestEchoSkipBytes_t;

/** Definition of pParameters for an "early stop" test.
 */
typedef struct {
    const char *pPrefix; /** The prefix at the start of the response. */
    size_t numParameters; /** The number of parameters to read before stopping. */
    /** The values of all the parameters that should result. */
    const uAtClientTestParameter_t parameters[U_AT_CLIENT_TEST_MAX_NUM_PARAMETERS];
} uAtClientTestEchoEarlyStop_t;

/** Definition of pParameters for a "wait for char" test.
 */
typedef struct {
    const char *pPrefix; /** The prefix at the start of the response. */
    char character; /** The character to wait for. */
    size_t numParameters; /** The number of parameters to read after the wait is over. */
    /** The values of all the parameters that should result. */
    const uAtClientTestParameter_t parameters[U_AT_CLIENT_TEST_MAX_NUM_PARAMETERS];
} uAtClientTestEchoWaitForChar_t;

/** Definition of pParameters for an error test.
 */
typedef struct {
    int32_t atTimeoutMs; /** The AT timeout to use. */
    int32_t timeMinMs; /** The minimum time all the checks should take. */
    int32_t timeMaxMs; /** The maximum time all the checks should take. */
} uAtClientTestEchoError_t;

/** Definition of pParameters for the misc test.
 */
typedef struct {
    const char *pPrefix; /** The prefix for the test response line. */
    size_t numParameters; /** The number of parameters in the test response line. */
    const uAtClientTestParameter_t parameters[U_AT_CLIENT_TEST_MAX_NUM_PARAMETERS];
    const uAtClientTestResponseLine_t *pUrc; /** The URC interleaved with it. */
} uAtClientTestEchoMisc_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

#if (U_CFG_TEST_UART_A >= 0) && (U_CFG_TEST_UART_B >= 0)

/** Loopback test data for the AT client, requires two UARTs.
 */
extern const uAtClientTestCommandResponse_t gAtClientTestSet1[];

/** Size of gAtClientTestSet1, has to be here because otherwise GCC
 * complains about asking for the size of an incomplete type.
 */
extern const size_t gAtClientTestSetSize1;

/** Echo test data for the AT client, requires two UARTs.
 */
extern const uAtClientTestEcho_t gAtClientTestSet2[];

/** Size of gAtClientTestSet2, has to be here because otherwise GCC
 * complains about asking for the size of an incomplete type.
 */
extern const size_t gAtClientTestSetSize2;

#endif

#ifdef __cplusplus
}
#endif

#endif // _U_AT_CLIENT_TEST_DATA_H_

// End of file
