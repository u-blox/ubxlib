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

/* This file implements a mechanism to run a set of examples or
 * tests, learning from the implementation of the esp-idf unity
 * component, which in turn learned it from the catch framework.
 * It may be included in a build for a platform which includes no
 * unit test framework of its own.
 */

/** @file
 * @brief This file implements ubxlib runner, which runs the
 * ubxlib examples and unit tests.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // strcmp() and strncmp()
#include "stdio.h"     // snprintf()

#include "u_port_clib_platform_specific.h" /* Integer stdio, must be included
                                              before the other port files if
                                              any print or scan function is used. */
#include "u_port.h"

#include "u_runner.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The prefix string which should always be sorted to the top of
 * the function list: usually this will be the examples which should
 * be obvious to the user.  Note that there should be NO QUOTES around
 * the string: this is because when a definition for this is passd
 * in from outside some tools, e.g. Make, don't like the quotes and
 * so quotes are added later.
 */
#ifndef U_RUNNER_TOP_STR
# define U_RUNNER_TOP_STR example
#endif

/** The prefix string which should form a preamble: this is
 * often necessary when running a suite of tests, to put
 * ensure that everything is in a good state before things
 * and to workaround issues such as memory leaks in the platform
 * itself which we can do nothing about.  Note that there should
 * be NO QUOTES around the string: this is because when a
 * definition for this is passed in from outside some tools,
 * e.g. Make, don't like the quotes and so quotes are added later.
 */
#ifndef U_RUNNER_PREAMBLE_STR
# define U_RUNNER_PREAMBLE_STR preamble
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Linked list anchor.
 */
static uRunnerFunctionDescription_t *gpFunctionList = NULL;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Run a function.
static void runFunction(const uRunnerFunctionDescription_t *pFunction,
                        const char *pPrefix)
{
    UNITY_PRINT_EOL();
    UnityPrint(pPrefix);
    UnityPrint("Running ");
    UnityPrint(pFunction->pName);
    UnityPrint("...");
    UNITY_PRINT_EOL();
    UNITY_OUTPUT_FLUSH();

    Unity.TestFile = pFunction->pFile;
    Unity.CurrentDetail1 = pFunction->pGroup;
    UnityDefaultTestRun(pFunction->pFunction,
                        pFunction->pName,
                        pFunction->line);
}

// Comparison function for list sort, returning
// true if 2 is higher than 1. See
// sortFunctionList() for the intended sort order.
static bool compareFunctionFileGroupName(const uRunnerFunctionDescription_t *pFunction1,
                                         const uRunnerFunctionDescription_t *pFunction2)
{
    int32_t x;

    // First check that we have different file names
    x = strcmp(pFunction1->pFile, pFunction2->pFile);
    if (x != 0) {
        // Then compare groups
        x = strcmp(pFunction1->pGroup, pFunction2->pGroup);
        if (x == 0) {
            // Then compare names
            x = strcmp(pFunction1->pName, pFunction2->pName);
        }
    }

    return (x > 0);
}

// Compare two functions on the basis of file and name with the given
// name on a "starts with" basis, returning true if they should be
// swapped (i.e. 2 is more like the given name than 1 and hence 2
// should be higher up the list).
static bool compareFunctionFileName(const uRunnerFunctionDescription_t *pFunction1,
                                    const uRunnerFunctionDescription_t *pFunction2,
                                    const char *pName)
{
    bool swap = false;
    size_t x = strlen(pName);

    if (strcmp(pFunction1->pFile, pFunction2->pFile) != 0) {
        // Files are different, check the name
        swap = (strncmp(pName, pFunction1->pName, x) != 0) &&
               (strncmp(pName, pFunction2->pName, x) == 0);
    }

    return swap;
}

// Swap an entry with its next entry.
static void swap(uRunnerFunctionDescription_t **ppFunction)
{
    uRunnerFunctionDescription_t *pTmp;

    // Remember where the tail of the pair was pointing to
    pTmp = (*ppFunction)->pNext->pNext;
    // Set it to point to the first of the pair instead
    (*ppFunction)->pNext->pNext = *ppFunction;
    // Move the current pointer
    *ppFunction = (*ppFunction)->pNext;
    // Put the remembered next point back in
    (*ppFunction)->pNext->pNext = pTmp;
}

// Bring the things that start with pPrefixStr to the top of
// returning the number of items that begin with pPrefixStr.
static size_t bringToTopFunctionList(uRunnerFunctionDescription_t **ppFunctionList,
                                     const char *pPrefixStr)
{
    size_t movedCount = 0;
    uRunnerFunctionDescription_t **ppFunction = ppFunctionList;
    size_t x = strlen(pPrefixStr);

    while ((*ppFunction != NULL) && ((*ppFunction)->pNext != NULL)) {
        // If the next entry begins with pPrefixStr and this
        // one doesn't, swap them
        if (compareFunctionFileName(*ppFunction, (*ppFunction)->pNext, pPrefixStr)) {
            // Yup, swap 'em.
            swap(ppFunction);
            // Start again
            ppFunction = ppFunctionList;
        } else {
            // Just move on
            ppFunction = &((*ppFunction)->pNext);
        }
    }
    // Count how many of them we moved
    ppFunction = ppFunctionList;
    while ((ppFunction != NULL) && (*ppFunction != NULL)) {
        if ((strncmp((*ppFunction)->pName, pPrefixStr, x) == 0)) {
            movedCount++;
            ppFunction = &((*ppFunction)->pNext);
        } else {
            ppFunction = NULL;
        }
    }

    return movedCount;
}

// Sort the function list.  The sort order is as follows:
//
// 1.  Puts any function beginning with pPreambleStr at the top.
// 2.  Then puts anything beginning with pTopStr next.
// 3.  Functions within the same file are not sorted.
// 4.  Otherwise sorts alphabetically by group and then name.
static void sortFunctionList(uRunnerFunctionDescription_t **ppFunctionList,
                             const char *pPreambleStr,
                             const char *pTopStr)
{
    uRunnerFunctionDescription_t **ppFunctionStart = ppFunctionList;
    uRunnerFunctionDescription_t **ppFunction;
    size_t ignoreCount;

    // Bring everything that begins with pPreambleStr
    // up to the top
    ignoreCount = bringToTopFunctionList(ppFunctionStart, pPreambleStr);

    // Then, ignoring those we just moved, bring everything that begins
    // with pTopStr up to the top
    for (size_t x = 0; (x < ignoreCount) && (*ppFunctionStart != NULL); x++) {
        ppFunctionStart = &((*ppFunctionStart)->pNext);
    }
    ignoreCount += bringToTopFunctionList(ppFunctionStart, pTopStr);

    // Then ignoring all of the ones we've moved, sort the
    // rest alphabetically
    ppFunctionStart = ppFunctionList;
    for (size_t x = 0; (x < ignoreCount) && (*ppFunctionStart != NULL); x++) {
        ppFunctionStart = &((*ppFunctionStart)->pNext);
    }
    ppFunction = ppFunctionStart;
    while ((*ppFunction != NULL) && ((*ppFunction)->pNext != NULL)) {
        // Compare the current entry with the next
        // to see if they need to be swapped
        if (compareFunctionFileGroupName(*ppFunction,
                                         (*ppFunction)->pNext)) {
            // Yup, swap 'em.
            swap(ppFunction);
            // Start again
            ppFunction = ppFunctionStart;
        } else {
            // Just move on
            ppFunction = &((*ppFunction)->pNext);
        }
    }
}

// Determine if the given name is included in the filter.
bool nameInFilter(const char *pName, const char *pFilter)
{
    bool inFilter = false;
    char buffer[U_RUNNER_NAME_MAX_LENGTH_BYTES + 1]; // +1 for terminator
    size_t filterLengthWithTerminator = strlen(pFilter) + 1;
    size_t y = 0;

    for (size_t x = 0; (x < filterLengthWithTerminator) && !inFilter; x++) {
        if ((*(pFilter + x) != '.') && (*(pFilter + x) != 0) && (y < sizeof(buffer) - 1)) {
            buffer[y] = *(pFilter + x);
            y++;
        } else {
            if (y < sizeof(buffer) - 1) {
                y++;
                buffer[y] = 0;
                if (strncmp(buffer, pName, y - 1) == 0) {
                    inFilter = true;
                }
                y = 0;
            }
        }
    }

    return inFilter;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Add a function to the list.
void uRunnerFunctionRegister(uRunnerFunctionDescription_t *pDescription)
{
    uRunnerFunctionDescription_t **ppFunction = &gpFunctionList;

    // On some platforms (e.g. Zephyr on Linux) the constructors can
    // be called more than once, so first check if this function
    // is already present in the list
    while ((*ppFunction != NULL) && (*ppFunction != pDescription)) {
        ppFunction = &((*ppFunction)->pNext);
    }

    if (*ppFunction == NULL) {
#if defined(__XTENSA__) || (defined (_WIN32) && !defined (_MSC_VER))
        // For ESP-IDF (xtensa compiler) and on GCC under Windows
        // (but not on MSVC under Windows) the constructors are found
        // in reverse order so need to add them on the front here to
        // get them the right way around
        uRunnerFunctionDescription_t *pFunction = gpFunctionList;
        gpFunctionList = pDescription;
        pDescription->pNext = pFunction;
#else
        // Add to the end
        *ppFunction = pDescription;
        // IMPORTANT: set pNext to NULL
        // This is done here rather than in the static initialisation
        // of the function because, if we did it there, the value
        // would be overwritten with NULL in the case where the
        // constructor gets called twice
        pDescription->pNext = NULL;
#endif

        // Re-sort the function list with U_RUNNER_PREAMBLE_STR at the top,
        // then U_RUNNER_TOP_STR
        sortFunctionList(&gpFunctionList,
                         U_PORT_STRINGIFY_QUOTED(U_RUNNER_PREAMBLE_STR),
                         U_PORT_STRINGIFY_QUOTED(U_RUNNER_TOP_STR));
    }
}

// Print out the function names and groups
void uRunnerPrintAll(const char *pPrefix)
{
    const uRunnerFunctionDescription_t *pFunction = gpFunctionList;
    size_t count = 0;
    char buffer[16];

    while (pFunction != NULL) {
        UnityPrint(pPrefix);
        snprintf(buffer, sizeof(buffer), "%3.d: ", count + 1);
        UnityPrint(buffer);
        UnityPrint(pFunction->pName);
        UnityPrint(pFunction->pGroup);
        UNITY_PRINT_EOL();
        pFunction = pFunction->pNext;
        count++;
    }
    UNITY_PRINT_EOL();
}

// Run a named function.
void uRunnerRunNamed(const char *pName,
                     const char *pPrefix)
{
    const uRunnerFunctionDescription_t *pFunction = gpFunctionList;

    while (pFunction != NULL) {
        if ((pName == NULL) ||
            (strcmp(pFunction->pName, pName) == 0)) {
            runFunction(pFunction, pPrefix);
        }
        pFunction = pFunction->pNext;
    }
}

// Run all of the functions whose names
// begin with the given filter string.
void uRunnerRunFiltered(const char *pFilter,
                        const char *pPrefix)
{
    const uRunnerFunctionDescription_t *pFunction = gpFunctionList;

    while (pFunction != NULL) {
        if ((pFilter == NULL) || nameInFilter(pFunction->pName, pFilter)
#ifdef U_RUNNER_PREAMBLE_STR
            || (strncmp(U_PORT_STRINGIFY_QUOTED(U_RUNNER_PREAMBLE_STR),
                        pFunction->pName,
                        strlen(U_PORT_STRINGIFY_QUOTED(U_RUNNER_PREAMBLE_STR))) == 0)
#endif
           ) {
            runFunction(pFunction, pPrefix);
        }
        pFunction = pFunction->pNext;
    }
}

// Run all of the functions in a group.
void uRunnerRunGroup(const char *pGroup,
                     const char *pPrefix)
{
    const uRunnerFunctionDescription_t *pFunction = gpFunctionList;

    while (pFunction != NULL) {
        if ((pGroup == NULL) ||
            (strcmp(pFunction->pGroup, pGroup) == 0)) {
            runFunction(pFunction, pPrefix);
        }
        pFunction = pFunction->pNext;
    }
}

// Run all of the functions.
void uRunnerRunAll(const char *pPrefix)
{
    const uRunnerFunctionDescription_t *pFunction = gpFunctionList;

    while (pFunction != NULL) {
        runFunction(pFunction, pPrefix);
        pFunction = pFunction->pNext;
    }
}

// End of file
