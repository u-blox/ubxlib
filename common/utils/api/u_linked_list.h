/*
 * Copyright 2019-2023 u-blox
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

#ifndef _U_LINKED_LIST_H_
#define _U_LINKED_LIST_H_

/** @file
 * @brief Linked list utilities.  These functions are NOT thread-safe:
 * should that be required you must provide it with some form of
 * mutex before the functions are called.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure to hold a linked-list entry.
 */
typedef struct uLinkedList_t {
    void *p;
    struct uLinkedList_t *pNext;
} uLinkedList_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Add an entry to the END of the linked list.  This function is
 * NOT thread-safe.
 *
 * @param[in] ppList  a pointer to the root of the linked list,
 *                    cannot be NULL.
 * @param[in] p       the entry to add to the linked list.
 * @return            true if addition of the entry was successful,
 *                    else false (for example if no memory was available
 *                    for the linked-list container).
 */
bool uLinkedListAdd(uLinkedList_t **ppList, void *p);

/** Find an entry in a linked list.  This function is NOT thread-safe.
 *
 * @param[in] ppList  a pointer to the root of the linked list,
 *                    cannot be NULL.
 * @param[in] p       the entry to find.
 * @return            a pointer to the linked list entry if p is found,
 *                    else NULL.
 */
uLinkedList_t *pULinkedListFind(uLinkedList_t **ppList, void *p);

/** Remove an entry from a linked list.  This function is NOT thread-safe.
 *
 * @param[in] ppList  a pointer to the root of the linked list,
 *                    cannot be NULL.
 * @param[in] p       the entry to remove; note that the memory pointed
 *                    to by p is not touched in any way: if the caller
 *                    had allocated memory from the heap it is up to the
 *                    caller to free that memory.
 * @return            true if removal was successful, else false (for
 *                    example if the entry could not be found in the list).
 */
bool uLinkedListRemove(uLinkedList_t **ppList, void *p);

#ifdef __cplusplus
}
#endif

#endif  // _U_LINKED_LIST_H_

// End of file
