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

#ifndef _U_RUNNER_H_
#define _U_RUNNER_H_

/** @file
 * @brief This file defines the API to runner, which runs  all of the
 * ubxlib examples and unit tests.
 */

#include "unity.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Macro to map a unit test assertion to Unity.
 */
#define U_PORT_UNITY_TEST_ASSERT(condition) TEST_ASSERT(condition)
#define U_PORT_UNITY_TEST_ASSERT_EQUAL(expected, actual) TEST_ASSERT_EQUAL(expected, actual)

/** Used by U_RUNNER_NAME_EXPAND.
 */
#define U_RUNNER_NAME_EXPAND2(x, y) x ## y

/** Used by U_RUNNER_NAME_UID.
 */
#define U_RUNNER_NAME_EXPAND(x, y) U_RUNNER_NAME_EXPAND2(x, y)

/** Make up a unique function name.
 */
#define U_RUNNER_NAME_UID(x) U_RUNNER_NAME_EXPAND(x, __LINE__)

/** The maximum length of pName (see uRunnerFunctionDescription_t below).
 */
#define U_RUNNER_NAME_MAX_LENGTH_BYTES 64

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** A function that ubxlib runner might want to run.
 */
typedef void (*pURunnerFunction_t)(void);

/** Full description of a function.
 */
typedef struct uRunnerFunctionDescription_t {
    const char *pName;
    const char *pGroup;
    const pURunnerFunction_t pFunction;
    const char *pFile;
    int32_t line;
    struct uRunnerFunctionDescription_t *pNext;
} uRunnerFunctionDescription_t;

/* ----------------------------------------------------------------
 * FUNCTION: REGISTRATION
 * -------------------------------------------------------------- */

/** Register a function with the system.
 *
 * @param pDescription the test case description.
 */
void uRunnerFunctionRegister(uRunnerFunctionDescription_t *pDescription);

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: FUNCTION DEFINITION MACRO
 * -------------------------------------------------------------- */

/** The function name prefix to use for all examples.
 */
#ifndef U_RUNNER_PREFIX_EXAMPLE
# define U_RUNNER_PREFIX_EXAMPLE example
#endif

/** The function name prefix to use for all tests.
 */
#ifndef U_RUNNER_PREFIX_TEST
# define U_RUNNER_PREFIX_TEST test
#endif

/** Macro to wrap the definition of an example function.
 */
#define U_APP_START(group, name) U_RUNNER_FUNCTION(U_RUNNER_PREFIX_EXAMPLE, \
                                                   group, \
                                                   name)

/** Macro to wrap the definition of a test function.
 */
#define U_PORT_UNITY_TEST_FUNCTION(group, name) U_RUNNER_FUNCTION(U_RUNNER_PREFIX_TEST, \
                                                                  group, \
                                                                  name)

/** Macro to wrap the definition of a function (used by
 * U_PORT_UNITY_TEST_FUNCTION and U_APP_START).  The macro
 * creates a uniquely named function and adds it to the
 * list of runnable functions.  A function would be either a
 * ubxlib test or a ubxlib example.
 */
#define U_RUNNER_FUNCTION(prefix, group, name)                                                            \
    /* Function prototype */                                                                              \
    static void U_RUNNER_NAME_UID(prefix) (void);                                                         \
    /* Use constructor attribute so that this is run during C initialisation before anything else runs */ \
    /*lint -esym(528,functionRegistrationHelper*) suppress "symbol not referenced": called at C startup */\
    static void __attribute__((constructor)) U_RUNNER_NAME_UID(functionRegistrationHelper) ()             \
    {                                                                                                     \
        /* Static description of the function to pass to the register function */                         \
        static uRunnerFunctionDescription_t U_RUNNER_NAME_UID(functionDescription) = {                    \
            .pName = name,                                                                                \
            .pGroup = group,                                                                              \
            .pFunction = &U_RUNNER_NAME_UID(prefix),                                                      \
            .pFile = __FILE__,                                                                            \
            .line = __LINE__,                                                                             \
            .pNext = NULL                                                                                 \
        };                                                                                                \
        /* Call the register function with the description so it can keep a list of them */               \
        uRunnerFunctionRegister(&U_RUNNER_NAME_UID(functionDescription));                                 \
    }                                                                                                     \
    /* Actual start of a function */                                                                      \
    static void U_RUNNER_NAME_UID(prefix) (void)

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Print out all the registered functions.
 *
 * @param pPrefix prefix string to print at start of line.
 */
void uRunnerPrintAll(const char *pPrefix);

/** Run a named function.
 *
 * @param pName   the name of the function to run; if
 *                NULL then all functions are run.
 * @param pPrefix prefix string to print at start of line.
 */
void uRunnerRunNamed(const char *pName,
                     const char *pPrefix);

/** Run all of the functions whose names begin with the
 * given filter string.  The filter string can include
 * multiple entries separated with a full stop
 * character (but no spaces), e.g "thinga.thingb"; think
 * of the full stop as an "or".
 * NOTE: in addition, if U_RUNNER_PREAMBLE_STR is defined,
 * then functions beginning with that string will also
 * be run.
 *
 * @param pFilter  the filter string; if NULL then all
 *                 functions are run.
 * @param pPrefix  prefix string to print at start of line.
 */
void uRunnerRunFiltered(const char *pFilter,
                        const char *pPrefix);

/** Run all of the functions in a group.
 *
 * @param pGroup   the name of the group to run; if
 *                 NULL then all groups are run.
 * @param pPrefix  prefix string to print at start of line.
 */
void uRunnerRunGroup(const char *pGroup,
                     const char *pPrefix);

/** Run all the registered functions.
 *
 * @param pPrefix prefix string to print at start of line.
 */
void uRunnerRunAll(const char *pPrefix);

#ifdef __cplusplus
}
#endif

#endif // _U_RUNNER_H_

// End of file
