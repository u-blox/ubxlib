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

#ifndef _U_PORT_PRIVATE_H_
#define _U_PORT_PRIVATE_H_

/** @file
 * @brief Stuff private to the Linux porting layer.
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
typedef struct uPortPrivateList_t {
    void *p;
    struct uPortPrivateList_t *pNext;
} uPortPrivateList_t;

/* ----------------------------------------------------------------
 * FUNCTIONS SPECIFIC TO THIS PORT, LIST OF POINTERS
 * -------------------------------------------------------------- */

/** Add an entry to a linked list.
 *
 * @param[in] ppList  a pointer to the root of the linked list.
 * @param[in] p       the entry to add to the linked list.
 * @return            true if addition of the entry was successful,
 *                    else false (for example if no memory was available
 *                    for the linked-list container).
 */
bool uPortPrivateListAdd(uPortPrivateList_t **ppList, void *p);

/** Find an entry in a linked list.
 *
 * @param[in] ppList  a pointer to the root of the linked list.
 * @param[in] p       the entry to find.
 * @return            a pointer to the linked list entry if p is found,
 *                    else NULL.
 */
uPortPrivateList_t *uPortPrivateListFind(uPortPrivateList_t **ppList, void *p);

/** Remove an entry from a linked list.
 *
 * @param[in] ppList  a pointer to the root of the linked list.
 * @param[in] p       the entry to remove; note that the memory pointed
 *                    to by p is not touched in any way: if the caller
 *                    had allocated memory from the heap it is up to the
 *                    caller to free that memory.
 * @return            true if removal was successful, else false (for
 *                    example if the entry could not be found in the list).
 */
bool uPortPrivateListRemove(uPortPrivateList_t **ppList, void *p);

#ifdef __cplusplus
}
#endif

#endif  // _U_PORT_PRIVATE_H_

// End of file
