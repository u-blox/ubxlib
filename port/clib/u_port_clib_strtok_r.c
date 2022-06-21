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

/** @file
 * @brief An implementation of the C library function strok_r().
 */

#include "string.h"
#include "u_port_clib_platform_specific.h"

/** strtok_r(): split a string into sub-strings at the
 * given delimiter by modifying the string in-place.  This function
 * is thread-safe.
 *
 * @param pStr        on first call this should be a pointer to
 *                    the string to tokenise.  On subsequent calls
 *                    it must be NULL to return further tokens.
 *                    The contents of pStr are modified by this
 *                    function (NULLs being written to delineate
 *                    sub-strings).
 * @param pDelimiters the delimiters to tokenise based on.
 * @param ppSave      a pointer to a pointer that can be used to
 *                    save context between calls.
 * @return            the token, NULL terminated.
 */
char *strtok_r(char *pStr, const char *pDelimiters, char **ppSave)
{
    char *pEnd;

    if (pStr == NULL) {
        // On subsequent calls, with pStr NULL,
        // set pStr to start from the saved position
        pStr = *ppSave;
    }

    if (*pStr != '\0') {
        // Find the position of any delimiters
        // at the beginning of the string/saved pointer
        pStr += strspn(pStr, pDelimiters);
        if (*pStr != '\0') {
            // Having found a token, find the start
            // of the next one
            pEnd = pStr + strcspn(pStr, pDelimiters);
            if (*pEnd != '\0') {
                // Found one: write a NULL to the position
                // of the start of the next token
                // in order to make sure the returned
                // token is terminated and make the
                // saved pointer point beyond it for
                // next time.
                *pEnd = '\0';
                *ppSave = ++pEnd;
            } else {
                // No next one, save the end
                // for next time
                *ppSave = pEnd;
            }
        } else {
            // None at all, save it and
            // set the return value to NULL
            *ppSave = pStr;
            pStr = NULL;
        }
    } else {
        // If we're at the terminator,
        // save where we are and set the
        // return value to NULL
        *ppSave = pStr;
        pStr = NULL;
    }

    return pStr;
}

// End of file
