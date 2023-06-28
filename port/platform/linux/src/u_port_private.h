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

#ifndef U_PORT_MAX_NUM_TASKS
/** The maximum number of tasks that can be created.
 */
#define U_PORT_MAX_NUM_TASKS 64
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

typedef struct uPortPrivateList_t {
    void *ptr;
    struct uPortPrivateList_t *pNext;
} uPortPrivateList_t;

/* ----------------------------------------------------------------
 * FUNCTIONS: LIST OF POINTERS
 * -------------------------------------------------------------- */

bool uPortPrivateListAdd(uPortPrivateList_t **ppList, void *ptr);

uPortPrivateList_t *uPortPrivateListFind(uPortPrivateList_t **ppList, void *ptr);

bool uPortPrivateListRemove(uPortPrivateList_t **ppList, void *ptr);

/* ----------------------------------------------------------------
 * FUNCTIONS: MISC
 * -------------------------------------------------------------- */

/** Initialise the private bits of the porting layer.
 *
 * @return: zero on success else negative error code.
 */
int32_t uPortPrivateInit(void);

/** Deinitialise the private bits of the porting layer.
 */
void uPortPrivateDeinit(void);

#ifdef __cplusplus
}
#endif

#endif  // _U_PORT_PRIVATE_H_

// End of file
