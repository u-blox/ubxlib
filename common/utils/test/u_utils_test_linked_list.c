/*
 * Copyright 2019-2024 u-blox
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
 * @brief Test for the linked list API
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // strncpy(), strcmp(), memcpy(), memset()

#include "u_cfg_sw.h"
#include "u_cfg_app_platform_specific.h"
#include "u_cfg_test_platform_specific.h"

#include "u_error_common.h"

#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"

#include "u_test_util_resource_check.h"

#include "u_linked_list.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The string to put at the start of all prints from this test.
 */
#define U_TEST_PREFIX "U_LINKED_LIST_TEST: "

/** Print a whole line, with terminator, prefixed for this test file.
 */
#define U_TEST_PRINT_LINE(format, ...) uPortLog(U_TEST_PREFIX format "\n", ##__VA_ARGS__)

#ifndef U_UTILS_TEST_LINKED_LIST_CONTENTS_1
/** The contents of the first linked list entry used during testing;
 * if you change this don't forget to change
 * #U_UTILS_TEST_LINKED_LIST_CONTENTS_LENGTH_1 also.
 */
# define  U_UTILS_TEST_LINKED_LIST_CONTENTS_1 "mumble"
#endif

#ifndef U_UTILS_TEST_LINKED_LIST_CONTENTS_2
/** The contents of the second linked list entry used during testing;
 * if you change this don't forget to change
 * #U_UTILS_TEST_LINKED_LIST_CONTENTS_LENGTH_2 also.
 */
# define  U_UTILS_TEST_LINKED_LIST_CONTENTS_2 "grumble"
#endif

#ifndef U_UTILS_TEST_LINKED_LIST_CONTENTS_LENGTH_1
/** The length of #U_UTILS_TEST_LINKED_LIST_CONTENTS_1 in bytes.
 */
# define  U_UTILS_TEST_LINKED_LIST_CONTENTS_LENGTH_1 7
#endif

#ifndef U_UTILS_TEST_LINKED_LIST_CONTENTS_LENGTH_2
/** The length of #U_UTILS_TEST_LINKED_LIST_CONTENTS_2 in bytes.
 */
# define  U_UTILS_TEST_LINKED_LIST_CONTENTS_LENGTH_2 8
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Root of the linked list used in testing.
 */
static uLinkedList_t *gpList = NULL;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: TESTS
 * -------------------------------------------------------------- */

U_PORT_TEST_FUNCTION("[linkedList]", "linkedListBasic")
{
    char contents1[] = U_UTILS_TEST_LINKED_LIST_CONTENTS_1;
    char contents2[] = U_UTILS_TEST_LINKED_LIST_CONTENTS_2;
    uLinkedList_t *pEntry = NULL;

    U_TEST_PRINT_LINE("testing linked list.");

    // Try passing in NULL pointer for list root, shouldn't go bang
    U_PORT_TEST_ASSERT(!uLinkedListAdd(NULL, contents1));
    U_PORT_TEST_ASSERT(pULinkedListFind(NULL, contents1) == NULL);
    U_PORT_TEST_ASSERT(!uLinkedListRemove(NULL, contents1));

    // Try to find/remove the entries before adding them
    U_PORT_TEST_ASSERT(pULinkedListFind(&gpList, contents1) == NULL);
    U_PORT_TEST_ASSERT(!uLinkedListRemove(&gpList, contents1));
    U_PORT_TEST_ASSERT(pULinkedListFind(&gpList, contents2) == NULL);
    U_PORT_TEST_ASSERT(!uLinkedListRemove(&gpList, contents2));

    // Add the first entry, make sure it is there and entry 2 still isn't
    U_PORT_TEST_ASSERT(uLinkedListAdd(&gpList, contents1));
    U_PORT_TEST_ASSERT(pULinkedListFind(&gpList, NULL) == NULL);
    U_PORT_TEST_ASSERT(!uLinkedListRemove(&gpList, NULL));
    pEntry = pULinkedListFind(&gpList, contents1);
    U_PORT_TEST_ASSERT(pEntry != NULL);
    U_PORT_TEST_ASSERT(pEntry->p == contents1);
    U_PORT_TEST_ASSERT(memcmp(pEntry->p, U_UTILS_TEST_LINKED_LIST_CONTENTS_1,
                              U_UTILS_TEST_LINKED_LIST_CONTENTS_LENGTH_1) == 0);
    U_PORT_TEST_ASSERT(pULinkedListFind(&gpList, contents2) == NULL);
    U_PORT_TEST_ASSERT(!uLinkedListRemove(&gpList, contents2));

    // Remove the first entry and make sure it's gone
    U_PORT_TEST_ASSERT(uLinkedListRemove(&gpList, contents1));
    U_PORT_TEST_ASSERT(pULinkedListFind(&gpList, contents1) == NULL);
    U_PORT_TEST_ASSERT(!uLinkedListRemove(&gpList, contents1));
    U_PORT_TEST_ASSERT(pULinkedListFind(&gpList, contents2) == NULL);
    U_PORT_TEST_ASSERT(!uLinkedListRemove(&gpList, contents2));

    // Add both entries and make sure they're there
    U_PORT_TEST_ASSERT(uLinkedListAdd(&gpList, contents1));
    U_PORT_TEST_ASSERT(uLinkedListAdd(&gpList, contents2));
    pEntry = pULinkedListFind(&gpList, contents2);
    U_PORT_TEST_ASSERT(pEntry != NULL);
    U_PORT_TEST_ASSERT(pEntry->p == contents2);
    U_PORT_TEST_ASSERT(memcmp(pEntry->p, U_UTILS_TEST_LINKED_LIST_CONTENTS_2,
                              U_UTILS_TEST_LINKED_LIST_CONTENTS_LENGTH_2) == 0);
    pEntry = pULinkedListFind(&gpList, contents1);
    U_PORT_TEST_ASSERT(pEntry != NULL);
    U_PORT_TEST_ASSERT(pEntry->p == contents1);
    U_PORT_TEST_ASSERT(memcmp(pEntry->p, U_UTILS_TEST_LINKED_LIST_CONTENTS_1,
                              U_UTILS_TEST_LINKED_LIST_CONTENTS_LENGTH_1) == 0);

    // Remove the first one we added, make sure it's gone and hasn't affected the second
    U_PORT_TEST_ASSERT(uLinkedListRemove(&gpList, contents1));
    pEntry = pULinkedListFind(&gpList, contents2);
    U_PORT_TEST_ASSERT(pEntry != NULL);
    U_PORT_TEST_ASSERT(pEntry->p == contents2);
    U_PORT_TEST_ASSERT(memcmp(pEntry->p, U_UTILS_TEST_LINKED_LIST_CONTENTS_2,
                              U_UTILS_TEST_LINKED_LIST_CONTENTS_LENGTH_2) == 0);
    U_PORT_TEST_ASSERT(!uLinkedListRemove(&gpList, contents1));
    U_PORT_TEST_ASSERT(pULinkedListFind(&gpList, contents1) == NULL);

    // Re-add the first one and remove the second this time
    U_PORT_TEST_ASSERT(uLinkedListAdd(&gpList, contents1));
    U_PORT_TEST_ASSERT(uLinkedListRemove(&gpList, contents2));
    pEntry = pULinkedListFind(&gpList, contents1);
    U_PORT_TEST_ASSERT(pEntry != NULL);
    U_PORT_TEST_ASSERT(pEntry->p == contents1);
    U_PORT_TEST_ASSERT(memcmp(pEntry->p, U_UTILS_TEST_LINKED_LIST_CONTENTS_1,
                              U_UTILS_TEST_LINKED_LIST_CONTENTS_LENGTH_1) == 0);
    U_PORT_TEST_ASSERT(pULinkedListFind(&gpList, contents2) == NULL);
    U_PORT_TEST_ASSERT(!uLinkedListRemove(&gpList, contents2));

    // Remove the first one and check they're both gone
    U_PORT_TEST_ASSERT(uLinkedListRemove(&gpList, contents1));
    U_PORT_TEST_ASSERT(pULinkedListFind(&gpList, contents1) == NULL);
    U_PORT_TEST_ASSERT(!uLinkedListRemove(&gpList, contents1));
    U_PORT_TEST_ASSERT(pULinkedListFind(&gpList, contents2) == NULL);
    U_PORT_TEST_ASSERT(!uLinkedListRemove(&gpList, contents2));

    // Memory leak checking is done in the clean-up
}

/** Clean-up to be run at the end of this round of tests, just
 * in case there were test failures which would have resulted
 * in the deinitialisation being skipped.
 */
U_PORT_TEST_FUNCTION("[linkedList]", "linkedListCleanUp")
{
    U_TEST_PRINT_LINE("cleaning up any outstanding resources.\n");

    while (gpList != NULL) {
        uLinkedListRemove(&gpList, gpList->p);
    }

    // Printed for information: asserting happens in the postamble
    uTestUtilResourceCheck(U_TEST_PREFIX, NULL, true);
}

// End of file