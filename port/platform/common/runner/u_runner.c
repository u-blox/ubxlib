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
#include "stdio.h"  // For snprintf()
#include "string.h" // For strstr() and strcmp()

#include "u_port.h"
#include "u_runner.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

// The prefix string which should always be sorted to the
// top of the function list.  Note that there should be NO QUOTES
// around the string: this is because when a definition for this
// is passed in from outside some tools, e.g. Make, don't like
// the quotes and so quotes are added later.
#ifndef U_RUNNER_TOP_STR
# define U_RUNNER_TOP_STR example
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

// Linked list anchor.
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
static bool compare(const uRunnerFunctionDescription_t *pFunction1,
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

// Sort the function list.  The sort order is as follows:
//
// 1.  Puts any function beginning with pTopStr at the top.
// 2.  Functions within the same file are not sorted.
// 3.  Otherwise sorts alphabetically by group and then name.
static void sortFunctionList(uRunnerFunctionDescription_t **ppFunctionList,
                             const char *pTopStr)
{
    uRunnerFunctionDescription_t **ppFunction = ppFunctionList;

    // First sort everything alphabetically
    while ((*ppFunction != NULL) && ((*ppFunction)->pNext != NULL)) {
        // Compare the current entry with the next
        // to see if they need to be swapped
        if (compare(*ppFunction, (*ppFunction)->pNext)) {
            // Yup, swap 'em.
            swap(ppFunction);
            // Start again
            ppFunction = ppFunctionList;
        } else {
            // Just move on
            ppFunction = &((*ppFunction)->pNext);
        }
    }

    // Then bring everything that begins with pTopStr
    // up to the top
    ppFunction = ppFunctionList;
    while ((*ppFunction != NULL) && ((*ppFunction)->pNext != NULL)) {
        // If the next entry begins with pTopStr and this
        // one doesn't, swap them
        if ((strstr((*ppFunction)->pName, pTopStr) != (*ppFunction)->pName) &&
            (strstr((*ppFunction)->pNext->pName, pTopStr) == (*ppFunction)->pNext->pName)) {
            // Yup, swap 'em.
            swap(ppFunction);
            // Start again
            ppFunction = ppFunctionList;
        } else {
            // Just move on
            ppFunction = &((*ppFunction)->pNext);
        }
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Add a function to the list.
void uRunnerFunctionRegister(uRunnerFunctionDescription_t *pDescription)
{
    uRunnerFunctionDescription_t **ppFunction = &gpFunctionList;

    while (*ppFunction != NULL) {
        ppFunction = &((*ppFunction)->pNext);
    }
    *ppFunction = pDescription;

    // Re-sort the function list
    sortFunctionList(&gpFunctionList, U_PORT_STRINGIFY_QUOTED(U_RUNNER_TOP_STR));
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
        UnityPrint(" [");
        UnityPrint(pFunction->pGroup);
        UnityPrint("]");
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
        if ((pFilter == NULL) ||
            (strstr(pFunction->pName, pFilter) == pFunction->pName)) {
            runFunction(pFunction, pPrefix);
        }
        pFunction = pFunction->pNext;
    }
}

// Run all of the function in a group.
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
