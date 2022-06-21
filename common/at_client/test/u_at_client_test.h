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

#ifndef _U_AT_CLIENT_TEST_H_
#define _U_AT_CLIENT_TEST_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief A few public functions needed in the AT client test files.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The maximum number of parameters to test on an AT command
 * or response.
 */
#define U_AT_CLIENT_TEST_MAX_NUM_PARAMETERS 32

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Union of parameters for an AT command or response.
 */
typedef union {
    int32_t  int32;
    uint64_t uint64;
    const char *pString;
    const char *pBytes;
} uAtClientTestParameterUnion_t;

/** The possible parameter types.
 */
typedef enum {
    U_AT_CLIENT_TEST_PARAMETER_NONE,
    /** int32_t parameter, command or response. */
    U_AT_CLIENT_TEST_PARAMETER_INT32,
    /** uint64_t parameter, command or response. */
    U_AT_CLIENT_TEST_PARAMETER_UINT64,
    /** String parameter, command or response. */
    U_AT_CLIENT_TEST_PARAMETER_STRING,
    /** String parameter in a command that must be quoted. */
    U_AT_CLIENT_TEST_PARAMETER_COMMAND_QUOTED_STRING,
    /** String parameter in a response where the stop tag should be ignored. */
    U_AT_CLIENT_TEST_PARAMETER_RESPONSE_STRING_IGNORE_STOP_TAG,
    /** An array of bytes parameter, command or response. */
    U_AT_CLIENT_TEST_PARAMETER_BYTES,
    /** An array of bytes in a response where the stop tag should be ignored. */
    U_AT_CLIENT_TEST_PARAMETER_RESPONSE_BYTES_IGNORE_STOP_TAG,
    /** An array of bytes in a response where "standalone" should be set to true. */
    U_AT_CLIENT_TEST_PARAMETER_RESPONSE_BYTES_STANDALONE,
    /** An array of bytes in a command where a delimiter should be used if necessary. */
    U_AT_CLIENT_TEST_PARAMETER_COMMAND_BYTES_STANDALONE
} uAtClientTestParameterType_t;

/** Definition of a single parameter.
 */
typedef struct {
    const uAtClientTestParameterType_t type;
    const uAtClientTestParameterUnion_t parameter;
    size_t length; /**< Required when using U_AT_CLIENT_TEST_PARAMETER_BYTES and xxx_IGNORE_STOP_TAG. */
} uAtClientTestParameter_t;

/** Definition of an array of bytes.
 */
typedef struct {
    size_t length;
    const char *pBytes;
} uAtBytes_t;

/** Definition of one line of incoming AT response or URC
 * plus its parameters, both what the AT server should send
 * and what the parameters should be read as by the AT client.
 */
typedef struct {
    const char *pPrefix;
    size_t numParameters;
    const uAtBytes_t parametersRaw[U_AT_CLIENT_TEST_MAX_NUM_PARAMETERS];
    const uAtClientTestParameter_t parameters[U_AT_CLIENT_TEST_MAX_NUM_PARAMETERS];
} uAtClientTestResponseLine_t;

/** Data structure to keep track of checking URCs.
 */
typedef struct {
    const uAtClientTestResponseLine_t *pUrc;
    size_t count;
    size_t passIndex;
    int32_t lastError;
} uAtClientTestCheckUrc_t;

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Print a string, displaying control characters in human-readable
 * form.
 *
 * @param pBytes  the bytes to print.
 * @param length  the number of bytes to print from pBytes.
 */
void uAtClientTestPrint(const char *pBytes, size_t length);

/** Check that stuff read from the AT stream contains the
 * given parameter.
 *
 * @param atClientHandle  the handle of the AT client to use
 *                        when reading the data.
 * @param pParameter      a pointer to uAtClientTestParameter_t
 *                        describing what the parameter should
 *                        be to compare against.
 * @param pPostfix        string to tack on the end of the usual
 *                        print prefix so that a parameter from
 *                        one thing (e.g. a URC check) can be
 *                        distinguished from another.
 * @return                zero on success else error code.
 */
int32_t uAtClientTestCheckParam(uAtClientHandle_t atClientHandle,
                                const uAtClientTestParameter_t *pParameter,
                                const char *pPostfix);

#ifdef __cplusplus
}
#endif

#endif // _U_AT_CLIENT_TEST_H_

// End of file
